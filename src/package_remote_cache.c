#define _POSIX_C_SOURCE 200809L
#include "package_remote_cache.h"
#include <string.h>
#include <stdio.h>
typedef struct {const char *(*control)(void *);void *request;const char *reason;
    int (*cancelled)(void *);void *cancellation;} RemoteControl;
static int interrupted(void *opaque){RemoteControl *c=opaque;
    if(c->control){c->reason=c->control(c->request);if(c->reason)return 1;}
    return c->cancelled&&c->cancelled(c->cancellation);
}
const char *package_remote_cache_open(void *opaque,const PackageFile *file,const char *(*control)(void *),void *request,int *out){
    PackageRemoteCacheSource *source=opaque;
    if(!source||!file||!out||!source->index||!source->index->count||source->index->count>PACKAGE_INDEX_FILES||
        source->count!=source->index->count||(source->files&&source->digest_source)||!package_file_name_valid(file->name,file->role))return "invalid_package_remote_source";
    size_t at=0;while(at<source->count&&strcmp(file->name,source->index->files[at].name))at++;
    if(at==source->count)return "package_remote_identity_conflict";
    const PackageFile *pinned=&source->index->files[at];
    if(file->bytes!=pinned->bytes||file->role!=pinned->role||strcmp(file->sha256,pinned->sha256))return "package_remote_identity_conflict";
    ArtifactConfig config=source->store;config.sha256=pinned->sha256;config.size=pinned->bytes;
    const char *e=artifact_open_verified(&config,control,request,out);
    /* Cache corruption and contention are not download misses. Offline hits
     * do not inspect or require the corresponding network credentials/URL. */
    if(!e||strcmp(e,"artifact_absent")||(!source->files&&!source->digest_source))return e;
    if(config.acquire)return "invalid_package_remote_source";
    RemoteControl controls={.control=control,.request=request,.cancelled=config.cancelled,.cancellation=config.cancellation};
    config.cancelled=interrupted;config.cancellation=&controls;
    RemoteArtifactSource download;char url[2049];
    if(source->digest_source){download=*source->digest_source;const char *base=download.url;
        if(!base||strncmp(base,"https://",8)||strchr(base,'?')||strchr(base,'#'))return "invalid_package_remote_source";
        size_t length=strlen(base);if(length<=8||length>1983||base[length-1]=='/')return "invalid_package_remote_source";
        int written=snprintf(url,sizeof(url),"%s/%s",base,pinned->sha256);
        if(written<0||(size_t)written>=sizeof(url))return "invalid_package_remote_source";download.url=url;
    }else download=source->files[at];
    e=artifact_install_remote(&config,&download);
    if(controls.reason)return controls.reason;
    if(e)return e;
    return artifact_open_verified(&config,control,request,out);
}
