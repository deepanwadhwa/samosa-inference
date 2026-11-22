#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "whisper_activation.h"
#include "whisper_audio.h"
#include "json.h"
#include "artifact.h"
#include "engine_runtime.h"
#include "sha256.h"
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>

static const char *owned(const WhisperActivation *h) {
    const char *error=activation_owned_status(&h->activation,h->state,h->config.engine,h->config.model,&h->inputs.engine,&h->inputs.model);
    if(!error&&!activation_input_unchanged(h->audio_fd,h->config.audio,&h->audio_before))error="provider_qualification_input_changed";
    return error;
}
const char *whisper_activation_status(const WhisperActivation *h) {
    if(!h||!h->initialized)return "provider_unavailable";
    if(atomic_load(&h->activation.cleanup_failed))return "engine_guardian_failed";
    if(!atomic_load(&h->qualified))return "provider_unavailable";
    return owned(h);
}
const char *whisper_activation_init(WhisperActivation *h,const WhisperActivationConfig *config) {
    memset(h,0,sizeof(*h));atomic_init(&h->qualified,0);h->audio_fd=-1;
    h->activation=(Activation){.process={.guardian=-1,.control=-1,.response=-1},.root=-1,.lease=-1,.engine_fd=-1,.model_fd=-1};
    if(!config||!config->state||!config->profile||!config->argv_template||!config->probe||
       config->engine_size<64||config->engine_size>67108864||!config->model_size||config->model_size>536870912||
       !config->audio||config->audio[0]!='/'||!config->audio_sha||strlen(config->audio_sha)!=64||
       strspn(config->audio_sha,"0123456789abcdef")!=64||config->audio_size<76||config->audio_size>WHISPER_WAV_MAX||
       !config->expected||!*config->expected||strlen(config->expected)>128||
       snprintf(h->state,sizeof(h->state),"%s/activation",config->state)>=(int)sizeof(h->state))
        return "invalid_whisper_host_configuration";
    h->config=*config;h->initialized=1;
    return NULL;
}
static const char *verify_runtime(void *context,int engine,int model) {
    (void)model;WhisperActivation *h=context;const char *error=engine_system_runtime(engine);
    if(error)return error;
    h->audio_fd=open(h->config.audio,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);
    struct stat before;
    if(h->audio_fd<0)return "qualification_audio_unavailable";
    if(fstat(h->audio_fd,&before)||!S_ISREG(before.st_mode)||before.st_uid!=geteuid()||before.st_nlink!=1||(before.st_mode&022))return "unsafe_qualification_audio";
    error=artifact_verify(h->audio_fd,h->config.audio_size,WHISPER_WAV_MAX,h->config.audio_sha);
    if(!error){h->audio_before=before;if(!activation_input_unchanged(h->audio_fd,h->config.audio,&h->audio_before))error="provider_qualification_input_changed";}
    return error;
}
static const char *qualify(void *context) {
    WhisperActivation *h=context;
    if(fstat(h->activation.engine_fd,&h->inputs.engine)||fstat(h->activation.model_fd,&h->inputs.model))return "provider_artifact_changed";
    const char *error=owned(h);unsigned char *audio=error?NULL:malloc(h->config.audio_size);
    if(!error&&!audio)error="resource_limit";
    size_t done=0;
    while(!error&&done<h->config.audio_size){ssize_t n=pread(h->audio_fd,audio+done,h->config.audio_size-done,(off_t)done);
        if(n<0&&errno==EINTR)continue;if(n<=0)error="qualification_audio_unavailable";else done+=(size_t)n;}
    if(!error&&!whisper_canonical_wav(audio,h->config.audio_size))error="invalid_qualification_audio";
    if(!error)error=owned(h);
    char *text=NULL;
    if(!error)error=h->config.probe(h->config.context,audio,h->config.audio_size,&text);
    if(!error&&(!text||!strstr(text,h->config.expected)))error="provider_qualification_failed";
    if(!error)error=owned(h);
    if(error&&!strcmp(error,"engine_guardian_failed"))activation_cleanup_failed(&h->activation);
    free(text);free(audio);return error;
}
const char *whisper_activation_stop(WhisperActivation *h) {
    if(!h||!h->initialized)return NULL;
    atomic_store(&h->qualified,0);
    if(atomic_load(&h->activation.cleanup_failed))return "engine_guardian_failed";
    const char *error=activation_stop(&h->activation);
    if(error&&!strcmp(error,"engine_shutdown_pending"))return error;
    if(h->audio_fd>=0)close(h->audio_fd);h->audio_fd=-1;h->attempted=0;
    return error;
}
const char *whisper_activation_start(WhisperActivation *h) {
    if(!h||!h->initialized)return "invalid_whisper_host_configuration";
    if(h->attempted)return whisper_activation_status(h);
    const WhisperActivationConfig *config=&h->config;
    if(mkdir(config->state,0700)&&errno!=EEXIST)return "unsafe_whisper_state";
    struct stat state;if(lstat(config->state,&state)||!S_ISDIR(state.st_mode)||state.st_uid!=geteuid()||(state.st_mode&077))return "unsafe_whisper_state";
    char *bundle=NULL;size_t bytes=0;FILE *out=open_memstream(&bundle,&bytes);if(!out)return "resource_limit";
    fputs("{\"profile\":",out);json_quote(out,config->profile,strlen(config->profile));
    fprintf(out,",\"audio_sha256\":\"%s\",\"audio_size\":%zu,\"expected_text\":",config->audio_sha,config->audio_size);
    json_quote(out,h->config.expected,strlen(h->config.expected));fputc('}',out);
    if(fclose(out)){free(bundle);return "resource_limit";}
    char identity[65];hash_bytes(bundle,bytes,identity);
    ActivationConfig activation={.state=h->state,.self=config->self,.engine=h->config.engine,.model=h->config.model,
        .engine_sha=config->engine_sha,.model_sha=config->model_sha,.engine_size=h->config.engine_size,.model_size=h->config.model_size,
        .byte_ceiling=536870912,.advance_current=1,.on_demand=1,
        .argv_template=config->argv_template,
        .verify_runtime=verify_runtime,.runtime_context=h,.bundle_identity=identity,.bundle_config=bundle,.qualify=qualify,.qualification=h};
    h->attempted=1;const char *error=activation_start(&activation,&h->activation);free(bundle);
    if(!error){atomic_store(&h->qualified,1);error=whisper_activation_status(h);}
    if(error){const char *cleanup=whisper_activation_stop(h);if(cleanup&&!strcmp(cleanup,"engine_shutdown_pending"))return cleanup;}
    return error;
}
