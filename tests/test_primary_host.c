#define main original_process_main
#include "test_backend_process.c"
#undef main
#include <samosa_inference/primary_host.h>
#include "json.h"

static const char digest[] = "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad";
static SamosaPrimaryInput pin(const char *path, const char *sha) {
    struct stat status;
    CHECK(!stat(path, &status) && status.st_size > 0);
    return (SamosaPrimaryInput){path,sha,(uint64_t)status.st_size};
}

static int serve(int argc,char **argv) {
    int port=0;
    for(int i=1;i+1<argc;i++)if(!strcmp(argv[i],"--port"))port=atoi(argv[i+1]);
    int server=listener(port,&port);
    const char *mode=getenv("SAMOSA_PRIMARY_PROBE_FIXTURE");
    for(;;){
        int fd=accept(server,NULL,NULL);
        if(fd<0&&errno==EINTR)continue;
        CHECK(fd>=0);
        char request[4096]={0};
        size_t used=0;
        while(used<sizeof(request)-1&&recv(fd,request+used,1,0)==1){
            used++;
            if(used>=4&&!memcmp(request+used-4,"\r\n\r\n",4))break;
        }
        if(!used){close(fd);continue;}
        const char *body="{}";
        int http=200;
        if(strstr(request,"POST /v1/chat/completions ")){
            const char *length=strstr(request,"Content-Length: ");
            CHECK(length);
            size_t size=strtoul(length+16,NULL,10),read_bytes=0;
            char payload[512]={0};
            CHECK(size>0&&size<sizeof(payload));
            while(read_bytes<size){
                ssize_t n=recv(fd,payload+read_bytes,size-read_bytes,0);
                CHECK(n>0);read_bytes+=(size_t)n;
            }
            int maple=!strcmp(argv[1],"--model-dir");
            CHECK(strstr(payload,maple?"\"max_tokens\":64,":"\"max_tokens\":4,")&&strstr(payload,"Say hello."));
            body="{\"choices\":[{\"message\":{\"content\":\"Hello!\"}}],\"usage\":{\"completion_tokens\":4}}";
            if(maple){
                CHECK(strstr(payload,"\"chat_template_kwargs\":{\"enable_thinking\":false}"));
                body="{\"choices\":[{\"message\":{\"role\":\"assistant\",\"content\":\"Hello!\"},\"finish_reason\":\"length\"}]}";
            }
            if(!strcmp(argv[1],"-m")){
                CHECK(strstr(payload,"\"chat_template_kwargs\":{\"enable_thinking\":false}"));
                int gateway=getenv("SAMOSA_PRIMARY_GATEWAY_FIXTURE")!=NULL;
                if(gateway && strstr(payload,"\"model\":\"ornith-1.0-9b\""))
                    body="{\"choices\":[{\"message\":{\"content\":\"\",\"reasoning_content\":\"Thinking\"}}],\"usage\":{\"completion_tokens\":4}}";
                else CHECK(strstr(payload,gateway?"\"model\":\"bonsai-27b-1bit\"":"\"model\":\"fixture-gguf\""));
            }
            if(mode&&!strcmp(mode,"empty"))body="{\"choices\":[{\"message\":{\"content\":\"\"}}],\"usage\":{\"completion_tokens\":4}}";
            if(mode&&!strcmp(mode,"zero"))body="{\"choices\":[{\"message\":{\"content\":\"Hello!\"}}],\"usage\":{\"completion_tokens\":0}}";
            if(mode&&!strcmp(mode,"http"))http=503;
        }
        char response[1024];
        size_t length=strlen(body);
        if(mode&&!strcmp(mode,"truncated")&&strstr(request,"POST "))length++;
        int n=snprintf(response,sizeof(response),"HTTP/1.1 %d Fixture\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n%s",http,length,body);
        CHECK(n>0&&(size_t)n<sizeof(response));
        (void)send(fd,response,(size_t)n,0);
        close(fd);
    }
}

static char *saved(const char *state) {
    char *body=NULL;size_t bytes;
    FILE *out=open_memstream(&body,&bytes);
    CHECK(out&&!samosa_primary_last_good(state,out)&&!fclose(out));
    return body;
}

static void check_error(const char *error,const char *expected) {
    if(!error||strcmp(error,expected))fprintf(stderr,"expected %s, got %s\n",expected,error?error:"ok");
    CHECK(error&&!strcmp(error,expected));
}

