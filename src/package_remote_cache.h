#ifndef SAMOSA_INFERENCE_PACKAGE_REMOTE_CACHE_H
#define SAMOSA_INFERENCE_PACKAGE_REMOTE_CACHE_H
#include "package_assemble.h"
#include "remote_artifact.h"
typedef struct {
    ArtifactConfig store;
    const PackageIndex *index;
    /* Trusted catalog configuration in pinned-index order, never manifest URLs.
     * NULL permits verified cache-only use without network credentials. */
    const RemoteArtifactSource *files;
    size_t count;
    /* Optional trusted HTTPS endpoint: URL + '/' + pinned SHA-256.
     * Mutually exclusive with files. Index bytes never choose the endpoint. */
    const RemoteArtifactSource *digest_source;
} PackageRemoteCacheSource;
const char *package_remote_cache_open(void *,const PackageFile *,const char *(*)(void *),void *,int *);
#endif
