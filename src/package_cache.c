#define _POSIX_C_SOURCE 200809L
#include "package_cache.h"
#include <stdio.h>
#include <string.h>
typedef struct {const char *(*control)(void *);void *request;const char *reason;
    int (*cancelled)(void *);void *cancellation;} CacheControl;
static int interrupted(void *opaque){CacheControl *c=opaque;
    if(c->control){c->reason=c->control(c->request);if(c->reason)return 1;}
    return c->cancelled&&c->cancelled(c->cancellation);
}
const char *package_cache_open(void *opaque,const PackageFile *file,const char *(*control)(void *),void *request,int *out){
    PackageCacheSource *source=opaque;
    if(!source||!file||!out||!package_file_name_valid(file->name,file->role))return "invalid_package_cache_source";
    ArtifactConfig config=source->store;config.sha256=file->sha256;config.size=file->bytes;
    const char *e=artifact_open_verified(&config,control,request,out);
    /* A corrupt/foreign/busy cache is never treated as a download miss. */
    if(!e||strcmp(e,"artifact_absent")||!source->source_directory)return e;
    if(source->source_directory[0]!='/'||config.acquire)return "invalid_package_cache_source";
    char path[4096];int length=snprintf(path,sizeof(path),"%s/%s",source->source_directory,file->name);
    if(length<0||(size_t)length>=sizeof(path))return "invalid_package_cache_source";
    config.source_path=path;
    CacheControl controls={.control=control,.request=request,.cancelled=config.cancelled,.cancellation=config.cancellation};
    config.cancelled=interrupted;config.cancellation=&controls;
    e=artifact_install(&config);
    if(controls.reason)return controls.reason;
    if(e)return e;
    return artifact_open_verified(&config,control,request,out);
}
