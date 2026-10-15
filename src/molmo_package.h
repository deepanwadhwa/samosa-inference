#ifndef SAMOSA_INFERENCE_MOLMO_PACKAGE_H
#define SAMOSA_INFERENCE_MOLMO_PACKAGE_H
#include "molmo_package_descriptor.h"
typedef struct {const char *engine_sha256,*kernels_sha256,*manifest_sha256;} MolmoPackagePolicy;
int molmo_package_policy_valid(const MolmoPackagePolicy *);
/* Independent host-qualified engine/kernel/manifest pins, native runtime closure
 * and the existing temporal model verifier. Caller holds package lease and root
 * pathname binding. Static installation admission only; output unchanged on error. */
const char *molmo_package_verify(int,const char *,const PackageIndex *,const MolmoPackagePolicy *,
                                const char *(*)(void *),void *,MolmoPackageDescriptor *);
#endif
