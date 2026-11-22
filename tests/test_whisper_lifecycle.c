#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#define SAMOSA_WHISPER_FIXTURE_ATOMIC
#include <samosa_inference/whisper_lifecycle.h>
#include "activation.h"
#include "whisper_fixture.h"
#include <dirent.h>
#include <pthread.h>

typedef struct {
    SamosaWhisperLifecycle *host;
    const char *input;
    atomic_int started;
    int result;
    pid_t engine;
    char *text;
} Call;

static int acquire(void *context, int preserve_cancel) {
    (void)context;
    (void)preserve_cancel;
    return 1;
}
static void started(void *context, pid_t engine) {
    Call *call=context;
    call->engine=engine;
    atomic_store(&call->started,1);
    const char *status;
    do {
        delay();
        status=samosa_whisper_lifecycle_status(call->host);
    } while (!status || strcmp(status,"provider_stopping"));
}
static void reaped(void *context, pid_t engine) {
    CHECK(((Call*)context)->engine==engine);
    CHECK(!kill(engine,0));
}
static void release(void *context) {
    Call *call=context;
    errno=0;
    CHECK(kill(call->engine,0)<0 && errno==ESRCH);
}
static int cancelled(void *context) {
    (void)context;
    return 0;
}
static void *run(void *context) {
    Call *call=context;
    SamosaWhisperProcessControl control={acquire,started,reaped,release,cancelled,call};
    call->result=samosa_whisper_lifecycle_run(call->host,NULL,&control,call->input,0,0,0,&call->text);
    return NULL;
}

static char *last_good(const char *state) {
    char activation[4096];
    CHECK(snprintf(activation,sizeof(activation),"%s/activation",state)<(int)sizeof(activation));
    char *body=NULL;
    size_t size=0;
    FILE *out=open_memstream(&body,&size);
    CHECK(out && !activation_get(activation,out) && !fclose(out));
    CHECK(strstr(body,"\"live_readiness\":\"unobserved\"") &&
          strstr(body,"on-demand-whisper-long-window-v1"));
    return body;
}

