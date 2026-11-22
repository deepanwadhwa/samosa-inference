#ifndef SAMOSA_WHISPER_H
#define SAMOSA_WHISPER_H
#include "llama.h"
#include "json.h"
struct WhisperConfig {
    const char *self,*executable,*model,*executable_sha256,*model_sha256;
    const char *(*run)(void *,const InferenceConfig *,const unsigned char *,size_t,char **,char **);
    void *context;
    /* Managed synchronous callers retain scope until guardian cleanup. */
    int retain_cleanup;
};
/* Existing whisper.cpp CLI adapter. Paths and hashes are trusted host config. */
/* Existing guarded request body; trusted owning adapters may wrap it. */
int whisper_audio_valid(const unsigned char *,size_t);
const char *whisper_run(const InferenceConfig *,const unsigned char *,size_t,char **,char **);
int whisper_dispatch(const InferenceConfig *,const Json *,FILE *);
char *whisper_normalized(const InferenceConfig *,const Json *);
#endif
