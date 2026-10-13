#ifndef SAMOSA_WHISPER_H
#define SAMOSA_WHISPER_H
#include "llama.h"
#include "json.h"
struct WhisperConfig {
    const char *self,*executable,*model,*executable_sha256,*model_sha256;
};
/* Existing whisper.cpp CLI adapter. Paths and hashes are trusted host config. */
int whisper_dispatch(const InferenceConfig *,const Json *,FILE *);
char *whisper_normalized(const InferenceConfig *,const Json *);
#endif
