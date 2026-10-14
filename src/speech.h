#ifndef SAMOSA_INFERENCE_SPEECH_H
#define SAMOSA_INFERENCE_SPEECH_H
#include "llama.h"
#include "json.h"
/* Trusted host callback receives provisional PCM and the active operation's
 * control hook. Bounded transports must check that hook during backpressure. */
typedef const char *(*SpeechOutputSink)(void *,const unsigned char *,size_t,int,
    const char *(*)(void *),void *);
struct SpeechConfig {const char *self,*package,*manifest_sha256;
    SpeechOutputSink pcm_sink;void *pcm_context;};
char *speech_normalized(const InferenceConfig *,const Json *);
int speech_dispatch(const InferenceConfig *,const Json *,FILE *);
#endif
