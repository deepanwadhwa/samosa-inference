#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "speech_package_loader.h"
#include "engine_runtime.h"
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int same(const struct stat *a,const struct stat *b) {
    if(a->st_dev!=b->st_dev||a->st_ino!=b->st_ino||a->st_size!=b->st_size||
        a->st_mode!=b->st_mode||a->st_uid!=b->st_uid||a->st_nlink!=b->st_nlink)return 0;
#ifdef __APPLE__
    return a->st_mtimespec.tv_sec==b->st_mtimespec.tv_sec&&a->st_mtimespec.tv_nsec==b->st_mtimespec.tv_nsec&&
        a->st_ctimespec.tv_sec==b->st_ctimespec.tv_sec&&a->st_ctimespec.tv_nsec==b->st_ctimespec.tv_nsec;
#else
    return a->st_mtim.tv_sec==b->st_mtim.tv_sec&&a->st_mtim.tv_nsec==b->st_mtim.tv_nsec&&
        a->st_ctim.tv_sec==b->st_ctim.tv_sec&&a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
#endif
}
static int open_file(int root,const char *name) {
    char path[256];if(strlen(name)>=sizeof(path))return -1;strcpy(path,name);
    int parent=openat(root,".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(parent<0)return -1;
    char *begin=path,*slash;
    while((slash=strchr(begin,'/'))) {
        *slash=0;int next=openat(parent,begin,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        close(parent);if(next<0)return -1;parent=next;begin=slash+1;
    }
    int fd=openat(parent,begin,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);close(parent);return fd;
}
static int basename_valid(const char *name) {
    if(!name||!*name||*name=='.'||strlen(name)>255)return 0;
    for(const unsigned char *p=(const unsigned char *)name;*p;p++)
        if(!((*p>='a'&&*p<='z')||(*p>='A'&&*p<='Z')||(*p>='0'&&*p<='9')||strchr("-_.",*p)))return 0;
    return 1;
}
static int sibling(const SpeechPackageReceipt *r,size_t source,const char *base) {
    const char *slash=strrchr(r->files[source].name,'/');
    size_t prefix=slash?(size_t)(slash-r->files[source].name)+1:0;
    char name[256];
    if(prefix+strlen(base)>=sizeof(name))return -1;
    memcpy(name,r->files[source].name,prefix);strcpy(name+prefix,base);
    for(size_t i=0;i<r->count;i++)if(!strcmp(name,r->files[i].name))return (int)i;
    return -1;
}
const char *speech_package_loader_verify(int root,const char *path,const char *pin,
    const LibraryRuntimePolicy *policy,const char *(*control)(void *),void *context,SpeechPackageReceipt *r) {
    if(!policy||!r||!r->verified||!r->count||r->count>SPEECH_PACKAGE_FILES)return "invalid_configuration";
    const char *e=engine_runtime_environment();
    if(!e)e=speech_package_verify(root,path,pin,control,context,r,1);
    if(e)return e;
    LibraryRuntimeImage *images=calloc(r->count,sizeof(*images));
    if(!images)return "resource_limit";
    /* Inspect every declared native dependency, including retained aliases and
     * currently unreferenced libraries. No declared binary escapes the gate. */
    for(size_t i=0;!e&&i<r->count;i++) {
        if(r->files[i].role!=0&&r->files[i].role!=14)continue;
        e=control?control(context):NULL;if(e)break;
        int fd=open_file(root,r->files[i].name);struct stat before,after;
        if(fd<0){e="artifact_changed";break;}
        if(fstat(fd,&before)||!same(&before,&r->files[i].identity))e="artifact_changed";
        if(!e)e=library_runtime_inspect(fd,policy,&images[i]);
        if(!e&&(fstat(fd,&after)||!same(&before,&after)))e="artifact_changed";
        close(fd);
    }
    for(size_t i=0;!e&&i<r->count;i++) {
        if(r->files[i].role!=0&&r->files[i].role!=14)continue;
        e=control?control(context):NULL;if(e)break;
        for(size_t k=0;!e&&k<images[i].count;k++) {
            const char *name=images[i].dependencies[k].name,*base=name;
            if(engine_trusted_system_library(policy->mach_o,name)) {
                /* ELF bare system names participate in origin search; refuse
                 * a package shadow instead of trusting its OS-looking name. */
                if(!policy->mach_o&&sibling(r,i,name)>=0)e="unqualified_library_dependencies";
                continue;
            }
            if(policy->mach_o) {
                if(!strncmp(name,"@loader_path/",13))base=name+13;
                else if(!strncmp(name,"@rpath/",7)&&images[i].origin_search)base=name+7;
                else {e="unqualified_library_dependencies";break;}
            } else if(!images[i].origin_search){e="unqualified_library_dependencies";break;}
            if(!basename_valid(base)){e="unqualified_library_dependencies";break;}
            int target=sibling(r,i,base);
            if(target<0||(r->files[target].role!=0&&r->files[target].role!=14))e="unqualified_library_dependencies";
            else if(policy->mach_o&&images[target].current_version<images[i].dependencies[k].compatibility_version)
                e="incompatible_library_version";
        }
    }
    if(!e)e=speech_package_verify(root,path,pin,control,context,r,1);
    free(images);return e;
}
