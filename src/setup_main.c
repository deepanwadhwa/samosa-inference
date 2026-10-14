#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "catalog.h"
#include "remote_artifact.h"
#include "private_key.h"
#include "json.h"
#include <signal.h>
static volatile sig_atomic_t interrupted;
static void stop(int sig){(void)sig;interrupted=1;}
static int cancelled(void *context){(void)context;return interrupted!=0;}
static int number(const char *text,uint64_t *result) {
    if(!text||!*text||text[0]<'0'||text[0]>'9')return 0;
    char *end;errno=0;unsigned long long n=strtoull(text,&end,10);
    if(errno||*end)return 0;
    *result=n;return 1;
}
static void property(const char *name,const char *value) {
    fputc(',',stdout);json_quote(stdout,name,strlen(name));fputc(':',stdout);json_quote(stdout,value,strlen(value));
}
static void descriptor(const ProviderEntry *e) {
    fputs("{\"provider_id\":",stdout);json_quote(stdout,e->id,strlen(e->id));
    property("version",e->version);property("artifact_id",e->sha256);
    fprintf(stdout,",\"size\":%llu",(unsigned long long)e->size);
    property("license",e->license);property("platform",e->platform);
    property("minimum_os",e->minimum_os);property("runtime_policy",e->runtime);property("recipe",e->recipe);
}
int main(int argc,char **argv) {
    ArtifactConfig c={.cancelled=cancelled};RemoteArtifactSource remote={0};
    const char *catalog_path=NULL,*key_path=NULL;unsigned seen=0;
    for(int i=1;i<argc;i++) {
        if(i+1>=argc)return 2;
        const char *key=argv[i],*value=argv[++i];unsigned bit=0;
        if(!strcmp(key,"--catalog-file")){bit=1;catalog_path=value;}
        else if(!strcmp(key,"--state")){bit=2;c.state_path=value;}
        else if(!strcmp(key,"--byte-ceiling")){bit=4;if(!number(value,&c.byte_ceiling))return 2;}
        else if(!strcmp(key,"--disk-ceiling")){bit=8;if(!number(value,&c.disk_ceiling))return 2;}
        else if(!strcmp(key,"--artifact-capacity")){uint64_t capacity;bit=64;
            if(!number(value,&capacity)||!capacity||capacity>ARTIFACT_CAPACITY_MAX)return 2;c.artifact_capacity=(size_t)capacity;}
        else if(!strcmp(key,"--ca-file")){bit=16;remote.ca_file=value;}
        else if(!strcmp(key,"--bearer-key-file")){bit=32;key_path=value;}
        else if(!strcmp(key,"--redirect-host")){if(remote.redirect_count==8)return 2;remote.redirect_hosts[remote.redirect_count++]=value;}
        else return 2;
        if(bit&&(seen&bit))return 2;
        seen|=bit;
    }
    char key[130]={0},platform[65]={0},os[65]={0};
    ProviderCatalog *catalog=malloc(sizeof(*catalog));
    const ProviderEntry *entry=NULL;const char *status=NULL,*error=NULL,*host_error=NULL;
    char request[4097];size_t n=fread(request,1,sizeof(request),stdin);Json j;size_t version;char *op=NULL,*id=NULL;
    if(ferror(stdin)||n>4096)error="request_too_large";
    else if(!json_parse(&j,request,n)||j.tokens[0].type!=JT_OBJECT)error="invalid_json";
    else if(!json_uint(&j,json_get(&j,0,"contract_version"),&version)||version!=1)error="unsupported_contract";
    else if(!(op=json_string(&j,json_get(&j,0,"op"))))error="invalid_request";
    else if(!strcmp(op,"provider_catalog")){if(j.tokens[0].count!=2)error="invalid_request";}
    else if(strcmp(op,"provider_install")&&strcmp(op,"provider_resume")&&strcmp(op,"provider_status"))error="unsupported_operation";
    else if(j.tokens[0].count!=3||!(id=json_string(&j,json_get(&j,0,"provider_id"))))error="invalid_request";
    if(!error&&!catalog)error="allocation_failed";
    if(!error)error=provider_catalog_load(catalog_path,catalog);
    if(!error)host_error=provider_catalog_host(platform,os);
    if(!error&&id) {
        error=host_error;
        if(!error)error=provider_catalog_select(catalog,id,platform,&entry);
        if(!error)error=provider_catalog_qualify(entry,os);
        if(!error)provider_catalog_artifact(entry,&c);
    }
    if(!error&&id&&!strcmp(op,"provider_status"))error=artifact_status(&c,&status);
    if(!error&&id&&strcmp(op,"provider_status")) {
        if(key_path&&!private_key_read(key_path,key))error="invalid_provider_credentials";
        if(!error) {
            struct sigaction sa;memset(&sa,0,sizeof(sa));sa.sa_handler=stop;sigemptyset(&sa.sa_mask);
            if(sigaction(SIGINT,&sa,NULL)||sigaction(SIGTERM,&sa,NULL))error="provider_signal_setup_failed";
        }
        if(!error) {
            remote.url=entry->url;remote.bearer_key=key_path?key:NULL;
            c.resume=!strcmp(op,"provider_resume");error=artifact_install_remote(&c,&remote);
            if(!error)status="verified";
        }
    }
    fputs("{\"contract_version\":1",stdout);
    if(error){fputs(",\"error\":{\"code\":",stdout);json_quote(stdout,error,strlen(error));fputc('}',stdout);}
    else {
        property("catalog_revision",catalog->revision);
        if(entry) {
            fputs(",\"provider\":",stdout);descriptor(entry);fputc('}',stdout);
            property("status",status);property("activation","not_requested");property("readiness","unqualified");
        } else {
            fputs(",\"providers\":[",stdout);
            for(size_t i=0;i<catalog->count;i++) {
                const ProviderEntry *e=&catalog->entries[i],*selected=NULL;const char *reason=host_error;
                if(!reason)reason=provider_catalog_select(catalog,e->id,platform,&selected);
                if(!reason)reason=provider_catalog_qualify(e,os);
                if(i)fputc(',',stdout);
                descriptor(e);fprintf(stdout,",\"supported\":%s",reason?"false":"true");
                if(reason)property("unavailable_reason",reason);
                fputc('}',stdout);
            }
            fputc(']',stdout);
        }
    }
    fputs("}\n",stdout);private_key_clear(key);free(op);free(id);free(catalog);
    return error?1:0;
}
