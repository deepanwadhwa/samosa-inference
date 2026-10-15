#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "catalog.h"
#include "remote_artifact.h"
#include "private_key.h"
#include "json.h"
#include "package_installer.h"
#include "package_compatibility.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <signal.h>
static volatile sig_atomic_t interrupted;
static void stop(int sig){(void)sig;interrupted=1;}
static int cancelled(void *context){(void)context;return interrupted!=0;}
typedef struct {const char *package_path,*pool_path;int package,pool;} SetupControl;
static int named_directory(const char *path,int fd){struct stat held,named;
    return path&&fd>=0&&!fstat(fd,&held)&&!lstat(path,&named)&&S_ISDIR(named.st_mode)&&named.st_uid==geteuid()&&!(named.st_mode&077)&&held.st_dev==named.st_dev&&held.st_ino==named.st_ino;
}
static const char *control(void *context){SetupControl *c=context;
    if(interrupted)return "cancelled";
    if(c&&(!named_directory(c->package_path,c->package)||(c->pool>=0&&!named_directory(c->pool_path,c->pool))))return "package_tree_changed";
    return NULL;
}
static int directory(const char *path){
    if(!path||path[0]!='/'||strlen(path)>4000)return -1;
    int fd=open(path,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);struct stat held,named;
    if(fd<0)return -1;
    if(fstat(fd,&held)||lstat(path,&named)||!S_ISDIR(held.st_mode)||held.st_uid!=geteuid()||(held.st_mode&077)||held.st_dev!=named.st_dev||held.st_ino!=named.st_ino){close(fd);return -1;}return fd;
}
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
    const char *catalog_path=NULL,*key_path=NULL,*package_path=NULL,*pool_path=NULL,*digest_url=NULL;unsigned seen=0;
    uint64_t copy_ceiling=0,pool_ceiling=0,pool_capacity=0;
    LlamaPackagePolicy llama_policy={0};
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
        else if(!strcmp(key,"--package-directory")){bit=128;package_path=value;}
        else if(!strcmp(key,"--reservation-directory")){bit=256;pool_path=value;}
        else if(!strcmp(key,"--copy-ceiling")){bit=512;if(!number(value,&copy_ceiling))return 2;}
        else if(!strcmp(key,"--reservation-ceiling")){bit=1024;if(!number(value,&pool_ceiling))return 2;}
        else if(!strcmp(key,"--reservation-capacity")){bit=2048;if(!number(value,&pool_capacity)||pool_capacity>1024)return 2;}
        else if(!strcmp(key,"--digest-endpoint")){bit=4096;digest_url=value;}
        else if(!strcmp(key,"--llama-runtime-revision")){bit=8192;llama_policy.runtime_revision=value;}
        else if(!strcmp(key,"--llama-kind")){bit=16384;llama_policy.kind=value;}
        else if(!strcmp(key,"--llama-architecture")){bit=32768;llama_policy.architecture=value;}
        else if(!strcmp(key,"--llama-engine-sha256")){bit=65536;llama_policy.engine_sha256=value;}
        else if(!strcmp(key,"--redirect-host")){if(remote.redirect_count==8)return 2;remote.redirect_hosts[remote.redirect_count++]=value;}
        else return 2;
        if(bit&&(seen&bit))return 2;
        seen|=bit;
    }
    unsigned llama_flags=8192|16384|32768|65536;
    if((seen&llama_flags)&&((seen&llama_flags)!=llama_flags||!llama_package_policy_valid(&llama_policy)))return 2;
    const LlamaPackagePolicy *qualified=(seen&llama_flags)?&llama_policy:NULL;
    char key[130]={0},platform[65]={0},os[65]={0};
    ProviderCatalog *catalog=malloc(sizeof(*catalog));
    const ProviderEntry *entry=NULL;const char *status=NULL,*error=NULL,*host_error=NULL;
    char request[4097];size_t n=fread(request,1,sizeof(request),stdin);Json j;size_t version;char *op=NULL,*id=NULL;
    if(ferror(stdin)||n>4096)error="request_too_large";
    else if(!json_parse(&j,request,n)||j.tokens[0].type!=JT_OBJECT)error="invalid_json";
    else if(!json_uint(&j,json_get(&j,0,"contract_version"),&version)||version!=1)error="unsupported_contract";
    else if(!(op=json_string(&j,json_get(&j,0,"op"))))error="invalid_request";
    else if(!strcmp(op,"provider_catalog")){if(j.tokens[0].count!=2)error="invalid_request";}
    else if(strcmp(op,"provider_install")&&strcmp(op,"provider_resume")&&strcmp(op,"provider_status")&&strcmp(op,"provider_release_empty")&&strcmp(op,"provider_remove"))error="unsupported_operation";
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
    if(!error&&id&&!strcmp(op,"provider_status")&&!package_path)error=artifact_status(&c,&status);
    if(!error&&id&&(!strcmp(op,"provider_release_empty")||!strcmp(op,"provider_remove"))&&!package_path)error="invalid_package_directory";
    if(!error&&id&&(!strcmp(op,"provider_install")||!strcmp(op,"provider_resume"))) {
        if(key_path&&!private_key_read(key_path,key))error="invalid_provider_credentials";
        if(!error) {
            struct sigaction sa;memset(&sa,0,sizeof(sa));sa.sa_handler=stop;sigemptyset(&sa.sa_mask);
            if(sigaction(SIGINT,&sa,NULL)||sigaction(SIGTERM,&sa,NULL))error="provider_signal_setup_failed";
        }
        if(!error) {
            remote.url=entry->url;remote.bearer_key=key_path?key:NULL;
            c.resume=!strcmp(op,"provider_resume");
            if(!package_path){error=artifact_install_remote(&c,&remote);if(!error)status="verified";}
        }
    }
    if(!error&&id&&package_path){
        int package=directory(package_path),pool=-1;char payload[4096];
        if(package<0)error="invalid_package_directory";
        if(!error&&snprintf(payload,sizeof(payload),"%s/payload",package_path)>=(int)sizeof(payload))error="invalid_package_directory";
        int observing=!strcmp(op,"provider_status");
        if(!error&&!observing){pool=directory(pool_path);if(pool<0)error="invalid_reservation_directory";}
        RemoteArtifactSource digest=remote;digest.url=digest_url;
        /* Recheck host-selected names throughout acquisition/publication, not
         * only once when opening the descriptors. */
        SetupControl controls={.package_path=package_path,.pool_path=pool_path,.package=package,.pool=pool};
        PackageInstaller installer={.entry=entry,.store=c,.index_source=&remote,.digest_source=digest_url?&digest:NULL,
            .assembly={.directory=package,.source=-1,.payload_path=payload,.copy_ceiling=copy_ceiling,.reservation_directory=pool,.reservation_ceiling=pool_ceiling,.reservation_capacity=(size_t)pool_capacity,.control=control,.context=&controls,.llama_policy=qualified}};
        if(!error&&observing){PackageAssemblyStatus observed;error=package_status_from_file(catalog_path,id,&installer,&observed);if(!error)status=observed.state;}
        else if(!error&&!strcmp(op,"provider_release_empty")){int released;error=package_release_empty_from_file(catalog_path,id,&installer,&released);if(!error)status=released?"reservation_released":"reservation_absent";}
        else if(!error&&!strcmp(op,"provider_remove")){int released;error=package_remove_from_file(catalog_path,id,&installer,&released);if(!error)status="removed";}
        else if(!error){PackageTreeResult installed;error=package_install_selected(catalog,id,&installer,&installed);if(!error)status="installed_verified";}
        /* A renamed configuration root cannot produce a successful response. */
        if(!error){int named=directory(package_path);struct stat a,b;if(named<0||fstat(named,&a)||fstat(package,&b)||a.st_dev!=b.st_dev||a.st_ino!=b.st_ino)error="package_tree_changed";if(named>=0)close(named);}
        if(pool>=0)close(pool);if(package>=0)close(package);
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
                if(!reason&&package_path&&!package_recipe_supported(e->recipe))reason="unsupported_package_recipe";
                if(!reason&&package_path&&!strcmp(e->recipe,"llama-gguf-package-v1")&&!qualified)reason="unqualified_llama_package_runtime";
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
