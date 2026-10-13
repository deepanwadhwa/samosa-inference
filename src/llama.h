#ifndef SAMOSA_LLAMA_H
#define SAMOSA_LLAMA_H
#include <stddef.h>
typedef struct WhisperConfig WhisperConfig;
typedef struct VisionConfig VisionConfig;
typedef struct SummaryConfig SummaryConfig;
typedef struct EmbeddingConfig EmbeddingConfig;
typedef struct TemporalConfig TemporalConfig;
typedef struct { const char *endpoint,*provider_id,*admission_state;size_t context_tokens;const char *model_alias,*owner;
    size_t queue_capacity,admission_wait_ms;
    int (*cancelled)(void *);void *cancellation;
    const char *api_key;size_t transfer_timeout_ms;
    /* Trusted owning supervisor observation; never populated by public JSON. */
    const char *(*owned_status)(void *);void *owned_context;
    double deadline_monotonic;
    const WhisperConfig *whisper;
    const VisionConfig *vision;
    const SummaryConfig *summary;
    const EmbeddingConfig *embedding;
    const TemporalConfig *temporal;
} InferenceConfig;
const char *llama_embed(const InferenceConfig *,const char *,size_t,float **,size_t *);
const char *llama_count(const InferenceConfig *,const char *,size_t *);
const char *llama_properties(const InferenceConfig *);
const char *llama_generate(const InferenceConfig *,const char *,size_t,char **,size_t *,size_t *,size_t *);
const char *llama_admit(const char *,int *);
#endif
