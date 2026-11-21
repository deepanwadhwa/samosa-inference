#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "managed_provider.h"
#include "embedding.h"
#include "inference.h"
#include "json.h"
#include "sha256.h"
#include "managed_host.h"
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>
static int safe(const char *path,int directory){struct stat s;return path&&path[0]=='/'&&!lstat(path,&s)&&s.st_uid==geteuid()&&!(s.st_mode&077)&&
    (directory?S_ISDIR(s.st_mode):S_ISREG(s.st_mode));}
static char *configuration(const char *path,size_t *length){
    if(!safe(path,0))return NULL;
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);if(fd<0)return NULL;
    struct stat s;char *out=NULL;
    if(!fstat(fd,&s)&&S_ISREG(s.st_mode)&&s.st_uid==geteuid()&&!(s.st_mode&077)&&s.st_size>0&&s.st_size<=16384){
        out=malloc((size_t)s.st_size+1);if(out){ssize_t n=pread(fd,out,(size_t)s.st_size,0);
            if(n!=s.st_size){free(out);out=NULL;}else {out[n]=0;*length=(size_t)n;}}}
    close(fd);return out;
}
static int number(const Json *j,const char *name,size_t minimum,size_t maximum,size_t *out){
    return json_uint(j,json_get(j,0,name),out)&&*out>=minimum&&*out<=maximum;
}
static int known_fields(const Json *j){
    const char *allowed[]={"contract_version","profile","state","engine","engine_sha256","engine_size","model","model_sha256","model_size","context_tokens","model_alias","owner","port","expected_revision","gpu_layers","threads","embedding_config_file"};
    unsigned seen=0;
    for(int t=1;t<j->tokens[0].next;){char *key=json_string(j,t);size_t field=0;
        for(;key&&field<sizeof(allowed)/sizeof(allowed[0]);field++)if(!strcmp(key,allowed[field]))break;
        int valid=key&&field<sizeof(allowed)/sizeof(allowed[0])&&!(seen&(1u<<field));free(key);
        if(!valid)return 0;
        seen|=1u<<field;t=j->tokens[j->tokens[t].next].next;}return 1;
}
static int key_remove(int root,int key,const char *name){
    struct stat held,named;
    return root>=0&&key>=0&&!fstat(key,&held)&&!fstatat(root,name,&named,AT_SYMLINK_NOFOLLOW)&&
        held.st_dev==named.st_dev&&held.st_ino==named.st_ino&&!unlinkat(root,name,0);
}
const char *managed_host_deactivate(ManagedHost *host) {
    const char *terminal=NULL;
    if(host->attempted){
        terminal=managed_provider_stop(&host->provider);
        /* Terminal failure is still reported, but a reaped process no longer
         * owns residency. Only unconfirmed shutdown retains the live owner. */
        if(terminal&&(host->provider.activation.process.guardian>0||host->provider.activation.process.response>=0))return terminal;
        host->attempted=0;
    }
    if(host->key_exists){if(!key_remove(host->key_root,host->key_fd,host->key_name))return "managed_key_cleanup_failed";host->key_exists=0;}
    if(host->key_fd>=0)close(host->key_fd);
    if(host->key_root>=0)close(host->key_root);
    host->key_fd=host->key_root=-1;
    free(host->args);host->args=NULL;host->key[0]=0;host->key_path[0]=0;host->key_name[0]=0;
    return terminal;
}
const char *managed_host_close(ManagedHost *host) {
    const char *error=managed_host_deactivate(host);if(error&&(host->attempted||host->key_exists))return error;
    free(host->profile);free(host->state);free(host->engine);free(host->engine_sha);free(host->model);free(host->model_sha);free(host->alias);free(host->owner);free(host->embedding_path);embedding_config_free(&host->embedding);
    memset(host,0,sizeof(*host));host->key_root=host->key_fd=-1;
    return error;
}
const char *managed_host_load(const char *config_path,ManagedHost *host) {
    memset(host,0,sizeof(*host));host->key_root=host->key_fd=-1;
    const char *failure=NULL;
    size_t config_length=0;char *text=configuration(config_path,&config_length);Json j;size_t version;
    if(!text||!json_parse(&j,text,config_length)||j.tokens[0].type!=JT_OBJECT||!known_fields(&j)||
        !number(&j,"contract_version",1,1,&version)||!number(&j,"engine_size",64,SIZE_MAX,&host->engine_size)||
        !number(&j,"model_size",1,SIZE_MAX,&host->model_size)||!number(&j,"context_tokens",128,131072,&host->context)||
        !number(&j,"port",1024,65535,&host->port)||!number(&j,"expected_revision",0,2147483645,&host->revision)||
        !number(&j,"gpu_layers",0,999,&host->gpu)||!number(&j,"threads",1,32,&host->threads)){
        free(text);return "invalid_managed_configuration";}
    host->profile=json_string(&j,json_get(&j,0,"profile")),host->state=json_string(&j,json_get(&j,0,"state")),
        host->engine=json_string(&j,json_get(&j,0,"engine")),host->engine_sha=json_string(&j,json_get(&j,0,"engine_sha256")),
        host->model=json_string(&j,json_get(&j,0,"model")),host->model_sha=json_string(&j,json_get(&j,0,"model_sha256")),
        host->alias=json_string(&j,json_get(&j,0,"model_alias")),host->owner=json_string(&j,json_get(&j,0,"owner")),
        host->embedding_path=json_string(&j,json_get(&j,0,"embedding_config_file"));
    int embedding_field=json_get(&j,0,"embedding_config_file")>=0;free(text);
    host->embedding_mode=host->profile&&!strcmp(host->profile,"llama-embedding-static-system-v1");
    if(!host->profile||(!host->embedding_mode&&strcmp(host->profile,"llama-static-system-v1"))||!host->state||!host->engine||!host->engine_sha||!host->model||!host->model_sha||!host->alias||!host->owner||
        !host->alias[0]||strlen(host->alias)>128||!host->owner[0]||strlen(host->owner)>64){failure="invalid_managed_configuration";goto done;}
    if((host->embedding_mode&&(!host->embedding_path||!embedding_config_load(host->embedding_path,&host->embedding)||
        strcmp(host->embedding.engine_sha256,host->engine_sha)||strcmp(host->embedding.model_sha256,host->model_sha)))||
        (!host->embedding_mode&&embedding_field)){failure="invalid_managed_embedding_configuration";goto done;}
    if(host->state[0]!='/' ||
        snprintf(host->activation_state,sizeof(host->activation_state),"%s/activation",host->state)>=(int)sizeof(host->activation_state)||
        snprintf(host->admission,sizeof(host->admission),"%s/admission",host->state)>=(int)sizeof(host->admission)){
        failure="unsafe_managed_state";goto done;
    }
    snprintf(host->endpoint,sizeof(host->endpoint),"http://127.0.0.1:%zu",host->port);
    ActivationConfig a={.require_system_runtime=1,.engine_sha=host->engine_sha,.model_sha=host->model_sha,
        .engine_size=host->engine_size,.model_size=host->model_size};
    InferenceConfig i={.model_alias=host->alias,.context_tokens=host->context};
    failure=managed_provider_identity(&a,&i,host->identity);if(failure)goto done;
    host->prepared=1;
    return NULL;
done: {
    const char *cleanup=managed_host_close(host);
    return cleanup?cleanup:failure;
}}
const char *managed_host_activate(const char *executable,int advance_current,
                                int (*cancelled)(void *),void *cancellation,ManagedHost *host) {
    if(!host->prepared||host->attempted)return "invalid_managed_lifecycle";
    const char *failure=NULL;
    if(!realpath(executable,host->self)||(mkdir(host->state,0700)&&errno!=EEXIST)||!safe(host->state,1)){failure="unsafe_managed_state";goto done;}
    host->key_root=open(host->state,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);struct stat held,named;
    if(host->key_root<0||fstat(host->key_root,&held)||lstat(host->state,&named)||held.st_dev!=named.st_dev||held.st_ino!=named.st_ino||
        held.st_uid!=geteuid()||(held.st_mode&077)){failure="unsafe_managed_state";goto done;}
    unsigned char secret[32];int random=open("/dev/urandom",O_RDONLY|O_CLOEXEC);
    if(random<0){failure="managed_resource_limit";host->key_path[0]=0;goto done;}
    ssize_t read_secret=read(random,secret,sizeof(secret));close(random);if(read_secret!=32){failure="managed_resource_limit";host->key_path[0]=0;goto done;}
    sha256_hex(secret,host->key);char name_hash[65];hash_bytes(host->key,64,name_hash);snprintf(host->key_name,sizeof(host->key_name),"key.%s",name_hash);
    if(snprintf(host->key_path,sizeof(host->key_path),"%s/%s",host->state,host->key_name)>=(int)sizeof(host->key_path)){failure="unsafe_managed_state";goto done;}
    host->key_fd=openat(host->key_root,host->key_name,O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    if(host->key_fd<0){failure="managed_resource_limit";goto done;}host->key_exists=1;
    if(write(host->key_fd,host->key,64)!=64||fsync(host->key_fd)||lstat(host->state,&named)||held.st_dev!=named.st_dev||held.st_ino!=named.st_ino){failure="unsafe_managed_state";goto done;}
    snprintf(host->endpoint,sizeof(host->endpoint),"http://127.0.0.1:%zu",host->port);size_t length;FILE *f=open_memstream(&host->args,&length);
    if(!f){failure="managed_resource_limit";goto done;}
    fputs("[\"@ENGINE@\",\"--model\",\"@MODEL@\",\"--host\",\"127.0.0.1\",\"--port\",",f);char value[32];snprintf(value,sizeof(value),"%zu",host->port);json_quote(f,value,strlen(value));
    fputs(",\"--alias\",",f);json_quote(f,host->alias,strlen(host->alias));fputs(",\"--ctx-size\",",f);snprintf(value,sizeof(value),"%zu",host->context);json_quote(f,value,strlen(value));
    fputs(",\"--parallel\",\"1\",\"--threads\",",f);snprintf(value,sizeof(value),"%zu",host->threads);json_quote(f,value,strlen(value));
    fputs(",\"--n-gpu-layers\",",f);snprintf(value,sizeof(value),"%zu",host->gpu);json_quote(f,value,strlen(value));
    if(host->embedding_mode){fputs(",\"--embedding\",\"--pooling\",\"mean\",\"--batch-size\",",f);snprintf(value,sizeof(value),"%zu",host->context);json_quote(f,value,strlen(value));fputs(",\"--ubatch-size\",",f);json_quote(f,value,strlen(value));}
    else fputs(",\"--ubatch-size\",\"64\",\"--reasoning\",\"off\"",f);
    fputs(",\"--no-webui\",\"--api-key-file\",",f);json_quote(f,host->key_path,strlen(host->key_path));fputs(",\"--threads-http\",\"2\",\"--slots\"]",f);
    if(fclose(f)){failure="managed_resource_limit";goto done;}
    ActivationConfig a={.state=host->activation_state,.self=host->self,.engine=host->engine,.model=host->model,.engine_sha=host->engine_sha,.model_sha=host->model_sha,
        .engine_size=host->engine_size,.model_size=host->model_size,.byte_ceiling=host->engine_size>host->model_size?host->engine_size:host->model_size,.argv_template=host->args,
        .expected_revision=(unsigned)host->revision,.advance_current=advance_current,.lifetime=86400,.log_limit=1048576,.require_system_runtime=1};
    InferenceConfig i={.endpoint=host->endpoint,.provider_id=host->identity,.admission_state=host->admission,.context_tokens=host->context,.model_alias=host->alias,.owner=host->owner,
        .queue_capacity=4,.api_key=host->key,.cancelled=cancelled,.cancellation=cancellation,.embedding=host->embedding_mode?&host->embedding:NULL};
    const char *e=managed_provider_identity(&a,&i,host->identity);if(e){failure=e;goto done;}
    host->attempted=1;e=managed_provider_start(&a,&i,&host->provider);if(e){failure=e;goto done;}
    if(lstat(host->state,&named)||held.st_dev!=named.st_dev||held.st_ino!=named.st_ino){failure="unsafe_managed_state";goto done;}
    /* The server has read its key; no secret file is retained across crashes. */
    if(!key_remove(host->key_root,host->key_fd,host->key_name)){failure="managed_key_cleanup_failed";goto done;}host->key_exists=0;
    return NULL;
done: {
    const char *cleanup=managed_host_deactivate(host);
    return cleanup?cleanup:failure;
}}
const char *managed_host_start(const char *config_path,const char *executable,int advance_current,
                              int (*cancelled)(void *),void *cancellation,ManagedHost *host) {
    const char *error=managed_host_load(config_path,host);
    if(error)return error;
    error=managed_host_activate(executable,advance_current,cancelled,cancellation,host);
    if(error){const char *cleanup=managed_host_close(host);if(cleanup)return cleanup;}
    return error;
}
