#include "decision_package_descriptor.h"
#ifdef JSON_TOKENS_MAX
#undef JSON_TOKENS_MAX
#endif
#define JSON_TOKENS_MAX 64
#include "json.h"
static int field(const Json *j,const char *key,char *out,size_t capacity) {
    char *text=json_string(j,json_get(j,0,key));
    int valid=text&&*text&&strlen(text)<capacity;
    if(valid)strcpy(out,text);free(text);return valid;
}
const char *decision_package_descriptor_parse(const char *body,size_t bytes,const PackageIndex *index,DecisionPackageDescriptor *out) {
    if(!body||!out||!index||index->count<5||index->count>PACKAGE_INDEX_FILES||!bytes||bytes>4096)
        return "invalid_decision_package_descriptor";
    Json j;
    if(!json_parse(&j,body,bytes)||j.tokens[0].type!=JT_OBJECT||j.tokens[0].count!=5)
        return "invalid_decision_package_descriptor";
    DecisionPackageDescriptor result={0};
    char format[65],profile[16],protocol[65],assets[16];
    if(!field(&j,"format",format,sizeof(format))||strcmp(format,"samosa.inference.decision-package.v1")||
       !field(&j,"profile",profile,sizeof(profile))||(strcmp(profile,"text")&&strcmp(profile,"image"))||
       !field(&j,"protocol",protocol,sizeof(protocol))||strcmp(protocol,"opendecision-framed-v1")||
       !field(&j,"assets",assets,sizeof(assets))||strcmp(assets,"assets")||
       !field(&j,"runtime_revision",result.runtime_revision,sizeof(result.runtime_revision))||
       strlen(result.runtime_revision)!=40||strspn(result.runtime_revision,"0123456789abcdef")!=40)
        return "invalid_decision_package_descriptor";
    result.profile=!strcmp(profile,"image")?DECISION_ASSETS_IMAGE:DECISION_ASSETS_TEXT;
    const enum PackageFileRole roles[]={PACKAGE_ENGINE,PACKAGE_LIBRARY,PACKAGE_MODEL,PACKAGE_DATA};
    unsigned found=0;int descriptor=0;
    for(size_t i=0;i<index->count;i++) {
        const PackageFile *file=&index->files[i];
        if(!memchr(file->name,0,sizeof(file->name))||!package_file_name_valid(file->name,file->role))
            return "invalid_package_index";
        for(size_t prior=0;prior<i;prior++)
            if(!strcmp(file->name,index->files[prior].name))return "decision_package_identity_conflict";
        if(!strcmp(file->name,"decision-package.json")) {
            if(file->role!=PACKAGE_METADATA||descriptor++)return "decision_package_identity_conflict";
            continue;
        }
        if(strncmp(file->name,"assets/",7)) {
            if(file->role!=PACKAGE_METADATA)return "decision_package_identity_conflict";
            continue;
        }
        unsigned asset;
        for(asset=0;asset<DECISION_ASSET_COUNT;asset++)
            if(!strcmp(file->name+7,decision_asset_name(asset)))break;
        if(asset==DECISION_ASSET_COUNT||file->role!=roles[asset]||(found&(1u<<asset)))
            return "decision_package_identity_conflict";
        found|=1u<<asset;result.files[asset]=i;
    }
    if(!descriptor||found!=((1u<<DECISION_ASSET_COUNT)-1))return "decision_package_identity_conflict";
    *out=result;return NULL;
}
