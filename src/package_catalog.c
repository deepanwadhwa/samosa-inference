#define _POSIX_C_SOURCE 200809L
#include "package_catalog.h"
#include <errno.h>
#include <fcntl.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
const char *package_catalog_read_index(const ProviderEntry *entry,int fd,const char *(*control)(void *),void *context,PackageIndex *out){
    if(!entry||!out||!entry->size||entry->size>PACKAGE_INDEX_BYTES||!memchr(entry->sha256,0,sizeof(entry->sha256))||
        !memchr(entry->platform,0,sizeof(entry->platform))||!memchr(entry->minimum_os,0,sizeof(entry->minimum_os))||
        !memchr(entry->recipe,0,sizeof(entry->recipe))||!memchr(entry->runtime,0,sizeof(entry->runtime)))return "invalid_package_catalog_entry";
    if(strcmp(entry->runtime,"system-runtime-v1"))return "unsupported_provider_runtime";
    struct stat s;if(fstat(fd,&s)||!S_ISREG(s.st_mode)||s.st_uid!=geteuid()||(s.st_mode&077)||(fcntl(fd,F_GETFL)&O_ACCMODE)!=O_RDONLY)return "unsafe_package_catalog_index";
    const char *e=artifact_verify_controlled(fd,entry->size,PACKAGE_INDEX_BYTES,entry->sha256,control,context);if(e)return e;
    char *body=malloc((size_t)entry->size);PackageIndex *index=calloc(1,sizeof(*index));
    if(!body||!index){free(body);free(index);return "allocation_failed";}
    size_t at=0;while(!e&&at<(size_t)entry->size){e=control?control(context):NULL;if(e)break;
        size_t n=(size_t)entry->size-at;if(n>65536)n=65536;ssize_t bytes=pread(fd,body+at,n,(off_t)at);
        if(bytes<0&&errno==EINTR)continue;if(bytes<=0){e="package_catalog_read_failed";break;}at+=(size_t)bytes;}
    if(!e)e=package_index_parse(body,at,entry->sha256,index);
    if(!e&&(strcmp(index->platform,entry->platform)||strcmp(index->minimum_os,entry->minimum_os)||strcmp(index->recipe,entry->recipe)))e="package_catalog_identity_conflict";
    if(!e)e=control?control(context):NULL;
    if(!e)*out=*index;
    free(body);free(index);return e;
}
