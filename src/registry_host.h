#ifndef SAMOSA_REGISTRY_HOST_H
#define SAMOSA_REGISTRY_HOST_H
#include "registry.h"
#include "embedding.h"
#include "summary.h"
#include "whisper.h"
#include "vision.h"
#include "temporal.h"
#include "speech.h"
typedef struct {
    InferenceRegistry registry;
    InferenceConfig providers[INFERENCE_PROVIDERS_MAX];
    EmbeddingConfig embeddings[INFERENCE_PROVIDERS_MAX];
    SummaryConfig summaries[INFERENCE_PROVIDERS_MAX];
    WhisperConfig transcriptions[INFERENCE_PROVIDERS_MAX];
    VisionConfig images[INFERENCE_PROVIDERS_MAX];
    TemporalConfig videos[INFERENCE_PROVIDERS_MAX];
    SpeechConfig speeches[INFERENCE_PROVIDERS_MAX];
    char *strings[256];size_t string_count;
    char keys[INFERENCE_PROVIDERS_MAX][130];
} InferenceRegistryHost;
/* Reads one bounded private regular config. All strings remain owned by host
 * until free, including credentials. Does not install or start an engine. */
const char *inference_registry_host_load(const char *,const char *,InferenceRegistryHost *);
void inference_registry_host_free(InferenceRegistryHost *);
#endif
