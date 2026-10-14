#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_lease.h"
#include "package_identity.h"
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"package lease failure %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static const char *cancel(void *unused){(void)unused;return "cancelled";}
int main(void){
    char path[]="/tmp/samosa-package-lease-XXXXXX";CHECK(mkdtemp(path));int root=open(path,O_RDONLY|O_DIRECTORY);CHECK(root>=0);
    PackageIndex *index=calloc(1,sizeof(*index));CHECK(index);memset(index->manifest_sha256,'a',64);int held=-7;
    CHECK(!strcmp(package_lease_acquire(root,index,cancel,NULL,&held),"cancelled"));CHECK(held==-7);CHECK(faccessat(root,"assemble.lock",F_OK,0)<0);
    CHECK(!strcmp(package_lease_acquire(root,index,NULL,NULL,&held),"package_lease_absent"));CHECK(held==-7);CHECK(faccessat(root,"assemble.lock",F_OK,0)<0);
    int writer=openat(root,"assemble.lock",O_RDWR|O_CREAT|O_EXCL,0600);CHECK(writer>=0);
    CHECK(!strcmp(package_lease_acquire(root,index,NULL,NULL,&held),"package_identity_absent"));CHECK(held==-7);
    CHECK(!flock(writer,LOCK_EX|LOCK_NB));CHECK(!package_identity_bind(root,index,NULL,NULL));
    CHECK(!strcmp(package_lease_acquire(root,index,NULL,NULL,&held),"package_assembly_busy"));CHECK(held==-7);CHECK(!flock(writer,LOCK_UN));
    CHECK(!package_lease_acquire(root,index,NULL,NULL,&held));CHECK(held>=0&&(fcntl(held,F_GETFD)&FD_CLOEXEC));int second=-7;CHECK(!package_lease_acquire(root,index,NULL,NULL,&second));
    CHECK(flock(writer,LOCK_EX|LOCK_NB)<0&&errno==EWOULDBLOCK);CHECK(!close(held));CHECK(flock(writer,LOCK_EX|LOCK_NB)<0&&errno==EWOULDBLOCK);
    index->manifest_sha256[0]='b';held=-7;CHECK(!strcmp(package_lease_acquire(root,index,NULL,NULL,&held),"package_assembly_identity_conflict"));CHECK(held==-7);index->manifest_sha256[0]='a';
    CHECK(!close(second));CHECK(!flock(writer,LOCK_EX|LOCK_NB));CHECK(!flock(writer,LOCK_UN));
    /* Observe a real child holding the shared lease; death must release it
     * without a stale persistent ownership guess. */
    int ready[2],stay[2];CHECK(!pipe(ready));CHECK(!pipe(stay));pid_t child=fork();CHECK(child>=0);
    if(!child){close(ready[0]);close(stay[1]);int lease=-1;if(package_lease_acquire(root,index,NULL,NULL,&lease))_exit(2);
        if(write(ready[1],"r",1)!=1)_exit(3);char byte;(void)read(stay[0],&byte,1);close(lease);_exit(0);}
    CHECK(!close(ready[1]));CHECK(!close(stay[0]));char byte;CHECK(read(ready[0],&byte,1)==1&&byte=='r');
    CHECK(flock(writer,LOCK_EX|LOCK_NB)<0&&errno==EWOULDBLOCK);CHECK(!kill(child,SIGKILL));int status;CHECK(waitpid(child,&status,0)==child&&WIFSIGNALED(status)&&WTERMSIG(status)==SIGKILL);
    CHECK(!flock(writer,LOCK_EX|LOCK_NB));CHECK(!flock(writer,LOCK_UN));CHECK(!close(ready[0]));CHECK(!close(stay[1]));
    CHECK(!linkat(root,"assemble.lock",root,"alias",0));CHECK(!strcmp(package_lease_acquire(root,index,NULL,NULL,&held),"unsafe_package_lease"));CHECK(held==-7);CHECK(!unlinkat(root,"alias",0));
    CHECK(!close(writer));CHECK(!unlinkat(root,"assemble.lock",0));CHECK(!symlinkat("assembly.identity",root,"assemble.lock"));CHECK(!strcmp(package_lease_acquire(root,index,NULL,NULL,&held),"unsafe_package_lease"));CHECK(held==-7);
    CHECK(!unlinkat(root,"assemble.lock",0));CHECK(!unlinkat(root,"assembly.identity",0));CHECK(!close(root));CHECK(!rmdir(path));free(index);
    printf("PASS package consumer lease: %u checks; reader sharing, writer exclusion, identity, unsafe aliases, controls and child death\n",checks);return 0;
}
