#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/whisper_process.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <pthread.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static atomic_uint checks;
static pid_t cleanup_host,cleanup_peer;
static void failed(const char *expression,int line) {
    fprintf(stderr,"FAIL %d: %s\n",line,expression);
    pid_t owned[]={cleanup_host,cleanup_peer};
    for(size_t i=0;i<2;i++)if(owned[i]>1){siginfo_t info={0};
        if(!waitid(P_PID,(id_t)owned[i],&info,WEXITED|WNOHANG|WNOWAIT)){(void)kill(owned[i],SIGKILL);while(waitpid(owned[i],NULL,0)<0&&errno==EINTR){}}}
    abort();
}
#undef assert
#define assert(expression) do{checks++;if(!(expression))failed(#expression,__LINE__);}while(0)
static void write_file(const char *path,const char *text) {
    FILE *f=fopen(path,"w");assert(f&&fputs(text,f)>=0&&!fclose(f));
}
static void empty(const char *path) {
    DIR *d=opendir(path);assert(d);struct dirent *entry;
    while((entry=readdir(d)))assert(!strcmp(entry->d_name,".")||!strcmp(entry->d_name,".."));
    assert(!closedir(d));
}
static int fixture(int argc,char **argv) {
    assert(argc==11&&!strcmp(argv[1],"-m")&&!strcmp(argv[3],"-f")&&!strcmp(argv[5],"-l")&&
        !strcmp(argv[6],"en")&&!strcmp(argv[7],"-nt")&&!strcmp(argv[9],"-of"));
    struct stat audio;
    if(getenv("SAMOSA_WHISPER_GATEWAY_ROOT")){
        const char *root=getenv("SAMOSA_WHISPER_GATEWAY_ROOT");
        assert(!strcmp(argv[2],getenv("SAMOSA_WHISPER_EXPECTED_MODEL"))&&!strncmp(argv[4],root,strlen(root)));
        assert(!stat(argv[4],&audio)&&audio.st_size==3244);
    }else if(getenv("SAMOSA_WHISPER_EXPECT_OWNED")){
        size_t prefix=strlen(argv[10])-strlen("result");
        assert(!strcmp(argv[2],getenv("SAMOSA_WHISPER_EXPECTED_MODEL"))&&!strncmp(argv[4],argv[10],prefix)&&!strcmp(argv[4]+prefix,"input.wav"));
        assert(!stat(argv[4],&audio)&&audio.st_size==44+271*32000&&!(audio.st_mode&077));
    }else{
        assert(!strcmp(argv[2],getenv("SAMOSA_WHISPER_EXPECTED_MODEL"))&&!strcmp(argv[4],getenv("SAMOSA_WHISPER_EXPECTED_INPUT")));
        assert(!stat(argv[4],&audio)&&audio.st_size==44+271*32000);
    }
    const char *mode=getenv("SAMOSA_WHISPER_FIXTURE_MODE");assert(mode);
    char output[4096];assert(snprintf(output,sizeof(output),"%s.%s",argv[10],!strcmp(argv[8],"-osrt")?"srt":"txt")<(int)sizeof(output));
    pid_t helper=0;
    if(strstr(mode,"helper")){helper=fork();assert(helper>=0);if(!helper){signal(SIGTERM,SIG_IGN);for(;;)pause();}}
    int blocking=!strcmp(mode,"block")||!strcmp(mode,"block-helper")||!strcmp(mode,"stubborn-helper");
    if(blocking){write_file(output,"partial output\n");if(!strcmp(mode,"stubborn-helper"))signal(SIGTERM,SIG_IGN);}
    if(strstr(mode,"helper")||!strcmp(mode,"block")){
        FILE *marker=fopen(getenv("SAMOSA_WHISPER_FIXTURE_MARKER"),"w");assert(marker);
        assert(fprintf(marker,"%ld %ld %ld\n%s\n%s\n",(long)getpid(),(long)helper,(long)getppid(),argv[10],argv[4])>0&&!fclose(marker));
    }
    if(!strcmp(mode,"crash")||!strcmp(mode,"crash-helper"))return 17;
    if(!strcmp(mode,"missing"))return 0;
    if(blocking){for(;;)pause();}
    if(!strcmp(mode,"noisy")){char bytes[4096];memset(bytes,'L',sizeof(bytes));for(int i=0;i<300;i++){assert(fwrite(bytes,sizeof(bytes),1,stdout)==1&&fwrite(bytes,sizeof(bytes),1,stderr)==1);}fflush(stdout);fflush(stderr);}
    assert(!strcmp(argv[8],"-osrt")||!strcmp(argv[8],"-otxt"));
    if(!strcmp(mode,"large")) {
        FILE *file=fopen(output,"w");assert(file);char chunk[65536];memset(chunk,'x',sizeof(chunk));
        for(size_t i=0;i<256;i++)assert(fwrite(chunk,1,sizeof(chunk),file)==sizeof(chunk));
        assert(!fclose(file));return 0;
    }
    if(!strcmp(mode,"oversized")) {
        int fd=open(output,O_WRONLY|O_CREAT,0600);assert(fd>=0&&!ftruncate(fd,16777217)&&!close(fd));return 0;
    }
    if(!strcmp(mode,"invalid"))write_file(output,"bad\xc0\xaf");
    else if(!strcmp(mode,"control"))write_file(output,"bad\001text");
    else if(!strcmp(mode,"blank"))write_file(output," \t\r\n");
    else if(!strcmp(argv[8],"-osrt"))write_file(output," \n1\n00:00:00,000 --> 00:04:31,000\nLong input: café 🥟\n\n");
    else write_file(output," \tHello café 🥟\r\n");
    return 0;
}
typedef struct {
    pthread_mutex_t mutex;
    int installing,active,attachment,acquires,starts,reaps,releases,cancel_on_start;
    pid_t pid,last_pid;atomic_int cancel;const char *temporary;int hold_notice;
} State;
static int acquire(void *context,int preserve) {
    State *s=context;pthread_mutex_lock(&s->mutex);s->acquires++;
    if(s->installing||s->active||(!preserve&&s->attachment)){pthread_mutex_unlock(&s->mutex);return 0;}
    s->active=1;if(!preserve)atomic_store(&s->cancel,0);pthread_mutex_unlock(&s->mutex);return 1;
}
static void started(void *context,pid_t pid) {
    State *s=context;pthread_mutex_lock(&s->mutex);s->pid=pid;s->starts++;
    if(s->cancel_on_start)atomic_store(&s->cancel,1);
    if(atomic_load(&s->cancel))assert(!kill(pid,SIGTERM));
    pthread_mutex_unlock(&s->mutex);
}
static void reaped(void *context,pid_t pid) {
    State *s=context;pthread_mutex_lock(&s->mutex);assert(pid==s->pid);errno=0;assert(waitpid(pid,NULL,WNOHANG)==-1&&errno==ECHILD);
    assert(!kill(pid,0));s->last_pid=pid;s->pid=0;s->reaps++;pthread_mutex_unlock(&s->mutex);
    if(s->hold_notice>0){char ready='R';assert(write(s->hold_notice,&ready,1)==1);for(;;)pause();}
}
static void release(void *context) {
    State *s=context;if(s->temporary)empty(s->temporary);
    if(s->last_pid>0){errno=0;assert(kill(s->last_pid,0)<0&&errno==ESRCH);}
    pthread_mutex_lock(&s->mutex);s->pid=0;s->active=0;s->releases++;pthread_mutex_unlock(&s->mutex);
}
static int cancelled(void *context) {return atomic_load(&((State *)context)->cancel);}
typedef struct {const SamosaWhisperProcessConfig *config;const SamosaWhisperProcessControl *control;const char *input;int result;char *text;atomic_int finished;int own_input;} Call;
static void *run(void *context) {
    Call *c=context;c->result=(c->own_input?samosa_whisper_process_run_owned:samosa_whisper_process_run)(c->config,c->control,c->input,1,1,&c->text);atomic_store(&c->finished,1);return NULL;
}

