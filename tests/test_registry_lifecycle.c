#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "registry_host.h"
#include "sha256.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <time.h>
#include <stdatomic.h>
#include <poll.h>
static atomic_uint checks;
static int embedding_test;
static pid_t unrelated;
static void cleanup_survivor(void){if(unrelated>0){kill(unrelated,SIGTERM);waitpid(unrelated,NULL,0);}}
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static char *call(InferenceRegistryHost *h,const char *op,const char *extra,int expected){
    char request[2048];snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"%s\",\"provider_id\":\"%s\"%s}",op,h->providers[0].provider_id,extra?extra:"");
    if(!strcmp(op,"describe"))snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"describe\"}");
    char *body=NULL;size_t n=0;FILE *out=open_memstream(&body,&n);CHECK(out);
    int rc=inference_registry_host_dispatch(h,request,strlen(request),out);CHECK(!fclose(out));
    if(rc!=expected)fprintf(stderr,"response: %s",body);
    CHECK(rc==expected);return body;
}
static void expect(InferenceRegistryHost *h,const char *op,const char *extra,int rc,const char *value){
    char *body=call(h,op,extra,rc);if(!strstr(body,value))fprintf(stderr,"%s expected %s, received %s",op,value,body);CHECK(strstr(body,value));free(body);
}
static void reaped(const char *state){
    char path[1024];snprintf(path,sizeof(path),"%s/fixture.pid",state);FILE *f=fopen(path,"r");CHECK(f);
    long pid=0;CHECK(fscanf(f,"%ld",&pid)==1&&pid>1);CHECK(!fclose(f));
    errno=0;CHECK(kill((pid_t)pid,0)==-1&&errno==ESRCH);
}
static char *saved(const char *state){
    char path[1024],*body=NULL;size_t n=0;snprintf(path,sizeof(path),"%s/activation",state);
    FILE *f=open_memstream(&body,&n);CHECK(f);CHECK(!activation_get(path,f));CHECK(!fclose(f));return body;
}
static void *generate(void *context){
    if(embedding_test)return call(context,"embed_text",",\"text\":\"code\",\"role\":\"query\",\"operation_id\":\"busy-generation\",\"timeout_ms\":5000",1);
    return call(context,"generate",",\"prompt\":\"hello\",\"max_output_tokens\":16,\"operation_id\":\"busy-generation\",\"timeout_ms\":5000",1);
}
static void wait_generation(const char *state){
    char path[1024];snprintf(path,sizeof(path),"%s/fixture.log%s",state,embedding_test?".blocked":"");
    for(unsigned i=0;i<200;i++){
        FILE *f=fopen(path,"r");int found=embedding_test&&f;char line[2048];
        if(f){while(fgets(line,sizeof(line),f))if(strstr(line,"\"n_predict\":16"))found=1;fclose(f);}
        if(found)return;
        struct timespec wait={0,10000000};nanosleep(&wait,NULL);
    }
    CHECK(0);
}
static pid_t session(const char *binary,const char *config,FILE **in,FILE **out){
    int request[2],response[2];CHECK(!pipe(request)&&!pipe(response));
    pid_t pid=fork();CHECK(pid>=0);
    if(!pid){
        if(dup2(request[0],STDIN_FILENO)<0||dup2(response[1],STDOUT_FILENO)<0)_exit(125);
        close(request[0]);close(request[1]);close(response[0]);close(response[1]);
        execl(binary,binary,"--config",config,"--session",(char *)NULL);_exit(126);
    }
    CHECK(!close(request[0])&&!close(response[1]));*in=fdopen(request[1],"w");*out=fdopen(response[0],"r");CHECK(*in&&*out);
    CHECK(!setvbuf(*out,NULL,_IONBF,0));return pid;
}
static void session_call(FILE *in,FILE *out,const char *identity,const char *op,const char *expected){
    CHECK(fprintf(in,"{\"contract_version\":1,\"request_id\":\"request\",\"request\":{\"contract_version\":1,\"op\":\"%s\",\"provider_id\":\"%s\"}}\n",op,identity)>0&&!fflush(in));
    struct pollfd wait={fileno(out),POLLIN,0};CHECK(poll(&wait,1,10000)>0);
    char *line=NULL;size_t capacity=0;CHECK(getline(&line,&capacity,out)>0);
    if(!strstr(line,expected))fprintf(stderr,"session %s: %s",op,line);
    CHECK(strstr(line,"\"request_id\":\"request\"")&&strstr(line,expected));free(line);
}
int main(int argc,char **argv){
    int guardian=engine_guardian_dispatch(argc,argv);if(guardian>=0)return guardian;
    CHECK((argc==3||(argc==4&&!strcmp(argv[3],"--embedding")))&&argv[0][0]=='/'&&argv[1][0]=='/'&&argv[2][0]=='/');
    embedding_test=argc==4;
    char root[]="/tmp/samosa-registry-lifecycle-XXXXXX";CHECK(mkdtemp(root));
    char managed[512],registry[512],model[512],state[512],engine[512],pin[65],mode[1024],embedding_file[512];
    snprintf(managed,sizeof(managed),"%s/managed",root);snprintf(registry,sizeof(registry),"%s/registry",root);
    snprintf(model,sizeof(model),"%s/model",root);snprintf(state,sizeof(state),"%s/state",root);snprintf(mode,sizeof(mode),"%s/mode",state);
    snprintf(engine,sizeof(engine),"%s/engine",root);
    FILE *f=fopen(model,"w");CHECK(f&&!fchmod(fileno(f),0600));CHECK(fwrite("abc",1,3,f)==3&&!fclose(f));
    f=fopen(argv[1],"rb");CHECK(f);FILE *copy=fopen(engine,"wb");CHECK(copy&&!fchmod(fileno(copy),0700));
    Sha256 hash;sha256_init(&hash);unsigned char block[65536],digest[32];size_t bytes=0,n;
    while((n=fread(block,1,sizeof(block),f))){CHECK(fwrite(block,1,n,copy)==n);sha256_update(&hash,block,n);bytes+=n;}
    CHECK(!ferror(f)&&!fclose(f)&&!fclose(copy));sha256_final(&hash,digest);sha256_hex(digest,pin);
    int fd=socket(AF_INET,SOCK_STREAM,0);CHECK(fd>=0);struct sockaddr_in a={.sin_family=AF_INET,.sin_addr.s_addr=htonl(INADDR_LOOPBACK)};
    CHECK(!bind(fd,(struct sockaddr*)&a,sizeof(a)));socklen_t len=sizeof(a);CHECK(!getsockname(fd,(struct sockaddr*)&a,&len));CHECK(!close(fd));
    snprintf(embedding_file,sizeof(embedding_file),"%s/embedding.json",root);
    if(embedding_test){
        f=fopen(embedding_file,"w");CHECK(f&&!fchmod(fileno(f),0600));
        fprintf(f,"{\"model_id\":\"fixture/embed\",\"model_revision\":\"v1\",\"model_sha256\":\"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\",\"engine_sha256\":\"%s\",\"tokenizer_hash\":\"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\",\"query_prefix\":\"task: search result | query: \",\"document_prefix\":\"\",\"recipe\":\"fixture-search-v1\",\"dimensions\":2}",pin);CHECK(!fclose(f));
    }
    f=fopen(managed,"w");CHECK(f&&!fchmod(fileno(f),0600));
    fprintf(f,"{\"contract_version\":1,\"profile\":\"%s\",\"state\":\"%s\",\"engine\":\"%s\",\"engine_sha256\":\"%s\",\"engine_size\":%zu,\"model\":\"%s\",\"model_sha256\":\"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\",\"model_size\":3,\"context_tokens\":128,\"model_alias\":\"fixture-native.gguf\",\"owner\":\"alice\",\"port\":%u,\"expected_revision\":0,\"gpu_layers\":0,\"threads\":1",embedding_test?"llama-embedding-static-system-v1":"llama-static-system-v1",state,engine,pin,bytes,model,ntohs(a.sin_port));
    if(embedding_test)fprintf(f,",\"embedding_config_file\":\"%s\"",embedding_file);
    CHECK(fputs("}",f)>=0&&!fclose(f));
    ManagedHost parsed;CHECK(!managed_host_load(managed,&parsed));char identity[100];strcpy(identity,parsed.identity);CHECK(!managed_host_close(&parsed));
    for(unsigned invalid=0;invalid<7;invalid++){
        f=fopen(registry,"w");CHECK(f&&!fchmod(fileno(f),0600));
        fprintf(f,"{\"contract_version\":1,\"owner\":\"%s\",\"providers\":[{\"provider_id\":\"%s\",\"kind\":\"%s\",\"state\":\"%s/%s\",\"context_tokens\":%u,\"managed_config_file\":\"%s\"%s}]}",
            invalid==0?"bob":"alice",invalid==1?"foreign":identity,(invalid==2)!=embedding_test?"embeddings":"text",state,invalid==3?"foreign":"admission",invalid==4?256u:128u,managed,invalid==5?",\"endpoint\":\"http://foreign\"":"");
        CHECK(!fclose(f));
        if(invalid<6){InferenceRegistryHost refused;const char *error=inference_registry_host_load(registry,argv[0],&refused);CHECK(error);
            CHECK(!strcmp(error,invalid==5?"unknown_registry_field":"managed_registry_binding_mismatch"));
            CHECK(!inference_registry_host_close(&refused));CHECK(access(state,F_OK)==-1&&errno==ENOENT);}
    }
    InferenceRegistryHost host;CHECK(!inference_registry_host_load(registry,argv[0],&host));
    expect(&host,"describe",NULL,0,"managed_lifecycle");
    CHECK(access(state,F_OK)==-1&&errno==ENOENT);
    expect(&host,"provider_lifecycle_status",NULL,0,"\"ready\":false");
    expect(&host,"provider_start",",\"engine\":\"/foreign\"",1,"unknown_field");CHECK(access(state,F_OK)==-1&&errno==ENOENT);
    CHECK(!atexit(cleanup_survivor));unrelated=fork();CHECK(unrelated>=0);if(!unrelated){for(;;)pause();}
    expect(&host,"provider_start",NULL,0,"\"ready\":true");CHECK(host.managed[0]->provider.activation.revision==1);
    expect(&host,"provider_start",NULL,0,"\"ready\":true");CHECK(host.managed[0]->provider.activation.revision==1);
    expect(&host,"provider_lifecycle_status",NULL,0,"\"ready\":true");
    char *retained=call(&host,embedding_test?"embed_text":"count_tokens",embedding_test?",\"text\":\"code\",\"role\":\"query\",\"operation_id\":\"retained-embedding\"":",\"prompt\":\"hello\"",0);
    CHECK(strstr(retained,embedding_test?"\"vector\":[1,0]":"tokens"));
    char *last_good=saved(state);
    expect(&host,"provider_stop",NULL,0,"\"state\":\"stopped\"");reaped(state);
    CHECK(host.managed[0]->prepared&&!host.managed[0]->key[0]);
    if(embedding_test){
        char *replay=call(&host,"embed_text",",\"text\":\"code\",\"role\":\"query\",\"operation_id\":\"retained-embedding\"",0);
        CHECK(!strcmp(replay,retained));free(replay);
    }
    free(retained);
    const char *bad[]={"embedding-zero","embedding-overflow","embedding-clipped","embedding-http-error"};
    for(unsigned failure=0;failure<(embedding_test?4u:1u);failure++){
        f=fopen(mode,"w");CHECK(f);CHECK(fputs(embedding_test?bad[failure]:"probe-error",f)>=0&&!fclose(f));
        expect(&host,"provider_start",NULL,1,embedding_test&&failure<3?"invalid_provider_response":"provider_http_error");reaped(state);
        char *current=saved(state);CHECK(!strcmp(current,last_good));free(current);
    }
    free(last_good);
    expect(&host,"provider_lifecycle_status",NULL,0,"\"ready\":false");
    f=fopen(mode,"w");CHECK(f);CHECK(fputs(embedding_test?"embedding-work-block":"work-block",f)>=0&&!fclose(f));
    expect(&host,"provider_start",NULL,0,"\"ready\":true");CHECK(host.managed[0]->provider.activation.revision==2);
    pthread_t worker;pthread_attr_t attributes;CHECK(!pthread_attr_init(&attributes));
    CHECK(!pthread_attr_setstacksize(&attributes,4u*1048576u));
    CHECK(!pthread_create(&worker,&attributes,generate,&host));CHECK(!pthread_attr_destroy(&attributes));wait_generation(state);
    expect(&host,"provider_stop",NULL,1,"provider_busy");
    expect(&host,"provider_start",NULL,1,"provider_busy");
    expect(&host,"operation_cancel",",\"operation_id\":\"busy-generation\"",0,"busy-generation");
    void *result;CHECK(!pthread_join(worker,&result));CHECK(strstr(result,"cancelled"));free(result);
    expect(&host,"provider_lifecycle_status",NULL,0,"\"ready\":true");
    if(embedding_test){
        expect(&host,"embed_text",",\"text\":\"code\",\"role\":\"query\",\"operation_id\":\"cancel-survivor\"",0,"\"vector\":[1,0]");
        expect(&host,"provider_stop",NULL,0,"\"ready\":false");
        expect(&host,"provider_start",NULL,0,"\"ready\":true");
        expect(&host,"embed_text",",\"text\":\"code\",\"role\":\"query\",\"operation_id\":\"deadline-embedding\",\"timeout_ms\":100",1,"deadline_exceeded");
        expect(&host,"provider_lifecycle_status",NULL,0,"\"ready\":true");
        expect(&host,"embed_text",",\"text\":\"code\",\"role\":\"query\",\"operation_id\":\"deadline-survivor\"",0,"\"vector\":[1,0]");
    }
    fd=open(model,O_WRONLY);CHECK(fd>=0);CHECK(pwrite(fd,"xyz",3,0)==3&&!close(fd));
    expect(&host,"provider_lifecycle_status",NULL,0,"provider_artifact_changed");
    expect(&host,"provider_start",NULL,1,"provider_artifact_changed");
    expect(&host,"provider_stop",NULL,0,"\"ready\":false");reaped(state);
    fd=open(model,O_WRONLY);CHECK(fd>=0);CHECK(pwrite(fd,"abc",3,0)==3&&!close(fd));
    expect(&host,"provider_start",NULL,0,"\"ready\":true");
    char pidpath[1024];snprintf(pidpath,sizeof(pidpath),"%s/fixture.pid",state);f=fopen(pidpath,"r");CHECK(f);long engine_pid;CHECK(fscanf(f,"%ld",&engine_pid)==1&&!fclose(f));
    CHECK(!kill((pid_t)engine_pid,SIGKILL));
    for(unsigned i=0;i<300&&!managed_provider_status(&host.managed[0]->provider);i++){struct timespec wait={0,10000000};nanosleep(&wait,NULL);}
    expect(&host,"provider_lifecycle_status",NULL,0,"owned_engine_exited");
    expect(&host,"provider_start",NULL,1,"owned_engine_exited");
    expect(&host,"provider_stop",NULL,1,"engine_failed");reaped(state);
    expect(&host,"provider_lifecycle_status",NULL,0,"\"state\":\"stopped\"");
    expect(&host,"provider_start",NULL,0,"\"ready\":true");
    f=fopen(pidpath,"r");CHECK(f);CHECK(fscanf(f,"%ld",&engine_pid)==1&&!fclose(f));
    CHECK(!kill((pid_t)engine_pid,SIGKILL));
    for(unsigned i=0;i<300&&!managed_provider_status(&host.managed[0]->provider);i++){struct timespec wait={0,10000000};nanosleep(&wait,NULL);}
    const char *closed=inference_registry_host_close(&host);CHECK(closed&&!strcmp(closed,"engine_failed"));
    CHECK(!host.registry.count&&!host.gate_initialized);CHECK(!inference_registry_host_close(&host));reaped(state);CHECK(kill(unrelated,0)==0);
    FILE *input,*output;pid_t child=session(argv[2],registry,&input,&output);
    session_call(input,output,identity,"provider_lifecycle_status","\"ready\":false");
    session_call(input,output,identity,"provider_start","\"ready\":true");
    session_call(input,output,identity,embedding_test?"provider_lifecycle_status":"provider_status","\"ready\":true");
    CHECK(!fclose(input));int result_status;CHECK(waitpid(child,&result_status,0)==child&&WIFEXITED(result_status)&&!WEXITSTATUS(result_status));
    CHECK(!fclose(output));reaped(state);CHECK(kill(unrelated,0)==0);
    child=session(argv[2],registry,&input,&output);session_call(input,output,identity,"provider_start","\"ready\":true");
    CHECK(!kill(child,SIGKILL));CHECK(waitpid(child,&result_status,0)==child&&WIFSIGNALED(result_status));
    CHECK(!fclose(input)&&!fclose(output));
    f=fopen(pidpath,"r");CHECK(f);CHECK(fscanf(f,"%ld",&engine_pid)==1&&!fclose(f));
    for(unsigned i=0;i<300&&kill((pid_t)engine_pid,0)==0;i++){struct timespec wait={0,10000000};nanosleep(&wait,NULL);}
    reaped(state);CHECK(kill(unrelated,0)==0);
    CHECK(!kill(unrelated,SIGTERM));int status;CHECK(waitpid(unrelated,&status,0)==unrelated);
    unrelated=0;
    char command[768];snprintf(command,sizeof(command),"rm -rf '%s'",root);CHECK(system(command)==0);
    printf("PASS registry %s lifecycle: %u checks; lazy discovery, actual qualification, stop/restart, last-good failure, mutation and survivor\n",embedding_test?"embedding":"text",checks);return 0;
}
