#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_assemble.h"
#include "package_compatibility.h"
#include "package_stage.h"
#include "catalog.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
static int private_directory(int fd){struct stat s;return !fstat(fd,&s)&&S_ISDIR(s.st_mode)&&s.st_uid==geteuid()&&!(s.st_mode&077)&&
    (fcntl(fd,F_GETFL)&O_ACCMODE)==O_RDONLY;}
static int bound(int parent,const char *name,int fd){struct stat a,b;return !fstat(fd,&a)&&!fstatat(parent,name,&b,AT_SYMLINK_NOFOLLOW)&&a.st_dev==b.st_dev&&a.st_ino==b.st_ino&&a.st_mode==b.st_mode&&a.st_uid==b.st_uid&&a.st_nlink==b.st_nlink;}
static int path_bound(const char *path,int fd){int named=open(path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);struct stat a,b;
    int same=named>=0&&!fstat(named,&a)&&!fstat(fd,&b)&&a.st_dev==b.st_dev&&a.st_ino==b.st_ino;if(named>=0)close(named);return same;}
const char *package_assemble(const PackageAssembly *c,PackageTreeResult *out){
    if(!c||!out||!c->index||!c->payload_path||c->payload_path[0]!='/'||!c->copy_ceiling||
        !c->index->count||c->index->count>PACKAGE_INDEX_FILES)return "invalid_package_assembly";
    const char *e=c->control?c->control(c->context):NULL;if(e)return e;
    if(c->index->payload_bytes>c->copy_ceiling)return "package_copy_quota_exceeded";
    if(!memchr(c->index->recipe,0,sizeof(c->index->recipe))||strcmp(c->index->recipe,"sherpa-speech-package-v1"))return "unsupported_package_recipe";
    if(!private_directory(c->directory)||(!c->source_open&&!private_directory(c->source)))return "unsafe_package_assembly";
    char platform[65],os[65];e=provider_catalog_host(platform,os);if(!e)e=package_index_qualify(c->index,platform,os);if(e)return e;
    int lock=openat(c->directory,"assemble.lock",O_RDWR|O_CREAT|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC,0600);
    if(lock<0)return "unsafe_package_assembly";
    struct stat identity;int payload=-1;
    if(fstat(lock,&identity)||!S_ISREG(identity.st_mode)||identity.st_uid!=geteuid()||(identity.st_mode&077)||identity.st_nlink!=1||!bound(c->directory,"assemble.lock",lock))e="unsafe_package_assembly";
    if(!e&&flock(lock,LOCK_EX|LOCK_NB))e=errno==EWOULDBLOCK?"package_assembly_busy":"package_assembly_unavailable";
    /* Existing completion is never repaired by overwriting or recopying bytes. */
    PackageCompatibilityContext compatibility={.payload_path=c->payload_path,.control=c->control,.context=c->context};
    PackagePublication publication={.directory=c->directory,.index=c->index,.compatible=package_backend_compatible,.compatibility=&compatibility,.control=c->control,.context=c->context};
    PackageTreeResult observed;
    int complete=0;
    if(!e){struct stat marker;if(!fstatat(c->directory,"complete",&marker,AT_SYMLINK_NOFOLLOW))complete=1;
        else if(errno!=ENOENT)e="unsafe_package_assembly";}
    if(!e&&complete){payload=openat(c->directory,"payload",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        if(payload<0||!private_directory(payload)||!bound(c->directory,"payload",payload)||!path_bound(c->payload_path,payload))e="unsafe_package_assembly";
        if(!e)e=package_publication_verify(&publication,&observed);}

    if(!e&&!complete&&!c->source_open)e=package_tree_verify(c->source,c->index,c->control,c->context,&observed);
    if(!e&&!complete){if(mkdirat(c->directory,"payload",0700)&&errno!=EEXIST)e="package_assembly_failed";
        if(!e){payload=openat(c->directory,"payload",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
            if(payload<0||!private_directory(payload)||!bound(c->directory,"payload",payload)||!path_bound(c->payload_path,payload))e="unsafe_package_assembly";}
        if(!e&&fsync(c->directory))e="package_assembly_failed";
        for(size_t i=0;!e&&i<c->index->count;i++){
            int input=-1;e=c->control?c->control(c->context):NULL;
            if(!e&&c->source_open)e=c->source_open(c->source_context,&c->index->files[i],c->control,c->context,&input);
            else if(!e)input=openat(c->source,c->index->files[i].name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
            if(!e&&input<0)e="package_source_unavailable";
            if(!e)e=package_stage_file(payload,c->index,i,input,c->control,c->context);
            if(input>=0)close(input);
        }
        /* Detect observed source changes as well as destination changes before
         * publishing; acquired source ownership remains the caller's contract. */
        if(!e&&!c->source_open)e=package_tree_verify(c->source,c->index,c->control,c->context,&observed);
        if(!e&&(!bound(c->directory,"payload",payload)||!path_bound(c->payload_path,payload)||!bound(c->directory,"assemble.lock",lock)))e="package_tree_changed";
        if(!e)e=package_publish(&publication,&observed);
    }
    if(!e&&(!bound(c->directory,"assemble.lock",lock)||!bound(c->directory,"payload",payload)||!path_bound(c->payload_path,payload)))e="package_tree_changed";
    if(!e)*out=observed;
    if(payload>=0)close(payload);close(lock);return e;
}
