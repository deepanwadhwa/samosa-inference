#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_reserve.h"
#include <fcntl.h>
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <signal.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"reservation failure %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static const char *cancel(void *unused){(void)unused;return "deadline_exceeded";}
static const char *pending_cancel(void *opaque){int pool=*(int *)opaque;struct stat s;
    return !fstatat(pool,".reservation.pending",&s,AT_SYMLINK_NOFOLLOW)?"cancelled":NULL;
}
static const char *pending_death(void *opaque){int pool=*(int *)opaque;struct stat s;
    if(!fstatat(pool,".reservation.pending",&s,AT_SYMLINK_NOFOLLOW))kill(getpid(),SIGKILL);
    return NULL;
}
int main(void){
    char root[]="/tmp/samosa-reserve-XXXXXX";CHECK(mkdtemp(root));int pool=open(root,O_RDONLY|O_DIRECTORY);CHECK(pool>=0);
    /* Package directories live outside the reserved ledger namespace. */
    char packages[]="/tmp/samosa-reserve-packages-XXXXXX";CHECK(mkdtemp(packages));int owner=open(packages,O_RDONLY|O_DIRECTORY);CHECK(owner>=0);
    int package[3];for(unsigned i=0;i<3;i++){char name[2]={(char)('a'+i),0};CHECK(!mkdirat(owner,name,0700));package[i]=openat(owner,name,O_RDONLY|O_DIRECTORY);CHECK(package[i]>=0);}
    PackageIndex *index=calloc(1,sizeof(*index));CHECK(index);memset(index->manifest_sha256,'a',64);index->payload_bytes=10;
    PackageReservationResult result;memset(&result,0xa5,sizeof(result));PackageReservationResult pristine=result;
    CHECK(!strcmp(package_reserve_controlled(pool,package[0],index,20,2,cancel,NULL,&result),"deadline_exceeded"));CHECK(!memcmp(&result,&pristine,sizeof(result)));CHECK(faccessat(pool,"reserve.lock",F_OK,0)<0);
    CHECK(!strcmp(package_reserve_controlled(pool,package[0],index,20,2,pending_cancel,&pool,&result),"cancelled"));CHECK(!memcmp(&result,&pristine,sizeof(result)));CHECK(faccessat(pool,".reservation.pending",F_OK,0)<0);
    /* Cancellation before publication leaves no charged record in the pool. */
    int inspection=openat(pool,".",O_RDONLY|O_DIRECTORY);CHECK(inspection>=0);DIR *entries=fdopendir(inspection);CHECK(entries);struct dirent *entry;
    while((entry=readdir(entries)))CHECK(!strcmp(entry->d_name,".")||!strcmp(entry->d_name,"..")||!strcmp(entry->d_name,"reserve.lock"));CHECK(!closedir(entries));
    /* A real killed publisher leaves a proposal; a lower ceiling refuses it
     * without changing names, and a permitted retry commits one charge. */
    pid_t publisher=fork();CHECK(publisher>=0);if(!publisher)_exit(package_reserve_controlled(pool,package[0],index,20,2,pending_death,&pool,&result)?2:3);
    int publisher_status;CHECK(waitpid(publisher,&publisher_status,0)==publisher&&WIFSIGNALED(publisher_status)&&WTERMSIG(publisher_status)==SIGKILL);
    CHECK(!strcmp(package_reserve(pool,package[1],index,20,2,&result),"unsafe_package_reservation"));CHECK(faccessat(pool,".reservation.pending",F_OK,0)==0);
    CHECK(!strcmp(package_reserve_controlled(pool,package[0],index,20,2,pending_cancel,&pool,&result),"cancelled"));CHECK(faccessat(pool,".reservation.pending",F_OK,0)==0);
    CHECK(!strcmp(package_reserve(pool,package[0],index,9,2,&result),"package_reservation_quota"));CHECK(faccessat(pool,".reservation.pending",F_OK,0)==0);
    CHECK(!package_reserve(pool,package[0],index,20,2,&result));CHECK(result.packages==1&&result.reserved_bytes==10);
    CHECK(faccessat(pool,".reservation.pending",F_OK,0)<0);
    struct stat package_identity;CHECK(!fstat(package[0],&package_identity));char record_name[128];snprintf(record_name,sizeof(record_name),"%s-%llx-%llx",index->manifest_sha256,(unsigned long long)package_identity.st_dev,(unsigned long long)package_identity.st_ino);
    CHECK(!linkat(pool,record_name,pool,".reservation.pending",0));CHECK(!package_reserve(pool,package[0],index,20,2,&result));CHECK(result.packages==1&&result.reserved_bytes==10);CHECK(faccessat(pool,".reservation.pending",F_OK,0)<0);
    CHECK(!package_reserve(pool,package[0],index,20,2,&result));CHECK(result.packages==1&&result.reserved_bytes==10);
    /* A committed reservation survives process exit without automatic release. */
    pid_t child=fork();CHECK(child>=0);if(!child)_exit(package_reserve(pool,package[1],index,20,2,&result)?1:0);
    int status;CHECK(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
    CHECK(!package_reserve(pool,package[0],index,20,2,&result));CHECK(result.packages==2&&result.reserved_bytes==20);
    memset(&result,0xa5,sizeof(result));PackageReservationResult unchanged=result;
    CHECK(!strcmp(package_reserve(pool,package[2],index,20,3,&result),"package_reservation_quota"));CHECK(!memcmp(&result,&unchanged,sizeof(result)));
    CHECK(!strcmp(package_reserve(pool,package[2],index,30,2,&result),"package_reservation_quota"));
    index->manifest_sha256[0]='b';CHECK(!strcmp(package_reserve(pool,package[0],index,30,3,&result),"package_reservation_identity_conflict"));index->manifest_sha256[0]='a';
    int lock=openat(pool,"reserve.lock",O_RDWR);CHECK(lock>=0);CHECK(!flock(lock,LOCK_EX|LOCK_NB));
    CHECK(!strcmp(package_reserve(pool,package[0],index,20,2,&result),"package_reservation_busy"));CHECK(!close(lock));
    /* Unregistered crash/foreign metadata is retained and blocks new writes. */
    int unknown=openat(pool,".reservation.pending",O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(unknown>=0);CHECK(write(unknown,"foreign",7)==7);CHECK(!close(unknown));
    CHECK(!strcmp(package_reserve(pool,package[2],index,30,3,&result),"unsafe_package_reservation"));CHECK(faccessat(pool,".reservation.pending",F_OK,0)==0);
    CHECK(!unlinkat(pool,".reservation.pending",0));
    CHECK(!package_reserve(pool,package[2],index,30,3,&result));CHECK(result.packages==3&&result.reserved_bytes==30);
    int released=-7;CHECK(!strcmp(package_reservation_release_empty(pool,package[0],index,cancel,NULL,&released),"deadline_exceeded"));CHECK(released==-7);
    CHECK(!mkdirat(package[0],"payload",0700));CHECK(!strcmp(package_reservation_release_empty(pool,package[0],index,NULL,NULL,&released),"package_reservation_not_empty"));CHECK(released==-7);CHECK(!unlinkat(package[0],"payload",AT_REMOVEDIR));
    int assembly=openat(package[0],"assemble.lock",O_RDWR|O_NOFOLLOW);CHECK(assembly>=0);CHECK(!flock(assembly,LOCK_EX|LOCK_NB));CHECK(!strcmp(package_reservation_release_empty(pool,package[0],index,NULL,NULL,&released),"package_assembly_busy"));CHECK(released==-7);CHECK(!close(assembly));
    index->payload_bytes=11;CHECK(!strcmp(package_reservation_release_empty(pool,package[0],index,NULL,NULL,&released),"package_reservation_identity_conflict"));CHECK(released==-7);index->payload_bytes=10;
    unknown=openat(pool,".reservation.pending",O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(unknown>=0);CHECK(write(unknown,"foreign",7)==7);CHECK(!close(unknown));CHECK(!strcmp(package_reservation_release_empty(pool,package[0],index,NULL,NULL,&released),"unsafe_package_reservation"));CHECK(released==-7);CHECK(!unlinkat(pool,".reservation.pending",0));
    CHECK(!package_reservation_release_empty(pool,package[0],index,NULL,NULL,&released));CHECK(released==1);CHECK(!package_reservation_release_empty(pool,package[0],index,NULL,NULL,&released));CHECK(released==0);
    CHECK(!package_reserve(pool,package[2],index,20,2,&result));CHECK(result.packages==2&&result.reserved_bytes==20);
    CHECK(!package_reserve(pool,package[0],index,30,3,&result));CHECK(result.packages==3&&result.reserved_bytes==30);
    for(unsigned i=0;i<3;i++)CHECK(!close(package[i]));CHECK(!close(owner));CHECK(!close(pool));free(index);
    printf("PASS persistent package reservations: %u checks; idempotency, process exit, shared quota, contention and foreign metadata refusal\n",checks);return 0;
}
