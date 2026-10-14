#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "llama_package.h"
#include "package_native_runtime.h"
#include "artifact.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
static int pin(const char *s,size_t n){return s&&strnlen(s,n+1)==n&&strspn(s,"0123456789abcdef")==n;}
static int open_file(int root,const char *name){
    char path[256];strcpy(path,name);int parent=openat(root,".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(parent<0)return -1;
    char *leaf=path;for(char *slash=strchr(leaf,'/');slash;slash=strchr(leaf,'/')){
        *slash=0;int next=openat(parent,leaf,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);close(parent);if(next<0)return -1;parent=next;leaf=slash+1;}
    int fd=openat(parent,leaf,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);close(parent);return fd;
}
const char *llama_package_verify(int root,const PackageIndex *index,const LlamaPackagePolicy *policy,const char *(*control)(void *),void *context,LlamaPackageDescriptor *out){
    if(!index||!out||!policy||!index->count||index->count>PACKAGE_INDEX_FILES||!pin(policy->runtime_revision,40)||!pin(policy->engine_sha256,64)||
        !policy->kind||(strcmp(policy->kind,"text")&&strcmp(policy->kind,"embedding"))||!policy->architecture||!policy->architecture[0]||strnlen(policy->architecture,65)>64)return "invalid_llama_package_policy";
    const char *e=control?control(context):NULL;if(e)return e;
    const PackageFile *manifest=NULL;
    for(size_t i=0;i<index->count;i++){
        const PackageFile *file=&index->files[i];if(!memchr(file->name,0,sizeof(file->name))||!package_file_name_valid(file->name,file->role)||!pin(file->sha256,64)||!file->bytes||file->bytes>PACKAGE_PAYLOAD_BYTES)return "invalid_package_index";
        if(!strcmp(file->name,"llama-package.json")){if(manifest)return "invalid_llama_package_descriptor";manifest=file;}
    }
    if(!manifest||manifest->role!=PACKAGE_METADATA||!manifest->bytes||manifest->bytes>4096)return "invalid_llama_package_descriptor";
    int fd=open_file(root,manifest->name);if(fd<0)return "artifact_changed";
    e=artifact_verify_controlled(fd,manifest->bytes,4096,manifest->sha256,control,context);char body[4096];size_t at=0;
    while(!e&&at<manifest->bytes){e=control?control(context):NULL;if(e)break;ssize_t n=pread(fd,body+at,(size_t)manifest->bytes-at,(off_t)at);
        if(n<0&&errno==EINTR)continue;if(n<=0){e="artifact_changed";break;}at+=(size_t)n;}
    LlamaPackageDescriptor descriptor;if(!e)e=llama_package_descriptor_parse(body,at,index,&descriptor);
    if(!e)e=artifact_verify_controlled(fd,manifest->bytes,4096,manifest->sha256,control,context);close(fd);
    if(!e&&(strcmp(descriptor.runtime_revision,policy->runtime_revision)||strcmp(descriptor.kind,policy->kind)||strcmp(descriptor.architecture,policy->architecture)||
        strcmp(index->files[descriptor.engine].sha256,policy->engine_sha256)))e="unqualified_llama_package_runtime";
    if(!e){const PackageFile *model=&index->files[descriptor.model];fd=open_file(root,model->name);if(fd<0)e="artifact_changed";
        else{GgufMetadata metadata;e=artifact_verify_controlled(fd,model->bytes,PACKAGE_PAYLOAD_BYTES,model->sha256,control,context);
            if(!e)e=gguf_metadata_read(fd,control,context,&metadata);if(!e)e=llama_package_descriptor_model(&descriptor,&metadata);
            if(!e)e=artifact_verify_controlled(fd,model->bytes,PACKAGE_PAYLOAD_BYTES,model->sha256,control,context);close(fd);}}
    if(!e)e=package_native_runtime_verify(root,index,control,context);
    if(!e)e=control?control(context):NULL;if(!e)*out=descriptor;return e;
}
