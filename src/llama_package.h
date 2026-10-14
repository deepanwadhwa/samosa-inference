#ifndef SAMOSA_INFERENCE_LLAMA_PACKAGE_H
#define SAMOSA_INFERENCE_LLAMA_PACKAGE_H
#include "llama_package_descriptor.h"
typedef struct {const char *runtime_revision,*kind,*architecture,*engine_sha256;} LlamaPackagePolicy;
int llama_package_policy_valid(const LlamaPackagePolicy *);
/* Host-owned qualified runtime policy, never derived from the package manifest.
 * Binds pinned descriptor/model metadata and full native package verification.
 * Static compatibility only, not tensor-layout validation or loaded readiness.
 * Caller retains package lease/named-root binding. Output untouched on failure. */
const char *llama_package_verify(int,const PackageIndex *,const LlamaPackagePolicy *,
    const char *(*)(void *),void *,LlamaPackageDescriptor *);
#endif