static void pause_ms(long ms){struct timespec delay={ms/1000,(ms%1000)*1000000};while(nanosleep(&delay,&delay)&&errno==EINTR){}}
static int gone(pid_t pid){for(int i=0;i<300;i++){if(kill(pid,0)<0&&errno==ESRCH)return 1;pause_ms(10);}return 0;}
typedef struct{pid_t engine,helper,guardian;char prefix[PATH_MAX];} Metadata;
static Metadata metadata(const char *path){
    Metadata m={0};for(int i=0;i<500;i++){FILE *f=fopen(path,"r");long engine,helper,guardian;
        if(f){char line[PATH_MAX];int ready=fscanf(f,"%ld %ld %ld\n",&engine,&helper,&guardian)==3&&fgets(line,sizeof(line),f)!=NULL;fclose(f);
            if(ready){size_t n=strlen(line);if(n&&line[n-1]=='\n')line[--n]=0;
                if(engine>1&&guardian>1&&n){m.engine=(pid_t)engine;m.helper=(pid_t)helper;m.guardian=(pid_t)guardian;assert(n<sizeof(m.prefix));memcpy(m.prefix,line,n+1);return m;}}}
        pause_ms(10);
    }assert(!"fixture metadata deadline");return m;
}
/* Same native fixture reproduces old direct-fork owner-death leakage. Always
 * remove the known surviving fixture PIDs/result before reporting failure. */