typedef struct { SamosaPrimaryHost *host; int paused, resumed, mutate; const char *asset; } Reclaim;
static int visual_available(void *context,int provider) { (void)context;return provider==2; }
static double required(void *context) { (void)context;return 1; }
static long long memory(void *context) { (void)context;return 16LL<<30; }
static int primary_running(void *context) { (void)context;return 1; }
static int primary_pause(void *context) {
    Reclaim *r=context;
    CHECK(!samosa_primary_pause(r->host,NULL));
    pid_t pid=1;unsigned revision=1;
    check_error(samosa_primary_status(r->host,&pid,&revision),"provider_unavailable");
    CHECK(!pid&&!revision);
    r->paused=1;return 1;
}
static int primary_resume(void *context) {
    Reclaim *r=context;CHECK(r->paused);
    const char *error=samosa_primary_resume(r->host);
    if(r->mutate){check_error(error,"provider_artifact_changed");return 0;}
    if(error)fprintf(stderr,"resume: %s\n",error);
    CHECK(!error);r->resumed=1;return 1;
}
static int primary_probe(void *context) {
    Reclaim *r=context;pid_t pid;unsigned revision;
    return !samosa_primary_status(r->host,&pid,&revision);
}
static int stopping(void *context) { (void)context;return 0; }
static long long now_ms(void *context) { (void)context;return (long long)(now()*1000); }
static void sleep_ms(void *context,long ms) { (void)context;pause_ms(ms); }
static SamosaVisionResourceBudget budget(void *context,int detail) {
    (void)context;(void)detail;return (SamosaVisionResourceBudget){.admitted=1,.available_gb=4};
}
static SamosaVisionResourceBudget wait_memory(void *context,int detail,double gb,SamosaVisionResourceBudget prior) {
    (void)detail;(void)gb;CHECK(((Reclaim *)context)->paused);return prior;
}
static int visual_start(void *context,int provider,SamosaMmSession *session) {
    Reclaim *r=context;CHECK(provider==2&&r->paused);session->active=1;return 1;
}
static void visual_stop(void *context,SamosaMmSession *session) {
    Reclaim *r=context;session->active=0;
    if(r->mutate)CHECK(!chmod(r->asset,0640));
}
static void reclaim(Reclaim *r,SamosaTurnGate *gate) {
    SamosaVisualTurnControl control={.gate=gate,.available=visual_available,.molmo_required_gb=required,
        .total_memory_bytes=memory,.primary_running=primary_running,.primary_stop=primary_pause,
        .primary_start=primary_resume,.primary_probe=primary_probe,.stopping=stopping,.now_ms=now_ms,
        .sleep_ms=sleep_ms,.resource_budget=budget,.wait_for_molmo_memory=wait_memory,
        .session_start=visual_start,.session_close=visual_stop,.context=r};
    SamosaVisualTurn turn={0};char error[96];
    CHECK(samosa_visual_turn_start(&control,&turn,error,sizeof(error)));
    CHECK(r->paused&&!r->resumed&&turn.session.active);
    samosa_visual_turn_release(&control,&turn);
    CHECK(r->resumed==!r->mutate&&!samosa_turn_status(gate).specialist_active);
}

