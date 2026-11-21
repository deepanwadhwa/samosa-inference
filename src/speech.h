#ifndef SAMOSA_INFERENCE_SPEECH_H
#define SAMOSA_INFERENCE_SPEECH_H
#include "llama.h"
#include "json.h"
#include "speech_runtime.h"
/* Trusted host callback receives provisional PCM and the active operation's
 * control hook. Bounded transports must check that hook during backpressure. */
typedef const char *(*SpeechOutputSink)(void *,const unsigned char *,size_t,int,
    const char *(*)(void *),void *);
struct SpeechConfig {const char *self,*package,*manifest_sha256;
    SpeechOutputSink pcm_sink;void *pcm_context;
    /* Trusted resident transport; public normalization, audio publication and
     * operation receipts stay in the existing speech adapter. */
    const char *(*run)(void *,const InferenceConfig *,const char *,size_t,int,
        SpeechPcmSink,void *,SpeechRuntimeResult *);
    void *context;};
char *speech_normalized(const InferenceConfig *,const Json *);
int speech_dispatch(const InferenceConfig *,const Json *,FILE *);
#endif
