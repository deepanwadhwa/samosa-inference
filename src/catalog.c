#include "catalog.h"
#define JSON_TOKENS_MAX 1024
#include "json.h"
static int field(const Json *j,int t,const char *key,char *out,size_t cap) {
    char *s=json_string(j,json_get(j,t,key));
    if(!s)return 0;
    size_t n=strlen(s);int ok=n>0&&n<cap;
    for(size_t i=0;ok&&i<n;i++)if((unsigned char)s[i]<32||(unsigned char)s[i]==127)ok=0;
    if(ok)memcpy(out,s,n+1);
    free(s);return ok;
}
static int identifier(const char *s) {
    for(;*s;s++)if(!((*s>='a'&&*s<='z')||(*s>='A'&&*s<='Z')||(*s>='0'&&*s<='9')||strchr("._-",*s)))return 0;
    return 1;
}
const char *provider_catalog_parse(const char *bytes,size_t length,ProviderCatalog *out) {
    if(!bytes||!out||length>PROVIDER_CATALOG_BYTES)return "invalid_catalog";
    Json j;size_t version;
    ProviderCatalog *c=calloc(1,sizeof(*c));if(!c)return "allocation_failed";
    const char *error="invalid_catalog";
    if(!json_parse(&j,bytes,length)||j.tokens[0].type!=JT_OBJECT||j.tokens[0].count!=3)goto done;
    if(!json_uint(&j,json_get(&j,0,"catalog_version"),&version)||version!=1)goto done;
    if(!field(&j,0,"revision",c->revision,sizeof(c->revision))||!identifier(c->revision))goto done;
    int a=json_get(&j,0,"entries");
    if(a<0||j.tokens[a].type!=JT_ARRAY||j.tokens[a].count>PROVIDER_CATALOG_MAX)goto done;
    for(int t=a+1;t<j.tokens[a].next;t=j.tokens[t].next) {
        ProviderEntry *e=&c->entries[c->count];size_t size;
        if(j.tokens[t].type!=JT_OBJECT||j.tokens[t].count!=10)goto done;
#define FIELD(name) if(!field(&j,t,#name,e->name,sizeof(e->name)))goto done
        FIELD(id);FIELD(version);FIELD(url);FIELD(license);FIELD(platform);
        FIELD(minimum_os);FIELD(runtime);FIELD(recipe);FIELD(sha256);
#undef FIELD
        if(!identifier(e->id)||!identifier(e->version)||!identifier(e->platform)||!identifier(e->minimum_os)||!identifier(e->runtime)||!identifier(e->recipe))goto done;
        uint32_t os[3];if(!provider_os_version(e->minimum_os,os))goto done;
        if(strncmp(e->url,"https://",8)||!e->url[8]||strchr(e->url,' ')||strchr(e->url,'@')||strchr(e->url,'#'))goto done;
        if(strlen(e->sha256)!=64)goto done;
        for(size_t k=0;k<64;k++)if(!((e->sha256[k]>='0'&&e->sha256[k]<='9')||(e->sha256[k]>='a'&&e->sha256[k]<='f')))goto done;
        if(!json_uint(&j,json_get(&j,t,"size"),&size)||!size||size>INT64_MAX)goto done;
        e->size=size;
        for(size_t k=0;k<c->count;k++)if(!strcmp(c->entries[k].id,e->id))goto done;
        c->count++;
    }
    *out=*c;error=NULL;
done:free(c);return error;
}
const char *provider_catalog_select(const ProviderCatalog *c,const char *id,const char *platform,const ProviderEntry **out) {
    if(!c||!id||!platform||!out||c->count>PROVIDER_CATALOG_MAX)return "invalid_catalog_selection";
    for(size_t i=0;i<c->count;i++)if(!strcmp(c->entries[i].id,id)) {
        if(strcmp(c->entries[i].platform,platform))return "unsupported_provider_platform";
        if(strcmp(c->entries[i].runtime,"system-runtime-v1"))return "unsupported_provider_runtime";
        *out=&c->entries[i];return NULL;
    }
    return "unknown_provider";
}
void provider_catalog_artifact(const ProviderEntry *entry,ArtifactConfig *config) {
    config->sha256=entry->sha256;config->size=entry->size;
}