static int parent_death(const SamosaWhisperProcessConfig *config,const char *input,const char *marker,int after_result,int own_input) {
    (void)unlink(marker);assert(!setenv("SAMOSA_WHISPER_FIXTURE_MODE",after_result?"helper":"stubborn-helper",1));
    int notice[2];assert(!pipe(notice)&&!fcntl(notice[0],F_SETFD,FD_CLOEXEC)&&!fcntl(notice[1],F_SETFD,FD_CLOEXEC));
    pid_t peer=fork();assert(peer>=0);if(!peer){close(notice[0]);close(notice[1]);for(;;)pause();}
    cleanup_peer=peer;pid_t host=fork();assert(host>=0);if(!host){close(notice[0]);
        State state={.temporary=config->temporary_directory,.hold_notice=after_result?notice[1]:0};assert(!pthread_mutex_init(&state.mutex,NULL));atomic_init(&state.cancel,0);
        SamosaWhisperProcessControl control={acquire,started,reaped,release,cancelled,&state};char *text=NULL;
        (void)(own_input?samosa_whisper_process_run_owned:samosa_whisper_process_run)(config,after_result?&control:NULL,input,1,0,&text);_exit(9);
    }
    cleanup_host=host;close(notice[1]);Metadata m=metadata(marker);assert(m.helper>1&&m.engine!=peer&&m.helper!=peer);
    if(after_result){char ready;assert(read(notice[0],&ready,1)==1&&ready=='R');assert(!kill(m.engine,0));}close(notice[0]);
    assert(!kill(host,SIGKILL));int status;assert(waitpid(host,&status,0)==host&&WIFSIGNALED(status));cleanup_host=0;
    int engine_gone=gone(m.engine),helper_gone=gone(m.helper),guardian_gone=gone(m.guardian);char output[PATH_MAX];assert(snprintf(output,sizeof(output),"%s.srt",m.prefix)<(int)sizeof(output));
    int output_gone=0;for(int i=0;i<300;i++){if(access(output,F_OK)<0&&errno==ENOENT){output_gone=1;break;}pause_ms(10);}
    // These are actual live controlled fixture identities, never adopted PIDs.
    if(!engine_gone){assert(!kill(m.engine,SIGKILL));assert(gone(m.engine));}
    if(!helper_gone){assert(!kill(m.helper,SIGKILL));assert(gone(m.helper));}
    if(!output_gone)assert(!unlink(output));char *slash=strrchr(m.prefix,'/');assert(slash);*slash=0;
    if(strcmp(m.prefix,config->temporary_directory))assert(!rmdir(m.prefix)||(errno==ENOENT));
    assert(!kill(peer,0)&&!kill(peer,SIGKILL)&&waitpid(peer,NULL,0)==peer);cleanup_peer=0;empty(config->temporary_directory);
    struct stat original;if(own_input)assert(access(input,F_OK)<0&&errno==ENOENT);else assert(!stat(input,&original));assert(!stat(config->model,&original));
    if(!engine_gone||!helper_gone||!guardian_gone||!output_gone){fprintf(stderr,"FAIL original owner death: engine_gone=%d helper_gone=%d result_gone=%d; remaining fixtures explicitly cleaned\n",engine_gone,helper_gone,output_gone);return 0;}
    assert(guardian_gone);
    return 1;
}
static void lifecycle(const SamosaWhisperProcessConfig *config,const SamosaWhisperProcessControl *control,State *state,const char *input,const char *marker) {
    const char *modes[]={"helper","crash-helper"};for(size_t i=0;i<2;i++){
        (void)unlink(marker);assert(!setenv("SAMOSA_WHISPER_FIXTURE_MODE",modes[i],1));char *text=NULL;
        assert(samosa_whisper_process_run(config,control,input,1,0,&text)==(i?SAMOSA_WHISPER_PROCESS_FAILED:SAMOSA_WHISPER_OK));free(text);
        Metadata m=metadata(marker);assert(gone(m.engine)&&gone(m.helper)&&gone(m.guardian));empty(config->temporary_directory);
    }
    for(int stopped=0;stopped<2;stopped++){
        (void)unlink(marker);assert(!setenv("SAMOSA_WHISPER_FIXTURE_MODE","stubborn-helper",1));atomic_store(&state->cancel,0);
        Call call={.config=config,.control=control,.input=input};pthread_t thread;assert(!pthread_create(&thread,NULL,run,&call));Metadata m=metadata(marker);
        if(stopped){assert(!kill(m.guardian,SIGSTOP));pause_ms(50);}
        pthread_mutex_lock(&state->mutex);assert(state->active&&state->pid==m.engine);atomic_store(&state->cancel,1);assert(!kill(m.engine,SIGTERM));pthread_mutex_unlock(&state->mutex);
        if(stopped){pause_ms(150);assert(!atomic_load(&call.finished));pthread_mutex_lock(&state->mutex);assert(state->active&&state->pid==m.engine);pthread_mutex_unlock(&state->mutex);assert(!kill(m.guardian,SIGCONT));}
        assert(!pthread_join(thread,NULL)&&call.result==SAMOSA_WHISPER_CANCELLED&&!call.text&&!state->active);
        assert(gone(m.engine)&&gone(m.helper)&&gone(m.guardian));empty(config->temporary_directory);
    }
    assert(parent_death(config,input,marker,0,0));assert(parent_death(config,input,marker,1,0));
}

