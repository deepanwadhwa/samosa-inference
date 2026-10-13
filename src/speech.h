#ifndef SAMOSA_INFERENCE_SPEECH_H
#define SAMOSA_INFERENCE_SPEECH_H
#include "llama.h"
#include "json.h"
struct SpeechConfig {const char *self,*package,*manifest_sha256;};
char *speech_normalized(const InferenceConfig *,const Json *);
int speech_dispatch(const InferenceConfig *,const Json *,FILE *);
#endif
