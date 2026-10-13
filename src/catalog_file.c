#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "catalog.h"
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
const char *provider_catalog_load(const char *path,ProviderCatalog *out) {
    if(!path||path[0]!='/'||strlen(path)>4095||!out)return "invalid_catalog_file";
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if(fd<0)return "invalid_catalog_file";
    struct stat before,after,named;char *bytes=NULL;size_t used=0;
    const char *error="invalid_catalog_file";
    if(fstat(fd,&before)||!S_ISREG(before.st_mode)||before.st_uid!=geteuid()||(before.st_mode&077)||before.st_nlink!=1||before.st_size<=0||before.st_size>PROVIDER_CATALOG_BYTES)goto done;
    bytes=malloc((size_t)before.st_size);if(!bytes){error="allocation_failed";goto done;}
    while(used<(size_t)before.st_size){ssize_t n=pread(fd,bytes+used,(size_t)before.st_size-used,(off_t)used);
        if(n<0&&errno==EINTR)continue;
        if(n<=0)goto done;
        used+=(size_t)n;
    }
    if(fstat(fd,&after)||lstat(path,&named)||!S_ISREG(named.st_mode)||after.st_uid!=geteuid()||(after.st_mode&077)||after.st_nlink!=1||before.st_dev!=after.st_dev||before.st_ino!=after.st_ino||before.st_size!=after.st_size||before.st_mtime!=after.st_mtime||before.st_ctime!=after.st_ctime||named.st_dev!=after.st_dev||named.st_ino!=after.st_ino)goto done;
#ifdef __APPLE__
    if(before.st_mtimespec.tv_nsec!=after.st_mtimespec.tv_nsec||before.st_ctimespec.tv_nsec!=after.st_ctimespec.tv_nsec)goto done;
#else
    if(before.st_mtim.tv_nsec!=after.st_mtim.tv_nsec||before.st_ctim.tv_nsec!=after.st_ctim.tv_nsec)goto done;
#endif
    error=provider_catalog_parse(bytes,used,out);
done:free(bytes);close(fd);return error;
}
