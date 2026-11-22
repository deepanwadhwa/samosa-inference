/* Reuse the original native owner fixture/helpers, not another fake engine. */
#define main backend_original_main
#include "test_backend_process.c"
#undef main
#ifdef SAMOSA_BACKEND_LIFECYCLE_API
#include "../src/backend_process_internal.h"
#include "../src/activation.h"
#include "../src/sha256.h"
#endif

typedef struct { pid_t worker,helper,guardian; } LostOwner;
static void guardian_loss(const char *self) {
    Case c;setup(&c,self,"qwen","helper");int notice[2],command[2];CHECK(!pipe(notice)&&!pipe(command));
    pid_t peer=fork();CHECK(peer>=0);if(!peer){for(;;)pause();}track(peer);
    pid_t host=fork();CHECK(host>=0);
    if(!host) {
        close(notice[0]);close(command[1]);LostOwner live={.worker=start(&c)};ready(&c);live.helper=read_pid(c.helper);CHECK(live.helper>1);track(live.helper);
        char record[8192];CHECK(read_text(c.record,record,sizeof(record)));char *parent=strstr(record,"PPID:");CHECK(parent);live.guardian=(pid_t)strtol(parent+5,NULL,10);CHECK(live.guardian>1);
        CHECK(write(notice[1],&live,sizeof(live))==sizeof(live));char op;CHECK(read(command[0],&op,1)==1&&op=='s');
        CHECK(!samosa_backend_process_running(&c.owner));
#ifdef SAMOSA_BACKEND_LIFECYCLE_API
        const char *error=samosa_backend_process_stop_owned(&c.owner,NULL);CHECK(error&&!strcmp(error,"engine_guardian_failed"));
        pid_t observed=99;error=samosa_backend_process_status(&c.owner,&observed);CHECK(error&&!strcmp(error,"engine_guardian_failed")&&observed==0);
        CHECK(c.state.guardian&&!samosa_backend_process_pid(&c.owner));
        error=samosa_backend_process_start_owned(&c.owner,&c.launch);CHECK(error&&!strcmp(error,"engine_guardian_failed"));
#else
        samosa_backend_process_stop(&c.owner,NULL);
#endif
        CHECK(write(notice[1],"r",1)==1);CHECK(read(command[0],&op,1)==1&&op=='x');
        /* Unknown cleanup is process-lifetime ownership. There is no reset API.
           The parent first removes only this controlled fixture's orphan helper. */
        _exit(0);
    }
    close(notice[1]);close(command[0]);track(host);LostOwner live;CHECK(read(notice[0],&live,sizeof(live))==sizeof(live));track(live.worker);track(live.helper);
    CHECK(!kill(live.guardian,SIGKILL));CHECK(!kill(live.worker,SIGTERM)&&gone(live.worker));forget(live.worker);CHECK(!kill(live.helper,0));
    CHECK(write(command[1],"s",1)==1);char reply;CHECK(read(notice[0],&reply,1)==1&&reply=='r');
    /* The old worker's port is free, but a resistant descendant is still alive.
       An unknown cleanup result must not release the marker fence. */
    CHECK(!lease_available(c.marker));
    CHECK(!samosa_backend_process_start(&c.owner,&c.launch));CHECK(!kill(live.helper,0)&&!kill(peer,0));
    CHECK(!kill(live.helper,SIGKILL)&&gone(live.helper));forget(live.helper);CHECK(!lease_available(c.marker));
    CHECK(write(command[1],"x",1)==1);int status;CHECK(waitpid(host,&status,0)==host&&WIFEXITED(status)&&!WEXITSTATUS(status));forget(host);CHECK(lease_available(c.marker));
    CHECK(!close(notice[0])&&!close(command[1]));
    pid_t replacement=start(&c);ready(&c);stop(&c,replacement);finish(&c);
    CHECK(!kill(peer,0)&&!kill(peer,SIGTERM)&&waitpid(peer,NULL,0)==peer);forget(peer);
}
#ifdef SAMOSA_BACKEND_LIFECYCLE_API
static void delegated_record(const char *self) {
    Case c;setup(&c,self,"qwen","ready");EngineProcess process={.guardian=-1,.control=-1,.response=-1};
    CHECK(!backend_process_start_delegated(&c.owner,&c.launch,&process));pid_t worker=samosa_backend_process_pid(&c.owner);CHECK(worker>1&&worker==process.engine&&process.guardian>1);track(worker);ready(&c);
    pid_t observed=0;CHECK(!samosa_backend_process_status(&c.owner,&observed)&&observed==worker);
    char record[8192];CHECK(read_text(c.record,record,sizeof(record)));char *parent=strstr(record,"PPID:");CHECK(parent&&(pid_t)strtol(parent+5,NULL,10)==process.guardian);
    CHECK(!samosa_backend_process_stop_owned(&c.owner,NULL));CHECK(process.guardian<0&&process.control<0&&process.response<0&&process.terminal);CHECK(!c.state.guardian&&gone(worker)&&lease_available(c.marker));forget(worker);
    CHECK(!samosa_backend_process_stop_owned(&c.owner,NULL));observed=99;const char *error=samosa_backend_process_status(&c.owner,&observed);CHECK(error&&!strcmp(error,"provider_unavailable")&&observed==0);finish(&c);
}
static void typed_pending(const char *self) {
    Case c;setup(&c,self,"qwen","ready");EngineProcess process={.guardian=-1,.control=-1,.response=-1};
    CHECK(!backend_process_start_delegated(&c.owner,&c.launch,&process));pid_t worker=process.engine;CHECK(worker>1&&process.guardian>1);track(worker);ready(&c);pid_t guardian=process.guardian;track(guardian);
    CHECK(!kill(guardian,SIGSTOP));const char *error=samosa_backend_process_stop_owned(&c.owner,NULL);CHECK(error&&!strcmp(error,"engine_shutdown_pending"));
    CHECK(process.guardian==guardian&&c.state.guardian&&!lease_available(c.marker));
    pid_t observed=99;error=samosa_backend_process_status(&c.owner,&observed);CHECK(error&&!strcmp(error,"engine_shutdown_pending")&&observed==0);
    c.launch.port=free_port();error=samosa_backend_process_start_owned(&c.owner,&c.launch);CHECK(error&&!strcmp(error,"engine_shutdown_pending"));
    CHECK(!kill(guardian,SIGCONT));CHECK(!samosa_backend_process_stop_owned(&c.owner,NULL));CHECK(gone(worker)&&gone(guardian)&&lease_available(c.marker)&&!c.state.guardian&&process.guardian<0);forget(worker);forget(guardian);finish(&c);
}
typedef struct {Case *owner;Activation *activation;int fail;} ActivationFixture;
static const char *activation_launch(void *context,const char *self,const char *args,double lifetime,size_t limit,int lease,int model,EngineProcess *process) {
    (void)self;(void)args;(void)lifetime;(void)limit;(void)lease;(void)model;
    ActivationFixture *f=context;return backend_process_start_delegated(&f->owner->owner,&f->owner->launch,process);
}
static const char *activation_qualify(void *context) {
    ActivationFixture *f=context;ready(f->owner);pid_t observed=0;
    CHECK(!samosa_backend_process_status(&f->owner->owner,&observed)&&observed==f->activation->process.engine);
    return f->fail?"fixture_qualification_failed":NULL;
}
static unsigned long long copy_engine(const char *from,const char *to,char pin[65]) {
    int in=open(from,O_RDONLY),out=open(to,O_CREAT|O_EXCL|O_WRONLY,0700);CHECK(in>=0&&out>=0);
    Sha256 hash;sha256_init(&hash);unsigned long long total=0;unsigned char block[65536];
    for(;;){ssize_t n=read(in,block,sizeof(block));CHECK(n>=0);if(!n)break;
        CHECK(write(out,block,(size_t)n)==n);sha256_update(&hash,block,(size_t)n);total+=(unsigned long long)n;}
    CHECK(!close(in)&&!close(out));unsigned char digest[32];sha256_final(&hash,digest);sha256_hex(digest,pin);return total;
}
static char *activation_saved(const char *path) {
    char *text=NULL;size_t n=0;FILE *out=open_memstream(&text,&n);CHECK(out&&!activation_get(path,out)&&!fclose(out));return text;
}
static void activation_record(const char *self) {
    Case c;setup(&c,self,"qwen","ready");char engine[PATH_MAX],model[PATH_MAX],state[PATH_MAX],pin[65];path_for(engine,"admitted-engine");path_for(model,"admitted-model");path_for(state,"activation");
    unsigned long long bytes=copy_engine(self,engine,pin);file_text(model,"abc");CHECK(!chmod(model,0600));c.launch.engine=engine;c.launch.model=model;
    Activation activation;ActivationFixture fixture={&c,&activation,0};
    ActivationConfig config={.state=state,.self=self,.engine=engine,.model=model,.engine_sha=pin,
        .model_sha="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",.engine_size=bytes,.model_size=3,.byte_ceiling=bytes,
        .advance_current=1,.argv_template="[\"@ENGINE@\",\"@MODEL@\"]",.require_system_runtime=1,
        .launch=activation_launch,.launch_context=&fixture,.qualify=activation_qualify,.qualification=&fixture};
    CHECK(!activation_start(&config,&activation)&&activation.revision==1);pid_t worker=activation.process.engine;CHECK(worker>1&&samosa_backend_process_pid(&c.owner)==worker);track(worker);
    char *saved=activation_saved(state);CHECK(strstr(saved,"\"revision\":1")&&strstr(saved,"\"live_readiness\":\"unobserved\""));
    CHECK(!samosa_backend_process_stop_owned(&c.owner,NULL)&&!activation_stop(&activation));CHECK(gone(worker)&&lease_available(c.marker));forget(worker);
    fixture.fail=1;c.launch.port=free_port();const char *error=activation_start(&config,&activation);CHECK(error&&!strcmp(error,"fixture_qualification_failed"));
    CHECK(activation.process.guardian<0);CHECK(!samosa_backend_process_stop_owned(&c.owner,NULL)&&!c.state.guardian&&lease_available(c.marker));
    char *unchanged=activation_saved(state);CHECK(!strcmp(saved,unchanged));free(unchanged);free(saved);finish(&c);
}

#endif
int main(int argc,char **argv) {
    int dispatched=samosa_backend_process_dispatch(argc,argv);if(dispatched>=0)return dispatched;
    signal(SIGPIPE,SIG_IGN);
    if(argc>1&&(!strcmp(argv[1],"--serve")||!strcmp(argv[1],"--model-dir")||!strcmp(argv[1],"-m")))return backend_original_main(argc,argv);
    CHECK(argc==2&&argv[1][0]=='/');alarm(45);CHECK(snprintf(temporary,sizeof(temporary),"/tmp/samosa-primary-lifecycle-XXXXXX")<(int)sizeof(temporary));CHECK(mkdtemp(temporary));
    guardian_loss(argv[1]);
#ifdef SAMOSA_BACKEND_LIFECYCLE_API
    delegated_record(argv[1]);
    typed_pending(argv[1]);
    activation_record(argv[1]);
#endif
    printf("PASS primary lifecycle boundary: %u checks; lost-guardian fence, resistant descendant, typed ownership and one delegated process record\n",checks);cleanup();return 0;
}
