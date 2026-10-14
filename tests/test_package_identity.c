#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_identity.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"identity failure %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static const char *pending_cancel(void *opaque){int directory=*(int *)opaque;struct stat s;
    return !fstatat(directory,".assembly.identity.pending",&s,AT_SYMLINK_NOFOLLOW)?"cancelled":NULL;
}
int main(void){
    char path[]="/tmp/samosa-package-identity-XXXXXX";CHECK(mkdtemp(path));int root=open(path,O_RDONLY|O_DIRECTORY);CHECK(root>=0);
    PackageIndex *index=calloc(1,sizeof(*index));CHECK(index);memset(index->manifest_sha256,'a',64);
    CHECK(!strcmp(package_identity_bind(root,index,pending_cancel,&root),"cancelled"));CHECK(faccessat(root,"assembly.identity",F_OK,0)<0);CHECK(faccessat(root,".assembly.identity.pending",F_OK,0)<0);
    CHECK(!package_identity_bind(root,index,NULL,NULL));CHECK(!package_identity_bind(root,index,NULL,NULL));
    int record=openat(root,"assembly.identity",O_RDONLY|O_NOFOLLOW);CHECK(record>=0);struct stat identity;CHECK(!fstat(record,&identity));CHECK((identity.st_mode&0777)==0400&&identity.st_nlink==1);
    index->manifest_sha256[0]='b';CHECK(!strcmp(package_identity_bind(root,index,NULL,NULL),"package_assembly_identity_conflict"));index->manifest_sha256[0]='a';CHECK(!package_identity_bind(root,index,NULL,NULL));
    /* Copying an identity record into another directory cannot claim ownership
     * there: the persisted device/inode belongs to the original package root. */
    CHECK(!mkdirat(root,"other",0700));int other=openat(root,"other",O_RDONLY|O_DIRECTORY);CHECK(other>=0);
    int copy=openat(other,"assembly.identity",O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(copy>=0);char body[256];ssize_t bytes=pread(record,body,sizeof(body),0);CHECK(bytes>0);CHECK(write(copy,body,(size_t)bytes)==bytes);CHECK(!close(copy));
    CHECK(!strcmp(package_identity_bind(other,index,NULL,NULL),"package_assembly_identity_conflict"));CHECK(!close(record));
    int pending=openat(root,".assembly.identity.pending",O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(pending>=0);CHECK(write(pending,"foreign",7)==7);CHECK(!close(pending));
    CHECK(!strcmp(package_identity_bind(root,index,NULL,NULL),"unsafe_package_identity"));CHECK(faccessat(root,".assembly.identity.pending",F_OK,0)==0);CHECK(!unlinkat(root,".assembly.identity.pending",0));
    CHECK(!package_identity_bind(root,index,NULL,NULL));
    CHECK(!mkdirat(root,"symlink",0700));int linked=openat(root,"symlink",O_RDONLY|O_DIRECTORY);CHECK(linked>=0);CHECK(!symlinkat("../assembly.identity",linked,"assembly.identity"));
    CHECK(!strcmp(package_identity_bind(linked,index,NULL,NULL),"unsafe_package_identity"));
    CHECK(!close(linked));CHECK(!close(other));CHECK(!close(root));free(index);
    printf("PASS persistent package identity: %u checks; cancellation, pin change, cloned root, unknown pending and symlink refusal\n",checks);return 0;
}
