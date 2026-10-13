#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "activation.h"
#include "managed_provider.h"
#include "inference.h"
#include "sha256.h"
#include "json.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>
#include <signal.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <spawn.h>
#include <curl/curl.h>
extern char **environ;
static unsigned checks;
static Activation active;
static int active_attempted;
static ManagedProvider managed;
static int using_managed;
static void cleanup(void){if(active_attempted){if(using_managed)(void)managed_provider_stop(&managed);else(void)activation_stop(&active);}}
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL actual activation %d: %s (terminal=%s)\n",__LINE__,#x,active.process.reason);exit(1);}}while(0)
static uint64_t digest_file(const char *path,char sha[65],const char *copy_path) {
    int fd=open(path,O_RDONLY|O_NOFOLLOW),copy=copy_path?open(copy_path,O_WRONLY|O_CREAT|O_EXCL,0500):-1;CHECK(fd>=0);if(copy_path)CHECK(copy>=0);
    struct stat before,after;CHECK(!fstat(fd,&before)&&S_ISREG(before.st_mode));Sha256 hash;sha256_init(&hash);
    unsigned char bytes[65536];uint64_t total=0;
    for(;;){ssize_t n=read(fd,bytes,sizeof(bytes));if(n<0&&errno==EINTR)continue;CHECK(n>=0);if(!n)break;
        sha256_update(&hash,bytes,(size_t)n);total+=(uint64_t)n;if(copy_path)CHECK(write(copy,bytes,(size_t)n)==n);}
    CHECK(!fstat(fd,&after)&&before.st_size==after.st_size&&total==(uint64_t)before.st_size);CHECK(!close(fd));if(copy_path)CHECK(!close(copy));
    unsigned char out[32];sha256_final(&hash,out);sha256_hex(out,sha);return total;
}
typedef struct {InferenceConfig inference;Activation *activation;const char *wrong_key;} Qualification;
static double now(void){struct timespec t;CHECK(!clock_gettime(CLOCK_MONOTONIC,&t));return (double)t.tv_sec+(double)t.tv_nsec/1e9;}
static const char *qualify(void *data){Qualification *q=data;InferenceConfig probe=q->inference;probe.transfer_timeout_ms=1000;double deadline=now()+120;
    while(now()<deadline){struct pollfd terminal={q->activation->process.response,POLLIN|POLLHUP,0};if(poll(&terminal,1,0)>0)return "owned_engine_exited";
        const char *e=llama_properties(&probe);if(!e){probe.api_key=q->wrong_key;const char *wrong=llama_properties(&probe);
            return wrong&&!strcmp(wrong,"provider_auth_failed")?NULL:"unqualified_endpoint";}
        if(strcmp(e,"provider_unavailable")&&strcmp(e,"provider_http_error")&&strcmp(e,"deadline_exceeded"))return e;
        struct timespec pause={0,100000000};nanosleep(&pause,NULL);}
    return "startup_deadline_exceeded";
}
static char *run(const InferenceConfig *c,const char *request){char *out=NULL;size_t n;FILE *f=open_memstream(&out,&n);CHECK(f);
    int rc=inference_dispatch_config(c,request,strlen(request),f);CHECK(!fclose(f));if(rc)fprintf(stderr,"actual provider result: %s",out);CHECK(!rc);return out;}
typedef struct {char bytes[16384];size_t used;} SlotReply;
static size_t slot_write(char *data,size_t size,size_t count,void *opaque){SlotReply *r=opaque;
    if(size&&count>SIZE_MAX/size)return 0;
    size_t n=size*count;if(n>sizeof(r->bytes)-r->used)return 0;memcpy(r->bytes+r->used,data,n);r->used+=n;return n;}
