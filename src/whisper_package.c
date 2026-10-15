#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "whisper_package.h"
#include "package_native_runtime.h"
#include "artifact.h"
#ifdef JSON_TOKENS_MAX
#undef JSON_TOKENS_MAX
#endif
#define JSON_TOKENS_MAX 64
#include "json.h"
#include <fcntl.h>
#include <unistd.h>
static int pin(const char *s,size_t n){return s&&strnlen(s,n+1)==n&&strspn(s,"0123456789abcdef")==n;}
int whisper_package_policy_valid(const WhisperPackagePolicy *p){
    return p&&pin(p->runtime_revision,40)&&p->profile&&strnlen(p->profile,32)<32&&!strcmp(p->profile,"base.en-f16")&&pin(p->engine_sha256,64)&&pin(p->model_sha256,64);
}
static int field(const Json *j,const char *name,char *out,size_t size){char *s=json_string(j,json_get(j,0,name));int valid=s&&*s&&strlen(s)<size;if(valid)strcpy(out,s);free(s);return valid;}
const char *whisper_package_descriptor_parse(const char *body,size_t bytes,const PackageIndex *index,WhisperPackageDescriptor *out){
    if(!body||!bytes||bytes>4096||!index||!out||!index->count||index->count>PACKAGE_INDEX_FILES)return "invalid_whisper_package_descriptor";
    Json j;if(!json_parse(&j,body,bytes)||j.tokens[0].type!=JT_OBJECT||j.tokens[0].count!=5)return "invalid_whisper_package_descriptor";
    WhisperPackageDescriptor result={0};char format[65],engine[256],model[256];
    if(!field(&j,"format",format,sizeof(format))||strcmp(format,"samosa.inference.whisper-package.v1")||
       !field(&j,"runtime_revision",result.runtime_revision,sizeof(result.runtime_revision))||!pin(result.runtime_revision,40)||
       !field(&j,"profile",result.profile,sizeof(result.profile))||strcmp(result.profile,"base.en-f16")||
       !field(&j,"engine",engine,sizeof(engine))||!field(&j,"model",model,sizeof(model))||
       !package_file_name_valid(engine,PACKAGE_ENGINE)||!package_file_name_valid(model,PACKAGE_MODEL)||!strcmp(engine,model))return "invalid_whisper_package_descriptor";
    size_t engines=0,models=0;int found_engine=0,found_model=0;
    for(size_t i=0;i<index->count;i++){
        const PackageFile *f=&index->files[i];if(!memchr(f->name,0,sizeof(f->name))||!package_file_name_valid(f->name,f->role))return "invalid_package_index";
        if(f->role==PACKAGE_ENGINE)engines++;if(f->role==PACKAGE_MODEL)models++;
        if(!strcmp(f->name,engine)){if(f->role!=PACKAGE_ENGINE||found_engine++)return "whisper_package_identity_conflict";result.engine=i;}
        if(!strcmp(f->name,model)){if(f->role!=PACKAGE_MODEL||found_model++)return "whisper_package_identity_conflict";result.model=i;}
    }
    if(engines!=1||models!=1||!found_engine||!found_model)return "whisper_package_identity_conflict";
    *out=result;return NULL;
}
static int open_file(int root,const char *name){
    char path[256];strcpy(path,name);int parent=openat(root,".",O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(parent<0)return -1;
    char *leaf=path;for(char *slash=strchr(leaf,'/');slash;slash=strchr(leaf,'/')){
        *slash=0;int next=openat(parent,leaf,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);close(parent);if(next<0)return -1;parent=next;leaf=slash+1;}
    int fd=openat(parent,leaf,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);close(parent);return fd;
}
static const char *read_at(int fd,void *data,size_t size,const char *(*control)(void *),void *context){
    unsigned char *out=data;size_t at=0;
    while(at<size){const char *e=control?control(context):NULL;if(e)return e;ssize_t n=pread(fd,out+at,size-at,(off_t)at);if(n<0&&errno==EINTR)continue;if(n<=0)return "artifact_changed";at+=(size_t)n;}
    return NULL;
}
static const char *model_header(int fd,const char *(*control)(void *),void *context){
    /* Pinned whisper.cpp f049fff: magic followed by eleven LE32 hparams.
     * This checks the supported profile only; the separate qualified full model
     * pin and native model tests establish its tensor/vocabulary contents. */
    const uint32_t expected[]={UINT32_C(0x67676d6c),51864,1500,512,8,6,448,512,8,6,80,1};
    unsigned char bytes[48];const char *e=read_at(fd,bytes,sizeof(bytes),control,context);if(e)return e;
    for(size_t i=0;i<12;i++){uint32_t value=0;for(unsigned b=0;b<4;b++)value|=(uint32_t)bytes[i*4+b]<<(8*b);if(value!=expected[i])return "incompatible_whisper_package_model";}
    return NULL;
}
const char *whisper_package_verify(int root,const PackageIndex *index,const WhisperPackagePolicy *policy,const char *(*control)(void *),void *context,WhisperPackageDescriptor *out){
    if(!out||!whisper_package_policy_valid(policy))return "invalid_whisper_package_policy";
    if(!index||!index->count||index->count>PACKAGE_INDEX_FILES)return "invalid_package_index";
    const char *e=control?control(context):NULL;if(e)return e;const PackageFile *manifest=NULL;
    for(size_t i=0;i<index->count;i++){
        const PackageFile *f=&index->files[i];if(!memchr(f->name,0,sizeof(f->name))||!package_file_name_valid(f->name,f->role)||!pin(f->sha256,64)||!f->bytes||f->bytes>PACKAGE_PAYLOAD_BYTES)return "invalid_package_index";
        if(!strcmp(f->name,"whisper-package.json")){if(manifest)return "invalid_whisper_package_descriptor";manifest=f;}
    }
    if(!manifest||manifest->role!=PACKAGE_METADATA||manifest->bytes>4096)return "invalid_whisper_package_descriptor";
    int fd=open_file(root,manifest->name);if(fd<0)return "artifact_changed";char body[4096];
    e=artifact_verify_controlled(fd,manifest->bytes,sizeof(body),manifest->sha256,control,context);
    if(!e)e=read_at(fd,body,(size_t)manifest->bytes,control,context);
    WhisperPackageDescriptor result;if(!e)e=whisper_package_descriptor_parse(body,(size_t)manifest->bytes,index,&result);
    if(!e)e=artifact_verify_controlled(fd,manifest->bytes,sizeof(body),manifest->sha256,control,context);close(fd);
    if(!e&&(strcmp(result.runtime_revision,policy->runtime_revision)||strcmp(result.profile,policy->profile)||strcmp(index->files[result.engine].sha256,policy->engine_sha256)))e="unqualified_whisper_package_runtime";
    if(!e&&strcmp(index->files[result.model].sha256,policy->model_sha256))e="unqualified_whisper_package_model";
    if(!e){const PackageFile *model=&index->files[result.model];fd=open_file(root,model->name);if(fd<0)e="artifact_changed";
        else{if(model->bytes<48)e="incompatible_whisper_package_model";else e=artifact_verify_controlled(fd,model->bytes,PACKAGE_PAYLOAD_BYTES,model->sha256,control,context);if(!e)e=model_header(fd,control,context);
            if(!e)e=artifact_verify_controlled(fd,model->bytes,PACKAGE_PAYLOAD_BYTES,model->sha256,control,context);close(fd);}}
    if(!e)e=package_native_runtime_verify(root,index,control,context);if(!e)e=control?control(context):NULL;if(!e)*out=result;return e;
}
