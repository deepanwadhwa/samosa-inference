#ifndef SAMOSA_PACKAGE_SOURCES_H
#define SAMOSA_PACKAGE_SOURCES_H
#include "package_index.h"
#include "remote_artifact.h"
typedef struct {
    size_t count;
    RemoteArtifactSource files[PACKAGE_INDEX_FILES];
    char urls[PACKAGE_INDEX_FILES][2049];
    char keys[PACKAGE_INDEX_FILES][130];
} PackageSources;
/* Trusted private host configuration, bound to the selected index SHA-256.
 * Returns an owned snapshot; free clears keys. No catalog key inheritance. */
const char *package_sources_load(const char *,const char *,const RemoteArtifactSource *,PackageSources **);
void package_sources_free(PackageSources *);
#endif
