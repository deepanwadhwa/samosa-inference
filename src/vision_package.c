#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "vision_package.h"
#include "package_native_runtime.h"
#include "artifact.h"
#ifdef JSON_TOKENS_MAX
#undef JSON_TOKENS_MAX
#endif
#define JSON_TOKENS_MAX 32
#include "json.h"
#include <fcntl.h>
#include <unistd.h>
static int pin(const char *s){return s&&strnlen(s,65)==64&&strspn(s,"0123456789abcdef")==64;}
int vision_package_policy_valid(const VisionPackagePolicy *p){
    return p&&pin(p->engine_sha256)&&pin(p->config_sha256)&&pin(p->tokenizer_sha256)&&pin(p->weights_sha256)&&pin(p->kernels_sha256);
}
static int equal(const Json *j,const char *key,const char *expected){
    char *value=json_string(j,json_get(j,0,key));int ok=value&&!strcmp(value,expected);free(value);return ok;
}
const char *vision_package_verify(int root,const PackageIndex *index,const VisionPackagePolicy *policy,
                                 const char *(*control)(void *),void *context,VisionPackageDescriptor *out){
    if(!out||!vision_package_policy_valid(policy))return "invalid_vision_package_policy";
    if(!index||!index->count||index->count>PACKAGE_INDEX_FILES)return "invalid_package_index";
    const char *e=control?control(context):NULL;if(e)return e;
    const char *names[]={"vision-package.json","model/config.json","model/tokenizer.json","model/model.safetensors","mlx.metallib"};
    const enum PackageFileRole roles[]={PACKAGE_METADATA,PACKAGE_METADATA,PACKAGE_DATA,PACKAGE_MODEL,PACKAGE_DATA};
    size_t positions[5]={0},counts[5]={0},engines=0,models=0;VisionPackageDescriptor result={0};
    for(size_t i=0;i<index->count;i++){
        const PackageFile *f=&index->files[i];
        if(!memchr(f->name,0,sizeof(f->name))||!package_file_name_valid(f->name,f->role)||!pin(f->sha256)||!f->bytes||f->bytes>PACKAGE_PAYLOAD_BYTES)return "invalid_package_index";
        if(f->role==PACKAGE_ENGINE){engines++;result.engine=i;}if(f->role==PACKAGE_MODEL)models++;
        for(size_t n=0;n<5;n++)if(!strcmp(f->name,names[n])){if(f->role!=roles[n]||counts[n]++)return "vision_package_identity_conflict";positions[n]=i;}
    }
    if(engines!=1||models!=1||strchr(index->files[result.engine].name,'/'))return "vision_package_identity_conflict";
    for(size_t n=0;n<5;n++)if(counts[n]!=1)return "vision_package_identity_conflict";
    result.config=positions[1];result.tokenizer=positions[2];result.weights=positions[3];
    if(strcmp(index->files[result.engine].sha256,policy->engine_sha256))return "unqualified_vision_package_runtime";
    if(strcmp(index->files[result.config].sha256,policy->config_sha256)||strcmp(index->files[result.tokenizer].sha256,policy->tokenizer_sha256)||strcmp(index->files[result.weights].sha256,policy->weights_sha256)||strcmp(index->files[positions[4]].sha256,policy->kernels_sha256))return "unqualified_vision_package_model";
    const PackageFile *descriptor=&index->files[positions[0]];
    if(index->files[positions[4]].bytes>UINT64_C(268435456)||descriptor->bytes>4096||index->files[result.config].bytes>1048576||index->files[result.tokenizer].bytes>8388608||index->files[result.weights].bytes>UINT64_C(8589934592))return "incompatible_vision_package_model";
    int fd=openat(root,"vision-package.json",O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);if(fd<0)return "artifact_changed";
    char body[4096];e=artifact_verify_controlled(fd,descriptor->bytes,sizeof(body),descriptor->sha256,control,context);
    size_t at=0;while(!e&&at<descriptor->bytes){e=control?control(context):NULL;if(e)break;ssize_t n=pread(fd,body+at,(size_t)descriptor->bytes-at,(off_t)at);if(n<0&&errno==EINTR)continue;if(n<=0)e="artifact_changed";else at+=(size_t)n;}
    Json j;
    /* Fixed paths match the existing public VisionPsy bridge. Neither package
     * metadata nor a public request can choose a loader command or model path. */
    if(!e&&(!json_parse(&j,body,(size_t)descriptor->bytes)||j.tokens[0].type!=JT_OBJECT||j.tokens[0].count!=4||
        !equal(&j,"format","samosa.inference.vision-package.v1")||!equal(&j,"profile","visionpsy-v1")||!equal(&j,"model_directory","model")))e="invalid_vision_package_descriptor";
    if(!e){char *engine=json_string(&j,json_get(&j,0,"engine"));if(!engine||strcmp(engine,index->files[result.engine].name))e="vision_package_identity_conflict";free(engine);}
    if(!e)e=artifact_verify_controlled(fd,descriptor->bytes,sizeof(body),descriptor->sha256,control,context);close(fd);
    if(!e)e=package_native_runtime_verify(root,index,control,context);
    if(!e)e=control?control(context):NULL;if(!e)*out=result;return e;
}
