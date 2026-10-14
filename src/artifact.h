#ifndef SAMOSA_ARTIFACT_H
#define SAMOSA_ARTIFACT_H
#include <stdint.h>
#include <stddef.h>
#define ARTIFACT_CAPACITY_DEFAULT 16u
#define ARTIFACT_CAPACITY_MAX 1024u
/* The caller owns the descriptor. Verification never seeks, closes, or writes it.
 * Regular immutable files only; a successful read proves this observation, not
 * future immutability or a relationship with an external engine. */
const char *artifact_verify(int fd, uint64_t expected_size,
                            uint64_t byte_ceiling, const char *expected_sha256);
/* Check request control between bounded reads, including before completion. */
const char *artifact_verify_controlled(int fd, uint64_t expected_size,
    uint64_t byte_ceiling, const char *expected_sha256,
    const char *(*interrupted)(void *), void *context);
typedef struct ArtifactConfig ArtifactConfig;
struct ArtifactConfig {
    const char *state_path,*source_path,*sha256;
    uint64_t size,byte_ceiling,disk_ceiling;
    int resume;
    /* Native test/process integration hook, never a public caller function. */
    int (*cancelled)(void *);void *cancellation;
    /* Trusted acquisition adapter under the same install lease/quota. It may
     * append only after proving retained bytes; publication stays here. */
    const char *(*acquire)(void *,int,int,uint64_t,const ArtifactConfig *);
    void *acquisition;
    /* Trusted shared-store bound. Zero preserves the original16 identities;
     * package coordination may explicitly reserve a larger bounded namespace. */
    size_t artifact_capacity;
#ifdef SAMOSA_INFERENCE_TEST_FAULTS
    void (*checkpoint)(const char *,void *);void *checkpoint_context;
#endif
};
/* Locally configured acquisition. Publication verifies complete staged bytes.
 * It does not activate an engine. Errors never delete foreign files. */
const char *artifact_install(const ArtifactConfig *);
const char *artifact_status(const ArtifactConfig *,const char **status);
/* Acquire a held read-only verified published descriptor under the store lease.
 * Output is untouched on failure; caller closes on success. The descriptor may
 * have the registered two cache links. It is an observation, not immutability.
 * Explicit request control applies between bounded verification reads. */
const char *artifact_open_verified(const ArtifactConfig *,const char *(*)(void *),void *,int *);
/* Only the registered native artifact inode is changed; never a source path. */
const char *artifact_make_executable(const ArtifactConfig *);
#endif
