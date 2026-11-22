#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "whisper_host.h"
#include "artifact.h"
#include "engine_runtime.h"
#include "budget.h"
#include "platform.h"
#include "sha256.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

static const char *owned(WhisperHost *h) {
    const WhisperConfig *w=h->inference.whisper;
    const char *error=activation_owned_status(&h->activation,h->state,w->executable,w->model,&h->engine_before,&h->model_before);
    if(!error&&!activation_input_unchanged(h->audio_fd,h->audio,&h->audio_before))error="provider_qualification_input_changed";
    return error;
}
const char *whisper_host_status(void *context) {
    WhisperHost *h=context;
    if(!h||!h->initialized)return "provider_unavailable";
    if(atomic_load(&h->activation.cleanup_failed))return "engine_guardian_failed";
    if(!atomic_load(&h->qualified))return "provider_unavailable";
    return owned(h);
}
const char *whisper_host_init(WhisperHost *h,const InferenceConfig *cfg,size_t engine_size,size_t model_size,
                              const char *audio,const char *audio_sha,size_t audio_size,const char *expected) {
    memset(h,0,sizeof(*h));atomic_init(&h->qualified,0);h->audio_fd=-1;
    h->activation=(Activation){.process={.guardian=-1,.control=-1,.response=-1},.root=-1,.lease=-1,.engine_fd=-1,.model_fd=-1};
    if(!cfg||!cfg->whisper||!cfg->admission_state||engine_size<64||engine_size>67108864||!model_size||model_size>536870912||
       !audio||audio[0]!='/'||!audio_sha||strlen(audio_sha)!=64||strspn(audio_sha,"0123456789abcdef")!=64||
       audio_size<76||audio_size>960044||!expected||!*expected||strlen(expected)>128||
       snprintf(h->state,sizeof(h->state),"%s/activation",cfg->admission_state)>=(int)sizeof(h->state))return "invalid_whisper_host_configuration";
    h->inference=*cfg;h->engine_size=engine_size;h->model_size=model_size;
    h->audio=audio;h->audio_sha=audio_sha;h->audio_size=audio_size;h->expected=expected;h->initialized=1;
    return NULL;
}
static const char *verify_runtime(void *context,int engine,int model) {
    (void)model;WhisperHost *h=context;const char *error=engine_system_runtime(engine);
    if(error)return error;
    h->audio_fd=open(h->audio,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    struct stat before;
    if(h->audio_fd<0)return "qualification_audio_unavailable";
    if(fstat(h->audio_fd,&before)||!S_ISREG(before.st_mode)||before.st_uid!=geteuid()||before.st_nlink!=1||(before.st_mode&022))return "unsafe_qualification_audio";
    error=artifact_verify(h->audio_fd,h->audio_size,960044,h->audio_sha);
    if(!error){h->audio_before=before;if(!activation_input_unchanged(h->audio_fd,h->audio,&h->audio_before))error="provider_qualification_input_changed";}
    return error;
}
static const char *qualify(void *context) {
    WhisperHost *h=context;
    if(fstat(h->activation.engine_fd,&h->engine_before)||fstat(h->activation.model_fd,&h->model_before))return "provider_artifact_changed";
    const char *error=owned(h);unsigned char *audio=error?NULL:malloc(h->audio_size);
    if(!error&&!audio)error="resource_limit";
    size_t done=0;
    while(!error&&done<h->audio_size){ssize_t n=pread(h->audio_fd,audio+done,h->audio_size-done,(off_t)done);
        if(n<0&&errno==EINTR)continue;if(n<=0)error="qualification_audio_unavailable";else done+=(size_t)n;}
    if(!error&&!whisper_audio_valid(audio,h->audio_size))error="invalid_qualification_audio";
    InferenceConfig probe=h->inference;WhisperConfig executor=*probe.whisper;executor.retain_cleanup=1;probe.whisper=&executor;probe.owned_status=NULL;probe.owned_context=NULL;
    double limit=inference_now()+60;if(probe.deadline_monotonic<=0||probe.deadline_monotonic>limit)probe.deadline_monotonic=limit;
    char *items=NULL,*text=NULL;
    if(!error)error=whisper_run(&probe,audio,h->audio_size,&items,&text);
    if(!error&&(!text||!strstr(text,h->expected)))error="provider_qualification_failed";
    if(!error)error=owned(h);
    if(error&&!strcmp(error,"engine_guardian_failed"))activation_cleanup_failed(&h->activation);
    free(items);free(text);free(audio);return error;
}
const char *whisper_host_stop(WhisperHost *h) {
    if(!h||!h->initialized)return NULL;
    atomic_store(&h->qualified,0);
    if(atomic_load(&h->activation.cleanup_failed))return "engine_guardian_failed";
    const char *error=activation_stop(&h->activation);
    if(error&&!strcmp(error,"engine_shutdown_pending"))return error;
    if(h->audio_fd>=0)close(h->audio_fd);h->audio_fd=-1;h->attempted=0;
    return error;
}
const char *whisper_host_start(WhisperHost *h,const InferenceConfig *cfg) {
    if(!h||!h->initialized)return "invalid_whisper_host_configuration";
    if(h->attempted)return whisper_host_status(h);
    if(!inference_platform_allowed(cfg->platform))return "unsupported_provider_platform";
    h->inference=*cfg;
    if(mkdir(cfg->admission_state,0700)&&errno!=EEXIST)return "unsafe_whisper_state";
    struct stat state;if(lstat(cfg->admission_state,&state)||!S_ISDIR(state.st_mode)||state.st_uid!=geteuid()||(state.st_mode&077))return "unsafe_whisper_state";
    char *bundle=NULL;size_t bytes=0;FILE *out=open_memstream(&bundle,&bytes);if(!out)return "resource_limit";
    fprintf(out,"{\"profile\":\"on-demand-whisper-v1\",\"audio_sha256\":\"%s\",\"audio_size\":%zu,\"expected_text\":",h->audio_sha,h->audio_size);
    json_quote(out,h->expected,strlen(h->expected));fputc('}',out);
    if(fclose(out)){free(bundle);return "resource_limit";}
    char identity[65];hash_bytes(bundle,bytes,identity);const WhisperConfig *w=cfg->whisper;
    ActivationConfig activation={.state=h->state,.self=w->self,.engine=w->executable,.model=w->model,
        .engine_sha=w->executable_sha256,.model_sha=w->model_sha256,.engine_size=h->engine_size,.model_size=h->model_size,
        .byte_ceiling=536870912,.advance_current=1,.on_demand=1,
        .argv_template="[\"@ENGINE@\",\"-m\",\"@MODEL@\",\"-l\",\"en\",\"-osrt\"]",
        .verify_runtime=verify_runtime,.runtime_context=h,.bundle_identity=identity,.bundle_config=bundle,.qualify=qualify,.qualification=h};
    h->attempted=1;const char *error=activation_start(&activation,&h->activation);free(bundle);
    if(!error){atomic_store(&h->qualified,1);error=whisper_host_status(h);}
    if(error){const char *cleanup=whisper_host_stop(h);if(cleanup&&!strcmp(cleanup,"engine_shutdown_pending"))return cleanup;}
    return error;
}
const char *whisper_host_run(void *context,const InferenceConfig *cfg,const unsigned char *audio,size_t bytes,char **items,char **text) {
    WhisperHost *h=context;const char *error=whisper_host_status(h);
    InferenceConfig request=*cfg;WhisperConfig executor=*cfg->whisper;executor.retain_cleanup=1;request.whisper=&executor;
    if(!error)error=whisper_run(&request,audio,bytes,items,text);
    if(!error)error=whisper_host_status(h);
    if(error){
        if(!strcmp(error,"engine_guardian_failed"))activation_cleanup_failed(&h->activation);
        if(strcmp(error,"cancelled")&&strcmp(error,"deadline_exceeded")&&strcmp(error,"busy")&&strcmp(error,"queue_full"))atomic_store(&h->qualified,0);
        free(*items);free(*text);*items=*text=NULL;
    }
    return error;
}
