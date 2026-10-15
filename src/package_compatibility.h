#ifndef SAMOSA_INFERENCE_PACKAGE_COMPATIBILITY_H
#define SAMOSA_INFERENCE_PACKAGE_COMPATIBILITY_H
#include "package_publish.h"
#include "llama_package.h"
#include "whisper_package.h"
#include "vision_package.h"
/* Admission only; supported recipe does not qualify package bytes. */
int package_recipe_supported(const char *);
typedef struct {const char *payload_path;const char *(*control)(void *);void *context;const LlamaPackagePolicy *llama_policy;const WhisperPackagePolicy *whisper_policy;const VisionPackagePolicy *vision_policy;} PackageCompatibilityContext;
/* Concrete recipe dispatch. Unknown recipes refuse; successful inspection does
 * not launch a model or establish readiness/loaded runtime ABI. */
const char *package_backend_compatible(int,const PackageIndex *,void *);
#endif
