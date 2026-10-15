#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "molmo_package.h"
#include "package_native_runtime.h"
#include "artifact.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static int pin(const char *s){return s&&strnlen(s,65)==64&&strspn(s,"0123456789abcdef")==64;}
int molmo_package_policy_valid(const MolmoPackagePolicy *p){return p&&pin(p->engine_sha256)&&pin(p->kernels_sha256)&&pin(p->manifest_sha256);}
static int bound(int root,const char *path){struct stat held,named;return !fstat(root,&held)&&!lstat(path,&named)&&S_ISDIR(held.st_mode)&&S_ISDIR(named.st_mode)&&held.st_dev==named.st_dev&&held.st_ino==named.st_ino;}
const char *molmo_package_verify(int root,const char *path,const PackageIndex *index,const MolmoPackagePolicy *policy,
                                const char *(*control)(void *),void *context,MolmoPackageDescriptor *out){
    if(!out||!molmo_package_policy_valid(policy))return "invalid_molmo_package_policy";
    if(!index||!index->count||index->count>PACKAGE_INDEX_FILES)return "invalid_package_index";
    const char *e=control?control(context):NULL;if(e)return e;
    if(!path||path[0]!='/'||strnlen(path,4096)>=4096||!bound(root,path))return "unsafe_molmo_package_root";
    const PackageFile *descriptor=NULL;
    for(size_t i=0;i<index->count;i++){
        const PackageFile *f=&index->files[i];if(!memchr(f->name,0,sizeof(f->name)))return "invalid_package_index";
        if(!strcmp(f->name,"molmo-package.json")){if(descriptor)return "molmo_package_identity_conflict";descriptor=f;}
    }
    if(!descriptor||descriptor->role!=PACKAGE_METADATA||!descriptor->bytes||descriptor->bytes>4096||!pin(descriptor->sha256))return "invalid_molmo_package_descriptor";
    int fd=openat(root,"molmo-package.json",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);if(fd<0)return "artifact_changed";
    char body[4096];e=artifact_verify_controlled(fd,descriptor->bytes,sizeof(body),descriptor->sha256,control,context);
    size_t at=0;while(!e&&at<descriptor->bytes){e=control?control(context):NULL;if(e)break;ssize_t n=pread(fd,body+at,(size_t)descriptor->bytes-at,(off_t)at);if(n<0&&errno==EINTR)continue;if(n<=0)e="artifact_changed";else at+=(size_t)n;}
    MolmoPackageDescriptor result;if(!e)e=molmo_package_descriptor_parse(body,(size_t)descriptor->bytes,index,&result);
    if(!e)e=artifact_verify_controlled(fd,descriptor->bytes,sizeof(body),descriptor->sha256,control,context);close(fd);
    if(!e&&(strcmp(index->files[result.engine].sha256,policy->engine_sha256)||strcmp(index->files[result.kernels].sha256,policy->kernels_sha256)))e="unqualified_molmo_package_runtime";
    if(!e&&strcmp(index->files[result.manifest].sha256,policy->manifest_sha256))e="unqualified_molmo_package_model";
    if(!e)e=package_native_runtime_verify(root,index,control,context);
    int model=-1;char model_path[4096];TemporalPackageReceipt *receipt=NULL;
    if(!e){int n=snprintf(model_path,sizeof(model_path),"%s/model",path);if(n<0||(size_t)n>=sizeof(model_path))e="unsafe_molmo_package_root";}
    if(!e){model=openat(root,"model",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(model<0)e="artifact_changed";}
    if(!e){receipt=calloc(1,sizeof(*receipt));if(!receipt)e="resource_limit";}
    /* Retain native model identity, upstream/profile and processor checks. The
     * generic index must describe exactly those same verified model files. */
    if(!e)e=temporal_package_verify(model,model_path,policy->manifest_sha256,control,context,receipt,0);
    if(!e)e=molmo_package_bind_model(index,receipt);
    if(!e)e=temporal_package_verify(model,model_path,policy->manifest_sha256,control,context,receipt,1);
    if(!e)e=package_native_runtime_verify(root,index,control,context);
    if(!e&&!bound(root,path))e="unsafe_molmo_package_root";
    if(!e)e=control?control(context):NULL;
    free(receipt);if(model>=0)close(model);if(!e)*out=result;return e;
}