#ifdef SAMOSA_WHISPER_LEGACY
/* Link-only compatibility for the old-runner owner-death reproduction. That
 * branch invokes only borrowed run(), never this new ownership entry point. */
int samosa_whisper_process_run_owned(const SamosaWhisperProcessConfig *config,const SamosaWhisperProcessControl *control,const char *input,int srt,int preserve,char **text){return samosa_whisper_process_run(config,control,input,srt,preserve,text);}
#endif
static void prepared(const char *path){int fd=open(path,O_CREAT|O_EXCL|O_WRONLY,0600);assert(fd>=0&&!ftruncate(fd,44+271*32000)&&!close(fd));}
static void owned_cases(const SamosaWhisperProcessConfig *config,const SamosaWhisperProcessControl *control,State *state,const char *input,const char *marker,const char *root){
    char owned[PATH_MAX],link[PATH_MAX];assert(snprintf(owned,sizeof(owned),"%s/prepared.wav",root)<(int)sizeof(owned)&&snprintf(link,sizeof(link),"%s/prepared-link.wav",root)<(int)sizeof(link));
    assert(!setenv("SAMOSA_WHISPER_EXPECT_OWNED","1",1)&&!setenv("SAMOSA_WHISPER_FIXTURE_MODE","valid",1));char *text=NULL;
    for(int i=0;i<2;i++){prepared(owned);assert(samosa_whisper_process_run_owned(config,control,owned,i,0,&text)==SAMOSA_WHISPER_OK&&text);free(text);text=NULL;assert(access(owned,F_OK)<0&&errno==ENOENT);empty(config->temporary_directory);}
    prepared(owned);state->active=1;assert(samosa_whisper_process_run_owned(config,control,owned,0,0,&text)==SAMOSA_WHISPER_BUSY&&!text&&!access(owned,F_OK));state->active=0;
    atomic_store(&state->cancel,1);assert(samosa_whisper_process_run_owned(config,control,owned,0,1,&text)==SAMOSA_WHISPER_CANCELLED&&!text&&!access(owned,F_OK));atomic_store(&state->cancel,0);
    assert(!linkat(AT_FDCWD,owned,AT_FDCWD,link,0));assert(samosa_whisper_process_run_owned(config,control,link,0,0,&text)==SAMOSA_WHISPER_PREPARE_FAILED&&!text&&!access(owned,F_OK)&&!access(link,F_OK));assert(!unlink(link)&&!unlink(owned));
    assert(!symlink(input,owned));assert(samosa_whisper_process_run_owned(config,control,owned,0,0,&text)==SAMOSA_WHISPER_PREPARE_FAILED&&!text&&!access(input,F_OK));assert(!unlink(owned));
    prepared(owned);assert(!chmod(owned,0644));assert(samosa_whisper_process_run_owned(config,control,owned,0,0,&text)==SAMOSA_WHISPER_OK&&text&&access(owned,F_OK)<0&&errno==ENOENT);free(text);text=NULL;prepared(owned);
    SamosaWhisperProcessConfig missing=*config;missing.engine="/missing-owned-whisper";assert(samosa_whisper_process_run_owned(&missing,control,owned,0,0,&text)==SAMOSA_WHISPER_PROCESS_FAILED&&!text&&access(owned,F_OK)<0&&errno==ENOENT);empty(config->temporary_directory);
    for(int after_result=0;after_result<2;after_result++){prepared(owned);assert(parent_death(config,owned,marker,after_result,1));}
    prepared(owned);(void)unlink(marker);assert(!setenv("SAMOSA_WHISPER_FIXTURE_MODE","stubborn-helper",1));atomic_store(&state->cancel,0);
    Call call={.config=config,.control=control,.input=owned,.own_input=1};pthread_t thread;assert(!pthread_create(&thread,NULL,run,&call));Metadata m=metadata(marker);
    pthread_mutex_lock(&state->mutex);assert(state->pid==m.engine);atomic_store(&state->cancel,1);assert(!kill(m.engine,SIGTERM));pthread_mutex_unlock(&state->mutex);
    assert(!pthread_join(thread,NULL)&&call.result==SAMOSA_WHISPER_CANCELLED&&!call.text&&gone(m.engine)&&gone(m.helper));assert(access(owned,F_OK)<0&&errno==ENOENT);empty(config->temporary_directory);
    assert(!unsetenv("SAMOSA_WHISPER_EXPECT_OWNED"));
}

