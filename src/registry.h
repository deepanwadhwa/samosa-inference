#ifndef SAMOSA_INFERENCE_REGISTRY_H
#define SAMOSA_INFERENCE_REGISTRY_H
#include "inference.h"
#define INFERENCE_PROVIDERS_MAX 16u
/* Immutable host configuration. Requests select identities, never commands,
 * credentials, owners or state paths. Each provider has its own operation/session
 * namespace. Engine ownership remains with the existing adapters/supervisor. */
typedef struct {
    const InferenceConfig *providers;
    size_t count;
} InferenceRegistry;
const char *inference_registry_validate(const InferenceRegistry *);
int inference_registry_dispatch(const InferenceRegistry *,const char *,size_t,FILE *);
#endif
