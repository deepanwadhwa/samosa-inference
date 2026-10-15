#ifndef SAMOSA_INFERENCE_WHISPER_PACKAGE_H
#define SAMOSA_INFERENCE_WHISPER_PACKAGE_H
#include "package_index.h"
typedef struct {const char *runtime_revision,*profile,*engine_sha256,*model_sha256;} WhisperPackagePolicy;
typedef struct {char runtime_revision[41],profile[32];size_t engine,model;} WhisperPackageDescriptor;
int whisper_package_policy_valid(const WhisperPackagePolicy *);
/* Pinned descriptor bytes resolve exactly one engine/model in index order.
 * The supported base.en-f16 profile is a legacy GGML format, not GGUF. */
const char *whisper_package_descriptor_parse(const char *,size_t,const PackageIndex *,WhisperPackageDescriptor *);
/* Separate host-qualified engine/model pair plus fixed profile header and shared
 * closed native package checks. No model load, inference or readiness claim.
 * Caller holds the package lease/root binding. Output unchanged on failure. */
const char *whisper_package_verify(int,const PackageIndex *,const WhisperPackagePolicy *,const char *(*)(void *),void *,WhisperPackageDescriptor *);
#endif
