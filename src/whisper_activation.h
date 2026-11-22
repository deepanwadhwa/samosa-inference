#ifndef SAMOSA_WHISPER_ACTIVATION_H
#define SAMOSA_WHISPER_ACTIVATION_H
#include "activation.h"
#include <samosa_inference/whisper_process.h>

typedef struct {
    const char *state, *self, *profile, *argv_template;
    const char *engine, *engine_sha, *model, *model_sha;
    const char *audio, *audio_sha, *expected;
    size_t engine_size, model_size, audio_size;
    const char *(*probe)(void *, const unsigned char *, size_t, char **);
    void *context;
} WhisperActivationConfig;

typedef struct {
    WhisperActivationConfig config;
    Activation activation;
    SamosaWhisperProcessIdentity inputs;
    struct stat audio_before;
    char state[4096];
    int initialized, attempted, audio_fd;
    atomic_int qualified;
} WhisperActivation;

const char *whisper_activation_init(WhisperActivation *, const WhisperActivationConfig *);
const char *whisper_activation_start(WhisperActivation *);
const char *whisper_activation_status(const WhisperActivation *);
const char *whisper_activation_stop(WhisperActivation *);
#endif
