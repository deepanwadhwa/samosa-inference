#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_remove.h"
#include "package_identity.h"
#include "package_reserve.h"
#include "package_stage.h"
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
        int i=directory_index(r,prefix);struct stat s;
        if(fstatat(parent,part,&s,AT_SYMLINK_NOFOLLOW)){int absent=errno==ENOENT;close(parent);return absent?-2:-1;}
        if(i<0||!matches(&r->directories[i].identity,&s,0)){close(parent);return -1;}
        int next=openat(parent,part,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(next<0||!bound(parent,part,next)){if(next>=0)close(next);close(parent);return -1;}close(parent);parent=next;part=slash+1;
    }
    memmove(leaf,part,strlen(part)+1);return parent;
}
static int deepest(const void *a,const void *b){const PackageRemovalDirectory *x=a,*y=b;size_t nx=strlen(x->name),ny=strlen(y->name);return nx>ny?-1:nx<ny?1:strcmp(x->name,y->name);}
static const char *lease(int directory,int *out){
    int fd=openat(directory,"assemble.lock",O_RDWR|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);struct stat s;
    if(fd<0)return "unsafe_package_removal_payload";const char *e=NULL;
    if(fstat(fd,&s)||!S_ISREG(s.st_mode)||s.st_uid!=geteuid()||(s.st_mode&077)||s.st_nlink!=1||!bound(directory,"assemble.lock",fd))e="unsafe_package_removal_payload";
    if(!e&&flock(fd,LOCK_EX|LOCK_NB))e=errno==EWOULDBLOCK?"package_assembly_busy":"package_assembly_unavailable";
    if(e)close(fd);else *out=fd;return e;
}
static const char *incomplete_namespace(int directory,const char *(*control)(void *),void *context){
    int fd=openat(directory,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC);DIR *scan=fd>=0?fdopendir(fd):NULL;
    if(!scan){if(fd>=0)close(fd);return "unsafe_package_removal_payload";}const char *e=NULL;
    for(;;){e=control?control(context):NULL;if(e)break;errno=0;struct dirent *entry=readdir(scan);if(!entry){if(errno)e="unsafe_package_removal_payload";break;}
        const char *name=entry->d_name;if(!strcmp(name,".")||!strcmp(name,".."))continue;
        int dir=!strcmp(name,"payload")||!strcmp(name,"copy-journal"),identity=!strcmp(name,"assembly.identity");
        if(!dir&&!identity&&strcmp(name,"assemble.lock")&&strcmp(name,"remove.lock")&&strcmp(name,"publish.lock")){e="unsafe_package_removal_payload";break;}
        int item=openat(directory,name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC|(dir?O_DIRECTORY:0));struct stat s;
        if(item<0||fstat(item,&s)||s.st_uid!=geteuid()||(s.st_mode&077)||!bound(directory,name,item)||
           (dir?!S_ISDIR(s.st_mode):!S_ISREG(s.st_mode)||s.st_nlink!=1||(!identity&&s.st_size)))e="unsafe_package_removal_payload";
        if(item>=0)close(item);if(e)break;
    }
    closedir(scan);return e;
}
const char *package_remove_begin(int directory,const PackageIndex *index,const char *(*control)(void *),void *context){
    if(!index)return "invalid_package_removal_record";const char *e=control?control(context):NULL;if(e)return e;
    int lock=-1,payload=-1,completion=-1;e=lease(directory,&lock);if(e)return e;
    e=package_identity_verify(directory,index,control,context);PackageRemovalRecord *r=NULL;PackageTreeSnapshot *snapshot=NULL;
    if(!e){r=malloc(sizeof(*r));if(!r)e="allocation_failed";}
    if(!e){e=package_removal_record_load_pending(directory,index,control,context,r);
        if(!e){e=package_removal_record_store(directory,index,r,control,context);goto done;}
        if(strcmp(e,"package_removal_record_absent"))goto done;
        e=package_removal_record_load(directory,index,control,context,r);
        if(!e)goto done;if(strcmp(e,"package_removal_record_absent"))goto done;e=NULL;
    }
    if(!e){snapshot=malloc(sizeof(*snapshot));if(!snapshot)e="allocation_failed";}
    if(!e){payload=openat(directory,"payload",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(payload<0||!bound(directory,"payload",payload))e="unsafe_package_removal_payload";}
    struct stat package_stat,complete_stat;
    if(!e){completion=openat(directory,"complete",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        if(completion<0&&errno!=ENOENT)e="unsafe_package_removal_payload";
        if(!e&&(fstat(directory,&package_stat)||(completion>=0&&(fstat(completion,&complete_stat)||!bound(directory,"complete",completion)))))e="unsafe_package_removal_payload";}
    if(!e&&completion<0){
        /* A pending completion must be recovered through publication first;
         * it cannot silently become authority for a partial-tree deletion. */
        if(!fstatat(directory,".complete.pending",&complete_stat,AT_SYMLINK_NOFOLLOW)||errno!=ENOENT)e="unsafe_package_removal_payload";
        if(!e)e=incomplete_namespace(directory,control,context);
        int journal=-1;if(!e){journal=openat(directory,"copy-journal",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);if((journal<0&&errno!=ENOENT)||(journal>=0&&!bound(directory,"copy-journal",journal)))e="unsafe_package_removal_payload";}
        if(!e)e=package_stage_prepare_removal(payload,journal,index,control,context);
        if(!e&&journal>=0&&!bound(directory,"copy-journal",journal))e="unsafe_package_removal_payload";
        if(journal>=0)close(journal);
        if(!e)e=package_tree_snapshot_incomplete(payload,index,control,context,snapshot);
        if(!e)e=package_removal_record_capture_incomplete(index,snapshot,&package_stat,r);
    }else if(!e){e=package_tree_snapshot(payload,index,control,context,snapshot);if(!e)e=package_removal_record_capture(index,snapshot,&package_stat,&complete_stat,r);}
    if(!e&&completion>=0){char expected[256],actual[256];int length=snprintf(expected,sizeof(expected),"samosa.package.complete.v1\n%s\n%llu %llu\n",index->manifest_sha256,(unsigned long long)r->payload.value[0],(unsigned long long)r->payload.value[1]);
        if(complete_stat.st_size!=length||pread(completion,actual,(size_t)length,0)!=length||memcmp(expected,actual,(size_t)length))e="unsafe_package_removal_payload";}
    if(!e)e=control?control(context):NULL;
    if(!e){struct stat current;
        if((completion>=0&&(fstat(completion,&current)||!matches(&r->completion,&current,1)||!bound(directory,"complete",completion)))||(completion<0&&(!fstatat(directory,"complete",&current,AT_SYMLINK_NOFOLLOW)||errno!=ENOENT))||!bound(directory,"payload",payload)||!bound(directory,"assemble.lock",lock))e="unsafe_package_removal_payload";}
    if(!e)e=package_removal_record_store(directory,index,r,control,context);
done:
    free(snapshot);free(r);if(completion>=0)close(completion);if(payload>=0)close(payload);close(lock);return e;
}
const char *package_remove_payload(int directory,const PackageIndex *index,const char *(*control)(void *),void *context){
    if(!index)return "invalid_package_removal_record";const char *e=control?control(context):NULL;if(e)return e;
    int lock=-1;struct stat s;int payload=-1,completion=-1;e=lease(directory,&lock);if(e)return e;
    if(!e)e=package_identity_verify(directory,index,control,context);
    PackageRemovalRecord *r=NULL;if(!e){r=malloc(sizeof(*r));if(!r)e="allocation_failed";}
    if(!e)e=package_removal_record_load(directory,index,control,context,r);
    if(!e){payload=openat(directory,"payload",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        if(payload<0||fstat(payload,&s)||!matches(&r->payload,&s,0)||!bound(directory,"payload",payload))e="unsafe_package_removal_payload";}
    if(!e){completion=openat(directory,"complete",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);char expected[256],actual[256];int length=snprintf(expected,sizeof(expected),"samosa.package.complete.v1\n%s\n%llu %llu\n",index->manifest_sha256,(unsigned long long)r->payload.value[0],(unsigned long long)r->payload.value[1]);
        if(!r->completion.value[1]){if(completion>=0||errno!=ENOENT)e="unsafe_package_removal_payload";}
        else if(completion<0||fstat(completion,&s)||!matches(&r->completion,&s,1)||!bound(directory,"complete",completion)||s.st_size!=length||pread(completion,actual,(size_t)length,0)!=length||memcmp(actual,expected,(size_t)length))e="unsafe_package_removal_payload";}
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
static const char *empty(int directory){
    int fd=openat(directory,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC);DIR *scan=fd>=0?fdopendir(fd):NULL;
    if(!scan){if(fd>=0)close(fd);return "unsafe_package_removal_payload";}const char *e=NULL;
    for(;;){errno=0;struct dirent *entry=readdir(scan);if(!entry){if(errno)e="unsafe_package_removal_payload";break;}
        if(strcmp(entry->d_name,".")&&strcmp(entry->d_name,"..")){e="package_removal_not_empty";break;}}
    closedir(scan);return e;
}
const char *package_remove_retire(int directory,const PackageIndex *index,const char *(*control)(void *),void *context){
    if(!index)return "invalid_package_removal_record";const char *e=control?control(context):NULL;if(e)return e;
    int lock=-1,payload=-1,completion=-1;e=lease(directory,&lock);if(e)return e;
    e=package_identity_verify(directory,index,control,context);PackageRemovalRecord *r=NULL;
    if(!e){r=malloc(sizeof(*r));if(!r)e="allocation_failed";}
    if(!e)e=package_removal_record_load(directory,index,control,context,r);
    struct stat s;
    if(!e){payload=openat(directory,"payload",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        if(payload<0){if(errno!=ENOENT)e="unsafe_package_removal_payload";}
        else if(fstat(payload,&s)||!matches(&r->payload,&s,0)||!bound(directory,"payload",payload))e="unsafe_package_removal_payload";
        else e=empty(payload);}
    if(!e){completion=openat(directory,"complete",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        if(completion<0){if(errno!=ENOENT)e="unsafe_package_removal_payload";}
        else{char expected[256],actual[256];int length=snprintf(expected,sizeof(expected),"samosa.package.complete.v1\n%s\n%llu %llu\n",index->manifest_sha256,(unsigned long long)r->payload.value[0],(unsigned long long)r->payload.value[1]);
            if(fstat(completion,&s)||!matches(&r->completion,&s,1)||!bound(directory,"complete",completion)||s.st_size!=length||pread(completion,actual,(size_t)length,0)!=length||memcmp(actual,expected,(size_t)length))e="unsafe_package_removal_payload";}}
    if(!e)e=control?control(context):NULL;
    if(!e&&completion>=0){
        if(fstat(completion,&s)||!matches(&r->completion,&s,1)||!bound(directory,"assemble.lock",lock)||!bound(directory,"complete",completion))e="unsafe_package_removal_payload";
        else if(unlinkat(directory,"complete",0)||fsync(directory))e="package_removal_failed";
    }
    /* This checkpoint makes marker retirement recoverable before root removal. */
    if(!e)e=control?control(context):NULL;
    if(!e&&payload>=0){e=empty(payload);
        if(!e&&(!bound(directory,"assemble.lock",lock)||!bound(directory,"payload",payload)||unlinkat(directory,"payload",AT_REMOVEDIR)||fsync(directory)))e="package_removal_failed";}
    if(!e&&(fsync(directory)||!bound(directory,"assemble.lock",lock)))e="package_removal_failed";
    free(r);if(completion>=0)close(completion);if(payload>=0)close(payload);close(lock);return e;
}
static const char *retired_check(int directory,const PackageIndex *index,const char *(*control)(void *),void *context){
    const char *e=package_identity_verify(directory,index,control,context);PackageRemovalRecord *r=NULL;
    if(!e){r=malloc(sizeof(*r));if(!r)e="allocation_failed";}
    if(!e)e=package_removal_record_load(directory,index,control,context,r);free(r);if(e)return e;
    const char *absent[]={"payload","complete",".removal.record.pending"};struct stat s;
    for(size_t i=0;i<sizeof(absent)/sizeof(*absent);i++){
        if(!fstatat(directory,absent[i],&s,AT_SYMLINK_NOFOLLOW))return "package_reservation_not_empty";
        if(errno!=ENOENT)return "unsafe_package_removal_payload";
    }
    int fd=openat(directory,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC);DIR *scan=fd>=0?fdopendir(fd):NULL;
    if(!scan){if(fd>=0)close(fd);return "unsafe_package_removal_payload";}
    for(;;){e=control?control(context):NULL;if(e)break;errno=0;struct dirent *entry=readdir(scan);
        if(!entry){if(errno)e="unsafe_package_removal_payload";break;}
        const char *name=entry->d_name;if(!strcmp(name,".")||!strcmp(name,".."))continue;
        int journal=!strcmp(name,"copy-journal");
        if(!journal&&strcmp(name,"remove.lock")&&strcmp(name,"assemble.lock")&&strcmp(name,"publish.lock")&&strcmp(name,"assembly.identity")&&strcmp(name,"removal.record")){e="package_reservation_not_empty";break;}
        int item=openat(directory,name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC|(journal?O_DIRECTORY:0));
        if(item<0||fstat(item,&s)||s.st_uid!=geteuid()||(s.st_mode&077)||!bound(directory,name,item)||
            (journal?!S_ISDIR(s.st_mode):!S_ISREG(s.st_mode)||s.st_nlink!=1))e="unsafe_package_removal_payload";
        if(!e&&journal)e=empty(item);
        if(!e&&(!strcmp(name,"remove.lock")||!strcmp(name,"assemble.lock")||!strcmp(name,"publish.lock"))&&s.st_size)e="unsafe_package_removal_payload";
        if(item>=0)close(item);if(e)break;
    }
    closedir(scan);return e;
}
const char *package_remove_release(int pool,int directory,const PackageIndex *index,const char *(*control)(void *),void *context,int *out){
    return package_reservation_release_checked(pool,directory,index,retired_check,control,context,out);
}
static const char *finished_check(int directory,const PackageIndex *index,const char *(*control)(void *),void *context){
    struct stat s;if(!fstatat(directory,"removal.record",&s,AT_SYMLINK_NOFOLLOW))return retired_check(directory,index,control,context);
    if(errno!=ENOENT)return "unsafe_package_removal_payload";
    const char *e=package_identity_verify(directory,index,control,context);if(e)return e;
    int fd=openat(directory,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC);DIR *scan=fd>=0?fdopendir(fd):NULL;
    if(!scan){if(fd>=0)close(fd);return "unsafe_package_removal_payload";}
    for(;;){e=control?control(context):NULL;if(e)break;errno=0;struct dirent *entry=readdir(scan);if(!entry){if(errno)e="unsafe_package_removal_payload";break;}
        if(strcmp(entry->d_name,".")&&strcmp(entry->d_name,"..")&&strcmp(entry->d_name,"remove.lock")&&strcmp(entry->d_name,"assemble.lock")&&strcmp(entry->d_name,"assembly.identity")){e="package_reservation_not_empty";break;}}
    closedir(scan);return e;
}
static const char *cleanup(int directory,const PackageIndex *index,const char *(*control)(void *),void *context){
    const char *e=finished_check(directory,index,control,context);if(e)return e;
    const char *names[]={"copy-journal","publish.lock","removal.record"};
    for(size_t i=0;!e&&i<sizeof(names)/sizeof(*names);i++){
        e=control?control(context):NULL;if(e)break;int fd=openat(directory,names[i],O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC|(i==0?O_DIRECTORY:0));
        if(fd<0){if(errno!=ENOENT)e="unsafe_package_removal_payload";continue;}struct stat s;
        if(fstat(fd,&s)||s.st_uid!=geteuid()||(s.st_mode&077)||!bound(directory,names[i],fd)||(i==0?!S_ISDIR(s.st_mode):!S_ISREG(s.st_mode)||s.st_nlink!=1))e="unsafe_package_removal_payload";
        if(!e&&i==0)e=empty(fd);if(!e&&i==1&&s.st_size)e="unsafe_package_removal_payload";
        if(!e)e=control?control(context):NULL;
        if(!e&&i==2)e=retired_check(directory,index,control,context);
        if(!e&&(!bound(directory,names[i],fd)||unlinkat(directory,names[i],i==0?AT_REMOVEDIR:0)||fsync(directory)))e="package_removal_failed";
        close(fd);
    }
    if(!e)e=finished_check(directory,index,control,context);if(!e&&fsync(directory))e="package_removal_failed";return e;
}
const char *package_remove_finish(int pool,int directory,const PackageIndex *index,const char *(*control)(void *),void *context,int *out){
    return package_reservation_release_finish(pool,directory,index,finished_check,cleanup,control,context,out);
}

const char *package_remove_run(int pool,int directory,const PackageIndex *index,const char *(*control)(void *),void *context,int *out){
    if(!index||!out)return "invalid_package_removal_record";const char *e=control?control(context):NULL;if(e)return e;
    struct stat s;if(fstat(directory,&s)||!S_ISDIR(s.st_mode)||s.st_uid!=geteuid()||(s.st_mode&077)||(fcntl(directory,F_GETFL)&O_ACCMODE)!=O_RDONLY)return "unsafe_package_removal_payload";
    if(fstat(pool,&s)||!S_ISDIR(s.st_mode)||s.st_uid!=geteuid()||(s.st_mode&077)||(fcntl(pool,F_GETFL)&O_ACCMODE)!=O_RDONLY)return "unsafe_package_reservation";
    int operation=openat(directory,"remove.lock",O_RDWR|O_CREAT|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC,0600);if(operation<0)return "unsafe_package_removal_payload";
    if(fstat(operation,&s)||!S_ISREG(s.st_mode)||s.st_uid!=geteuid()||(s.st_mode&077)||s.st_nlink!=1||s.st_size||!bound(directory,"remove.lock",operation))e="unsafe_package_removal_payload";
    if(!e&&flock(operation,LOCK_EX|LOCK_NB))e=errno==EWOULDBLOCK?"package_removal_busy":"package_removal_unavailable";
    int payload=0,complete=0,record=0,pending=0;
    const char *names[]={"payload","complete","removal.record",".removal.record.pending"};int *present[]={&payload,&complete,&record,&pending};
    for(size_t i=0;!e&&i<4;i++){if(!fstatat(directory,names[i],&s,AT_SYMLINK_NOFOLLOW))*present[i]=1;else if(errno!=ENOENT)e="unsafe_package_removal_payload";}
    /* A fully cleaned retry must not recreate an inventory. The finish validator
     * admits only its exact empty registered namespace. */
    if(!e&&(payload||complete||record||pending))e=package_remove_begin(directory,index,control,context);
    int remove_payload=complete;
    if(!e&&payload&&!complete){
        PackageRemovalRecord *r=malloc(sizeof(*r));if(!r)e="allocation_failed";
        else{e=package_removal_record_load(directory,index,control,context,r);if(!e)remove_payload=!r->completion.value[1];free(r);}
    }
    /* Completed removal may already have retired its marker. In that state
     * retirement verifies an empty root; incomplete inventories need no marker. */
    if(!e&&payload&&remove_payload)e=package_remove_payload(directory,index,control,context);
    if(!e&&(payload||complete))e=package_remove_retire(directory,index,control,context);
    int released=0;if(!e)e=package_remove_finish(pool,directory,index,control,context,&released);
    if(!e&&!bound(directory,"remove.lock",operation))e="unsafe_package_removal_payload";
    if(!e)*out=released;close(operation);return e;
}