static int engine_processing(const InferenceConfig *c){
    CURL *curl=curl_easy_init();CHECK(curl);char url[256],authorization[256];snprintf(url,sizeof(url),"%s/slots",c->endpoint);
    snprintf(authorization,sizeof(authorization),"Authorization: Bearer %s",c->api_key);struct curl_slist *headers=curl_slist_append(NULL,authorization);CHECK(headers);
    SlotReply reply={0};curl_easy_setopt(curl,CURLOPT_URL,url);curl_easy_setopt(curl,CURLOPT_HTTPHEADER,headers);curl_easy_setopt(curl,CURLOPT_TIMEOUT_MS,1000L);
    curl_easy_setopt(curl,CURLOPT_NOSIGNAL,1L);curl_easy_setopt(curl,CURLOPT_PROXY,"");curl_easy_setopt(curl,CURLOPT_WRITEFUNCTION,slot_write);curl_easy_setopt(curl,CURLOPT_WRITEDATA,&reply);
    CURLcode rc=curl_easy_perform(curl);long status=0;curl_easy_getinfo(curl,CURLINFO_RESPONSE_CODE,&status);curl_easy_cleanup(curl);curl_slist_free_all(headers);
    if(rc==CURLE_OPERATION_TIMEDOUT||(rc==CURLE_OK&&status==503))return 0;
    if(rc!=CURLE_OK||status!=200)fprintf(stderr,"slots observation failed: curl=%d http=%ld body=%.*s\n",(int)rc,status,(int)(reply.used>512?512:reply.used),reply.bytes);
    CHECK(rc==CURLE_OK&&status==200);Json j;CHECK(json_parse(&j,reply.bytes,reply.used)&&j.tokens[0].type==JT_ARRAY&&j.tokens[0].count==1);
    int processing=json_get(&j,1,"is_processing");CHECK(processing>=0&&j.tokens[processing].type==JT_BOOL);return j.text[j.tokens[processing].begin]=='t';
}
static void actual_cancel(const char *self,const char *root,const char *key_path,const InferenceConfig *cfg){
    fprintf(stderr,"phase: actual engine active cancellation\n");char binary[4096],output_path[4096];CHECK(strlen(self)<sizeof(binary));strcpy(binary,self);char *last=strrchr(binary,'/');CHECK(last);
    CHECK((size_t)(last-binary)+strlen("/samosa-inference-llama")<sizeof(binary));strcpy(last,"/samosa-inference-llama");
    CHECK(snprintf(output_path,sizeof(output_path),"%s/cancelled-result",root)<(int)sizeof(output_path));int output=open(output_path,O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC,0600);CHECK(output>=0);
    int input[2];CHECK(!pipe(input));posix_spawn_file_actions_t actions;CHECK(!posix_spawn_file_actions_init(&actions));
    CHECK(!posix_spawn_file_actions_adddup2(&actions,input[0],STDIN_FILENO));CHECK(!posix_spawn_file_actions_adddup2(&actions,output,STDOUT_FILENO));
    CHECK(!posix_spawn_file_actions_addclose(&actions,input[1]));
    char *args[]={binary,"--llama-endpoint",(char*)cfg->endpoint,"--provider-id",(char*)cfg->provider_id,"--model-alias",(char*)cfg->model_alias,
        "--context-tokens","4096","--owner","qa","--admission-state",(char*)cfg->admission_state,"--api-key-file",(char*)key_path,NULL};
    pid_t child;CHECK(!posix_spawn(&child,binary,&actions,NULL,args,environ));CHECK(!posix_spawn_file_actions_destroy(&actions));CHECK(!close(input[0]));
    char request[1024];int n=snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"generate\",\"operation_id\":\"owned-active-cancel\",\"provider_id\":\"%s\",\"prompt\":\"Write every integer from 1 to 10000, separated by spaces. Do not abbreviate or skip any integers.\",\"max_output_tokens\":2048}",cfg->provider_id);
    CHECK(n>0&&(size_t)n<sizeof(request));CHECK(write(input[1],request,(size_t)n)==n);CHECK(!close(input[1]));
    double deadline=now()+15;int observed=0;
    while(now()<deadline){int status;CHECK(waitpid(child,&status,WNOHANG)==0);if(engine_processing(cfg)){observed=1;break;}struct timespec pause={0,20000000};nanosleep(&pause,NULL);}
    CHECK(observed);char *cancelled=run(cfg,"{\"contract_version\":1,\"op\":\"operation_cancel\",\"operation_id\":\"owned-active-cancel\"}");CHECK(strstr(cancelled,"cancel_requested"));free(cancelled);
    deadline=now()+5;int status=0,done=0;
    while(now()<deadline){pid_t result=waitpid(child,&status,WNOHANG);CHECK(result>=0);if(result==child){done=1;break;}struct timespec pause={0,20000000};nanosleep(&pause,NULL);}
    if(!done){kill(child,SIGKILL);waitpid(child,&status,0);}CHECK(done&&WIFEXITED(status)&&WEXITSTATUS(status)==1);
    char bytes[4096];ssize_t length=pread(output,bytes,sizeof(bytes)-1,0);CHECK(length>0);bytes[length]=0;CHECK(strstr(bytes,"\"code\":\"cancelled\""));CHECK(!close(output));
    char *stored=run(cfg,"{\"contract_version\":1,\"op\":\"operation_get\",\"operation_id\":\"owned-active-cancel\"}");CHECK(strstr(stored,"\"status\":\"cancelled\""));free(stored);
}
int main(int argc,char **argv){int guardian=engine_guardian_dispatch(argc,argv);if(guardian>=0)return guardian;
    if(argc<4||argc>6||(argc==6&&strcmp(argv[5],"system-runtime-v1"))){fputs("usage: activation-native EXTERNAL_ENGINE READ_ONLY_MODEL QA_ROOT [PINNED_MODEL_SHA256 [system-runtime-v1]]\n",stderr);return 2;}
    CHECK(argv[0][0]=='/');CHECK(!mkdir(argv[3],0700));CHECK(!atexit(cleanup));
    char engine[4096],state[4096],admission[4096],key_path[4096];
    CHECK(snprintf(engine,sizeof(engine),"%s/engine",argv[3])<(int)sizeof(engine));CHECK(snprintf(state,sizeof(state),"%s/state",argv[3])<(int)sizeof(state));
    CHECK(snprintf(admission,sizeof(admission),"%s/admission",argv[3])<(int)sizeof(admission));CHECK(snprintf(key_path,sizeof(key_path),"%s/key",argv[3])<(int)sizeof(key_path));
    char engine_sha[65],model_sha[65];fprintf(stderr,"phase: engine fingerprint/copy\n");uint64_t engine_size=digest_file(argv[1],engine_sha,engine);
    uint64_t model_size;
    if(argc>=5){CHECK(strlen(argv[4])==64);memcpy(model_sha,argv[4],65);struct stat s;CHECK(!lstat(argv[2],&s)&&S_ISREG(s.st_mode)&&s.st_size>0);model_size=(uint64_t)s.st_size;}
    else{fprintf(stderr,"phase: read-only model fingerprint\n");model_size=digest_file(argv[2],model_sha,NULL);}
    unsigned char secret[32];int random=open("/dev/urandom",O_RDONLY);CHECK(random>=0);CHECK(read(random,secret,sizeof(secret))==(ssize_t)sizeof(secret));CHECK(!close(random));
    char key[65];sha256_hex(secret,key);int key_fd=open(key_path,O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(key_fd>=0);CHECK(write(key_fd,key,64)==64);CHECK(!close(key_fd));
    int reservation=socket(AF_INET,SOCK_STREAM,0);CHECK(reservation>=0);struct sockaddr_in address={.sin_family=AF_INET,.sin_addr.s_addr=htonl(INADDR_LOOPBACK)};
    CHECK(!bind(reservation,(struct sockaddr*)&address,sizeof(address)));socklen_t length=sizeof(address);CHECK(!getsockname(reservation,(struct sockaddr*)&address,&length));
    char port[16],endpoint[128];snprintf(port,sizeof(port),"%u",ntohs(address.sin_port));snprintf(endpoint,sizeof(endpoint),"http://127.0.0.1:%s",port);CHECK(!close(reservation));
    char binding[256],version[65],provider[100];snprintf(binding,sizeof(binding),"%s:%s:4096",engine_sha,model_sha);hash_bytes(binding,strlen(binding),version);snprintf(provider,sizeof(provider),"owned-text-%s",version);
    char *template=NULL;size_t n;FILE *f=open_memstream(&template,&n);CHECK(f);
    fputs("[\"@ENGINE@\",\"--model\",\"@MODEL@\",\"--host\",\"127.0.0.1\",\"--port\",",f);json_quote(f,port,strlen(port));
    fputs(",\"--alias\",\"owned-s13a-qa\",\"--ctx-size\",\"4096\",\"--parallel\",\"1\",\"--threads\",\"4\",\"--n-gpu-layers\",\"99\",\"--ubatch-size\",\"64\",\"--reasoning\",\"off\",\"--no-webui\",\"--api-key-file\",",f);
    json_quote(f,key_path,strlen(key_path));fputs(",\"--threads-http\",\"2\",\"--slots\"]",f);CHECK(!fclose(f));
    Qualification q={.inference={.endpoint=endpoint,.provider_id=provider,.admission_state=admission,.context_tokens=4096,
        .model_alias="owned-s13a-qa",.owner="qa",.queue_capacity=4,.api_key=key},.activation=&active,.wrong_key="wrong-owned-qa-key"};
    ActivationConfig c={.state=state,.self=argv[0],.engine=engine,.model=argv[2],.engine_sha=engine_sha,.model_sha=model_sha,
        .engine_size=engine_size,.model_size=model_size,.byte_ceiling=model_size,.argv_template=template,.lifetime=180,.log_limit=1048576,.require_system_runtime=argc==6,.qualify=qualify,.qualification=&q};
    fprintf(stderr,"phase: verified owned activation (%s)\n",c.require_system_runtime?"system-runtime-v1":"entry/model only; dynamic dependencies external");active_attempted=1;
    using_managed=c.require_system_runtime;
    if(using_managed)CHECK(!managed_provider_identity(&c,&q.inference,provider));
    const char *e=using_managed?managed_provider_start(&c,&q.inference,&managed):activation_start(&c,&active);
    if(using_managed){active=managed.activation;q.inference=managed.inference;}
    if(e)fprintf(stderr,"activation error: %s\n",e);
    CHECK(!e);free(template);
    if(using_managed){char status_request[256];snprintf(status_request,sizeof(status_request),"{\"contract_version\":1,\"op\":\"provider_status\",\"provider_id\":\"%s\"}",provider);
        char *status=run(&q.inference,status_request);CHECK(strstr(status,"\"ready\":true")&&strstr(status,"owned_system_runtime_v1"));free(status);}
    fprintf(stderr,"phase: authenticated count/generation\n");char request[512];
    snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"count_tokens\",\"operation_id\":\"owned-count\",\"provider_id\":\"%s\",\"prompt\":\"Compute 2 + 2. Answer only the integer.\"}",provider);
    char *counted=run(&q.inference,request);Json counted_result;CHECK(json_parse(&counted_result,counted,strlen(counted)));size_t input_tokens;
    CHECK(json_uint(&counted_result,json_get(&counted_result,0,"input_tokens"),&input_tokens)&&input_tokens>0);free(counted);
    snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"generate\",\"operation_id\":\"owned-generate\",\"provider_id\":\"%s\",\"prompt\":\"Compute 2 + 2. Answer only the integer.\",\"max_output_tokens\":32}",provider);
    char *out=run(&q.inference,request);Json result;CHECK(json_parse(&result,out,strlen(out)));char *answer=json_string(&result,json_get(&result,0,"answer"));CHECK(answer);
    size_t generated_input;int usage=json_get(&result,0,"usage");CHECK(json_uint(&result,json_get(&result,usage,"input_tokens"),&generated_input)&&generated_input==input_tokens);
    char *start=answer;while(isspace((unsigned char)*start))start++;CHECK(*start++=='4');while(isspace((unsigned char)*start))start++;CHECK(!*start);free(answer);
    if(using_managed){actual_cancel(argv[0],argv[3],key_path,&q.inference);
        char survivor[512];snprintf(survivor,sizeof(survivor),"{\"contract_version\":1,\"op\":\"generate\",\"operation_id\":\"owned-survivor\",\"provider_id\":\"%s\",\"prompt\":\"Compute 2 + 2. Answer only the integer.\",\"max_output_tokens\":32}",provider);
        char *result=run(&q.inference,survivor);CHECK(strstr(result,"\"answer\":\"4\"")&&!managed_provider_status(&managed));free(result);}
    if(using_managed){CHECK(!chmod(engine,0400));CHECK(!strcmp(managed_provider_status(&managed),"provider_artifact_changed"));
        char status_request[256],*status=NULL;size_t length;
        snprintf(status_request,sizeof(status_request),"{\"contract_version\":1,\"op\":\"provider_status\",\"provider_id\":\"%s\"}",provider);
        FILE *stream=open_memstream(&status,&length);CHECK(stream);CHECK(inference_dispatch_config(&q.inference,status_request,strlen(status_request),stream)==1);
        CHECK(!fclose(stream));CHECK(strstr(status,"provider_artifact_changed")&&!strstr(status,"\"ready\":true"));free(status);}
    CHECK(!(using_managed?managed_provider_stop(&managed):activation_stop(&active)));active_attempted=0;
    char *cached=run(&q.inference,request);CHECK(!strcmp(out,cached));free(cached);free(out);
    CHECK(!activation_get(state,stdout));
    printf("PASS actual owned activation: %u checks; fd5 model, authenticated metadata/generation, durable config, offline tracked retry; %s\n",checks,c.require_system_runtime?"system-runtime-v1, observed active cancellation and survivor":"external dynamic dependencies");
    return 0;
}
