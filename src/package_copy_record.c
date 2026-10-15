#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_copy_record.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static int private(int fd,int directory){struct stat s;return !fstat(fd,&s)&&s.st_uid==geteuid()&&!(s.st_mode&077)&&(directory?S_ISDIR(s.st_mode):S_ISREG(s.st_mode));}
static int bound(int parent,const char *name,int fd){struct stat a,b;return !fstat(fd,&a)&&!fstatat(parent,name,&b,AT_SYMLINK_NOFOLLOW)&&a.st_dev==b.st_dev&&a.st_ino==b.st_ino&&a.st_mode==b.st_mode&&a.st_nlink==b.st_nlink;}
static int matches(int directory,const char *name,int fd,const char *body,int length,unsigned links){
    struct stat s;char actual[768];
    return private(fd,0)&&!fstat(fd,&s)&&s.st_nlink==links&&s.st_size==length&&
        pread(fd,actual,(size_t)length,0)==length&&!memcmp(actual,body,(size_t)length)&&bound(directory,name,fd);
}
static const char *record(int journal,int root,int parent,int copy,const PackageIndex *index,size_t selected,int action,int intent,int absent){
    if(!index||selected>=index->count||index->count>PACKAGE_INDEX_FILES||action<0||action>2||
        !memchr(index->manifest_sha256,0,sizeof(index->manifest_sha256))||strlen(index->manifest_sha256)!=64||strspn(index->manifest_sha256,"0123456789abcdef")!=64)return "invalid_package_copy_record";
    const PackageFile *file=&index->files[selected];
    if(!memchr(file->sha256,0,sizeof(file->sha256))||strlen(file->sha256)!=64||strspn(file->sha256,"0123456789abcdef")!=64||
        !memchr(file->name,0,sizeof(file->name))||!package_file_name_valid(file->name,file->role))return "invalid_package_copy_record";
    struct stat r,p,c={0};
    if(!private(journal,1)||!private(root,1)||!private(parent,1)||(!intent&&!absent&&(!private(copy,0)||fstat(copy,&c)))||fstat(root,&r)||fstat(parent,&p))return "unsafe_package_copy_record";
    char name[96],pending_name[112],body[768];snprintf(name,sizeof(name),"%s-%zu-%s",intent?"intent":"copy",selected,file->sha256);snprintf(pending_name,sizeof(pending_name),".pending-%s",name);
    unsigned long long copy_dev=(unsigned long long)c.st_dev,copy_ino=(unsigned long long)c.st_ino;
    if(absent){
        /* No payload is deleted here. Reconstruct the exact canonical journal
         * body only after both possible payload names are absent. This permits
         * recovery after unlink+fsync without adopting an unregistered inode. */
        const char *leaf=strrchr(file->name,'/');leaf=leaf?leaf+1:file->name;
        char temporary[80];snprintf(temporary,sizeof(temporary),".copy-%zu-%s",selected,file->sha256);
        struct stat s;
        if(!fstatat(parent,leaf,&s,AT_SYMLINK_NOFOLLOW)||errno!=ENOENT||
           !fstatat(parent,temporary,&s,AT_SYMLINK_NOFOLLOW)||errno!=ENOENT)return "unsafe_package_copy_record";
        int retained=openat(journal,name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        if(retained<0&&errno==ENOENT)retained=openat(journal,pending_name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        if(retained<0)return errno==ENOENT?"package_copy_unregistered":"unsafe_package_copy_record";
        char saved[768];ssize_t n=pread(retained,saved,sizeof(saved)-1,0);close(retained);
        if(n<=0||n>=(ssize_t)sizeof(saved)-1||saved[n-1]!='\n')return "unsafe_package_copy_record";
        saved[n-1]=0;char *last=strrchr(saved,'\n');if(!last)return "unsafe_package_copy_record";
        unsigned long long tuple[6];char *cursor=last+1;
        for(size_t i=0;i<6;i++){
            if(*cursor<'0'||*cursor>'9')return "unsafe_package_copy_record";
            char *end;errno=0;tuple[i]=strtoull(cursor,&end,10);
            if(errno||end==cursor||(i<5?*end!=' ':*end!=0))return "unsafe_package_copy_record";
            cursor=end+(i<5);
        }
        copy_dev=tuple[4];copy_ino=tuple[5];
        if(tuple[0]!=(unsigned long long)r.st_dev||tuple[1]!=(unsigned long long)r.st_ino||
           tuple[2]!=(unsigned long long)p.st_dev||tuple[3]!=(unsigned long long)p.st_ino||!copy_ino)return "unsafe_package_copy_record";
    }
    int length=snprintf(body,sizeof(body),"%s\n%s\n%s\n%s %llu\n%llu %llu %llu %llu %llu %llu\n",intent?"samosa.package.copy.intent.v1":"samosa.package.copy.v1",index->manifest_sha256,file->name,file->sha256,(unsigned long long)file->bytes,
        (unsigned long long)r.st_dev,(unsigned long long)r.st_ino,(unsigned long long)p.st_dev,(unsigned long long)p.st_ino,copy_dev,copy_ino);
    if(length<=0||(size_t)length>=sizeof(body))return "invalid_package_copy_record";
    int record=openat(journal,name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if(record>=0){struct stat s;const char *e=NULL;
        if(fstat(record,&s)||(s.st_nlink!=1&&s.st_nlink!=2)||!matches(journal,name,record,body,length,(unsigned)s.st_nlink))e="unsafe_package_copy_record";
        int alias=-1;
        if(!e){alias=openat(journal,pending_name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
            if(s.st_nlink==2){struct stat a;
                if(alias<0||fstat(alias,&a)||a.st_dev!=s.st_dev||a.st_ino!=s.st_ino||!matches(journal,pending_name,alias,body,length,2))e="unsafe_package_copy_record";
                else if(unlinkat(journal,pending_name,0)||fsync(journal))e="package_copy_record_failed";
            }else if(alias>=0||errno!=ENOENT)e="unsafe_package_copy_record";
        }
        if(!e&&!matches(journal,name,record,body,length,1))e="unsafe_package_copy_record";
        if(!e&&action==2&&(unlinkat(journal,name,0)||fsync(journal)))e="package_copy_record_failed";
        if(alias>=0)close(alias);close(record);return e;
    }
    if(errno!=ENOENT)return "unsafe_package_copy_record";
    /* A fully written pending record proves the exact index/root/parent/copy
     * tuple. Recover its publication only after comparing every expected byte;
     * malformed or unrelated pending metadata is never adopted or removed. */
    int retained=openat(journal,pending_name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if(retained>=0){const char *e=NULL;
        if(!matches(journal,pending_name,retained,body,length,1))e="unsafe_package_copy_record";
        if(!e&&(fchmod(retained,0400)||fsync(retained)||linkat(journal,pending_name,journal,name,0)))e="package_copy_record_failed";
        if(!e&&(!bound(journal,pending_name,retained)||unlinkat(journal,pending_name,0)||fsync(journal)))e="package_copy_record_failed";
        if(!e&&!matches(journal,name,retained,body,length,1))e="unsafe_package_copy_record";
        if(!e&&action==2&&(unlinkat(journal,name,0)||fsync(journal)))e="package_copy_record_failed";
        close(retained);return e;
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
const char *package_copy_record(int journal,int root,int parent,int copy,const PackageIndex *index,size_t selected,int action){return record(journal,root,parent,copy,index,selected,action,0,0);}
const char *package_copy_intent(int journal,int root,int parent,const PackageIndex *index,size_t selected,int action){return record(journal,root,parent,-1,index,selected,action,1,0);}

const char *package_copy_forget_absent(int journal,int root,int parent,const PackageIndex *index,size_t selected){return record(journal,root,parent,-1,index,selected,2,0,1);}

const char *package_copy_check_absent(int journal,int root,int parent,const PackageIndex *index,size_t selected){return record(journal,root,parent,-1,index,selected,0,0,1);}
