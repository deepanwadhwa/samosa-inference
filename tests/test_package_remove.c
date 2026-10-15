#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_remove.h"
#include "package_identity.h"
#include "package_reserve.h"
#include "sha256.h"
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"payload removal failure %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static const char *cancel(void *unused){(void)unused;return "cancelled";}
static const char *kill_after_first(void *opaque){int package=*(int *)opaque;
    if(faccessat(package,"payload/models/a",F_OK,0)<0)kill(getpid(),SIGKILL);return NULL;
}
static const char *kill_after_marker(void *opaque){int package=*(int *)opaque;
    if(faccessat(package,"complete",F_OK,0)<0)kill(getpid(),SIGKILL);return NULL;
}
static const char *kill_during_cleanup(void *opaque){int package=*(int *)opaque;
    if(faccessat(package,"copy-journal",F_OK,0)<0&&faccessat(package,"removal.record",F_OK,0)==0)kill(getpid(),SIGKILL);return NULL;
}
static const char *kill_after_inventory(void *opaque){int package=*(int *)opaque;struct stat s;
    if(!fstatat(package,".removal.record.pending",&s,AT_SYMLINK_NOFOLLOW)&&(s.st_mode&0777)==0400)kill(getpid(),SIGKILL);return NULL;
}
int main(void){
    char path[]="/tmp/samosa-remove-payload-XXXXXX";CHECK(mkdtemp(path));int package=open(path,O_RDONLY|O_DIRECTORY);CHECK(package>=0);
    int lock=openat(package,"assemble.lock",O_RDWR|O_CREAT|O_EXCL,0600);CHECK(lock>=0);CHECK(!mkdirat(package,"payload",0700));int payload=openat(package,"payload",O_RDONLY|O_DIRECTORY);CHECK(payload>=0);
    CHECK(!mkdirat(payload,"models",0700));CHECK(!mkdirat(payload,"models/sub",0700));
    PackageIndex *index=calloc(1,sizeof(*index));CHECK(index);memset(index->manifest_sha256,'a',64);index->count=2;index->payload_bytes=6;
    const char *names[]={"models/a","models/sub/b"},*data[]={"abc","def"};
    for(size_t i=0;i<2;i++){strcpy(index->files[i].name,names[i]);index->files[i].role=PACKAGE_MODEL;index->files[i].bytes=3;hash_bytes(data[i],3,index->files[i].sha256);
        int fd=openat(payload,names[i],O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(fd>=0);CHECK(write(fd,data[i],3)==3);CHECK(!close(fd));}
    CHECK(!package_identity_bind(package,index,NULL,NULL));struct stat p,c,r;CHECK(!fstat(payload,&p));char complete[256];int n=snprintf(complete,sizeof(complete),"samosa.package.complete.v1\n%s\n%llu %llu\n",index->manifest_sha256,(unsigned long long)p.st_dev,(unsigned long long)p.st_ino);
    int fd=openat(package,"complete",O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(fd>=0);CHECK(write(fd,complete,(size_t)n)==n);CHECK(!close(fd));CHECK(!fstat(package,&r));CHECK(!fstatat(package,"complete",&c,AT_SYMLINK_NOFOLLOW));
    PackageTreeSnapshot *snapshot=malloc(sizeof(*snapshot));PackageRemovalRecord *record=malloc(sizeof(*record));CHECK(snapshot&&record);
    CHECK(!strcmp(package_remove_begin(package,index,cancel,NULL),"cancelled"));CHECK(faccessat(package,"removal.record",F_OK,0)<0);
    CHECK(!flock(lock,LOCK_SH|LOCK_NB));CHECK(!strcmp(package_remove_begin(package,index,NULL,NULL),"package_assembly_busy"));CHECK(!flock(lock,LOCK_UN));
    fd=openat(payload,"unknown",O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(fd>=0);CHECK(!close(fd));CHECK(package_remove_begin(package,index,NULL,NULL));CHECK(faccessat(package,"removal.record",F_OK,0)<0);CHECK(!unlinkat(payload,"unknown",0));
    fd=openat(package,".removal.record.pending",O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(fd>=0);CHECK(write(fd,"foreign",7)==7);CHECK(!close(fd));
    CHECK(!strcmp(package_remove_begin(package,index,NULL,NULL),"unsafe_package_removal_record"));CHECK(faccessat(package,".removal.record.pending",F_OK,0)==0);CHECK(faccessat(payload,names[0],F_OK,0)==0);CHECK(!unlinkat(package,".removal.record.pending",0));
    pid_t starter=fork();CHECK(starter>=0);if(!starter)_exit(package_remove_begin(package,index,kill_after_inventory,&package)?2:3);int starter_status;
    CHECK(waitpid(starter,&starter_status,0)==starter&&WIFSIGNALED(starter_status)&&WTERMSIG(starter_status)==SIGKILL);CHECK(faccessat(package,"removal.record",F_OK,0)<0);
    CHECK(!package_remove_begin(package,index,NULL,NULL));CHECK(!package_remove_begin(package,index,NULL,NULL));CHECK(faccessat(package,".removal.record.pending",F_OK,0)<0);
    CHECK(!linkat(package,"removal.record",package,".removal.record.pending",0));CHECK(!package_remove_begin(package,index,NULL,NULL));CHECK(faccessat(package,".removal.record.pending",F_OK,0)<0);
    char pool_path[256];CHECK(snprintf(pool_path,sizeof(pool_path),"%s.pool",path)<(int)sizeof(pool_path));CHECK(!mkdir(pool_path,0700));int pool=open(pool_path,O_RDONLY|O_DIRECTORY);CHECK(pool>=0);
    PackageReservationResult charge;CHECK(!package_reserve(pool,package,index,6,1,&charge));CHECK(charge.packages==1&&charge.reserved_bytes==6);int released=-7;
    CHECK(!strcmp(package_remove_release(pool,package,index,NULL,NULL,&released),"package_reservation_not_empty"));CHECK(released==-7);
    CHECK(!strcmp(package_remove_payload(package,index,cancel,NULL),"cancelled"));CHECK(faccessat(payload,names[0],F_OK,0)==0);
    CHECK(!strcmp(package_remove_retire(package,index,NULL,NULL),"package_removal_not_empty"));CHECK(faccessat(package,"complete",F_OK,0)==0);
    CHECK(!flock(lock,LOCK_SH|LOCK_NB));CHECK(!strcmp(package_remove_payload(package,index,NULL,NULL),"package_assembly_busy"));CHECK(!flock(lock,LOCK_UN));
    fd=openat(payload,"foreign",O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(fd>=0);CHECK(write(fd,"unknown",7)==7);CHECK(!close(fd));
    CHECK(!strcmp(package_remove_payload(package,index,NULL,NULL),"unsafe_package_removal_payload"));CHECK(faccessat(payload,names[0],F_OK,0)==0);CHECK(faccessat(payload,"foreign",F_OK,0)==0);CHECK(!unlinkat(payload,"foreign",0));
    /* Unknown symlinks likewise refuse before any indexed deletion. */
    CHECK(!symlinkat("models/a",payload,"alias"));CHECK(!strcmp(package_remove_payload(package,index,NULL,NULL),"unsafe_package_removal_payload"));CHECK(faccessat(payload,names[0],F_OK,0)==0);CHECK(!unlinkat(payload,"alias",0));
    CHECK(!renameat(payload,names[1],package,"saved"));fd=openat(payload,names[1],O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(fd>=0);CHECK(write(fd,"def",3)==3);CHECK(!close(fd));
    CHECK(!strcmp(package_remove_payload(package,index,NULL,NULL),"unsafe_package_removal_payload"));CHECK(faccessat(payload,names[0],F_OK,0)==0);CHECK(faccessat(payload,names[1],F_OK,0)==0);
    CHECK(!unlinkat(payload,names[1],0));CHECK(!renameat(package,"saved",payload,names[1]));
    /* The test owner explicitly recaptures after restoring its renamed inode;
     * removal itself never updates a journal to adopt changed identities. */
    CHECK(!unlinkat(package,"removal.record",0));CHECK(!fstat(package,&r));CHECK(!package_tree_snapshot(payload,index,NULL,NULL,snapshot));CHECK(!package_removal_record_capture(index,snapshot,&r,&c,record));CHECK(!package_removal_record_store(package,index,record,NULL,NULL));
    pid_t child=fork();CHECK(child>=0);if(!child)_exit(package_remove_payload(package,index,kill_after_first,&package)?2:3);int status;CHECK(waitpid(child,&status,0)==child&&WIFSIGNALED(status)&&WTERMSIG(status)==SIGKILL);
    CHECK(faccessat(payload,names[0],F_OK,0)<0);CHECK(faccessat(payload,names[1],F_OK,0)==0);CHECK(faccessat(package,"complete",F_OK,0)==0);CHECK(faccessat(package,"removal.record",F_OK,0)==0);
    CHECK(!package_remove_payload(package,index,NULL,NULL));CHECK(faccessat(payload,names[1],F_OK,0)<0);CHECK(faccessat(payload,"models",F_OK,0)<0);CHECK(!package_remove_payload(package,index,NULL,NULL));
    CHECK(faccessat(package,"payload",F_OK,0)==0&&faccessat(package,"complete",F_OK,0)==0&&faccessat(package,"removal.record",F_OK,0)==0);
    CHECK(!strcmp(package_remove_retire(package,index,cancel,NULL),"cancelled"));CHECK(faccessat(package,"complete",F_OK,0)==0);
    CHECK(!flock(lock,LOCK_SH|LOCK_NB));CHECK(!strcmp(package_remove_retire(package,index,NULL,NULL),"package_assembly_busy"));CHECK(!flock(lock,LOCK_UN));
    CHECK(!renameat(package,"payload",package,"saved-payload"));CHECK(!mkdirat(package,"payload",0700));CHECK(!strcmp(package_remove_retire(package,index,NULL,NULL),"unsafe_package_removal_payload"));CHECK(faccessat(package,"complete",F_OK,0)==0);
    CHECK(!unlinkat(package,"payload",AT_REMOVEDIR));CHECK(!renameat(package,"saved-payload",package,"payload"));
    child=fork();CHECK(child>=0);if(!child)_exit(package_remove_retire(package,index,kill_after_marker,&package)?2:3);
    CHECK(waitpid(child,&status,0)==child&&WIFSIGNALED(status)&&WTERMSIG(status)==SIGKILL);CHECK(faccessat(package,"complete",F_OK,0)<0);CHECK(faccessat(package,"payload",F_OK,0)==0);CHECK(faccessat(package,"removal.record",F_OK,0)==0);
    CHECK(!package_remove_retire(package,index,NULL,NULL));CHECK(faccessat(package,"payload",F_OK,0)<0);CHECK(!package_remove_retire(package,index,NULL,NULL));CHECK(faccessat(package,"removal.record",F_OK,0)==0);
    CHECK(!mkdirat(package,"payload",0700));CHECK(!strcmp(package_remove_retire(package,index,NULL,NULL),"unsafe_package_removal_payload"));CHECK(faccessat(package,"payload",F_OK,0)==0);CHECK(!unlinkat(package,"payload",AT_REMOVEDIR));
    fd=openat(package,"complete",O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(fd>=0);CHECK(write(fd,"foreign",7)==7);CHECK(!close(fd));CHECK(!strcmp(package_remove_retire(package,index,NULL,NULL),"unsafe_package_removal_payload"));CHECK(faccessat(package,"complete",F_OK,0)==0);CHECK(!unlinkat(package,"complete",0));
    CHECK(!strcmp(package_remove_release(pool,package,index,cancel,NULL,&released),"cancelled"));CHECK(released==-7);
    fd=openat(package,"foreign",O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(fd>=0);CHECK(write(fd,"unknown",7)==7);CHECK(!close(fd));CHECK(!strcmp(package_remove_release(pool,package,index,NULL,NULL,&released),"package_reservation_not_empty"));CHECK(released==-7);CHECK(!unlinkat(package,"foreign",0));
    CHECK(!mkdirat(package,"copy-journal",0700));int journal=openat(package,"copy-journal",O_RDONLY|O_DIRECTORY);CHECK(journal>=0);fd=openat(journal,"unknown",O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(fd>=0);CHECK(!close(fd));CHECK(!strcmp(package_remove_release(pool,package,index,NULL,NULL,&released),"package_removal_not_empty"));CHECK(released==-7);CHECK(!unlinkat(journal,"unknown",0));CHECK(!close(journal));
    CHECK(!flock(lock,LOCK_SH|LOCK_NB));CHECK(!strcmp(package_remove_release(pool,package,index,NULL,NULL,&released),"package_assembly_busy"));CHECK(released==-7);CHECK(!flock(lock,LOCK_UN));
    int pool_lock=openat(pool,"reserve.lock",O_RDWR);CHECK(pool_lock>=0);CHECK(!flock(pool_lock,LOCK_EX|LOCK_NB));CHECK(!strcmp(package_remove_release(pool,package,index,NULL,NULL,&released),"package_reservation_busy"));CHECK(released==-7);CHECK(!flock(pool_lock,LOCK_UN));
    CHECK(!package_reserve(pool,package,index,6,1,&charge));CHECK(charge.packages==1&&charge.reserved_bytes==6);
    CHECK(!package_remove_release(pool,package,index,NULL,NULL,&released));CHECK(released==1);CHECK(!package_remove_release(pool,package,index,NULL,NULL,&released));CHECK(released==0);CHECK(faccessat(package,"removal.record",F_OK,0)==0);
    CHECK(!strcmp(package_remove_finish(pool,package,index,cancel,NULL,&released),"cancelled"));CHECK(faccessat(package,"removal.record",F_OK,0)==0);
    fd=openat(package,"publish.lock",O_RDWR|O_CREAT|O_EXCL,0600);CHECK(fd>=0);CHECK(!close(fd));
    child=fork();CHECK(child>=0);if(!child){int outcome=-7;_exit(package_remove_finish(pool,package,index,kill_during_cleanup,&package,&outcome)?2:3);}
    CHECK(waitpid(child,&status,0)==child&&WIFSIGNALED(status)&&WTERMSIG(status)==SIGKILL);CHECK(faccessat(package,"removal.record",F_OK,0)==0);CHECK(faccessat(package,"copy-journal",F_OK,0)<0);
    CHECK(!package_remove_finish(pool,package,index,NULL,NULL,&released));CHECK(released==0);CHECK(faccessat(package,"removal.record",F_OK,0)<0);CHECK(faccessat(package,"copy-journal",F_OK,0)<0);CHECK(faccessat(package,"publish.lock",F_OK,0)<0);CHECK(!package_remove_finish(pool,package,index,NULL,NULL,&released));
    CHECK(!package_reserve(pool,package,index,6,1,&charge));CHECK(charge.packages==1&&charge.reserved_bytes==6);CHECK(!package_remove_finish(pool,package,index,NULL,NULL,&released));CHECK(released==1);
    CHECK(!package_remove_run(pool,package,index,NULL,NULL,&released));CHECK(released==0);
    int operation=openat(package,"remove.lock",O_RDWR|O_NOFOLLOW);CHECK(operation>=0);CHECK(!flock(operation,LOCK_EX|LOCK_NB));released=-7;CHECK(!strcmp(package_remove_run(pool,package,index,NULL,NULL,&released),"package_removal_busy"));CHECK(released==-7);CHECK(!flock(operation,LOCK_UN));CHECK(!close(operation));
    CHECK(!package_remove_run(pool,package,index,NULL,NULL,&released));CHECK(released==0);CHECK(!unlinkat(package,"remove.lock",0));
    CHECK(!close(pool_lock));CHECK(!unlinkat(pool,"reserve.lock",0));CHECK(!close(pool));CHECK(!rmdir(pool_path));
    CHECK(!close(payload));CHECK(!unlinkat(package,"assembly.identity",0));CHECK(!close(lock));CHECK(!unlinkat(package,"assemble.lock",0));CHECK(!close(package));CHECK(!rmdir(path));free(index);free(snapshot);free(record);
    printf("PASS payload removal: %u checks; recorded inodes, writer exclusion, foreign refusal, SIGKILL resume and idempotent payload emptying\n",checks);return 0;
}
