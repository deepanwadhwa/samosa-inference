#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_remove.h"
#include "package_identity.h"
#include "artifact.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <unistd.h>
static int matches(const PackageRemovalIdentity *id,const struct stat *s,int exact){
    uint64_t v[10]={(uint64_t)s->st_dev,(uint64_t)s->st_ino,(uint64_t)s->st_uid,(uint64_t)s->st_mode,(uint64_t)s->st_nlink,(uint64_t)s->st_size,0,0,0,0};
#ifdef __APPLE__
    v[6]=(uint64_t)s->st_mtimespec.tv_sec;v[7]=(uint64_t)s->st_mtimespec.tv_nsec;v[8]=(uint64_t)s->st_ctimespec.tv_sec;v[9]=(uint64_t)s->st_ctimespec.tv_nsec;
#else
    v[6]=(uint64_t)s->st_mtim.tv_sec;v[7]=(uint64_t)s->st_mtim.tv_nsec;v[8]=(uint64_t)s->st_ctim.tv_sec;v[9]=(uint64_t)s->st_ctim.tv_nsec;
#endif
    return !memcmp(v,id->value,(exact?10u:4u)*sizeof(*v));
}
static int bound(int parent,const char *name,int fd){struct stat a,b;return !fstat(fd,&a)&&!fstatat(parent,name,&b,AT_SYMLINK_NOFOLLOW)&&a.st_dev==b.st_dev&&a.st_ino==b.st_ino&&a.st_mode==b.st_mode&&a.st_nlink==b.st_nlink;}
static int directory_index(const PackageRemovalRecord *r,const char *name){for(size_t i=0;i<r->directory_count;i++)if(!strcmp(r->directories[i].name,name))return (int)i;return -1;}
static const char *file_check(int parent,const char *leaf,const PackageFile *file,const PackageRemovalIdentity *id,const char *(*control)(void *),void *context){
    int fd=openat(parent,leaf,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);struct stat s;const char *e=NULL;
    if(fd<0||fstat(fd,&s)||!matches(id,&s,1)||!bound(parent,leaf,fd))e="unsafe_package_removal_payload";
    if(!e)e=artifact_verify_controlled(fd,file->bytes,PACKAGE_PAYLOAD_BYTES,file->sha256,control,context);
    if(!e&&(fstat(fd,&s)||!matches(id,&s,1)||!bound(parent,leaf,fd)))e="unsafe_package_removal_payload";
    if(fd>=0)close(fd);return e;
}
static const char *walk(int fd,const char *prefix,const PackageIndex *index,const PackageRemovalRecord *r,size_t *visited,const char *(*control)(void *),void *context){
    int copy=openat(fd,".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);DIR *scan=copy>=0?fdopendir(copy):NULL;
    if(!scan){if(copy>=0)close(copy);return "unsafe_package_removal_payload";}const char *e=NULL;
    for(;;){e=control?control(context):NULL;if(e)break;errno=0;struct dirent *entry=readdir(scan);if(!entry){if(errno)e="unsafe_package_removal_payload";break;}
        if(!strcmp(entry->d_name,".")||!strcmp(entry->d_name,".."))continue;
        if(++*visited>r->file_count+r->directory_count){e="unsafe_package_removal_payload";break;}
        char path[256];int n=snprintf(path,sizeof(path),"%s%s%s",prefix,*prefix?"/":"",entry->d_name);struct stat s;
        if(n<0||(size_t)n>=sizeof(path)||fstatat(fd,entry->d_name,&s,AT_SYMLINK_NOFOLLOW)){e="unsafe_package_removal_payload";break;}
        if(S_ISREG(s.st_mode)){size_t i=0;for(;i<index->count;i++)if(!strcmp(index->files[i].name,path))break;
            if(i==index->count)e="unsafe_package_removal_payload";else e=file_check(fd,entry->d_name,&index->files[i],&r->files[i],control,context);
        }else if(S_ISDIR(s.st_mode)){int i=directory_index(r,path),child=-1;
            if(i<0||!matches(&r->directories[i].identity,&s,0))e="unsafe_package_removal_payload";
            else{child=openat(fd,entry->d_name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
                if(child<0||!bound(fd,entry->d_name,child))e="unsafe_package_removal_payload";
                if(!e)e=walk(child,path,index,r,visited,control,context);if(child>=0)close(child);}
        }else e="unsafe_package_removal_payload";
        if(e)break;
    }
    closedir(scan);return e;
}
/* -2 means a recorded ancestor is already absent; all existing ancestors must
 * still be their captured private inodes before any leaf can be touched. */
static int parent_open(int root,const PackageRemovalRecord *r,const char *name,char leaf[256]){
    strcpy(leaf,name);int parent=openat(root,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC);if(parent<0)return -1;
    char prefix[256]={0};char *part=leaf;
    for(char *slash=strchr(part,'/');slash;slash=strchr(part,'/')){*slash=0;size_t length=strlen(prefix);if(length)strcat(prefix,"/");strcat(prefix,part);
        int i=directory_index(r,prefix);struct stat s;if(i<0){close(parent);return -1;}
        if(fstatat(parent,part,&s,AT_SYMLINK_NOFOLLOW)){int absent=errno==ENOENT;close(parent);return absent?-2:-1;}
        if(!matches(&r->directories[i].identity,&s,0)){close(parent);return -1;}
        int next=openat(parent,part,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(next<0||!bound(parent,part,next)){if(next>=0)close(next);close(parent);return -1;}close(parent);parent=next;part=slash+1;
    }
    memmove(leaf,part,strlen(part)+1);return parent;
}
static int deepest(const void *a,const void *b){const PackageRemovalDirectory *x=a,*y=b;size_t nx=strlen(x->name),ny=strlen(y->name);return nx>ny?-1:nx<ny?1:strcmp(x->name,y->name);}
const char *package_remove_payload(int directory,const PackageIndex *index,const char *(*control)(void *),void *context){
    if(!index)return "invalid_package_removal_record";const char *e=control?control(context):NULL;if(e)return e;
    int lock=openat(directory,"assemble.lock",O_RDWR|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);struct stat s;int payload=-1,completion=-1;
    if(lock<0)return "unsafe_package_removal_payload";
    if(fstat(lock,&s)||!S_ISREG(s.st_mode)||s.st_uid!=geteuid()||(s.st_mode&077)||s.st_nlink!=1||!bound(directory,"assemble.lock",lock))e="unsafe_package_removal_payload";
    if(!e&&flock(lock,LOCK_EX|LOCK_NB))e=errno==EWOULDBLOCK?"package_assembly_busy":"package_assembly_unavailable";
    if(!e)e=package_identity_verify(directory,index,control,context);
    PackageRemovalRecord *r=NULL;if(!e){r=malloc(sizeof(*r));if(!r)e="allocation_failed";}
    if(!e)e=package_removal_record_load(directory,index,control,context,r);
    if(!e){payload=openat(directory,"payload",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        if(payload<0||fstat(payload,&s)||!matches(&r->payload,&s,0)||!bound(directory,"payload",payload))e="unsafe_package_removal_payload";}
    if(!e){completion=openat(directory,"complete",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);char expected[256],actual[256];int length=snprintf(expected,sizeof(expected),"samosa.package.complete.v1\n%s\n%llu %llu\n",index->manifest_sha256,(unsigned long long)r->payload.value[0],(unsigned long long)r->payload.value[1]);
        if(completion<0||fstat(completion,&s)||!matches(&r->completion,&s,1)||!bound(directory,"complete",completion)||s.st_size!=length||pread(completion,actual,(size_t)length,0)!=length||memcmp(actual,expected,(size_t)length))e="unsafe_package_removal_payload";}
    size_t visited=0;if(!e)e=walk(payload,"",index,r,&visited,control,context);
    for(size_t i=0;!e&&i<index->count;i++){
        e=control?control(context):NULL;if(e)break;char leaf[256];int parent=parent_open(payload,r,index->files[i].name,leaf);if(parent==-2)continue;if(parent<0){e="unsafe_package_removal_payload";break;}
        if(fstatat(parent,leaf,&s,AT_SYMLINK_NOFOLLOW)){if(errno!=ENOENT)e="unsafe_package_removal_payload";}
        else{e=file_check(parent,leaf,&index->files[i],&r->files[i],control,context);if(!e)e=control?control(context):NULL;
            if(!e&&(fstatat(parent,leaf,&s,AT_SYMLINK_NOFOLLOW)||!matches(&r->files[i],&s,1)||!bound(directory,"payload",payload)||!bound(directory,"assemble.lock",lock)))e="unsafe_package_removal_payload";
            if(!e&&unlinkat(parent,leaf,0))e="package_removal_failed";}
        if(!e&&fsync(parent))e="package_removal_failed";close(parent);
    }
    if(!e)qsort(r->directories,r->directory_count,sizeof(*r->directories),deepest);
    for(size_t i=0;!e&&i<r->directory_count;i++){
        e=control?control(context):NULL;if(e)break;char leaf[256];int parent=parent_open(payload,r,r->directories[i].name,leaf);if(parent==-2)continue;if(parent<0){e="unsafe_package_removal_payload";break;}
        if(fstatat(parent,leaf,&s,AT_SYMLINK_NOFOLLOW)){if(errno!=ENOENT)e="unsafe_package_removal_payload";}
        else if(!matches(&r->directories[i].identity,&s,0)||!bound(directory,"payload",payload)||!bound(directory,"assemble.lock",lock))e="unsafe_package_removal_payload";
        else if(unlinkat(parent,leaf,AT_REMOVEDIR))e="package_removal_failed";
        if(!e&&fsync(parent))e="package_removal_failed";close(parent);
    }
    if(!e){visited=0;e=walk(payload,"",index,r,&visited,control,context);if(!e&&visited)e="package_removal_failed";}
    if(!e&&(fsync(payload)||!bound(directory,"payload",payload)||!bound(directory,"assemble.lock",lock)))e="package_removal_failed";
    free(r);if(completion>=0)close(completion);if(payload>=0)close(payload);close(lock);return e;
}
