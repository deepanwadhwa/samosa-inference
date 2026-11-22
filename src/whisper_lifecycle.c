#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/whisper_lifecycle.h>
#include "whisper_activation.h"
#include "whisper_process_internal.h"
#include "input_identity.h"
#include <limits.h>
#include <pthread.h>
#include <stdlib.h>

struct SamosaWhisperLifecycle {
    WhisperActivation owner;
    SamosaWhisperProcessConfig process;
    char self[PATH_MAX];
    pthread_mutex_t mutex;
    pthread_cond_t idle;
    atomic_int stopping;
    int starting, active;
};

static const char *probe(void *context, const unsigned char *audio, size_t bytes, char **output) {
    SamosaWhisperLifecycle *h=context;
    WhisperProcessAdmission admission={.lease=h->owner.activation.lease,.lifetime=60,
        .probe_audio=audio,.probe_bytes=bytes,.stop_requested=&h->stopping};
    int result=whisper_process_run_admitted(&h->process,&h->owner.inputs,NULL,NULL,0,0,0,&admission,output);
    switch (result) {
        case SAMOSA_WHISPER_OK: return NULL;
        case SAMOSA_WHISPER_GUARDIAN_FAILED: return "engine_guardian_failed";
        case SAMOSA_WHISPER_DEADLINE_EXCEEDED: return "deadline_exceeded";
        case SAMOSA_WHISPER_CANCELLED: return "cancelled";
        case SAMOSA_WHISPER_NOT_READY: return "provider_artifact_changed";
        default: return "provider_qualification_failed";
    }
}

const char *samosa_whisper_lifecycle_create(const SamosaWhisperLifecycleConfig *config, SamosaWhisperLifecycle **out) {
    if (!out) return "invalid_whisper_host_configuration";
    *out=NULL;
    if (!config || !config->process.temporary_directory) return "invalid_whisper_host_configuration";
    SamosaWhisperLifecycle *h=calloc(1,sizeof(*h));
    if (!h) return "resource_limit";
    if (!engine_process_executable(h->self)) {
        free(h);
        return "engine_unavailable";
    }
    h->process=config->process;
    WhisperActivationConfig activation={.state=config->state,.self=h->self,.profile="on-demand-whisper-long-window-v1",
        .engine=h->process.engine,.engine_sha=config->engine_sha256,.engine_size=config->engine_size,
        .model=h->process.model,.model_sha=config->model_sha256,.model_size=config->model_size,
        .audio=config->qualification_wav,.audio_sha=config->qualification_sha256,.audio_size=config->qualification_size,
        .expected=config->expected_text,.argv_template="[\"@ENGINE@\",\"-m\",\"@MODEL@\",\"-l\",\"en\",\"-nt\",\"-otxt\"]",
        .probe=probe,.context=h};
    const char *error=whisper_activation_init(&h->owner,&activation);
    if (!error) {
        if (pthread_mutex_init(&h->mutex,NULL)) error="resource_limit";
        else if (pthread_cond_init(&h->idle,NULL)) {
            pthread_mutex_destroy(&h->mutex);
            error="resource_limit";
        }
    }
    if (error) {
        free(h);
        return error;
    }
    atomic_init(&h->stopping,0);
    *out=h;
    return NULL;
}

const char *samosa_whisper_lifecycle_start(SamosaWhisperLifecycle *h) {
    if (!h) return "invalid_whisper_host_configuration";
    pthread_mutex_lock(&h->mutex);
    if (h->starting || atomic_load(&h->stopping)) {
        pthread_mutex_unlock(&h->mutex);
        return "busy";
    }
    if (h->owner.attempted) {
        const char *error=whisper_activation_status(&h->owner);
        pthread_mutex_unlock(&h->mutex);
        return error;
    }
    h->starting=1;
    pthread_mutex_unlock(&h->mutex);
    const char *error=whisper_activation_start(&h->owner);
    pthread_mutex_lock(&h->mutex);
    if (!error && atomic_load(&h->stopping)) error="cancelled";
    h->starting=0;
    pthread_cond_broadcast(&h->idle);
    pthread_mutex_unlock(&h->mutex);
    return error;
}

const char *samosa_whisper_lifecycle_status(SamosaWhisperLifecycle *h) {
    if (!h) return "provider_unavailable";
    pthread_mutex_lock(&h->mutex);
    const char *error=h->starting?"provider_starting":atomic_load(&h->stopping)?"provider_stopping":
        whisper_activation_status(&h->owner);
    pthread_mutex_unlock(&h->mutex);
    return error;
}

const char *samosa_whisper_lifecycle_stop(SamosaWhisperLifecycle *h) {
    if (!h) return NULL;
    pthread_mutex_lock(&h->mutex);
    if (atomic_exchange(&h->stopping,1)) {
        pthread_mutex_unlock(&h->mutex);
        return "busy";
    }
    while (h->starting || h->active) pthread_cond_wait(&h->idle,&h->mutex);
    const char *error=whisper_activation_stop(&h->owner);
    atomic_store(&h->stopping,0);
    pthread_mutex_unlock(&h->mutex);
    return error;
}

const char *samosa_whisper_lifecycle_destroy(SamosaWhisperLifecycle *h) {
    const char *error=samosa_whisper_lifecycle_stop(h);
    if (error || !h) return error;
    pthread_cond_destroy(&h->idle);
    pthread_mutex_destroy(&h->mutex);
    free(h);
    return NULL;
}

int samosa_whisper_lifecycle_run(SamosaWhisperLifecycle *h, const SamosaWhisperProcessIdentity *expected,
    const SamosaWhisperProcessControl *control, const char *wav_path, int timestamped,
    int preserve_cancel, int own_input, char **output) {
    if (!output) return SAMOSA_WHISPER_OUTPUT_INVALID;
    *output=NULL;
    if (!h) return SAMOSA_WHISPER_NOT_READY;
    pthread_mutex_lock(&h->mutex);
    int result=SAMOSA_WHISPER_NOT_READY;
    if (h->starting || h->active || atomic_load(&h->stopping)) result=SAMOSA_WHISPER_BUSY;
    else if (!whisper_activation_status(&h->owner) &&
             (!expected || (inference_same_input(&expected->engine,&h->owner.inputs.engine) &&
                            inference_same_input(&expected->model,&h->owner.inputs.model)))) {
        h->active=1;
        result=SAMOSA_WHISPER_OK;
    }
    pthread_mutex_unlock(&h->mutex);
    if (result!=SAMOSA_WHISPER_OK) return result;

    WhisperProcessAdmission admission={.lease=h->owner.activation.lease,.stop_requested=&h->stopping};
    result=whisper_process_run_admitted(&h->process,&h->owner.inputs,control,wav_path,timestamped,
        preserve_cancel,own_input,&admission,output);
    pthread_mutex_lock(&h->mutex);
    if (result==SAMOSA_WHISPER_GUARDIAN_FAILED) activation_cleanup_failed(&h->owner.activation);
    if (result==SAMOSA_WHISPER_PROCESS_FAILED) atomic_store(&h->owner.qualified,0);
    if (result==SAMOSA_WHISPER_OK) {
        if (atomic_load(&h->stopping)) result=SAMOSA_WHISPER_CANCELLED;
        else if (whisper_activation_status(&h->owner)) result=SAMOSA_WHISPER_NOT_READY;
        if (result!=SAMOSA_WHISPER_OK) {
            free(*output);
            *output=NULL;
        }
    }
    h->active=0;
    pthread_cond_broadcast(&h->idle);
    pthread_mutex_unlock(&h->mutex);
    return result;
}
