#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "managed_provider.h"
#include "embedding.h"
#include "inference.h"
#include "json.h"
#include "sha256.h"
#include "managed_stream.h"
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>
_Static_assert(ATOMIC_INT_LOCK_FREE==2,"Signal cancellation requires lock-free atomics");
static atomic_int stopping;
static atomic_int stream_stop;
static void stop_signal(int number){(void)number;atomic_store(&stopping,1);}
static int cancelled(void *unused){(void)unused;return atomic_load(&stopping)||atomic_load(&stream_stop);}
static int dispatch_request(void *context,const char *request,size_t length,FILE *out){return inference_dispatch_config(context,request,length,out);}
typedef struct {ManagedProvider *provider;} StreamOwner;
static int interrupted(void *context){StreamOwner *owner=context;struct pollfd terminal={owner->provider->activation.process.response,POLLIN|POLLHUP,0};return atomic_load(&stopping)||poll(&terminal,1,0)!=0;}
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
static void error(const char *code){fputs("{\"contract_version\":1,\"error\":{\"code\":",stdout);json_quote(stdout,code,strlen(code));fputs("}}\n",stdout);fflush(stdout);}
int main(int argc,char **argv){
    int guardian=engine_guardian_dispatch(argc,argv);if(guardian>=0)return guardian;
    if((argc!=3&&argc!=4)||strcmp(argv[1],"--config")||(argc==4&&strcmp(argv[3],"--single")&&strcmp(argv[3],"--single-current"))){fputs("usage: samosa-inference-managed --config ABSOLUTE_PRIVATE_JSON [--single | --single-current]\n",stderr);return 2;}
    int single=argc==4;
    int advance_current=single&&!strcmp(argv[3],"--single-current");
    size_t config_length=0;char *text=configuration(argv[2],&config_length);Json j;size_t version,engine_size,model_size,context,port,revision,gpu,threads;
    if(!text||!json_parse(&j,text,config_length)||j.tokens[0].type!=JT_OBJECT||!known_fields(&j)||
        !number(&j,"contract_version",1,1,&version)||!number(&j,"engine_size",64,SIZE_MAX,&engine_size)||
        !number(&j,"model_size",1,SIZE_MAX,&model_size)||!number(&j,"context_tokens",128,131072,&context)||
        !number(&j,"port",1024,65535,&port)||!number(&j,"expected_revision",0,2147483645,&revision)||
        !number(&j,"gpu_layers",0,999,&gpu)||!number(&j,"threads",1,32,&threads)){
        free(text);error("invalid_managed_configuration");return 2;}
    char *profile=json_string(&j,json_get(&j,0,"profile")),*state=json_string(&j,json_get(&j,0,"state")),
        *engine=json_string(&j,json_get(&j,0,"engine")),*engine_sha=json_string(&j,json_get(&j,0,"engine_sha256")),
        *model=json_string(&j,json_get(&j,0,"model")),*model_sha=json_string(&j,json_get(&j,0,"model_sha256")),
        *alias=json_string(&j,json_get(&j,0,"model_alias")),*owner=json_string(&j,json_get(&j,0,"owner")),
        *embedding_path=json_string(&j,json_get(&j,0,"embedding_config_file"));
    int embedding_field=json_get(&j,0,"embedding_config_file")>=0;free(text);
    EmbeddingConfig embedding={0};
    int embedding_mode=profile&&!strcmp(profile,"llama-embedding-static-system-v1");
    int rc=2;char *args=NULL;char key_path[4096]={0},key_name[80]={0};
    int key_root=-1,key_fd=-1,key_exists=0;ManagedProvider provider;int attempted=0;
    if(!profile||(!embedding_mode&&strcmp(profile,"llama-static-system-v1"))||!state||!engine||!engine_sha||!model||!model_sha||!alias||!owner||
        !alias[0]||strlen(alias)>128||!owner[0]||strlen(owner)>64){error("invalid_managed_configuration");goto done;}
    if((embedding_mode&&(!embedding_path||!embedding_config_load(embedding_path,&embedding)||
        strcmp(embedding.engine_sha256,engine_sha)||strcmp(embedding.model_sha256,model_sha)))||
        (!embedding_mode&&embedding_field)){error("invalid_managed_embedding_configuration");goto done;}
    char self[4096],activation_state[4096],admission[4096],endpoint[128],identity[100];
    if(!realpath(argv[0],self)||state[0]!='/'||(mkdir(state,0700)&&errno!=EEXIST)||!safe(state,1)||
        snprintf(activation_state,sizeof(activation_state),"%s/activation",state)>=(int)sizeof(activation_state)||
        snprintf(admission,sizeof(admission),"%s/admission",state)>=(int)sizeof(admission)){error("unsafe_managed_state");goto done;}
    key_root=open(state,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);struct stat held,named;
    if(key_root<0||fstat(key_root,&held)||lstat(state,&named)||held.st_dev!=named.st_dev||held.st_ino!=named.st_ino||
        held.st_uid!=geteuid()||(held.st_mode&077)){error("unsafe_managed_state");goto done;}
    unsigned char secret[32];char key[65];int random=open("/dev/urandom",O_RDONLY|O_CLOEXEC);
    if(random<0){error("managed_resource_limit");key_path[0]=0;goto done;}
    ssize_t read_secret=read(random,secret,sizeof(secret));close(random);if(read_secret!=32){error("managed_resource_limit");key_path[0]=0;goto done;}
    sha256_hex(secret,key);char name_hash[65];hash_bytes(key,64,name_hash);snprintf(key_name,sizeof(key_name),"key.%s",name_hash);
    if(snprintf(key_path,sizeof(key_path),"%s/%s",state,key_name)>=(int)sizeof(key_path)){error("unsafe_managed_state");goto done;}
    key_fd=openat(key_root,key_name,O_RDWR|O_CREAT|O_EXCL|O_NOFOLLOW|O_CLOEXEC,0600);
    if(key_fd<0){error("managed_resource_limit");goto done;}key_exists=1;
    if(write(key_fd,key,64)!=64||fsync(key_fd)||lstat(state,&named)||held.st_dev!=named.st_dev||held.st_ino!=named.st_ino){error("unsafe_managed_state");goto done;}
    snprintf(endpoint,sizeof(endpoint),"http://127.0.0.1:%zu",port);size_t length;FILE *f=open_memstream(&args,&length);
    if(!f){error("managed_resource_limit");goto done;}
    fputs("[\"@ENGINE@\",\"--model\",\"@MODEL@\",\"--host\",\"127.0.0.1\",\"--port\",",f);char value[32];snprintf(value,sizeof(value),"%zu",port);json_quote(f,value,strlen(value));
    fputs(",\"--alias\",",f);json_quote(f,alias,strlen(alias));fputs(",\"--ctx-size\",",f);snprintf(value,sizeof(value),"%zu",context);json_quote(f,value,strlen(value));
    fputs(",\"--parallel\",\"1\",\"--threads\",",f);snprintf(value,sizeof(value),"%zu",threads);json_quote(f,value,strlen(value));
    fputs(",\"--n-gpu-layers\",",f);snprintf(value,sizeof(value),"%zu",gpu);json_quote(f,value,strlen(value));
    if(embedding_mode){fputs(",\"--embedding\",\"--pooling\",\"mean\",\"--batch-size\",",f);snprintf(value,sizeof(value),"%zu",context);json_quote(f,value,strlen(value));fputs(",\"--ubatch-size\",",f);json_quote(f,value,strlen(value));}
    else fputs(",\"--ubatch-size\",\"64\",\"--reasoning\",\"off\"",f);
    fputs(",\"--no-webui\",\"--api-key-file\",",f);json_quote(f,key_path,strlen(key_path));fputs(",\"--threads-http\",\"2\",\"--slots\"]",f);
    if(fclose(f)){error("managed_resource_limit");goto done;}
    struct sigaction action={.sa_handler=stop_signal};sigemptyset(&action.sa_mask);sigaction(SIGTERM,&action,NULL);sigaction(SIGINT,&action,NULL);signal(SIGPIPE,SIG_IGN);
    ActivationConfig a={.state=activation_state,.self=self,.engine=engine,.model=model,.engine_sha=engine_sha,.model_sha=model_sha,
        .engine_size=engine_size,.model_size=model_size,.byte_ceiling=engine_size>model_size?engine_size:model_size,.argv_template=args,
        .expected_revision=(unsigned)revision,.advance_current=advance_current,.lifetime=86400,.log_limit=1048576,.require_system_runtime=1};
    InferenceConfig i={.endpoint=endpoint,.provider_id=identity,.admission_state=admission,.context_tokens=context,.model_alias=alias,.owner=owner,
        .queue_capacity=4,.api_key=key,.cancelled=cancelled,.embedding=embedding_mode?&embedding:NULL};
    const char *e=managed_provider_identity(&a,&i,identity);if(e){error(e);goto done;}
    attempted=1;e=managed_provider_start(&a,&i,&provider);if(e){error(e);goto done;}
    if(lstat(state,&named)||held.st_dev!=named.st_dev||held.st_ino!=named.st_ino){error("unsafe_managed_state");goto done;}
    /* The server has read its key; no secret file is retained across crashes. */
    if(!key_remove(key_root,key_fd,key_name)){error("managed_key_cleanup_failed");goto done;}key_exists=0;
    if(single){char *request=malloc(INFERENCE_REQUEST_MAX+1);
        if(!request){error("managed_resource_limit");goto done;}
        size_t bytes=fread(request,1,INFERENCE_REQUEST_MAX+1,stdin);
        rc=ferror(stdin)?2:inference_dispatch_config(&provider.inference,request,bytes,stdout);
        free(request);goto done;}
    fputs("{\"contract_version\":1,\"event\":\"provider_started\",\"provider_id\":",stdout);json_quote(stdout,identity,strlen(identity));
    fprintf(stdout,",\"activation_revision\":%u,\"runtime_policy\":\"system-runtime-v1\"}\n",provider.activation.revision);fflush(stdout);
    StreamOwner stream_owner={&provider};
    ManagedStreamConfig stream={.input=STDIN_FILENO,.output=STDOUT_FILENO,
        .dispatch=dispatch_request,.context=&provider.inference,.interrupted=interrupted,.interruption=&stream_owner,.stop=&stream_stop};
    rc=managed_stream_run(&stream);
    if(rc==2)_exit(1); /* Guardian lifeline closes; never free live worker state. */
done:
    if(attempted){const char *e=managed_provider_stop(&provider);if(e){error(e);rc=1;}}
    if(key_exists)(void)key_remove(key_root,key_fd,key_name);
    if(key_fd>=0)close(key_fd);
    if(key_root>=0)close(key_root);
    free(args);free(profile);free(state);free(engine);free(engine_sha);free(model);free(model_sha);free(alias);free(owner);free(embedding_path);embedding_config_free(&embedding);return rc;
}
