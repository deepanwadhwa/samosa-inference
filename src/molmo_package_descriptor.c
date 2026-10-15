#define _POSIX_C_SOURCE 200809L
#include "molmo_package_descriptor.h"
#ifdef JSON_TOKENS_MAX
#undef JSON_TOKENS_MAX
#endif
#define JSON_TOKENS_MAX 32
#include "json.h"
static int pin(const char *s){return s&&strnlen(s,65)==64&&strspn(s,"0123456789abcdef")==64;}
static int equal(const Json *j,const char *key,const char *expected){
    char *value=json_string(j,json_get(j,0,key));int ok=value&&!strcmp(value,expected);free(value);return ok;
}
const char *molmo_package_descriptor_parse(const char *body,size_t bytes,const PackageIndex *index,MolmoPackageDescriptor *out){
    if(!body||!bytes||bytes>4096||!index||!out||!index->count||index->count>PACKAGE_INDEX_FILES)return "invalid_molmo_package_descriptor";
    Json j;if(!json_parse(&j,body,bytes)||j.tokens[0].type!=JT_OBJECT||j.tokens[0].count!=4||
       !equal(&j,"format","samosa.inference.molmo-package.v1")||!equal(&j,"profile","molmo2-4b-mlx-q4-v1")||!equal(&j,"model_directory","model"))return "invalid_molmo_package_descriptor";
    char *engine=json_string(&j,json_get(&j,0,"engine"));
    if(!engine||!package_file_name_valid(engine,PACKAGE_ENGINE)||strchr(engine,'/')){free(engine);return "invalid_molmo_package_descriptor";}
    MolmoPackageDescriptor result={0};size_t engines=0,kernels=0,manifests=0,descriptors=0,models=0;const char *e=NULL;
    for(size_t i=0;!e&&i<index->count;i++){
        const PackageFile *f=&index->files[i];
        if(!memchr(f->name,0,sizeof(f->name))||!package_file_name_valid(f->name,f->role)||!pin(f->sha256)||!f->bytes||f->bytes>PACKAGE_PAYLOAD_BYTES){e="invalid_package_index";break;}
        if(f->role==PACKAGE_ENGINE){engines++;result.engine=i;if(strcmp(f->name,engine))e="molmo_package_identity_conflict";}
        if(!strcmp(f->name,"mlx.metallib")){kernels++;result.kernels=i;if(f->role!=PACKAGE_DATA||f->bytes>UINT64_C(268435456))e="molmo_package_identity_conflict";}
        if(!strcmp(f->name,"model/manifest.json")){manifests++;result.manifest=i;if(f->role!=PACKAGE_METADATA||f->bytes>1048576)e="molmo_package_identity_conflict";}
        if(!strcmp(f->name,"molmo-package.json")){descriptors++;if(f->role!=PACKAGE_METADATA||f->bytes>4096)e="molmo_package_identity_conflict";}
        if(!strncmp(f->name,"model/",6)&&strchr(f->name+6,'/'))e="molmo_package_identity_conflict";
        if(f->role==PACKAGE_MODEL){models++;if(strncmp(f->name,"model/",6))e="molmo_package_identity_conflict";}
    }
    free(engine);if(!e&&(engines!=1||kernels!=1||manifests!=1||descriptors!=1||!models))e="molmo_package_identity_conflict";
    if(!e)*out=result;return e;
}
const char *molmo_package_bind_model(const PackageIndex *index,const TemporalPackageReceipt *receipt){
    if(!index||!index->count||index->count>PACKAGE_INDEX_FILES||!receipt||receipt->count<4||receipt->count>64)return "invalid_molmo_package_binding";
    unsigned char found[64]={0};size_t manifests=0,matched=0;
    for(size_t i=0;i<index->count;i++){
        const PackageFile *f=&index->files[i];if(!memchr(f->name,0,sizeof(f->name)))return "invalid_package_index";
        if(strncmp(f->name,"model/",6))continue;
        if(!strcmp(f->name,"model/manifest.json")){if(f->role!=PACKAGE_METADATA)return "molmo_package_identity_conflict";manifests++;continue;}
        size_t n=0;for(;n<receipt->count;n++){
            const TemporalPackageFile *r=&receipt->files[n];
            if(!memchr(r->name,0,sizeof(r->name))||!package_file_name_valid(r->name,PACKAGE_DATA)||strchr(r->name,'/')||!pin(r->sha256)||!r->bytes)return "invalid_molmo_package_binding";
            if(!strcmp(f->name+6,r->name))break;
        }
        if(n==receipt->count||found[n])return "molmo_package_identity_conflict";
        const TemporalPackageFile *r=&receipt->files[n];size_t length=strlen(r->name);
        enum PackageFileRole role=length>12&&!strcmp(r->name+length-12,".safetensors")?PACKAGE_MODEL:!strcmp(r->name,"tokenizer.json")?PACKAGE_DATA:PACKAGE_METADATA;
        if(f->role!=role||f->bytes!=r->bytes||!memchr(f->sha256,0,sizeof(f->sha256))||strcmp(f->sha256,r->sha256))return "molmo_package_identity_conflict";
        found[n]=1;matched++;
    }
    return manifests==1&&matched==receipt->count?NULL:"molmo_package_identity_conflict";
}
