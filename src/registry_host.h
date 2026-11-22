#ifndef SAMOSA_REGISTRY_HOST_H
#define SAMOSA_REGISTRY_HOST_H
#include "registry.h"
#include "embedding.h"
#include "summary.h"
#include "whisper.h"
#include "whisper_host.h"
#include "vision.h"
#include "temporal.h"
#include "speech.h"
#include "decision.h"
#include "managed_host.h"
#include "summary_host.h"
#include "speech_host.h"
#include "decision_host.h"
#include "visual_host.h"
#include <pthread.h>
typedef struct {
    InferenceRegistry registry;
    InferenceConfig providers[INFERENCE_PROVIDERS_MAX];
    EmbeddingConfig embeddings[INFERENCE_PROVIDERS_MAX];
    SummaryConfig summaries[INFERENCE_PROVIDERS_MAX];
    WhisperConfig transcriptions[INFERENCE_PROVIDERS_MAX];
    VisionConfig images[INFERENCE_PROVIDERS_MAX];
    TemporalConfig videos[INFERENCE_PROVIDERS_MAX];
    DecisionConfig decisions[INFERENCE_PROVIDERS_MAX];
    SpeechConfig speeches[INFERENCE_PROVIDERS_MAX];
    char *strings[256];size_t string_count;
    char keys[INFERENCE_PROVIDERS_MAX][130];
    ManagedHost *managed[INFERENCE_PROVIDERS_MAX];
    SummaryHost *resident_summaries[INFERENCE_PROVIDERS_MAX];
    WhisperHost *on_demand_whispers[INFERENCE_PROVIDERS_MAX];
    SpeechHost *resident_speeches[INFERENCE_PROVIDERS_MAX];
    DecisionHost *resident_decisions[INFERENCE_PROVIDERS_MAX];
    VisualHost *resident_visuals[INFERENCE_PROVIDERS_MAX];
    const char *self;
    pthread_mutex_t gate;
    int gate_initialized,users[INFERENCE_PROVIDERS_MAX];
    int (*cancelled)(void *);void *cancellation;
} InferenceRegistryHost;
/* Reads one bounded private regular config. All strings remain owned by host
 * until free, including credentials. Does not install or start an engine. */
const char *inference_registry_host_load(const char *,const char *,InferenceRegistryHost *);
void inference_registry_host_free(InferenceRegistryHost *);
/* Join request users before close. Unconfirmed shutdown retains ownership;
 * confirmed terminal engine errors are reported after releasing storage. */
const char *inference_registry_host_close(InferenceRegistryHost *);
int inference_registry_host_dispatch(void *,const char *,size_t,FILE *);
#endif
