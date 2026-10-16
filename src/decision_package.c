#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "decision_package.h"
#include "package_native_runtime.h"
#include "artifact.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
static int pin(const char *s,size_t length) {
    return s&&strnlen(s,length+1)==length&&strspn(s,"0123456789abcdef")==length;
}
int decision_package_policy_valid(const DecisionPackagePolicy *policy) {
    if(!policy||policy->profile>DECISION_ASSETS_IMAGE||!pin(policy->runtime_revision,40))return 0;
    for(unsigned i=0;i<DECISION_ASSET_COUNT;i++)if(!pin(policy->pins[i],64))return 0;
    return 1;
}
const char *decision_package_verify(int root,const char *path,const PackageIndex *index,const DecisionPackagePolicy *policy,
                                    const char *(*control)(void *),void *context,DecisionPackageDescriptor *out) {
    if(!out||!decision_package_policy_valid(policy))return "invalid_decision_package_policy";
    if(!index||!index->count||index->count>PACKAGE_INDEX_FILES||
       !memchr(index->recipe,0,sizeof(index->recipe))||strcmp(index->recipe,"opendecision-package-v1"))return "invalid_package_index";
    const char *error=control?control(context):NULL;if(error)return error;
    char assets_path[4096];
    if(!path||strnlen(path,sizeof(assets_path))>=sizeof(assets_path)||!package_native_runtime_bound(root,path))return "unsafe_decision_package_root";
    int length=snprintf(assets_path,sizeof(assets_path),"%s/assets",path);
    if(length<0||(size_t)length>=sizeof(assets_path))return "unsafe_decision_package_root";
    const PackageFile *descriptor=NULL;
    for(size_t i=0;i<index->count;i++) {
        const PackageFile *file=&index->files[i];
        if(!memchr(file->name,0,sizeof(file->name))||!package_file_name_valid(file->name,file->role)||
           !pin(file->sha256,64)||!file->bytes||file->bytes>PACKAGE_PAYLOAD_BYTES)return "invalid_package_index";
        if(!strcmp(file->name,"decision-package.json")) {
            if(descriptor)return "decision_package_identity_conflict";
            descriptor=file;
        }
    }
    if(!descriptor||descriptor->role!=PACKAGE_METADATA||descriptor->bytes>4096)return "invalid_decision_package_descriptor";
    int fd=openat(root,"decision-package.json",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    if(fd<0)return "artifact_changed";
    char body[4096];size_t at=0;
    error=artifact_verify_controlled(fd,descriptor->bytes,sizeof(body),descriptor->sha256,control,context);
    while(!error&&at<descriptor->bytes) {
        error=control?control(context):NULL;if(error)break;
        ssize_t count=pread(fd,body+at,(size_t)descriptor->bytes-at,(off_t)at);
        if(count<0&&errno==EINTR)continue;
        if(count<=0)error="artifact_changed";else at+=(size_t)count;
    }
    DecisionPackageDescriptor result;
    if(!error)error=decision_package_descriptor_parse(body,at,index,&result);
    if(!error)error=artifact_verify_controlled(fd,descriptor->bytes,sizeof(body),descriptor->sha256,control,context);
    close(fd);
    if(!error&&(result.profile!=policy->profile||strcmp(result.runtime_revision,policy->runtime_revision)))error="unqualified_decision_package_profile";
    for(unsigned i=0;!error&&i<DECISION_ASSET_COUNT;i++)
        if(strcmp(index->files[result.files[i]].sha256,policy->pins[i]))
            error=i<=DECISION_RUNTIME?"unqualified_decision_package_runtime":"unqualified_decision_package_model";
    if(!error)error=package_native_runtime_verify(root,index,control,context);
    DecisionAssets assets={0};
    if(!error)error=decision_assets_open_profile(assets_path,policy->pins,policy->profile,control,context,&assets);
    if(!error) {
        struct stat held;
        if(fstatat(root,"assets",&held,AT_SYMLINK_NOFOLLOW)||!S_ISDIR(held.st_mode)||
           held.st_dev!=assets.directory.st_dev||held.st_ino!=assets.directory.st_ino)
            error="artifact_changed";
    }
    if(!error)error=decision_assets_recheck(&assets,control,context);
    decision_assets_close(&assets);
    if(!error) {
        PackageTreeResult observed;
        error=package_tree_verify(root,index,control,context,&observed);
    }
    if(!error&&!package_native_runtime_bound(root,path))error="unsafe_decision_package_root";
    if(!error)error=control?control(context):NULL;
    if(!error)*out=result;
    return error;
}
