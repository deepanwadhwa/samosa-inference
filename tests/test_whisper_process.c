#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/whisper_process.h>
#include <assert.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
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
    assert(!strcmp(argv[2],getenv("SAMOSA_WHISPER_EXPECTED_MODEL"))&&!strcmp(argv[4],getenv("SAMOSA_WHISPER_EXPECTED_INPUT")));
    struct stat audio;assert(!stat(argv[4],&audio)&&audio.st_size==44+271*32000);
    const char *mode=getenv("SAMOSA_WHISPER_FIXTURE_MODE");assert(mode);
    if(!strcmp(mode,"crash"))return 17;
    if(!strcmp(mode,"missing"))return 0;
    if(!strcmp(mode,"block")) {
        write_file(getenv("SAMOSA_WHISPER_FIXTURE_MARKER"),"ready");for(;;)pause();
    }
    char output[4096];assert(snprintf(output,sizeof(output),"%s.%s",argv[10],!strcmp(argv[8],"-osrt")?"srt":"txt")<(int)sizeof(output));
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
    pid_t pid;atomic_int cancel;
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
    State *s=context;assert(pid==s->pid);errno=0;assert(waitpid(pid,NULL,WNOHANG)==-1&&errno==ECHILD);s->reaps++;
}
static void release(void *context) {
    State *s=context;pthread_mutex_lock(&s->mutex);s->pid=0;s->active=0;s->releases++;pthread_mutex_unlock(&s->mutex);
}
static int cancelled(void *context) {return atomic_load(&((State *)context)->cancel);}
typedef struct {const SamosaWhisperProcessConfig *config;const SamosaWhisperProcessControl *control;const char *input;int result;char *text;} Call;
static void *run(void *context) {
    Call *c=context;c->result=samosa_whisper_process_run(c->config,c->control,c->input,1,1,&c->text);return NULL;
}
int main(int argc,char **argv) {
    if(argc>1&&!strcmp(argv[1],"-m"))return fixture(argc,argv);
    assert(argc==2&&argv[1][0]=='/');alarm(120);
    char root[]="/tmp/samosa-whisper-process-XXXXXX";assert(mkdtemp(root));
    char model[1024],input[1024],temporary[1024],marker[1024];
    snprintf(model,sizeof(model),"%s/model with 'quotes' $(literal).bin",root);
    snprintf(input,sizeof(input),"%s/audio with 'quotes' $(literal).wav",root);
    snprintf(temporary,sizeof(temporary),"%s/results with spaces",root);snprintf(marker,sizeof(marker),"%s/marker",root);
    write_file(model,"controlled model fixture");int fd=open(input,O_WRONLY|O_CREAT,0600);assert(fd>=0);
    assert(!ftruncate(fd,44+271*32000)&&!close(fd));
    assert(!setenv("SAMOSA_WHISPER_EXPECTED_MODEL",model,1)&&!setenv("SAMOSA_WHISPER_EXPECTED_INPUT",input,1));
    assert(!setenv("SAMOSA_WHISPER_FIXTURE_MARKER",marker,1));
    SamosaWhisperProcessConfig config={argv[1],model,temporary};State state={0};
    assert(!pthread_mutex_init(&state.mutex,NULL));atomic_init(&state.cancel,0);
    SamosaWhisperProcessControl control={acquire,started,reaped,release,cancelled,&state};char *text=NULL;
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
    Call call={&config,&control,input,0,NULL};pthread_t thread;assert(!pthread_create(&thread,NULL,run,&call));
    for(int i=0;i<500&&access(marker,F_OK);i++){struct timespec t={0,10000000};nanosleep(&t,NULL);}assert(!access(marker,F_OK));
    pthread_mutex_lock(&state.mutex);pid_t child=state.pid;assert(child>1&&child!=peer);atomic_store(&state.cancel,1);assert(!kill(child,SIGTERM));pthread_mutex_unlock(&state.mutex);
    assert(!pthread_join(thread,NULL)&&call.result==SAMOSA_WHISPER_CANCELLED&&!call.text&&!state.active&&!state.pid);
    errno=0;assert(kill(child,0)<0&&errno==ESRCH);assert(!kill(peer,0));empty(temporary);
    assert(!kill(peer,SIGKILL)&&waitpid(peer,NULL,0)==peer);assert(state.starts==state.reaps);
    assert(!pthread_mutex_destroy(&state.mutex));assert(!unlink(model)&&!unlink(input)&&!unlink(marker)&&!rmdir(temporary)&&!rmdir(root));
    puts("PASS extracted Whisper runner: original txt/SRT/argv,271-second input, UTF-8/16-MiB bounds, busy/preserve-cancel, failure, exact-child cancellation/reap and complete result cleanup");return 0;
}
