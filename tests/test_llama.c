#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "inference.h"
#include "embedding.h"
#include "budget.h"
#include "json.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
extern char **environ;
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
#include "llama_http_fixture.h"
static pid_t start(const char *executable,const char *fixture,const char *mode,char endpoint[128]) {
    char port[512],log[512];snprintf(port,sizeof(port),"%s/%s.port",fixture,mode);snprintf(log,sizeof(log),"%s/%s.log",fixture,mode);
    CHECK(!unlink(port)||errno==ENOENT);
    char *args[]={(char*)executable,"--server",port,(char*)mode,log,NULL};pid_t pid;CHECK(posix_spawnp(&pid,executable,NULL,NULL,args,environ)==0);
    for(int i=0;i<500;i++){unsigned number=0;FILE *file=fopen(port,"r");if(file){int rc=fscanf(file,"%u",&number);fclose(file);if(rc==1&&number){snprintf(endpoint,128,"http://127.0.0.1:%u",number);return pid;}}struct timespec delay={0,10000000};nanosleep(&delay,NULL);}
    kill(pid,SIGKILL);waitpid(pid,NULL,0);CHECK(0);return -1;
}
static void stop(pid_t pid){CHECK(kill(pid,SIGTERM)==0);int status;CHECK(waitpid(pid,&status,0)==pid);}
static int cancel_probe(void *context){(void)context;return 1;}
static char *run(const InferenceConfig *cfg,const char *body,int expected) {
    char *out=NULL;size_t n;FILE *stream=open_memstream(&out,&n);CHECK(stream);CHECK(inference_dispatch_config(cfg,body,strlen(body),stream)==expected);CHECK(!fclose(stream));Json j;CHECK(json_parse(&j,out,n));return out;
}
static void error(const InferenceConfig *cfg,const char *body,const char *code){char *out=run(cfg,body,1);CHECK(strstr(out,code));free(out);}
static void embedding_cli(const char *fixture,const char *endpoint,const char *state,const char *config){
 const char *binary=getenv("TEST_EMBEDDING_CLI");if(!binary)return;
 char input[600],output[600];snprintf(input,sizeof(input),"%s/cli-request",fixture);snprintf(output,sizeof(output),"%s/cli-output",fixture);
 FILE *file=fopen(input,"w");CHECK(file&&fputs("{\"contract_version\":1,\"op\":\"embed_text\",\"text\":\"code\",\"role\":\"query\",\"operation_id\":\"embedding-cli\"}",file)>=0&&!fclose(file));char first[4096]={0};
 for(unsigned i=0;i<2;i++){
 char *args[]={(char*)binary,"--llama-endpoint",(char*)(i?"http://127.0.0.1:1":endpoint),"--provider-id","native-arbitrary-v7","--admission-state",(char*)state,"--owner","alice","--context-tokens","128","--model-alias","fixture-native.gguf","--embedding-config-file",(char*)config,NULL};
 posix_spawn_file_actions_t actions;CHECK(!posix_spawn_file_actions_init(&actions));CHECK(!posix_spawn_file_actions_addopen(&actions,STDIN_FILENO,input,O_RDONLY,0));CHECK(!posix_spawn_file_actions_addopen(&actions,STDOUT_FILENO,output,O_WRONLY|O_CREAT|O_TRUNC,0600));pid_t child;CHECK(!posix_spawnp(&child,binary,&actions,NULL,args,environ));CHECK(!posix_spawn_file_actions_destroy(&actions));int status;CHECK(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
 char body[4096];file=fopen(output,"r");CHECK(file);size_t n=fread(body,1,sizeof(body)-1,file);CHECK(!ferror(file)&&feof(file)&&!fclose(file));body[n]=0;CHECK(strstr(body,"\"vector\":[1,0]")&&strstr(body,"text_embedding"));if(!i)memcpy(first,body,n+1);else CHECK(!strcmp(first,body));
 }
}
int main(int argc,char **argv) {
    if(argc==5&&!strcmp(argv[1],"--server"))return llama_fixture_server(argv[2],argv[3],argv[4],0,NULL);
    if(argc==6&&!strcmp(argv[1],"--cancel-worker")){
        InferenceConfig worker_cfg={.endpoint=argv[3],.provider_id="native-arbitrary-v7",.admission_state=argv[2],.context_tokens=128,.model_alias="fixture-native.gguf",.owner="alice",.queue_capacity=4};
        char request[1024];snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"generate\",\"operation_id\":\"active-native\",\"provider_id\":\"native-arbitrary-v7\",\"prompt\":\"Code question\",\"max_output_tokens\":16,\"session_id\":\"%s\",\"expected_session_revision\":1}",argv[4]);
        char *response=run(&worker_cfg,request,1);CHECK(strstr(response,"\"code\":\"cancelled\""));FILE *file=fopen(argv[5],"w");CHECK(file);CHECK(fputs(response,file)>=0);CHECK(!fclose(file));free(response);return 0;
    }
    char fixture[]="/tmp/samosa-native-llama-XXXXXX";CHECK(mkdtemp(fixture));char state[512],endpoint[128];snprintf(state,sizeof(state),"%s/admission",fixture);
    pid_t pid=start(argv[0],fixture,"good",endpoint);InferenceConfig cfg={.endpoint=endpoint,.provider_id="native-arbitrary-v7",.admission_state=state,.context_tokens=128,.model_alias="fixture-native.gguf",.owner="alice"};
    CHECK(!llama_probe(&cfg));
    float *embedded=NULL;size_t embedded_tokens=0;
    CHECK(!llama_embed(&cfg,"task: search result | query: code",2,&embedded,&embedded_tokens));CHECK(embedded&&embedded[0]==1&&embedded[1]==0&&embedded_tokens==3);free(embedded);
    CHECK(llama_embed(&cfg,"task: search result | query: code",3,&embedded,&embedded_tokens)&&!embedded);
    EmbeddingConfig embedding={.model_id="fixture/embed",.model_revision="v1",.model_sha256="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",.engine_sha256="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",.tokenizer_hash="cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",.query_prefix="task: search result | query: ",.document_prefix="",.recipe="fixture-search-v1",.dimensions=2};cfg.embedding=&embedding;
    CHECK(!llama_probe(&cfg));
    char embedding_file[600],embedding_link[600];snprintf(embedding_file,sizeof(embedding_file),"%s/embedding.json",fixture);snprintf(embedding_link,sizeof(embedding_link),"%s/embedding-link",fixture);
    FILE *embedding_config=fopen(embedding_file,"w");CHECK(embedding_config&&!fchmod(fileno(embedding_config),0600));
    fprintf(embedding_config,"{\"model_id\":\"fixture/embed\",\"model_revision\":\"v1\",\"model_sha256\":\"%s\",\"engine_sha256\":\"%s\",\"tokenizer_hash\":\"%s\",\"query_prefix\":\"task: search result | query: \",\"document_prefix\":\"\",\"recipe\":\"fixture-search-v1\",\"dimensions\":2}",embedding.model_sha256,embedding.engine_sha256,embedding.tokenizer_hash);CHECK(!fclose(embedding_config));
    EmbeddingConfig loaded;CHECK(embedding_config_load(embedding_file,&loaded));CHECK(loaded.dimensions==2&&!strcmp(loaded.model_id,embedding.model_id));embedding_config_free(&loaded);
    CHECK(!chmod(embedding_file,0644));CHECK(!embedding_config_load(embedding_file,&loaded));CHECK(!chmod(embedding_file,0600));CHECK(!symlink(embedding_file,embedding_link));CHECK(!embedding_config_load(embedding_link,&loaded));CHECK(!unlink(embedding_link));CHECK(!link(embedding_file,embedding_link));CHECK(!embedding_config_load(embedding_file,&loaded));CHECK(!unlink(embedding_link));
    embedding_cli(fixture,endpoint,state,embedding_file);
    const char *embed_request="{\"contract_version\":1,\"op\":\"embed_text\",\"text\":\"code\",\"role\":\"query\",\"operation_id\":\"embedding-query\"}";
    char *embedding_response=run(&cfg,embed_request,0);CHECK(strstr(embedding_response,"text_embedding")&&strstr(embedding_response,"\"vector\":[1,0]"));
    const char *saved_endpoint=cfg.endpoint;cfg.endpoint=NULL;char *embedding_replay=run(&cfg,embed_request,0);CHECK(!strcmp(embedding_response,embedding_replay));free(embedding_replay);cfg.endpoint=saved_endpoint;free(embedding_response);
    embedding.recipe="fixture-search-changed";error(&cfg,embed_request,"idempotency_conflict");embedding.recipe="fixture-search-v1";
    const char *document_embed="{\"contract_version\":1,\"op\":\"embed_text\",\"text\":\"Document evidence\",\"title\":\"Record\",\"role\":\"document\",\"operation_id\":\"embedding-document\"}";
    embedding_response=run(&cfg,document_embed,0);CHECK(strstr(embedding_response,"\"title\":\"Record\"")&&strstr(embedding_response,"prepared_input_sha256"));free(embedding_response);
    error(&cfg,"{\"contract_version\":1,\"op\":\"embed_text\",\"text\":\"code\",\"role\":\"query\",\"op|text\":\"override\"}","invalid_request");
    error(&cfg,"{\"contract_version\":1,\"op\":\"embed_text\",\"text\":\"code\",\"role\":\"query\",\"title\":\"bad\"}","invalid_request");
    cfg.owner="bob";error(&cfg,"{\"contract_version\":1,\"op\":\"operation_get\",\"operation_id\":\"embedding-query\"}","unknown_operation");cfg.owner="alice";
    const char *count=
"{\"contract_version\":1,\"op\":\"count_tokens\",\"provider_id\":\"native-arbitrary-v7\",\"prompt\":\"Code question\"}";
    const char *generate="{\"contract_version\":1,\"op\":\"generate\",\"provider_id\":\"native-arbitrary-v7\",\"prompt\":\"Code question\",\"max_output_tokens\":16}";
    const char *readiness="{\"contract_version\":1,\"op\":\"provider_status\",\"provider_id\":\"native-arbitrary-v7\"}";
    char *ready=run(&cfg,readiness,0);CHECK(strstr(ready,"\"status\":\"metadata_compatible\""));
    CHECK(strstr(ready,"\"ready\":false"));CHECK(strstr(ready,"\"artifact_identity\":\"unqualified_external_engine\""));free(ready);
    char *out=run(&cfg,count,0);CHECK(strstr(out,"\"input_tokens\":3"));free(out);out=run(&cfg,generate,0);CHECK(strstr(out,"7319"));CHECK(strstr(out,"\"input_tokens\":3"));CHECK(strstr(out,"\"processed_input_tokens\":2"));free(out);
    out=run(&cfg,"{\"contract_version\":1,\"op\":\"session_create\",\"provider_id\":\"native-arbitrary-v7\"}",0);
    Json session_reply;CHECK(json_parse(&session_reply,out,strlen(out)));char *session=json_string(&session_reply,json_get(&session_reply,0,"session_id"));CHECK(session);free(out);
    char session_request[512];snprintf(session_request,sizeof(session_request),"{\"contract_version\":1,\"op\":\"generate\",\"provider_id\":\"native-arbitrary-v7\",\"prompt\":\"Code question\",\"max_output_tokens\":16,\"session_id\":\"%s\",\"expected_session_revision\":0}",session);
    out=run(&cfg,session_request,0);CHECK(strstr(out,"\"session_revision\":1"));free(out);
    snprintf(session_request,sizeof(session_request),"{\"contract_version\":1,\"op\":\"generate\",\"provider_id\":\"native-arbitrary-v7\",\"prompt\":\"Code question\",\"max_output_tokens\":16,\"session_id\":\"%s\",\"expected_session_revision\":1}",session);
    int lock=-1;CHECK(llama_admit(state,&lock)==NULL);error(&cfg,count,"busy");error(&cfg,readiness,"busy");close(lock);
    error(&cfg,"{\"contract_version\":1,\"op\":\"generate\",\"provider_id\":\"native-arbitrary-v7\",\"prompt\":\"Code question\",\"max_output_tokens\":127}","context_limit");
    error(&cfg,"{\"contract_version\":1,\"op\":\"count_tokens\",\"provider_id\":\"foreign\",\"prompt\":\"Code question\"}","provider_unavailable");
    stop(pid);
    const char *modes[]={"http-error","oversize","negative-tokens","usage-overflow","model-changed"};const char *errors[]={"provider_http_error","response_too_large","invalid_provider_response","invalid_provider_response","provider_changed"};
    for(int i=0;i<5;i++) {
        pid=start(argv[0],fixture,modes[i],endpoint);error(&cfg,session_request,errors[i]);
        char get[256];snprintf(get,sizeof(get),"{\"contract_version\":1,\"op\":\"session_get\",\"session_id\":\"%s\"}",session);
        out=run(&cfg,get,0);CHECK(strstr(out,"\"revision\":1")&&strstr(out,"\"busy\":false"));free(out);stop(pid);
    }
    const char *status_modes[]={"http-error","oversize","model-changed","context-changed","props-missing","props-malformed"};
    const char *status_errors[]={"provider_http_error","response_too_large","provider_changed","provider_changed","provider_changed","invalid_provider_response"};
    for(size_t i=0;i<6;i++){pid=start(argv[0],fixture,status_modes[i],endpoint);error(&cfg,readiness,status_errors[i]);stop(pid);}
    pid=start(argv[0],fixture,"auth",endpoint);cfg.api_key="fixture-secret";
    out=run(&cfg,count,0);CHECK(strstr(out,"\"input_tokens\":3"));free(out);
    cfg.api_key="wrong-secret";error(&cfg,readiness,"provider_auth_failed");
    cfg.api_key="bad\r\nheader";error(&cfg,readiness,"invalid_configuration");cfg.api_key=NULL;stop(pid);
    pid=start(argv[0],fixture,"cancel-active",endpoint);cfg.queue_capacity=4;
    char cancel_output[1024],blocked[1024];snprintf(cancel_output,sizeof(cancel_output),"%s/cancelled-result",fixture);snprintf(blocked,sizeof(blocked),"%s/cancel-active.log.blocked",fixture);
    char *worker_args[]={argv[0],"--cancel-worker",state,endpoint,session,cancel_output,NULL};pid_t worker;CHECK(!posix_spawnp(&worker,argv[0],NULL,NULL,worker_args,environ));
    int observed=0;for(int i=0;i<500;i++){if(!access(blocked,F_OK)){observed=1;break;}struct timespec delay={0,10000000};nanosleep(&delay,NULL);}CHECK(observed);
    const char *cancel="{\"contract_version\":1,\"op\":\"operation_cancel\",\"operation_id\":\"active-native\"}";
    cfg.owner="bob";error(&cfg,cancel,"unknown_operation");cfg.owner="alice";
    out=run(&cfg,cancel,0);CHECK(strstr(out,"\"cancel_requested\":true"));free(out);
    int worker_status;CHECK(waitpid(worker,&worker_status,0)==worker);CHECK(WIFEXITED(worker_status)&&!WEXITSTATUS(worker_status));
    out=run(&cfg,"{\"contract_version\":1,\"op\":\"operation_get\",\"operation_id\":\"active-native\"}",0);CHECK(strstr(out,"\"status\":\"cancelled\""));free(out);
    char get[256];snprintf(get,sizeof(get),"{\"contract_version\":1,\"op\":\"session_get\",\"session_id\":\"%s\"}",session);
    out=run(&cfg,get,0);CHECK(strstr(out,"\"revision\":1")&&strstr(out,"\"busy\":false"));free(out);
    CHECK(!kill(pid,0));out=run(&cfg,generate,0);CHECK(strstr(out,"7319"));free(out);stop(pid);
    const char *budget_modes[]={"budget-props","budget-template","budget-tokenize","budget-generate","budget-aggregate"};
    for(unsigned i=0;i<5;i++){
        pid=start(argv[0],fixture,budget_modes[i],endpoint);
        char timed[1024];snprintf(timed,sizeof(timed),"{\"contract_version\":1,\"op\":\"generate\",\"operation_id\":\"deadline-%u\",\"parent_operation_id\":\"deadline-parent\",\"provider_id\":\"native-arbitrary-v7\",\"prompt\":\"Code question\",\"max_output_tokens\":16,\"session_id\":\"%s\",\"expected_session_revision\":1,\"timeout_ms\":500}",i,session);
        double before=inference_now();out=run(&cfg,timed,1);CHECK(inference_now()-before<3);CHECK(strstr(out,"deadline_exceeded"));CHECK(strstr(out,"\"parent_operation_id\":\"deadline-parent\""));free(out);
        char operation[256];snprintf(operation,sizeof(operation),"{\"contract_version\":1,\"op\":\"operation_get\",\"operation_id\":\"deadline-%u\"}",i);out=run(&cfg,operation,0);CHECK(strstr(out,"\"status\":\"failed\""));CHECK(strstr(out,"\"cancel_requested\":false"));free(out);
        out=run(&cfg,get,0);CHECK(strstr(out,"\"revision\":1")&&strstr(out,"\"busy\":false"));free(out);
        out=run(&cfg,generate,0);CHECK(strstr(out,"7319"));free(out);stop(pid);
    }
    const char *embedding_modes[]={"embedding-zero","embedding-overflow","embedding-clipped"};
    for(unsigned i=0;i<3;i++){pid=start(argv[0],fixture,embedding_modes[i],endpoint);const char *embedding_error=llama_embed(&cfg,"task: search result | query: code",2,&embedded,&embedded_tokens);CHECK(embedding_error&&!strcmp(embedding_error,"invalid_provider_response")&&!embedded);stop(pid);}
    const char *probe_modes[]={"probe-empty","probe-zero","probe-error","usage-overflow"};
    for(unsigned i=0;i<4;i++){
        pid=start(argv[0],fixture,probe_modes[i],endpoint);
        InferenceConfig probe=cfg;probe.embedding=NULL;
        size_t counted=0;CHECK(!llama_count(&probe,"x",&counted)&&counted==3);
        const char *failure=llama_probe(&probe);
        CHECK(failure&&!strcmp(failure,i==2?"provider_http_error":"invalid_provider_response"));
        CHECK(!llama_count(&probe,"x",&counted));stop(pid);
    }
    pid=start(argv[0],fixture,"good",endpoint);
    InferenceConfig probe=cfg;probe.embedding=NULL;
    CHECK(!llama_probe(&probe));probe.cancelled=cancel_probe;
    const char *failure=llama_probe(&probe);CHECK(failure&&!strcmp(failure,"cancelled"));
    probe.cancelled=NULL;probe.deadline_monotonic=inference_now()-1;
    failure=llama_probe(&probe);CHECK(failure&&!strcmp(failure,"deadline_exceeded"));
    probe.deadline_monotonic=0;CHECK(!llama_probe(&probe));stop(pid);
    cfg.endpoint="http://example.com:9999";
error(&cfg,count,"invalid_configuration");
    free(session);
    printf("PASS native llama adapter: %u controlled-HTTP checks; exact token IDs, bounded HTTP, budget/provider/admission/last-good sessions\n",checks);
    char *cleanup[]={"find",fixture,"-depth","-delete",NULL};CHECK(posix_spawnp(&pid,cleanup[0],NULL,NULL,cleanup,environ)==0);int status;CHECK(waitpid(pid,&status,0)==pid);CHECK(WIFEXITED(status)&&WEXITSTATUS(status)==0);return 0;
}
