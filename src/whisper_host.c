#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "whisper_host.h"
#include "budget.h"
#include "platform.h"

const char *whisper_host_status(void *context) {
    WhisperHost *h=context;
    return whisper_activation_status(h?&h->lifecycle:NULL);
}

static const char *structured_probe(void *context, const unsigned char *audio, size_t bytes, char **text) {
    WhisperHost *h=context;
    InferenceConfig probe=h->inference;
    WhisperConfig executor=*probe.whisper;
    executor.retain_cleanup=1;
    probe.whisper=&executor;
    probe.owned_status=NULL;
    probe.owned_context=NULL;
    double limit=inference_now()+60;
    if (probe.deadline_monotonic<=0 || probe.deadline_monotonic>limit) probe.deadline_monotonic=limit;
    char *items=NULL;
    const char *error=whisper_run(&probe,audio,bytes,&items,text);
    free(items);
    return error;
}

const char *whisper_host_init(WhisperHost *h, const InferenceConfig *cfg, size_t engine_size, size_t model_size,
                             const char *audio, const char *audio_sha, size_t audio_size, const char *expected) {
    if (!cfg || !cfg->whisper) return whisper_activation_init(&h->lifecycle,NULL);
    const WhisperConfig *w=cfg->whisper;
    h->inference=*cfg;
    WhisperActivationConfig config={.state=cfg->admission_state,.self=w->self,.profile="on-demand-whisper-v1",
        .engine=w->executable,.engine_sha=w->executable_sha256,.engine_size=engine_size,
        .model=w->model,.model_sha=w->model_sha256,.model_size=model_size,
        .audio=audio,.audio_sha=audio_sha,.audio_size=audio_size,.expected=expected,
        .argv_template="[\"@ENGINE@\",\"-m\",\"@MODEL@\",\"-l\",\"en\",\"-osrt\"]",
        .probe=structured_probe,.context=h};
    return whisper_activation_init(&h->lifecycle,&config);
}

const char *whisper_host_start(WhisperHost *h, const InferenceConfig *cfg) {
    if (!h || !h->lifecycle.initialized) return "invalid_whisper_host_configuration";
    if (h->lifecycle.attempted) return whisper_host_status(h);
    if (!inference_platform_allowed(cfg->platform)) return "unsupported_provider_platform";
    h->inference=*cfg;
    return whisper_activation_start(&h->lifecycle);
}

const char *whisper_host_stop(WhisperHost *h) {
    return whisper_activation_stop(h?&h->lifecycle:NULL);
}

const char *whisper_host_run(void *context,const InferenceConfig *cfg,const unsigned char *audio,size_t bytes,char **items,char **text) {
    WhisperHost *h=context;const char *error=whisper_host_status(h);
    InferenceConfig request=*cfg;WhisperConfig executor=*cfg->whisper;executor.retain_cleanup=1;request.whisper=&executor;
    if(!error)error=whisper_run(&request,audio,bytes,items,text);
    if(!error)error=whisper_host_status(h);
    if(error){
        if(!strcmp(error,"engine_guardian_failed"))activation_cleanup_failed(&h->lifecycle.activation);
        if(strcmp(error,"cancelled")&&strcmp(error,"deadline_exceeded")&&strcmp(error,"busy")&&strcmp(error,"queue_full"))atomic_store(&h->lifecycle.qualified,0);
        free(*items);free(*text);*items=*text=NULL;
    }
    return error;
}