int main(int argc,char **argv) {
    int dispatch=samosa_backend_process_dispatch(argc,argv);
    if(dispatch>=0)return dispatch;
    signal(SIGPIPE,SIG_IGN);
    if(argc>1&&(!strcmp(argv[1],"--serve")||!strcmp(argv[1],"--model-dir")||!strcmp(argv[1],"-m")))
        return serve(argc,argv);
    CHECK(argc==3||argc==7);
    int actual=argc==7;
    CHECK(snprintf(temporary,sizeof(temporary),"/tmp/samosa-primary-host-XXXXXX")<(int)sizeof(temporary));
    CHECK(mkdtemp(temporary));CHECK(!atexit(cleanup));
    Case c;setup(&c,argv[1],"qwen","ready");
    char model[PATH_MAX],activation[PATH_MAX],chats[PATH_MAX],files[6][PATH_MAX];
    path_for(model,"model");path_for(activation,"activation");path_for(chats,"chats");CHECK(!mkdir(chats,0700));
    const char *names[]={"manifest.json","config.json","experts.bin","resident.safetensors","tokenizer_qwen36.json","generation_config.json"};
    SamosaPrimaryInput inputs[5],library,anchor;
    const char *directory=actual?argv[3]:model;
    if(!actual)CHECK(!mkdir(directory,0700));
    for(size_t i=0;i<6;i++){
        CHECK(snprintf(files[i],PATH_MAX,"%s/%s",directory,names[i])<PATH_MAX);
        if(!actual)file_text(files[i],"abc");
        const char *hash=actual?getenv(names[i]):digest;CHECK(hash);
        SamosaPrimaryInput p=pin(files[i],hash);if(i)inputs[i-1]=p;else anchor=p;
    }
    c.launch.model=directory;c.launch.chats=chats;c.launch.tokenizer=files[4];
    c.launch.runtime.context_effective=512;
    c.launch.engine=actual?argv[2]:argv[1];
    SamosaTurnGate gate={0};CHECK(samosa_turn_gate_init(&gate));int upstream=-1;
    SamosaPrimaryConfig config={
        .owner=c.owner,.launch=c.launch,.state=activation,
        .model=anchor,.engine=pin(c.launch.engine,actual?argv[4]:argv[2]),
        .inputs=inputs,.input_count=5,.gate=&gate,.upstream_fd=&upstream,
        .startup_seconds=30,.generation_seconds=actual?120:3
    };
    if(actual){library=pin(argv[5],argv[6]);config.libraries=&library;config.library_count=1;}
    SamosaPrimaryHost *host=NULL;
    if(actual){
        SamosaPrimaryConfig unbound=config;unbound.library_count=0;
        check_error(samosa_primary_start(&host,&unbound),"unqualified_library_dependencies");CHECK(!host);
    }
    const char *error=samosa_primary_start(&host,&config);
    if(error)fprintf(stderr,"primary start: %s\n",error);
    CHECK(!error&&host);
    pid_t worker=0;unsigned revision=0;
    CHECK(!samosa_primary_status(host,&worker,&revision)&&worker>1&&revision==1);
    CHECK(worker==read_pid(c.marker));track(worker);
    char *last=saved(activation);
    CHECK(strstr(last,"native-primary-v1")&&strstr(last,"\"revision\":1")&&strstr(last,"\"live_readiness\":\"unobserved\""));
    Reclaim visual={.host=host};
    reclaim(&visual,&gate);
    CHECK(gone(worker));forget(worker);
    CHECK(!samosa_primary_status(host,&worker,&revision)&&worker>1&&revision==1);
    track(worker);
    char *resumed=saved(activation);CHECK(!strcmp(last,resumed));free(resumed);
    if(!actual){
        CHECK(!chmod(inputs[1].path,0600));
        check_error(samosa_primary_status(host,&worker,&revision),"provider_artifact_changed");
        CHECK(!worker&&!revision);worker=read_pid(c.marker);CHECK(worker>1);
    }
    CHECK(!samosa_primary_stop(&host,NULL)&&!host&&gone(worker));forget(worker);
    CHECK(lease_available(c.marker)&&upstream==-1&&!samosa_turn_generating(&gate));
    if(!actual){
        const char *modes[]={"empty","zero","http","truncated"};
        for(size_t i=0;i<sizeof(modes)/sizeof(modes[0]);i++){
            CHECK(!setenv("SAMOSA_PRIMARY_PROBE_FIXTURE",modes[i],1));config.launch.port=free_port();
            check_error(samosa_primary_start(&host,&config),"provider_qualification_failed");
            CHECK(!host&&!c.state.guardian&&lease_available(c.marker));
            char *current=saved(activation);CHECK(!strcmp(last,current));free(current);
        }
        CHECK(!unsetenv("SAMOSA_PRIMARY_PROBE_FIXTURE"));config.launch.port=free_port();
        char extra[PATH_MAX];CHECK(snprintf(extra,sizeof(extra),"%s/extra.safetensors",model)<PATH_MAX);
        file_text(extra,"abc");
        check_error(samosa_primary_start(&host,&config),"unbound_primary_input");
        CHECK(!host&&!unlink(extra));
        CHECK(!samosa_primary_start(&host,&config));
        CHECK(!samosa_primary_status(host,&worker,&revision)&&worker>1&&revision==2);
        track(worker);visual=(Reclaim){.host=host,.mutate=1,.asset=inputs[1].path};
        reclaim(&visual,&gate);
        CHECK(!samosa_primary_stop(&host,NULL)&&gone(worker));forget(worker);

        config.launch.backend="maple";config.launch.port=free_port();
        const char *maple[]={"maple-manifest.json","config.json","maple-experts.bin","maple-resident.safetensors","tokenizer.json"};
        for(size_t i=0;i<5;i++){
            CHECK(snprintf(files[i],PATH_MAX,"%s/%s",model,maple[i])<PATH_MAX);
            file_text(files[i],i?"abc":"{}");
            if(i)inputs[i-1]=pin(files[i],digest);
            else config.model=pin(files[i],"44136fa355b3678a1146ad16f7e8649e94fb4fc21fe77e8310c060f61caaff8a");
        }
        config.input_count=4;
        CHECK(!samosa_primary_start(&host,&config));
        CHECK(!samosa_primary_status(host,&worker,&revision)&&worker>1&&revision==3);
        track(worker);CHECK(!samosa_primary_stop(&host,NULL)&&gone(worker));forget(worker);

        config.launch.backend="ornith";config.launch.port=free_port();
        config.launch.llama.engine=config.engine.path;
        config.launch.llama.model=files[1];config.model=pin(files[1],digest);
        config.launch.llama.alias="fixture-gguf";config.launch.llama.port=config.launch.port;
        config.launch.llama.projector=NULL;config.input_count=0;
        CHECK(!samosa_primary_start(&host,&config));
        CHECK(!samosa_primary_status(host,&worker,&revision)&&worker>1&&revision==4);
        track(worker);CHECK(!samosa_primary_stop(&host,NULL)&&gone(worker));forget(worker);
    }
    free(last);samosa_turn_gate_destroy(&gate);CHECK(!pthread_mutex_destroy(&c.mutex));
    printf("PASS primary host %s: %u assertions, actual generation response required before readiness\n",actual?"retained native Qwen":"fixture",checks);
    return 0;
}