static void escaped_paths(const char *root,const char *engine,const char *model,const char *input) {
    char deep[PATH_MAX],levels[4][PATH_MAX];assert(snprintf(deep,sizeof(deep),"%s",root)<(int)sizeof(deep));
    for(int i=0;i<4;i++){size_t n=strlen(deep);assert(n+182<sizeof(deep));deep[n++]='/';memset(deep+n,1,180);deep[n+180]=(char)('a'+i);deep[n+181]=0;assert(!mkdir(deep,0700));strcpy(levels[i],deep);}
    char exe[PATH_MAX],weights[PATH_MAX],audio[PATH_MAX],temporary[PATH_MAX];
    assert(snprintf(exe,sizeof(exe),"%s/engine",deep)<(int)sizeof(exe)&&snprintf(weights,sizeof(weights),"%s/model",deep)<(int)sizeof(weights)&&snprintf(audio,sizeof(audio),"%s/audio",deep)<(int)sizeof(audio)&&snprintf(temporary,sizeof(temporary),"%s/results",deep)<(int)sizeof(temporary));
    assert(!symlink(engine,exe)&&!symlink(model,weights)&&!symlink(input,audio));assert(!setenv("SAMOSA_WHISPER_EXPECTED_MODEL",weights,1)&&!setenv("SAMOSA_WHISPER_EXPECTED_INPUT",audio,1)&&!setenv("SAMOSA_WHISPER_FIXTURE_MODE","noisy",1));
    SamosaWhisperProcessConfig config={exe,weights,temporary};char *text=NULL;
    // Four paths containing720 escaped control bytes exceed the old16-KiB JSON cap.
    assert(4*720*6>16384);assert(samosa_whisper_process_run(&config,NULL,audio,0,0,&text)==SAMOSA_WHISPER_OK&&!strcmp(text,"Hello café 🥟"));free(text);empty(temporary);
    assert(!unlink(exe)&&!unlink(weights)&&!unlink(audio)&&!rmdir(temporary));for(int i=3;i>=0;i--)assert(!rmdir(levels[i]));
    assert(!setenv("SAMOSA_WHISPER_EXPECTED_MODEL",model,1)&&!setenv("SAMOSA_WHISPER_EXPECTED_INPUT",input,1));
}