int main(int argc, char **argv) {
    int dispatched=samosa_whisper_process_dispatch(argc,argv);
    if (dispatched>=0) return dispatched;
    CHECK(argc==5 && argv[1][0]=='/' && argv[2][0]=='/' && argv[3][0]=='/' && argv[4][0]=='/');
    alarm(180);
    char root[]="/tmp/samosa-whisper-lifecycle-XXXXXX";
    CHECK(mkdtemp(root));
    char audio[4096], state[4096], temporary[4096];
    CHECK(snprintf(audio,sizeof(audio),"%s/probe.wav",root)<(int)sizeof(audio));
    CHECK(snprintf(state,sizeof(state),"%s/state",root)<(int)sizeof(state));
    CHECK(snprintf(temporary,sizeof(temporary),"%s/tmp",root)<(int)sizeof(temporary));
    file_copy(argv[3],audio);
    CHECK(!chmod(audio,0600));
    char engine_sha[65], model_sha[65], audio_sha[65];
    file_sha(argv[1],engine_sha);
    file_sha(argv[2],model_sha);
    file_sha(audio,audio_sha);
    struct stat engine_stat, model_stat, audio_stat;
    CHECK(!stat(argv[1],&engine_stat) && !stat(argv[2],&model_stat) && !stat(audio,&audio_stat));
    SamosaWhisperLifecycleConfig config={.process={argv[1],argv[2],temporary},.state=state,
        .engine_sha256=engine_sha,.model_sha256=model_sha,.engine_size=(size_t)engine_stat.st_size,
        .model_size=(size_t)model_stat.st_size,.qualification_wav=audio,.qualification_sha256=audio_sha,
        .qualification_size=(size_t)audio_stat.st_size,.expected_text="quick brown fox"};
    SamosaWhisperLifecycle *host=NULL;
    CHECK(!samosa_whisper_lifecycle_create(&config,&host));
    CHECK(samosa_whisper_lifecycle_status(host));
    char *text=NULL;
    CHECK(samosa_whisper_lifecycle_run(host,NULL,NULL,argv[4],0,0,0,&text)==SAMOSA_WHISPER_NOT_READY && !text);
    const char *error=samosa_whisper_lifecycle_start(host);
    if (error) fprintf(stderr,"Whisper qualification failed: %s\n",error);
    CHECK(!error && !samosa_whisper_lifecycle_status(host));
    errno=0;
    CHECK(waitpid(-1,NULL,WNOHANG)==-1 && errno==ECHILD);
    char *first=last_good(state);
    CHECK(!samosa_whisper_lifecycle_start(host));
    char *same=last_good(state);
    CHECK(!strcmp(first,same));
    free(same);
    SamosaWhisperLifecycle *other=NULL;
    CHECK(!samosa_whisper_lifecycle_create(&config,&other));
    error=samosa_whisper_lifecycle_start(other);
    CHECK(error && !strcmp(error,"activation_busy"));
    CHECK(!samosa_whisper_lifecycle_destroy(other));
    for (int srt=0;srt<2;srt++) {
        CHECK(samosa_whisper_lifecycle_run(host,NULL,NULL,argv[4],srt,0,0,&text)==SAMOSA_WHISPER_OK);
        CHECK(strstr(text,"silver compass") || strstr(text,"Silver compass"));
        if (srt) CHECK(strstr(text,"00:00:30,000 --> "));
        free(text);
        text=NULL;
    }
    Call call={.host=host,.input=argv[4]};
    pthread_t thread;
    CHECK(!pthread_create(&thread,NULL,run,&call));
    for (int i=0;i<500 && !atomic_load(&call.started);i++) delay();
    CHECK(atomic_load(&call.started));
    CHECK(samosa_whisper_lifecycle_run(host,NULL,NULL,argv[4],0,0,0,&text)==SAMOSA_WHISPER_BUSY && !text);
    CHECK(!samosa_whisper_lifecycle_stop(host));
    CHECK(!pthread_join(thread,NULL) && call.result==SAMOSA_WHISPER_CANCELLED && !call.text);
    CHECK(samosa_whisper_lifecycle_status(host));
    CHECK(!samosa_whisper_lifecycle_destroy(host));

    config.expected_text="a different expected phrase";
    CHECK(!samosa_whisper_lifecycle_create(&config,&host));
    error=samosa_whisper_lifecycle_start(host);
    CHECK(error && !strcmp(error,"provider_qualification_failed"));
    same=last_good(state);
    CHECK(!strcmp(first,same));
    free(first);
    free(same);
    CHECK(!samosa_whisper_lifecycle_destroy(host));

    config.expected_text="quick brown fox";
    CHECK(!samosa_whisper_lifecycle_create(&config,&host));
    CHECK(!samosa_whisper_lifecycle_start(host));
    CHECK(!chmod(audio,0400));
    error=samosa_whisper_lifecycle_status(host);
    CHECK(error && !strcmp(error,"provider_qualification_input_changed"));
    CHECK(samosa_whisper_lifecycle_run(host,NULL,NULL,argv[4],0,0,0,&text)==SAMOSA_WHISPER_NOT_READY && !text);
    CHECK(!samosa_whisper_lifecycle_destroy(host));
    DIR *dir=opendir(temporary);
    CHECK(dir);
    struct dirent *entry;
    while ((entry=readdir(dir))) CHECK(!strcmp(entry->d_name,".") || !strcmp(entry->d_name,".."));
    CHECK(!closedir(dir));
    cleanup(root);
    printf("PASS actual long-window Whisper lifecycle (%u checks): bounded speech, unchanged last-good, text/SRT beyond 30s, active stop, lease, restart and mutation\n",atomic_load(&checks));
    return 0;
}
