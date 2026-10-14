#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_lease.h"
#include "package_identity.h"
#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
static int bound(int root,int lock){struct stat held,named;
    return !fstat(lock,&held)&&!fstatat(root,"assemble.lock",&named,AT_SYMLINK_NOFOLLOW)&&S_ISREG(held.st_mode)&&
        held.st_uid==geteuid()&&!(held.st_mode&077)&&held.st_nlink==1&&held.st_dev==named.st_dev&&held.st_ino==named.st_ino&&held.st_mode==named.st_mode&&held.st_nlink==named.st_nlink;
}
const char *package_lease_acquire(int root,const PackageIndex *index,const char *(*control)(void *),void *context,int *out){
    if(!index||!out)return "invalid_package_lease";
    const char *e=control?control(context):NULL;if(e)return e;
    struct stat directory;if(fstat(root,&directory)||!S_ISDIR(directory.st_mode)||directory.st_uid!=geteuid()||(directory.st_mode&077)||(fcntl(root,F_GETFL)&O_ACCMODE)!=O_RDONLY)return "unsafe_package_lease";
    int lock=openat(root,"assemble.lock",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if(lock<0)return errno==ENOENT?"package_lease_absent":"unsafe_package_lease";
    if(!bound(root,lock))e="unsafe_package_lease";
    if(!e&&flock(lock,LOCK_SH|LOCK_NB))e=errno==EWOULDBLOCK?"package_assembly_busy":"package_assembly_unavailable";
    if(!e)e=package_identity_verify(root,index,control,context);
    if(!e)e=control?control(context):NULL;
    if(!e&&!bound(root,lock))e="unsafe_package_lease";
    if(!e)*out=lock;else close(lock);return e;
}
