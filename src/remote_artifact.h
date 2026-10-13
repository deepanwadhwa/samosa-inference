#ifndef SAMOSA_REMOTE_ARTIFACT_H
#define SAMOSA_REMOTE_ARTIFACT_H
#include "artifact.h"
#include <stddef.h>
typedef struct {
    const char *url,*ca_file,*bearer_key;
    const char *redirect_hosts[8];size_t redirect_count,timeout_ms;
#ifdef SAMOSA_INFERENCE_TEST_FAULTS
    int loopback_http;
    uint64_t checkpoint_bytes;
#endif
} RemoteArtifactSource;
const char *artifact_install_remote(const ArtifactConfig *,const RemoteArtifactSource *);
#endif
