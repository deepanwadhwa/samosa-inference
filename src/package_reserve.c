#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_reserve.h"
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
static int private(int fd,int directory){struct stat s;return !fstat(fd,&s)&&s.st_uid==geteuid()&&!(s.st_mode&077)&&
    (directory?S_ISDIR(s.st_mode):S_ISREG(s.st_mode)&&s.st_nlink==1);}
static int bound(int parent,const char *name,int fd){struct stat a,b;return !fstat(fd,&a)&&!fstatat(parent,name,&b,AT_SYMLINK_NOFOLLOW)&&
    a.st_dev==b.st_dev&&a.st_ino==b.st_ino&&a.st_mode==b.st_mode&&a.st_uid==b.st_uid&&a.st_nlink==b.st_nlink;}
static int matching_record(int pool,const char *name,int fd,const char *body,int length,unsigned links){
    struct stat s;char actual[256];return fd>=0&&!fstat(fd,&s)&&S_ISREG(s.st_mode)&&s.st_uid==geteuid()&&!(s.st_mode&077)&&s.st_nlink==links&&s.st_size==length&&
        pread(fd,actual,(size_t)length,0)==length&&!memcmp(actual,body,(size_t)length)&&bound(pool,name,fd);
}
static int name(char out[128],const char *sha,unsigned long long dev,unsigned long long ino){
    int n=snprintf(out,128,"%s-%llx-%llx",sha,dev,ino);return n>0&&n<128;
}
const char *package_reserve_controlled(int pool,int package,const PackageIndex *index,uint64_t ceiling,size_t capacity,const char *(*control)(void *),void *context,PackageReservationResult *out){
    if(!index||!out||!ceiling||!capacity||capacity>PACKAGE_RESERVATION_CAPACITY||!index->payload_bytes||index->payload_bytes>PACKAGE_PAYLOAD_BYTES||
        !memchr(index->manifest_sha256,0,sizeof(index->manifest_sha256))||strlen(index->manifest_sha256)!=64||strspn(index->manifest_sha256,"0123456789abcdef")!=64)return "invalid_package_reservation";
    const char *initial=control?control(context):NULL;if(initial)return initial;
    if(!private(pool,1)||!private(package,1)||(fcntl(pool,F_GETFL)&O_ACCMODE)!=O_RDONLY)return "unsafe_package_reservation";
    struct stat identity;if(fstat(package,&identity))return "unsafe_package_reservation";
    unsigned long long dev=(unsigned long long)identity.st_dev,ino=(unsigned long long)identity.st_ino;
    char target[128];if(!name(target,index->manifest_sha256,dev,ino))return "invalid_package_reservation";
    int lock=openat(pool,"reserve.lock",O_RDWR|O_CREAT|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC,0600);
    if(lock<0)return "unsafe_package_reservation";
    const char *e=NULL;int pending=-1,owned=0,found=0,retained=0;
    if(!private(lock,0)||!bound(pool,"reserve.lock",lock))e="unsafe_package_reservation";
    if(!e&&flock(lock,LOCK_EX|LOCK_NB))e=errno==EWOULDBLOCK?"package_reservation_busy":"package_reservation_unavailable";
    char expected[256];int expected_length=snprintf(expected,sizeof(expected),"samosa.package.reserve.v1\n%s\n%llu %llu %llu\n",index->manifest_sha256,(unsigned long long)index->payload_bytes,dev,ino);
    struct stat retained_identity;
    if(!e){pending=openat(pool,".reservation.pending",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
        if(pending>=0){
            if(fstat(pending,&retained_identity)||(retained_identity.st_nlink!=1&&retained_identity.st_nlink!=2)||!matching_record(pool,".reservation.pending",pending,expected,expected_length,(unsigned)retained_identity.st_nlink))e="unsafe_package_reservation";
            else retained=1;
        }else if(errno!=ENOENT)e="unsafe_package_reservation";
    }
    PackageReservationResult result={0};DIR *scan=NULL;
    if(!e){int fd=openat(pool,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC);scan=fd>=0?fdopendir(fd):NULL;if(!scan){if(fd>=0)close(fd);e="package_reservation_unavailable";}}
    struct dirent *entry;
    while(!e&&(errno=0,(entry=readdir(scan)))){
        e=control?control(context):NULL;if(e)break;
        if(!strcmp(entry->d_name,".")||!strcmp(entry->d_name,"..")||!strcmp(entry->d_name,"reserve.lock")||(retained&&!strcmp(entry->d_name,".reservation.pending")))continue;
        if(++result.packages>capacity){e="package_reservation_quota";break;}
        int record=openat(pool,entry->d_name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);struct stat s;char body[256],sha[65],tail,canonical[256],expected_name[128];unsigned long long bytes,d,i;
        if(record<0||fstat(record,&s)||!S_ISREG(s.st_mode)||s.st_uid!=geteuid()||(s.st_mode&077)||
            (s.st_nlink!=1&&!(retained&&s.st_nlink==2&&!strcmp(entry->d_name,target)&&s.st_dev==retained_identity.st_dev&&s.st_ino==retained_identity.st_ino))||
            s.st_size<=0||s.st_size>=256||!bound(pool,entry->d_name,record))e="unsafe_package_reservation";
        else{ssize_t n=pread(record,body,(size_t)s.st_size,0);
            if(n!=s.st_size)e="unsafe_package_reservation";
            else{body[n]=0;
                if(sscanf(body,"samosa.package.reserve.v1\n%64[0123456789abcdef]\n%llu %llu %llu\n%c",sha,&bytes,&d,&i,&tail)!=4||strlen(sha)!=64||!bytes||bytes>PACKAGE_PAYLOAD_BYTES||!name(expected_name,sha,d,i))e="unsafe_package_reservation";
                else{int length=snprintf(canonical,sizeof(canonical),"samosa.package.reserve.v1\n%s\n%llu %llu %llu\n",sha,bytes,d,i);
                    if(length!=n||memcmp(canonical,body,(size_t)n)||strcmp(expected_name,entry->d_name)||!bound(pool,entry->d_name,record))e="unsafe_package_reservation";
                    else if(bytes>ceiling||result.reserved_bytes>ceiling-bytes)e="package_reservation_quota";
                    else{result.reserved_bytes+=bytes;if(d==dev&&i==ino){
                        if(strcmp(sha,index->manifest_sha256)||bytes!=index->payload_bytes)e="package_reservation_identity_conflict";else found=1;}}
                }
            }
        }
        if(record>=0)close(record);
    }
    if(!e&&errno)e="package_reservation_unavailable";
    if(scan)closedir(scan);
    if(!e)e=control?control(context):NULL;
    if(!e&&!found&&(result.packages>=capacity||index->payload_bytes>ceiling||result.reserved_bytes>ceiling-index->payload_bytes))e="package_reservation_quota";
    /* Recover only a proposal for this exact held package/index, and only
     * after the entire pool has passed its count/byte policy. */
    if(!e&&retained){
        if(!matching_record(pool,".reservation.pending",pending,expected,expected_length,found?2:1))e="unsafe_package_reservation";
        if(!e&&(fchmod(pending,0400)||fsync(pending)))e="package_reservation_unavailable";
        if(!e&&!found&&linkat(pool,".reservation.pending",pool,target,0))e="package_reservation_identity_conflict";
        if(!e&&(!bound(pool,".reservation.pending",pending)||unlinkat(pool,".reservation.pending",0)||fsync(pool)))e="package_reservation_unavailable";
        if(!e&&!matching_record(pool,target,pending,expected,expected_length,1))e="unsafe_package_reservation";
    }
    if(!e&&!found&&!retained){
        char body[256];int length=snprintf(body,sizeof(body),"samosa.package.reserve.v1\n%s\n%llu %llu %llu\n",index->manifest_sha256,(unsigned long long)index->payload_bytes,dev,ino);
        pending=openat(pool,".reservation.pending",O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
        if(pending<0)e="unsafe_package_reservation";
        else{owned=1;size_t at=0;while(at<(size_t)length){ssize_t n=write(pending,body+at,(size_t)length-at);if(n<0&&errno==EINTR)continue;if(n<=0){e="package_reservation_unavailable";break;}at+=(size_t)n;}
            if(!e&&(fchmod(pending,0400)||fsync(pending)||!bound(pool,".reservation.pending",pending)))e="package_reservation_unavailable";
            if(!e)e=control?control(context):NULL;
            if(!e&&linkat(pool,".reservation.pending",pool,target,0))e="package_reservation_identity_conflict";
        }
    }
    if(owned){if(bound(pool,".reservation.pending",pending)){if(unlinkat(pool,".reservation.pending",0)&&!e)e="package_reservation_unavailable";}else if(!e)e="unsafe_package_reservation";}
    if(!e&&owned&&(!private(pending,0)||!bound(pool,target,pending)))e="unsafe_package_reservation";
    if(!e&&(!bound(pool,"reserve.lock",lock)||fsync(pool)))e="package_reservation_unavailable";
    if(!e){if(!found){result.packages++;result.reserved_bytes+=index->payload_bytes;}*out=result;}
    if(pending>=0)close(pending);close(lock);return e;
}

const char *package_reserve(int pool,int package,const PackageIndex *index,uint64_t ceiling,size_t capacity,PackageReservationResult *out){
    return package_reserve_controlled(pool,package,index,ceiling,capacity,NULL,NULL,out);
}

static const char *empty_check(int package,const PackageIndex *index,const char *(*control)(void *),void *context){
    (void)index;const char *e=NULL;
    DIR *scan=NULL;if(!e){int fd=openat(package,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC);scan=fd>=0?fdopendir(fd):NULL;if(!scan){if(fd>=0)close(fd);e="package_reservation_unavailable";}}
    struct dirent *entry;while(!e&&(errno=0,(entry=readdir(scan)))){
        e=control?control(context):NULL;if(e)break;
        if(strcmp(entry->d_name,".")&&strcmp(entry->d_name,"..")&&strcmp(entry->d_name,"remove.lock")&&strcmp(entry->d_name,"assemble.lock")&&strcmp(entry->d_name,"assembly.identity"))e="package_reservation_not_empty";
        if(!e&&(!strcmp(entry->d_name,"assembly.identity")||!strcmp(entry->d_name,"remove.lock"))){int metadata=openat(package,entry->d_name,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
            if(metadata<0||!private(metadata,0)||!bound(package,entry->d_name,metadata))e="unsafe_package_reservation";if(metadata>=0)close(metadata);}
    }
    if(!e&&errno)e="package_reservation_unavailable";if(scan)closedir(scan);
    return e;
}
const char *package_reservation_release_finish(int pool,int package,const PackageIndex *index,PackageReservationReleaseCheck check,PackageReservationReleaseCheck finish,const char *(*control)(void *),void *context,int *out){
    if(!index||!out||!check||!index->payload_bytes||index->payload_bytes>PACKAGE_PAYLOAD_BYTES||
        !memchr(index->manifest_sha256,0,sizeof(index->manifest_sha256))||strlen(index->manifest_sha256)!=64||strspn(index->manifest_sha256,"0123456789abcdef")!=64)return "invalid_package_reservation";
    const char *e=control?control(context):NULL;if(e)return e;
    if(!private(pool,1)||!private(package,1)||(fcntl(pool,F_GETFL)&O_ACCMODE)!=O_RDONLY||(fcntl(package,F_GETFL)&O_ACCMODE)!=O_RDONLY)return "unsafe_package_reservation";
    int assembly=openat(package,"assemble.lock",O_RDWR|O_CREAT|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC,0600),lock=-1,record=-1;
    if(assembly<0)return "unsafe_package_reservation";
    if(!private(assembly,0)||!bound(package,"assemble.lock",assembly))e="unsafe_package_reservation";
    if(!e&&flock(assembly,LOCK_EX|LOCK_NB))e=errno==EWOULDBLOCK?"package_assembly_busy":"package_assembly_unavailable";
    if(!e)e=check(package,index,control,context);
    if(!e){lock=openat(pool,"reserve.lock",O_RDWR|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);if(lock<0||!private(lock,0)||!bound(pool,"reserve.lock",lock))e="unsafe_package_reservation";}
    if(!e&&flock(lock,LOCK_EX|LOCK_NB))e=errno==EWOULDBLOCK?"package_reservation_busy":"package_reservation_unavailable";
    struct stat identity;char target[128],expected[256];int length=0,released=0;
    if(!e){if(fstat(package,&identity)||!name(target,index->manifest_sha256,(unsigned long long)identity.st_dev,(unsigned long long)identity.st_ino))e="unsafe_package_reservation";
        else length=snprintf(expected,sizeof(expected),"samosa.package.reserve.v1\n%s\n%llu %llu %llu\n",index->manifest_sha256,(unsigned long long)index->payload_bytes,(unsigned long long)identity.st_dev,(unsigned long long)identity.st_ino);}
    if(!e){struct stat pending;if(!fstatat(pool,".reservation.pending",&pending,AT_SYMLINK_NOFOLLOW)||errno!=ENOENT)e="unsafe_package_reservation";}
    if(!e){record=openat(pool,target,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);if(record<0&&errno!=ENOENT)e="unsafe_package_reservation";
        else if(record>=0&&!matching_record(pool,target,record,expected,length,1))e="package_reservation_identity_conflict";}
    if(!e)e=control?control(context):NULL;
    if(!e&&(!bound(package,"assemble.lock",assembly)||!bound(pool,"reserve.lock",lock)))e="unsafe_package_reservation";
    if(!e)e=check(package,index,control,context);
    if(!e&&(!bound(package,"assemble.lock",assembly)||!bound(pool,"reserve.lock",lock)))e="unsafe_package_reservation";
    if(!e&&record>=0){if(!matching_record(pool,target,record,expected,length,1))e="package_reservation_identity_conflict";
        else if(unlinkat(pool,target,0)||fsync(pool))e="package_reservation_unavailable";else released=1;}
    /* A retry after death between unlink and fsync must durably settle the
     * already-absent state before the removal journal may be retired. */
    if(!e&&(fsync(pool)||!bound(package,"assemble.lock",assembly)||!bound(pool,"reserve.lock",lock)))e="package_reservation_unavailable";
    if(!e&&finish)e=finish(package,index,control,context);
    if(!e)*out=released;
    if(record>=0)close(record);if(lock>=0)close(lock);close(assembly);return e;
}

const char *package_reservation_release_checked(int pool,int package,const PackageIndex *index,PackageReservationReleaseCheck check,const char *(*control)(void *),void *context,int *out){
    return package_reservation_release_finish(pool,package,index,check,NULL,control,context,out);
}

const char *package_reservation_release_empty(int pool,int package,const PackageIndex *index,const char *(*control)(void *),void *context,int *out){
    return package_reservation_release_checked(pool,package,index,empty_check,control,context,out);
}