int main(int argc,char **argv) {
#ifndef SAMOSA_WHISPER_LEGACY
    int dispatched=samosa_whisper_process_dispatch(argc,argv);if(dispatched>=0)return dispatched;
#endif
    if(argc>1&&!strcmp(argv[1],"-m"))return fixture(argc,argv);
    assert((argc==2||argc==3)&&argv[1][0]=='/');alarm(120);
    char root[]="/tmp/samosa-whisper-process-XXXXXX";assert(mkdtemp(root));
    char model[1024],input[1024],temporary[1024],marker[1024];
    snprintf(model,sizeof(model),"%s/model with 'quotes' $(literal).bin",root);
    snprintf(input,sizeof(input),"%s/audio with 'quotes' $(literal).wav",root);
    snprintf(temporary,sizeof(temporary),"%s/results with spaces",root);snprintf(marker,sizeof(marker),"%s/marker",root);
    write_file(model,"controlled model fixture");int fd=open(input,O_WRONLY|O_CREAT,0600);assert(fd>=0);
    assert(!ftruncate(fd,44+271*32000)&&!close(fd));
    assert(!setenv("SAMOSA_WHISPER_EXPECTED_MODEL",model,1)&&!setenv("SAMOSA_WHISPER_EXPECTED_INPUT",input,1));
    assert(!setenv("SAMOSA_WHISPER_FIXTURE_MARKER",marker,1));
    SamosaWhisperProcessConfig config={argv[1],model,temporary};State state={.temporary=temporary};
    assert(!pthread_mutex_init(&state.mutex,NULL));atomic_init(&state.cancel,0);
    SamosaWhisperProcessControl control={acquire,started,reaped,release,cancelled,&state};char *text=NULL;
    if(argc==3){assert(!strcmp(argv[2],"--owner-death-only"));int ok=parent_death(&config,input,marker,0,0);assert(!pthread_mutex_destroy(&state.mutex));assert(!unlink(model)&&!unlink(input)&&!unlink(marker)&&!rmdir(temporary)&&!rmdir(root));if(ok)puts("PASS actual CLI owner death: primary/helper/result gone; unrelated process and borrowed input/model survive");return ok?0:1;}
    assert(!setenv("SAMOSA_WHISPER_FIXTURE_MODE","valid",1));
    for(int srt=0;srt<2;srt++) {
        assert(samosa_whisper_process_run(&config,&control,input,srt,0,&text)==SAMOSA_WHISPER_OK);
        assert(!strcmp(text,srt?"1\n00:00:00,000 --> 00:04:31,000\nLong input: café 🥟":"Hello café 🥟"));free(text);text=NULL;
        empty(temporary);assert(!state.active&&!state.pid&&state.starts==state.reaps);
    }
    for(int i=0;i<3;i++) {
        state.installing=i==0;state.active=i==1;state.attachment=i==2;int before=state.releases;
        assert(samosa_whisper_process_run(&config,&control,input,0,0,&text)==SAMOSA_WHISPER_BUSY&&!text&&state.releases==before);
    }
    state.installing=state.active=0;state.attachment=1;
    assert(samosa_whisper_process_run(&config,&control,input,0,1,&text)==SAMOSA_WHISPER_OK);free(text);text=NULL;state.attachment=0;
    atomic_store(&state.cancel,1);int starts=state.starts;
    assert(samosa_whisper_process_run(&config,&control,input,0,1,&text)==SAMOSA_WHISPER_CANCELLED&&!text&&state.starts==starts);empty(temporary);
    assert(samosa_whisper_process_run(&config,&control,input,0,0,&text)==SAMOSA_WHISPER_OK);free(text);text=NULL;
    const char *modes[]={"invalid","control","blank","missing","oversized","crash"};
    for(size_t i=0;i<sizeof(modes)/sizeof(*modes);i++) {
        assert(!setenv("SAMOSA_WHISPER_FIXTURE_MODE",modes[i],1));
        assert(samosa_whisper_process_run(&config,&control,input,0,0,&text)==(i==5?SAMOSA_WHISPER_PROCESS_FAILED:SAMOSA_WHISPER_OUTPUT_INVALID)&&!text);
        empty(temporary);assert(!state.active&&!state.pid&&state.starts==state.reaps);
    }
    assert(!setenv("SAMOSA_WHISPER_FIXTURE_MODE","large",1));
    assert(samosa_whisper_process_run(&config,&control,input,0,0,&text)==SAMOSA_WHISPER_OK&&strlen(text)==16777216);free(text);text=NULL;empty(temporary);
    SamosaWhisperProcessConfig bad=config;bad.engine="/nonexistent/samosa-whisper";
    assert(samosa_whisper_process_run(&bad,&control,input,0,0,&text)==SAMOSA_WHISPER_PROCESS_FAILED&&!text);empty(temporary);
    bad=config;bad.temporary_directory=model;
    assert(samosa_whisper_process_run(&bad,&control,input,0,0,&text)==SAMOSA_WHISPER_PREPARE_FAILED&&!text&&!state.active);
    assert(!setenv("SAMOSA_WHISPER_FIXTURE_MODE","valid",1));state.cancel_on_start=1;
    assert(samosa_whisper_process_run(&config,&control,input,0,0,&text)==SAMOSA_WHISPER_CANCELLED&&!text&&!state.active);state.cancel_on_start=0;empty(temporary);
    assert(!setenv("SAMOSA_WHISPER_FIXTURE_MODE","block",1));atomic_store(&state.cancel,0);
    pid_t peer=fork();assert(peer>=0);if(!peer){for(;;)pause();}
    cleanup_peer=peer;Call call={.config=&config,.control=&control,.input=input};pthread_t thread;assert(!pthread_create(&thread,NULL,run,&call));
    for(int i=0;i<500&&access(marker,F_OK);i++){struct timespec t={0,10000000};nanosleep(&t,NULL);}assert(!access(marker,F_OK));
    pthread_mutex_lock(&state.mutex);pid_t child=state.pid;assert(child>1&&child!=peer);atomic_store(&state.cancel,1);assert(!kill(child,SIGTERM));pthread_mutex_unlock(&state.mutex);
    assert(!pthread_join(thread,NULL)&&call.result==SAMOSA_WHISPER_CANCELLED&&!call.text&&!state.active&&!state.pid);
    errno=0;assert(kill(child,0)<0&&errno==ESRCH);assert(!kill(peer,0));empty(temporary);
    assert(!kill(peer,SIGKILL)&&waitpid(peer,NULL,0)==peer);cleanup_peer=0;assert(state.starts==state.reaps);
    lifecycle(&config,&control,&state,input,marker);owned_cases(&config,&control,&state,input,marker,root);escaped_paths(root,argv[1],model,input);
    char cwd[PATH_MAX],relative_engine[PATH_MAX];assert(getcwd(cwd,sizeof(cwd))&&snprintf(relative_engine,sizeof(relative_engine),"%s/local engine",root)<(int)sizeof(relative_engine));assert(!symlink(argv[1],relative_engine)&&!chdir(root));
    SamosaWhisperProcessConfig relative={"./local engine",strrchr(model,'/')+1,"results with spaces"};const char *relative_input=strrchr(input,'/')+1;
    assert(!setenv("SAMOSA_WHISPER_EXPECTED_MODEL",relative.model,1)&&!setenv("SAMOSA_WHISPER_EXPECTED_INPUT",relative_input,1)&&!setenv("SAMOSA_WHISPER_FIXTURE_MODE","valid",1));
    assert(samosa_whisper_process_run(&relative,NULL,relative_input,0,0,&text)==SAMOSA_WHISPER_OK);free(text);text=NULL;assert(!chdir(cwd)&&!unlink(relative_engine));empty(temporary);
    assert(!pthread_mutex_destroy(&state.mutex));assert(!unlink(model)&&!unlink(input)&&!unlink(marker)&&!rmdir(temporary)&&!rmdir(root));
    printf("PASS extracted Whisper runner (%u checks): original txt/SRT/argv,271-second input, UTF-8/16-MiB bounds, busy/preserve-cancel, failure, exact-child cancellation/reap and complete result cleanup; guardian parent-death, descendant/foreign-process safety, held-PID/workspace cleanup, stalled guardian, relative/escaped paths and discarded verbose logs\n",atomic_load(&checks));return 0;
}
