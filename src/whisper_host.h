#ifndef SAMOSA_WHISPER_HOST_H
#define SAMOSA_WHISPER_HOST_H
#include "activation.h"
#include "whisper.h"
#include <stdatomic.h>
/* Nonresident owner. The host config and borrowed strings survive stop/join.
 * Never copy a live host. No engine/model remains resident between requests. */
typedef struct {
    Activation activation;
    InferenceConfig inference;
    struct stat engine_before,model_before,audio_before;
    const char *audio,*audio_sha,*expected;
    char state[4096];
    size_t engine_size,model_size,audio_size;
    int initialized,attempted,audio_fd;
    atomic_int qualified;
} WhisperHost;
const char *whisper_host_init(WhisperHost *,const InferenceConfig *,size_t,size_t,
                              const char *,const char *,size_t,const char *);
const char *whisper_host_start(WhisperHost *,const InferenceConfig *);
const char *whisper_host_status(void *);
const char *whisper_host_stop(WhisperHost *);
const char *whisper_host_run(void *,const InferenceConfig *,const unsigned char *,size_t,char **,char **);
#endif
