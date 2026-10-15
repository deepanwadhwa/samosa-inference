#ifndef SAMOSA_INFERENCE_LLAMA_PACKAGE_DESCRIPTOR_H
#define SAMOSA_INFERENCE_LLAMA_PACKAGE_DESCRIPTOR_H
#include "package_index.h"
#include "gguf_metadata.h"
typedef struct {char kind[16],runtime_revision[41],architecture[65];size_t engine,model;} LlamaPackageDescriptor;
/* Trusted pinned text/embedding/T5-summary manifest bytes. Resolves one engine/model against the
 * parsed package index. No commands, URLs, credentials or loader policy fields.
 * This parses declarations; runtime qualification is a separate gate. */
const char *llama_package_descriptor_parse(const char *,size_t,const PackageIndex *,LlamaPackageDescriptor *);
/* Compare observed GGUF metadata with the descriptor's declared architecture.
 * Tensor layout/runtime capability still require separate qualification. */
const char *llama_package_descriptor_model(const LlamaPackageDescriptor *,const GgufMetadata *);
#endif
