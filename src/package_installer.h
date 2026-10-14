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
    const RemoteArtifactSource *digest_source;
} PackageInstaller;
/* Complete native catalog -> verified index -> verified files -> compatible
 * publication. Configuration is trusted; public requests select catalog IDs.
 * NULL remote sources permit cache-only retries. Output untouched on failure. */
const char *package_install_catalog(const PackageInstaller *,PackageTreeResult *);
/* Select by public identity from a trusted parsed catalog; the template entry
 * pointer is ignored. No acquisition occurs for unknown/incompatible IDs. */
const char *package_install_selected(const ProviderCatalog *,const char *,const PackageInstaller *,PackageTreeResult *);
/* Load the host-selected private catalog with existing ownership/binding checks
 * before ID selection. The path is configuration, never request input. */
const char *package_install_from_file(const char *,const char *,const PackageInstaller *,PackageTreeResult *);
/* Observe a catalog-selected installation using only its verified cached index.
 * Ignores acquisition and reservation configuration; never repairs state.
 * Absence of the index is reported as artifact_absent, not package readiness. */
const char *package_status_from_file(const char *,const char *,const PackageInstaller *,PackageAssemblyStatus *);
/* Catalog-selected exact-charge reclamation; verified cached index required.
 * Never downloads or deletes package contents. Output unchanged on failure. */
const char *package_release_empty_from_file(const char *,const char *,const PackageInstaller *,int *);
#endif
