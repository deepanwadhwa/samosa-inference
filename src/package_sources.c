#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "package_sources.h"
#include "private_config.h"
#include "private_key.h"
#define JSON_TOKENS_MAX 8192
#include "json.h"
void package_sources_free(PackageSources *sources){
    if(!sources)return;
    for(size_t i=0;i<PACKAGE_INDEX_FILES;i++)private_key_clear(sources->keys[i]);
    free(sources);
}
const char *package_sources_load(const char *path,const char *index_pin,const RemoteArtifactSource *transport,PackageSources **out){
    if(!out||!transport||!index_pin||strlen(index_pin)!=64||strspn(index_pin,"0123456789abcdef")!=64)return "invalid_package_sources";
    char *bytes=NULL;size_t length=0;
    const char *e=private_config_read(path,PACKAGE_INDEX_BYTES,&bytes,&length);
    if(e)return !strcmp(e,"allocation_failed")?e:"invalid_package_sources_file";
    PackageSources *next=calloc(1,sizeof(*next));Json *j=malloc(sizeof(*j));
    char *format=NULL,*pin=NULL,*key_path=NULL;
    if(!next||!j){e="allocation_failed";goto done;}
    e="invalid_package_sources";
    if(!json_parse(j,bytes,length)||j->tokens[0].type!=JT_OBJECT||j->tokens[0].count!=3||
       !(format=json_string(j,json_get(j,0,"format")))||strcmp(format,"samosa.inference.package-sources.v1")||
       !(pin=json_string(j,json_get(j,0,"index_sha256"))))goto done;
    if(strcmp(pin,index_pin)){e="package_sources_identity_conflict";goto done;}
    int files=json_get(j,0,"files");
    if(files<0||j->tokens[files].type!=JT_ARRAY||!j->tokens[files].count||(size_t)j->tokens[files].count>PACKAGE_INDEX_FILES)goto done;
    for(int t=files+1;t<j->tokens[files].next;t=j->tokens[t].next){
        if(j->tokens[t].type!=JT_OBJECT)goto done;
        int credential=json_get(j,t,"bearer_key_file");
        if(j->tokens[t].count!=(credential<0?1:2))goto done;
        char *url=json_string(j,json_get(j,t,"url"));
        int valid=url&&strlen(url)>8&&strlen(url)<=2048&&!strncmp(url,"https://",8)&&!strchr(url,'#')&&!strchr(url,'@');
        if(valid)for(const unsigned char *p=(const unsigned char *)url;*p;p++)if(*p<=32||*p==127){valid=0;break;}
        if(!valid){free(url);goto done;}
        size_t at=next->count;strcpy(next->urls[at],url);free(url);
        next->files[at]=*transport;next->files[at].url=next->urls[at];next->files[at].bearer_key=NULL;
        if(credential>=0){
            key_path=json_string(j,credential);
            if(!key_path||!private_key_read(key_path,next->keys[at])){e="invalid_provider_credentials";goto done;}
            free(key_path);key_path=NULL;next->files[at].bearer_key=next->keys[at];
        }
        next->count++;
    }
    *out=next;next=NULL;e=NULL;
done:free(key_path);free(format);free(pin);free(j);free(bytes);package_sources_free(next);return e;
}
