#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_copy_record.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static int private(int fd,int directory){struct stat s;return !fstat(fd,&s)&&s.st_uid==geteuid()&&!(s.st_mode&077)&&(directory?S_ISDIR(s.st_mode):S_ISREG(s.st_mode));}
static int bound(int parent,const char *name,int fd){struct stat a,b;return !fstat(fd,&a)&&!fstatat(parent,name,&b,AT_SYMLINK_NOFOLLOW)&&a.st_dev==b.st_dev&&a.st_ino==b.st_ino&&a.st_mode==b.st_mode&&a.st_nlink==b.st_nlink;}
const char *package_copy_record(int journal,int root,int parent,int copy,const PackageIndex *index,size_t selected,int action){
    if(!index||selected>=index->count||index->count>PACKAGE_INDEX_FILES||action<0||action>2||
        !memchr(index->manifest_sha256,0,sizeof(index->manifest_sha256))||strlen(index->manifest_sha256)!=64||strspn(index->manifest_sha256,"0123456789abcdef")!=64)return "invalid_package_copy_record";
    const PackageFile *file=&index->files[selected];
    if(!memchr(file->sha256,0,sizeof(file->sha256))||strlen(file->sha256)!=64||strspn(file->sha256,"0123456789abcdef")!=64||
        !memchr(file->name,0,sizeof(file->name))||!package_file_name_valid(file->name,file->role))return "invalid_package_copy_record";
    struct stat r,p,c;
    if(!private(journal,1)||!private(root,1)||!private(parent,1)||!private(copy,0)||fstat(root,&r)||fstat(parent,&p)||fstat(copy,&c))return "unsafe_package_copy_record";
    char name[96],pending_name[112],body[768];snprintf(name,sizeof(name),"copy-%zu-%s",selected,file->sha256);snprintf(pending_name,sizeof(pending_name),".pending-%s",name);
    int length=snprintf(body,sizeof(body),"samosa.package.copy.v1\n%s\n%s\n%s %llu\n%llu %llu %llu %llu %llu %llu\n",index->manifest_sha256,file->name,file->sha256,(unsigned long long)file->bytes,
        (unsigned long long)r.st_dev,(unsigned long long)r.st_ino,(unsigned long long)p.st_dev,(unsigned long long)p.st_ino,(unsigned long long)c.st_dev,(unsigned long long)c.st_ino);
    if(length<=0||(size_t)length>=sizeof(body))return "invalid_package_copy_record";
    int record=openat(journal,name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if(record>=0){struct stat s;char actual[768];
        const char *e=!private(record,0)||fstat(record,&s)||s.st_nlink!=1||s.st_size!=length||pread(record,actual,(size_t)length,0)!=length||memcmp(actual,body,(size_t)length)||!bound(journal,name,record)?"unsafe_package_copy_record":NULL;
        if(!e&&action==2&&(unlinkat(journal,name,0)||fsync(journal)))e="package_copy_record_failed";
        close(record);return e;
    }
    if(errno!=ENOENT)return "unsafe_package_copy_record";
    if(action!=1)return "package_copy_unregistered";
    int pending=openat(journal,pending_name,O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    if(pending<0)return "unsafe_package_copy_record";
    const char *e=NULL;size_t at=0;while(at<(size_t)length){ssize_t n=write(pending,body+at,(size_t)length-at);if(n<0&&errno==EINTR)continue;if(n<=0){e="package_copy_record_failed";break;}at+=(size_t)n;}
    if(!e&&(fchmod(pending,0400)||fsync(pending)||!bound(journal,pending_name,pending)))e="package_copy_record_failed";
    if(!e&&linkat(journal,pending_name,journal,name,0))e="package_copy_record_failed";
    if(bound(journal,pending_name,pending)){if(unlinkat(journal,pending_name,0)&&!e)e="package_copy_record_failed";}else if(!e)e="unsafe_package_copy_record";
    if(!e&&(!bound(journal,name,pending)||fsync(journal)))e="package_copy_record_failed";
    close(pending);return e;
}
