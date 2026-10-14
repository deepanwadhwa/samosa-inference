#ifndef SAMOSA_INFERENCE_PACKAGE_CACHE_H
#define SAMOSA_INFERENCE_PACKAGE_CACHE_H
#include "package_assemble.h"
#include "artifact.h"
typedef struct {
    ArtifactConfig store;
    /* Optional trusted acquired/local source root. NULL requires cache-only
     * operation. Public manifests never choose source paths or store limits. */
    const char *source_directory;
} PackageCacheSource;
/* PackageSourceOpen adapter. Verify cache first; optionally acquire missing or
 * interrupted files through the existing resumable local artifact installer.
 * Resume, capacity, byte ceiling and disk ceiling retain trusted store policy.
 * Copies into the package remain controlled/verified by the assembler. */
const char *package_cache_open(void *,const PackageFile *,const char *(*)(void *),void *,int *);
#endif
