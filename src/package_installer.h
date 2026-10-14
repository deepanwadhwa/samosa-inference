#ifndef SAMOSA_INFERENCE_PACKAGE_INSTALLER_H
#define SAMOSA_INFERENCE_PACKAGE_INSTALLER_H
#include "package_catalog.h"
#include "package_remote_cache.h"
typedef struct {
    const ProviderEntry *entry;
    ArtifactConfig store;
    const RemoteArtifactSource *index_source,*file_sources;
    size_t file_count;
    PackageAssembly assembly;
} PackageInstaller;
/* Complete native catalog -> verified index -> verified files -> compatible
 * publication. Configuration is trusted; public requests select catalog IDs.
 * NULL remote sources permit cache-only retries. Output untouched on failure. */
const char *package_install_catalog(const PackageInstaller *,PackageTreeResult *);
#endif
