#ifndef SAMOSA_INFERENCE_VISION_PACKAGE_H
#define SAMOSA_INFERENCE_VISION_PACKAGE_H
#include "package_index.h"
typedef struct {
    const char *engine_sha256, *config_sha256, *tokenizer_sha256, *weights_sha256, *kernels_sha256;
} VisionPackagePolicy;
typedef struct {size_t engine, config, tokenizer, weights;} VisionPackageDescriptor;
int vision_package_policy_valid(const VisionPackagePolicy *);
/* Fixed visionpsy-v1 model layout and independently qualified engine/model pins.
 * Static installation admission only; does not load a model or imply readiness.
 * Caller retains the package lease. Output is unchanged on failure. */
const char *vision_package_verify(int,const PackageIndex *,const VisionPackagePolicy *,
                                 const char *(*)(void *),void *,VisionPackageDescriptor *);
#endif
