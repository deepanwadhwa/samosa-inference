#ifndef SAMOSA_INFERENCE_EMBEDDING_H
#define SAMOSA_INFERENCE_EMBEDDING_H
#include "llama.h"
#include "json.h"
struct EmbeddingConfig {
 const char *model_id,*model_revision,*model_sha256,*engine_sha256,*tokenizer_hash;
 const char *query_prefix,*document_prefix,*recipe;
 size_t dimensions;
};
int embedding_config_load(const char *,EmbeddingConfig *);
void embedding_config_free(EmbeddingConfig *);
char *embedding_normalized(const InferenceConfig *,const Json *);
int embedding_dispatch(const InferenceConfig *,const Json *,FILE *);
int embedding_describe(const InferenceConfig *,const Json *,FILE *);
#endif
