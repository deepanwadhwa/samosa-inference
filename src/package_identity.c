#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_identity.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static int bound(int parent,const char *name,int fd){struct stat a,b;return !fstat(fd,&a)&&!fstatat(parent,name,&b,AT_SYMLINK_NOFOLLOW)&&
    a.st_dev==b.st_dev&&a.st_ino==b.st_ino&&a.st_mode==b.st_mode&&a.st_uid==b.st_uid&&a.st_nlink==b.st_nlink;}
static int matches(int directory,const char *name,int record,const char *expected,int length){
    struct stat s;char actual[256];
    return !fstat(record,&s)&&S_ISREG(s.st_mode)&&s.st_uid==geteuid()&&!(s.st_mode&077)&&s.st_nlink==1&&s.st_size==length&&
        pread(record,actual,(size_t)length,0)==length&&!memcmp(actual,expected,(size_t)length)&&bound(directory,name,record);
}
static const char *identity(int directory,const PackageIndex *index,const char *(*control)(void *),void *context,int create){
    if(!index||!memchr(index->manifest_sha256,0,sizeof(index->manifest_sha256))||strlen(index->manifest_sha256)!=64||strspn(index->manifest_sha256,"0123456789abcdef")!=64)return "invalid_package_identity";
    const char *e=control?control(context):NULL;if(e)return e;
    struct stat root;
    if(fstat(directory,&root)||!S_ISDIR(root.st_mode)||root.st_uid!=geteuid()||(root.st_mode&077)||(fcntl(directory,F_GETFL)&O_ACCMODE)!=O_RDONLY)return "unsafe_package_identity";
    char expected[256];int length=snprintf(expected,sizeof(expected),"samosa.package.identity.v1\n%s\n%llu %llu\n",index->manifest_sha256,(unsigned long long)root.st_dev,(unsigned long long)root.st_ino);
    if(length<=0||(size_t)length>=sizeof(expected))return "invalid_package_identity";
    int record=openat(directory,"assembly.identity",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if(record>=0){struct stat pending;
        if(!matches(directory,"assembly.identity",record,expected,length))e="package_assembly_identity_conflict";
        if(!e){if(!fstatat(directory,".assembly.identity.pending",&pending,AT_SYMLINK_NOFOLLOW)||errno!=ENOENT)e="unsafe_package_identity";}
        if(!e)e=control?control(context):NULL;
        if(!e&&!matches(directory,"assembly.identity",record,expected,length))e="package_assembly_identity_conflict";
        close(record);return e;
    }
    if(errno!=ENOENT)return "unsafe_package_identity";
    if(!create)return "package_identity_absent";
    int pending=openat(directory,".assembly.identity.pending",O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    if(pending<0)return "unsafe_package_identity";
    size_t at=0;while(at<(size_t)length){ssize_t n=write(pending,expected+at,(size_t)length-at);
        if(n<0&&errno==EINTR)continue;if(n<=0){e="package_identity_unavailable";break;}at+=(size_t)n;}
    if(!e&&(fchmod(pending,0400)||fsync(pending)||!bound(directory,".assembly.identity.pending",pending)))e="package_identity_unavailable";
    if(!e)e=control?control(context):NULL;
    if(!e&&!matches(directory,".assembly.identity.pending",pending,expected,length))e="unsafe_package_identity";
    if(!e&&linkat(directory,".assembly.identity.pending",directory,"assembly.identity",0))e="package_assembly_identity_conflict";
    if(bound(directory,".assembly.identity.pending",pending)){if(unlinkat(directory,".assembly.identity.pending",0)&&!e)e="package_identity_unavailable";}
    else if(!e)e="unsafe_package_identity";
    if(!e&&(!matches(directory,"assembly.identity",pending,expected,length)||fsync(directory)))e="package_identity_unavailable";
    close(pending);return e;
}

const char *package_identity_bind(int directory,const PackageIndex *index,const char *(*control)(void *),void *context){return identity(directory,index,control,context,1);}
const char *package_identity_verify(int directory,const PackageIndex *index,const char *(*control)(void *),void *context){return identity(directory,index,control,context,0);}
