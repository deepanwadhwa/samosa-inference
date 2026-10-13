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
static int send_all(int fd,const void *data,size_t n) {
    const char *p=data;while(n){ssize_t got=send(fd,p,n,0);if(got<0&&errno==EINTR)continue;if(got<=0)return 0;p+=got;n-=(size_t)got;}return 1;
}
static int server(const char *portfile,const char *mode,const char *logfile) {
    signal(SIGPIPE,SIG_IGN);
    int listener=socket(AF_INET,SOCK_STREAM,0);if(listener<0)return 3;
    struct sockaddr_in address={.sin_family=AF_INET,.sin_addr.s_addr=htonl(INADDR_LOOPBACK)};
    if(bind(listener,(struct sockaddr*)&address,sizeof(address))||listen(listener,8))return 3;
    socklen_t n=sizeof(address);if(getsockname(listener,(struct sockaddr*)&address,&n))return 3;
    FILE *file=fopen(portfile,"w");if(!file)return 3;fprintf(file,"%u\n",ntohs(address.sin_port));fclose(file);
    unsigned completions=0,budget_blocked=0;
    for(;;) {
        int fd=accept(listener,NULL,NULL);if(fd<0){if(errno==EINTR)continue;return 3;}
        char headers[4096];size_t used=0;int complete=0;
        while(used<sizeof(headers)-1){ssize_t got=recv(fd,headers+used,1,0);if(got!=1)break;used++;if(used>=4&&!memcmp(headers+used-4,"\r\n\r\n",4)){complete=1;break;}}
        headers[used]=0;if(!complete)return 3;
        int unauthorized=!strcmp(mode,"auth")&&!strstr(headers,"Authorization: Bearer fixture-secret\r\n");
        int props=strstr(headers,"GET /props ")!=NULL;
        char *length=strstr(headers,"Content-Length:");if(!length&&!props)return 3;
        unsigned long bytes=length?strtoul(length+15,NULL,10):0;if(bytes>1048576)return 3;
        char *body=malloc(bytes+1);if(!body)return 3;
        size_t read_bytes=0;while(read_bytes<bytes){ssize_t got=recv(fd,body+read_bytes,bytes-read_bytes,0);if(got<=0)return 3;read_bytes+=(size_t)got;}body[bytes]=0;
        Json request;if(!props&&!json_parse(&request,body,bytes))return 3;
        file=fopen(logfile,"a");if(!file)return 3;fprintf(file,"%s\n",body);fclose(file);
        if(!strcmp(mode,"budget-aggregate")){struct timespec wait={0,150000000};nanosleep(&wait,NULL);}
        int stage=(!strcmp(mode,"budget-props")&&props)||(!strcmp(mode,"budget-template")&&strstr(headers,"POST /apply-template "))||(!strcmp(mode,"budget-tokenize")&&strstr(headers,"POST /tokenize "))||(!strcmp(mode,"budget-generate")&&strstr(headers,"POST /completion "));
        if(stage&&!budget_blocked++){
            struct timeval timeout={5,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));char byte;ssize_t closed=recv(fd,&byte,1,0);free(body);close(fd);if(closed!=0)return 3;continue;
        }
        char *reply=NULL;size_t reply_bytes=0;FILE *out=open_memstream(&reply,&reply_bytes);if(!out)return 3;
        if(!strcmp(mode,"oversize")){for(size_t i=0;i<1048577;i++)fputc('x',out);}
        else if(props){
            if(!strcmp(mode,"props-missing"))fputs("{}",out);
            else if(!strcmp(mode,"props-malformed"))fputs("[]",out);
            else fprintf(out,"{\"model_alias\":\"%s\",\"default_generation_settings\":{\"n_ctx\":%u}}",!strcmp(mode,"model-changed")?"foreign.gguf":"fixture-native.gguf",!strcmp(mode,"context-changed")?256:128);
        }
        else if(strstr(headers,"POST /apply-template ")) {
            int thinking=json_get(&request,0,"enable_thinking");
            if(thinking<0||request.tokens[thinking].type!=JT_BOOL||memcmp(body+request.tokens[thinking].begin,"false",5))return 3;
            int messages=json_get(&request,0,"messages");if(messages<0||request.tokens[messages].type!=JT_ARRAY||request.tokens[messages].count!=1)return 3;
            char *prompt=json_string(&request,json_get(&request,messages+1,"content"));if(!prompt)return 3;
            fputs("{\"prompt\":",out);size_t total=strlen(prompt)+10;char *rendered=malloc(total);if(!rendered)return 3;snprintf(rendered,total,"RENDERED %s",prompt);json_quote(out,rendered,strlen(rendered));fputc('}',out);free(prompt);free(rendered);
        } else if(strstr(headers,"POST /tokenize ")) {
            char *content=json_string(&request,json_get(&request,0,"content"));if(!content)return 3;int embedding=!strncmp(content,"task: search result | query: ",28)||!strncmp(content,"title: ",7);if(!embedding&&strncmp(content,"RENDERED ",9))return 3;free(content);
            int special=json_get(&request,0,"add_special"),parse=json_get(&request,0,"parse_special");if(special<0||parse<0||memcmp(body+request.tokens[special].begin,"true",4)||memcmp(body+request.tokens[parse].begin,embedding?"false":"true",embedding?5:4))return 3;
            fputs(!strcmp(mode,"negative-tokens")?"{\"tokens\":[-1,2]}":"{\"tokens\":[10,20,30]}",out);
        } else if(strstr(headers,"POST /v1/embeddings ")) {
            int input=json_get(&request,0,"input");if(input<0||request.tokens[input].type!=JT_ARRAY||request.tokens[input].count!=3)return 3;
            if(!strcmp(mode,"embedding-zero"))fputs("{\"data\":[{\"index\":0,\"embedding\":[0,0]}],\"usage\":{\"prompt_tokens\":3}}",out);
            else if(!strcmp(mode,"embedding-overflow"))fputs("{\"data\":[{\"index\":0,\"embedding\":[1e100,0]}],\"usage\":{\"prompt_tokens\":3}}",out);
            else if(!strcmp(mode,"embedding-clipped"))fputs("{\"data\":[{\"index\":0,\"embedding\":[1,0]}],\"usage\":{\"prompt_tokens\":2}}",out);
            else fputs("{\"data\":[{\"index\":0,\"embedding\":[1,0]}],\"usage\":{\"prompt_tokens\":3}}",out);

        } else if(strstr(headers,"POST /completion ")) {

            if(!strcmp(mode,"cancel-active")&&++completions==1){
                char marker[1024];snprintf(marker,sizeof(marker),"%s.blocked",logfile);
                FILE *started=fopen(marker,"w");if(!started)return 3;fclose(started);
                struct timeval timeout={10,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
                char byte;ssize_t closed=recv(fd,&byte,1,0);
                fclose(out);free(reply);free(body);close(fd);
                if(closed!=0)return 3;
                continue;
            }
            int tokens=json_get(&request,0,"prompt");if(tokens<0||request.tokens[tokens].type!=JT_ARRAY||request.tokens[tokens].count!=3)return 3;
            size_t expected=10;for(int t=tokens+1;t<request.tokens[tokens].next;t=request.tokens[t].next){size_t value;if(!json_uint(&request,t,&value)||value!=expected)return 3;expected+=10;}
            fputs(!strcmp(mode,"usage-overflow")?"{\"content\":\"7319\",\"tokens_predicted\":999,\"timings\":{\"prompt_n\":3}}":"{\"content\":\"7319\",\"tokens_predicted\":4,\"timings\":{\"prompt_n\":2}}",out);
        } else return 3;
        fclose(out);char response_header[256];int h=snprintf(response_header,sizeof(response_header),"HTTP/1.1 %s\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",!strcmp(mode,"http-error")?"503 Unavailable":"200 OK",reply_bytes);
        if(unauthorized)h=snprintf(response_header,sizeof(response_header),"HTTP/1.1 401 Unauthorized\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",reply_bytes);
        int sent=send_all(fd,response_header,(size_t)h)&&send_all(fd,reply,reply_bytes);close(fd);free(body);free(reply);if(!sent&&strcmp(mode,"budget-aggregate"))return 3;
    }
}
static pid_t start(const char *executable,const char *fixture,const char *mode,char endpoint[128]) {
    char port[512],log[512];snprintf(port,sizeof(port),"%s/%s.port",fixture,mode);snprintf(log,sizeof(log),"%s/%s.log",fixture,mode);
    CHECK(!unlink(port)||errno==ENOENT);
    char *args[]={(char*)executable,"--server",port,(char*)mode,log,NULL};pid_t pid;CHECK(posix_spawnp(&pid,executable,NULL,NULL,args,environ)==0);
    for(int i=0;i<500;i++){unsigned number=0;FILE *file=fopen(port,"r");if(file){int rc=fscanf(file,"%u",&number);fclose(file);if(rc==1&&number){snprintf(endpoint,128,"http://127.0.0.1:%u",number);return pid;}}struct timespec delay={0,10000000};nanosleep(&delay,NULL);}
    kill(pid,SIGKILL);waitpid(pid,NULL,0);CHECK(0);return -1;
}
static void stop(pid_t pid){CHECK(kill(pid,SIGTERM)==0);int status;CHECK(waitpid(pid,&status,0)==pid);}
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
    if(argc==5&&!strcmp(argv[1],"--server"))return server(argv[2],argv[3],argv[4]);
    if(argc==6&&!strcmp(argv[1],"--cancel-worker")){
        InferenceConfig worker_cfg={.endpoint=argv[3],.provider_id="native-arbitrary-v7",.admission_state=argv[2],.context_tokens=128,.model_alias="fixture-native.gguf",.owner="alice",.queue_capacity=4};
        char request[1024];snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"generate\",\"operation_id\":\"active-native\",\"provider_id\":\"native-arbitrary-v7\",\"prompt\":\"Code question\",\"max_output_tokens\":16,\"session_id\":\"%s\",\"expected_session_revision\":1}",argv[4]);
        char *response=run(&worker_cfg,request,1);CHECK(strstr(response,"\"code\":\"cancelled\""));FILE *file=fopen(argv[5],"w");CHECK(file);CHECK(fputs(response,file)>=0);CHECK(!fclose(file));free(response);return 0;
    }
    char fixture[]="/tmp/samosa-native-llama-XXXXXX";CHECK(mkdtemp(fixture));char state[512],endpoint[128];snprintf(state,sizeof(state),"%s/admission",fixture);
    pid_t pid=start(argv[0],fixture,"good",endpoint);InferenceConfig cfg={.endpoint=endpoint,.provider_id="native-arbitrary-v7",.admission_state=state,.context_tokens=128,.model_alias="fixture-native.gguf",.owner="alice"};
    float *embedded=NULL;size_t embedded_tokens=0;
    CHECK(!llama_embed(&cfg,"task: search result | query: code",2,&embedded,&embedded_tokens));CHECK(embedded&&embedded[0]==1&&embedded[1]==0&&embedded_tokens==3);free(embedded);
    CHECK(llama_embed(&cfg,"task: search result | query: code",3,&embedded,&embedded_tokens)&&!embedded);
    EmbeddingConfig embedding={.model_id="fixture/embed",.model_revision="v1",.model_sha256="aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa",.engine_sha256="bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb",.tokenizer_hash="cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc",.query_prefix="task: search result | query: ",.document_prefix="",.recipe="fixture-search-v1",.dimensions=2};cfg.embedding=&embedding;
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
    cfg.endpoint="http://example.com:9999";
error(&cfg,count,"invalid_configuration");
    free(session);
    printf("PASS native llama adapter: %u controlled-HTTP checks; exact token IDs, bounded HTTP, budget/provider/admission/last-good sessions\n",checks);
    char *cleanup[]={"find",fixture,"-depth","-delete",NULL};CHECK(posix_spawnp(&pid,cleanup[0],NULL,NULL,cleanup,environ)==0);int status;CHECK(waitpid(pid,&status,0)==pid);CHECK(WIFEXITED(status)&&WEXITSTATUS(status)==0);return 0;
}
