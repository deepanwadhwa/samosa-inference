#include "llama_package_descriptor.h"
#ifdef JSON_TOKENS_MAX
#undef JSON_TOKENS_MAX
#endif
#define JSON_TOKENS_MAX 64
#include "json.h"
static int field(const Json *j,const char *key,char *out,size_t capacity){
    char *s=json_string(j,json_get(j,0,key));int valid=s&&*s&&strlen(s)<capacity;
    if(valid)strcpy(out,s);free(s);return valid;
}
const char *llama_package_descriptor_parse(const char *body,size_t bytes,const PackageIndex *index,LlamaPackageDescriptor *out){
    if(!body||!out||!index||!index->count||index->count>PACKAGE_INDEX_FILES||!bytes||bytes>4096)return "invalid_llama_package_descriptor";
    Json j;if(!json_parse(&j,body,bytes)||j.tokens[0].type!=JT_OBJECT||j.tokens[0].count!=6)return "invalid_llama_package_descriptor";
    LlamaPackageDescriptor result={0};char format[65],engine[256],model[256];
    if(!field(&j,"format",format,sizeof(format))||strcmp(format,"samosa.inference.llama-package.v1")||
        !field(&j,"kind",result.kind,sizeof(result.kind))||(strcmp(result.kind,"text")&&strcmp(result.kind,"embedding"))||
        !field(&j,"runtime_revision",result.runtime_revision,sizeof(result.runtime_revision))||strlen(result.runtime_revision)!=40||strspn(result.runtime_revision,"0123456789abcdef")!=40||
        !field(&j,"architecture",result.architecture,sizeof(result.architecture))||
        !field(&j,"engine",engine,sizeof(engine))||!field(&j,"model",model,sizeof(model))||
        !package_file_name_valid(engine,PACKAGE_ENGINE)||!package_file_name_valid(model,PACKAGE_MODEL)||!strcmp(engine,model))return "invalid_llama_package_descriptor";
    for(const unsigned char *p=(const unsigned char *)result.architecture;*p;p++)if(!((*p>='a'&&*p<='z')||(*p>='0'&&*p<='9')||strchr("._-",*p)))return "invalid_llama_package_descriptor";
    size_t engines=0,models=0;int engine_found=0,model_found=0;
    for(size_t i=0;i<index->count;i++){
        const PackageFile *file=&index->files[i];
        if(!memchr(file->name,0,sizeof(file->name))||!package_file_name_valid(file->name,file->role))return "invalid_package_index";
        if(file->role==PACKAGE_ENGINE)engines++;if(file->role==PACKAGE_MODEL)models++;
        if(!strcmp(file->name,engine)){if(file->role!=PACKAGE_ENGINE||engine_found++)return "llama_package_identity_conflict";result.engine=i;}
        if(!strcmp(file->name,model)){if(file->role!=PACKAGE_MODEL||model_found++)return "llama_package_identity_conflict";result.model=i;}
    }
    if(engines!=1||models!=1||!engine_found||!model_found)return "llama_package_identity_conflict";
    *out=result;return NULL;
}
const char *llama_package_descriptor_model(const LlamaPackageDescriptor *descriptor,const GgufMetadata *metadata){
    if(!descriptor||!metadata||!memchr(descriptor->architecture,0,sizeof(descriptor->architecture))||!descriptor->architecture[0]||
        !memchr(metadata->architecture,0,sizeof(metadata->architecture))||!metadata->architecture[0])return "invalid_llama_package_descriptor";
    if(metadata->version!=3||!metadata->tensors||strcmp(descriptor->architecture,metadata->architecture))return "incompatible_llama_package_model";
    return NULL;
}
