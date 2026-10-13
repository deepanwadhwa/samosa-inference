#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "artifact.h"
#include <sys/stat.h>
#include <sys/file.h>
#include <fcntl.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <limits.h>
#include <time.h>

typedef struct {int root,lock,entry,stage;struct stat root_identity;} Store;
static const char *valid(const ArtifactConfig *c) {
    if(!c || !c->state_path || c->state_path[0]!='/' || !c->sha256 ||
       strlen(c->sha256)!=64 || !c->size || c->size>INT64_MAX ||
       c->size>c->byte_ceiling || c->size>c->disk_ceiling/2)
        return "invalid_artifact_configuration";
    for(size_t i=0;i<64;i++)if(!strchr("0123456789abcdef",c->sha256[i]))
        return "invalid_artifact_configuration";
    return NULL;
}
static int private_fd(int fd,int directory) {
    struct stat s;return !fstat(fd,&s) && s.st_uid==geteuid() &&
        !(s.st_mode&077) && (directory?S_ISDIR(s.st_mode):S_ISREG(s.st_mode));
}
static ssize_t identity_read(int fd,char text[128]) {
    struct stat st;
    if(fd<0 || !private_fd(fd,0) || fstat(fd,&st) || st.st_size<=0 || st.st_size>=128)return -1;
    ssize_t n=pread(fd,text,(size_t)st.st_size,0);
    return n==st.st_size?n:-1;
}
static void close_store(Store *s) {
    if(s->stage>=0)close(s->stage);
    if(s->entry>=0)close(s->entry);
    if(s->lock>=0)close(s->lock);
    if(s->root>=0)close(s->root);
}
static int path_same(int parent,const char *name,int fd) {
    struct stat a,b;return !fstat(fd,&a) && !fstatat(parent,name,&b,AT_SYMLINK_NOFOLLOW) &&
        a.st_dev==b.st_dev && a.st_ino==b.st_ino && a.st_mode==b.st_mode;
}
static int root_same(const ArtifactConfig *c,Store *s) {
    struct stat now;
    return !lstat(c->state_path,&now) && S_ISDIR(now.st_mode) &&
        now.st_uid==geteuid() && !(now.st_mode&077) &&
        now.st_dev==s->root_identity.st_dev && now.st_ino==s->root_identity.st_ino &&
        path_same(s->root,"install.lock",s->lock);
}
static int unchanged(const struct stat *a,const struct stat *b) {
    return a->st_dev==b->st_dev && a->st_ino==b->st_ino && a->st_size==b->st_size &&
#ifdef __APPLE__
        a->st_mtimespec.tv_sec==b->st_mtimespec.tv_sec && a->st_mtimespec.tv_nsec==b->st_mtimespec.tv_nsec &&
        a->st_ctimespec.tv_sec==b->st_ctimespec.tv_sec && a->st_ctimespec.tv_nsec==b->st_ctimespec.tv_nsec;
#else
        a->st_mtim.tv_sec==b->st_mtim.tv_sec && a->st_mtim.tv_nsec==b->st_mtim.tv_nsec &&
        a->st_ctim.tv_sec==b->st_ctim.tv_sec && a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
#endif
}
static const char *open_store(const ArtifactConfig *c,Store *s,int create) {
    *s=(Store){.root=-1,.lock=-1,.entry=-1,.stage=-1};
    const char *error=valid(c);if(error)return error;
    if(create && mkdir(c->state_path,0700) && errno!=EEXIST)return "artifact_state_unavailable";
    s->root=open(c->state_path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if(s->root<0)return errno==ENOENT?"artifact_absent":"artifact_state_unavailable";
    if(!private_fd(s->root,1) || fstat(s->root,&s->root_identity))return "unsafe_artifact_state";
    s->lock=openat(s->root,"install.lock",O_RDWR|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC|(create?O_CREAT:0),0600);
    if(s->lock<0 && errno==ENOENT && !create)return "artifact_absent";
    if(s->lock<0 || !private_fd(s->lock,0))return "unsafe_artifact_state";
    if(flock(s->lock,LOCK_EX|LOCK_NB))return errno==EWOULDBLOCK?"artifact_busy":"artifact_state_unavailable";
    if(!root_same(c,s))return "artifact_state_changed";
    return NULL;
}
static int write_all(int fd,const void *data,size_t n) {
    const unsigned char *p=data;
    while(n){ssize_t wrote=write(fd,p,n);if(wrote<0&&errno==EINTR)continue;
        if(wrote<=0)return 0;
        p+=wrote;n-=(size_t)wrote;}
    return 1;
}
static const char *quota(const ArtifactConfig *c,Store *s,int adding) {
    int scan=openat(s->root,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    DIR *d=scan>=0?fdopendir(scan):NULL;if(!d){if(scan>=0)close(scan);return "artifact_state_unavailable";}
    unsigned count=0;uint64_t reserved=0;const char *error=NULL;struct dirent *e;
    while((e=readdir(d))) {
        if(!strcmp(e->d_name,".")||!strcmp(e->d_name,"..")||!strcmp(e->d_name,"install.lock"))continue;
        if(++count>16 || strlen(e->d_name)!=64){error="artifact_quota";break;}
        int entry=openat(s->root,e->d_name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        int meta=entry>=0?openat(entry,"identity",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC):-1;
        char text[128];ssize_t n=identity_read(meta,text);
        if(meta>=0)close(meta);
        if(entry>=0)close(entry);
        unsigned long long size,dev,ino;char tail;
        if(n<=0){error="unsafe_artifact_state";break;}text[n]=0;
        if(sscanf(text,"%llu %llu %llu %c",&size,&dev,&ino,&tail)!=3 || !size || size>c->disk_ceiling/2 ||
           reserved>c->disk_ceiling-2*size){error="artifact_quota";break;}
        reserved+=2*size;
    }
    closedir(d);
    if(!error && adding && (count>=16 || reserved>c->disk_ceiling-2*c->size))error="artifact_quota";
    return error;
}
static const char *open_entry(const ArtifactConfig *c,Store *s,int create) {
    s->entry=openat(s->root,c->sha256,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if(s->entry<0 && errno==ENOENT && create) {
        const char *e=quota(c,s,1);if(e)return e;
        if(mkdirat(s->root,c->sha256,0700))return "artifact_state_unavailable";
        s->entry=openat(s->root,c->sha256,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        if(s->entry<0)return "artifact_state_unavailable";
        s->stage=openat(s->entry,"stage",O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC,0600);
        struct stat st;if(s->stage<0 || fstat(s->stage,&st))return "artifact_state_unavailable";
        char text[128];int n=snprintf(text,sizeof(text),"%llu %llu %llu\n",
            (unsigned long long)c->size,(unsigned long long)st.st_dev,(unsigned long long)st.st_ino);
        int meta=openat(s->entry,"identity",O_WRONLY|O_CREAT|O_EXCL|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC,0600);
        int ok=meta>=0 && n>0 && (size_t)n<sizeof(text) && write_all(meta,text,(size_t)n) && !fsync(meta);
        if(meta>=0)close(meta);
        if(!ok || fsync(s->entry) || fsync(s->root))return "artifact_state_unavailable";
        return NULL;
    }
    if(s->entry<0)return errno==ENOENT?"artifact_absent":"unsafe_artifact_state";
    if(!private_fd(s->entry,1))return "unsafe_artifact_state";
    int meta=openat(s->entry,"identity",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    char text[128];ssize_t n=identity_read(meta,text);
    if(meta>=0)close(meta);
    if(n<=0)return "unsafe_artifact_state";
    text[n]=0;
    unsigned long long size,dev,ino;char tail;
    if(sscanf(text,"%llu %llu %llu %c",&size,&dev,&ino,&tail)!=3 || size!=c->size)return "artifact_identity_conflict";
    s->stage=openat(s->entry,"stage",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    struct stat st;
    if(s->stage<0 || !private_fd(s->stage,0) || fstat(s->stage,&st) ||
       dev!=(unsigned long long)st.st_dev || ino!=(unsigned long long)st.st_ino)
        return "unsafe_artifact_state";
    return NULL;
}
static int scopes_same(const ArtifactConfig *c,Store *s) {
    return root_same(c,s) && path_same(s->root,c->sha256,s->entry) &&
        path_same(s->entry,"stage",s->stage);
}
static const char *published(const ArtifactConfig *c,Store *s,int *exists) {
    *exists=0;int fd=openat(s->entry,"artifact",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if(fd<0){struct stat stage;
        return errno==ENOENT&&!fstat(s->stage,&stage)&&stage.st_nlink==1?NULL:"foreign_artifact";}
    *exists=1;struct stat a,b;
    const char *e=fstat(fd,&a)||fstat(s->stage,&b)||a.st_dev!=b.st_dev||a.st_ino!=b.st_ino||a.st_nlink!=2?
        "foreign_artifact":artifact_verify(fd,c->size,c->byte_ceiling,c->sha256);
    if(!e && !path_same(s->entry,"artifact",fd))e="artifact_changed";
    close(fd);return e;
}
const char *artifact_status(const ArtifactConfig *c,const char **status) {
    *status="invalid";Store s;const char *e=open_store(c,&s,0);
    if(e && !strcmp(e,"artifact_absent")){*status="absent";e=NULL;}
    else if(e && !strcmp(e,"artifact_busy")){
        char target[65];ssize_t n=pread(s.lock,target,sizeof(target),0);
        *status=n==64 && !memcmp(target,c->sha256,64)?"installing":"busy";e=NULL;}
    else if(!e) {
        e=open_entry(c,&s,0);
        if(e && !strcmp(e,"artifact_absent")){*status="absent";e=NULL;}
        else if(!e){int exists;e=published(c,&s,&exists);
            if(!e)*status=exists?"verified":"interrupted";
            if(!e && !scopes_same(c,&s)){*status="invalid";e="artifact_state_changed";}}
    }
    close_store(&s);return e;
}
const char *artifact_install(const ArtifactConfig *c) {
    Store s;const char *e=open_store(c,&s,1);int input=-1;
    if(!e)e=quota(c,&s,0);
    if(!e)e=open_entry(c,&s,1);
    int exists=0;if(!e)e=published(c,&s,&exists);
    if(!e && exists){
        if(!scopes_same(c,&s))e="artifact_state_changed";
        else if(fsync(s.entry))e="artifact_publish_failed";
        goto done;
    }
    if(!e){int writable=openat(s.entry,"stage",O_RDWR|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        struct stat original,reopened;
        if(writable<0||fstat(s.stage,&original)||fstat(writable,&reopened)||
           original.st_dev!=reopened.st_dev||original.st_ino!=reopened.st_ino){
            if(writable>=0)close(writable);
            e="unsafe_artifact_state";
        }else{close(s.stage);s.stage=writable;}}
    if(!e && (ftruncate(s.lock,0)||pwrite(s.lock,c->sha256,64,0)!=64||fsync(s.lock)))e="artifact_state_unavailable";
    struct stat before,stage;
    if(!e && !c->acquire && (!c->source_path || c->source_path[0]!='/'))e="invalid_artifact_configuration";
    if(!e && !c->acquire){input=open(c->source_path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        if(input<0)e="artifact_source_unavailable";}
    if(!e && !c->acquire && (fstat(input,&before)||!S_ISREG(before.st_mode)||before.st_size<0))e="unsupported_artifact";
    if(!e && !c->acquire && (uint64_t)before.st_size!=c->size)e="artifact_size_mismatch";
    if(!e && (fstat(s.stage,&stage)||stage.st_size<0||(uint64_t)stage.st_size>c->size))e="unsafe_artifact_state";
    if(!e && stage.st_size && !c->resume)e="artifact_resume_required";
    if(!e && c->acquire)e=c->acquire(c->acquisition,s.entry,s.stage,(uint64_t)stage.st_size,c);
    unsigned char *working=!e&&!c->acquire?malloc(65536):NULL;
    if(!e && !c->acquire && !working)e="artifact_resource_limit";
    unsigned char *source=working,*retained=working?working+32768:NULL;uint64_t offset=0;
    struct timespec started,now;
    if(!e && clock_gettime(CLOCK_MONOTONIC,&started))e="artifact_read_failed";
    while(!e && !c->acquire && offset<c->size) {
        if(clock_gettime(CLOCK_MONOTONIC,&now)){e="artifact_read_failed";break;}
        if(now.tv_sec-started.tv_sec>=600){e="artifact_deadline_exceeded";break;}
        if(c->cancelled && c->cancelled(c->cancellation)){e="artifact_interrupted";break;}
        size_t want=32768;if(c->size-offset<want)want=(size_t)(c->size-offset);
        ssize_t n=pread(input,source,want,(off_t)offset);
        if(n<0&&errno==EINTR)continue;
        if(n<=0){e="artifact_read_failed";break;}
        size_t old=0;
        if(offset<(uint64_t)stage.st_size){old=(size_t)((uint64_t)stage.st_size-offset);if(old>(size_t)n)old=(size_t)n;
            if(pread(s.stage,retained,old,(off_t)offset)!=(ssize_t)old || memcmp(source,retained,old)){e="artifact_resume_conflict";break;}}
        size_t written=old;
        while(written<(size_t)n){ssize_t w=pwrite(s.stage,source+written,(size_t)n-written,(off_t)(offset+written));
            if(w<0&&errno==EINTR)continue;
            if(w<=0){e="artifact_write_failed";break;}
            written+=(size_t)w;}
        offset+=(uint64_t)n;
    }
    free(working);
    if(!e && fsync(s.stage))e="artifact_write_failed";
    if(!e)e=artifact_verify(s.stage,c->size,c->byte_ceiling,c->sha256);
    struct stat after,path;
    if(!e && !c->acquire && (fstat(input,&after)||lstat(c->source_path,&path)||
        !unchanged(&before,&after)||
        before.st_dev!=path.st_dev||before.st_ino!=path.st_ino||!S_ISREG(path.st_mode)))e="artifact_source_changed";
    if(!e && !scopes_same(c,&s))e="artifact_state_changed";
    if(!e && c->cancelled && c->cancelled(c->cancellation))e="artifact_interrupted";
    if(!e && linkat(s.entry,"stage",s.entry,"artifact",0)) {
        if(errno==EEXIST)e=published(c,&s,&exists);else e="artifact_publish_failed";
    }
#ifdef SAMOSA_INFERENCE_TEST_FAULTS
    if(!e && c->checkpoint)c->checkpoint("published",c->checkpoint_context);
#endif
    if(!e && fsync(s.entry))e="artifact_publish_failed";
    if(!e && !scopes_same(c,&s))e="artifact_state_changed";
done:
    if(input>=0)close(input);
    close_store(&s);return e;
}
const char *artifact_make_executable(const ArtifactConfig *c) {
    Store s;const char *e=open_store(c,&s,0);int exists=0;
    if(!e)e=open_entry(c,&s,0);
    if(!e)e=published(c,&s,&exists);
    if(!e&&!exists)e="artifact_absent";
    unsigned char header[64];
    if(!e && pread(s.stage,header,sizeof(header),0)!=(ssize_t)sizeof(header))e="unsupported_executable";
    if(!e){int native=0;
#if defined(__APPLE__) && defined(__aarch64__)
        native=!memcmp(header,"\xcf\xfa\xed\xfe\x0c\x00\x00\x01",8)&&header[12]==2&&!header[13]&&!header[14]&&!header[15];
#elif defined(__linux__) && defined(__aarch64__)
        unsigned entry=0;for(size_t i=24;i<32;i++)entry|=header[i];
        native=!memcmp(header,"\x7f" "ELF",4)&&header[4]==2&&header[5]==1&&header[18]==183&&header[19]==0&&
            (header[16]==2||header[16]==3)&&!header[17]&&entry;
#endif
        if(!native)e="unsupported_executable";
    }
    if(!e&&!scopes_same(c,&s))e="artifact_state_changed";
    if(!e&&(fchmod(s.stage,0500)||fsync(s.stage)||fsync(s.entry)))e="artifact_publish_failed";
    if(!e&&!scopes_same(c,&s))e="artifact_state_changed";
    close_store(&s);return e;
}
