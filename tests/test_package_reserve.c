#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_reserve.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"reservation failure %d: %s\n",__LINE__,#x);exit(1);}}while(0)
int main(void){
    char root[]="/tmp/samosa-reserve-XXXXXX";CHECK(mkdtemp(root));int pool=open(root,O_RDONLY|O_DIRECTORY);CHECK(pool>=0);
    /* Package directories live outside the reserved ledger namespace. */
    char packages[]="/tmp/samosa-reserve-packages-XXXXXX";CHECK(mkdtemp(packages));int owner=open(packages,O_RDONLY|O_DIRECTORY);CHECK(owner>=0);
    int package[3];for(unsigned i=0;i<3;i++){char name[2]={(char)('a'+i),0};CHECK(!mkdirat(owner,name,0700));package[i]=openat(owner,name,O_RDONLY|O_DIRECTORY);CHECK(package[i]>=0);}
    PackageIndex *index=calloc(1,sizeof(*index));CHECK(index);memset(index->manifest_sha256,'a',64);index->payload_bytes=10;
    PackageReservationResult result;CHECK(!package_reserve(pool,package[0],index,20,2,&result));CHECK(result.packages==1&&result.reserved_bytes==10);
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
    for(unsigned i=0;i<3;i++)CHECK(!close(package[i]));CHECK(!close(owner));CHECK(!close(pool));free(index);
    printf("PASS persistent package reservations: %u checks; idempotency, process exit, shared quota, contention and foreign metadata refusal\n",checks);return 0;
}
