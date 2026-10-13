#ifndef SAMOSA_INFERENCE_SPEECH_PACKAGE_H
#define SAMOSA_INFERENCE_SPEECH_PACKAGE_H
#include "speech_runtime.h"
#include <stdint.h>
#include <sys/stat.h>
#define SPEECH_PACKAGE_FILES 512
#define SPEECH_PACKAGE_DIRECTORIES 512
#define SPEECH_PACKAGE_PATH 4096
/* One receipt per verified observation, owned by the host. Keep the bound root
 * descriptor alive and recheck immediately before/after use. A receipt does not
 * make mutable files immutable or qualify arbitrary native loader behavior. */
typedef struct {
    char name[256], sha256[65];
    uint64_t bytes;
    unsigned role;
    struct stat identity;
} SpeechPackageFile;
typedef struct { char name[256]; struct stat identity; } SpeechPackageDirectory;
typedef struct {
    int verified, pocket, threads;
    struct stat root, manifest;
    char root_path[SPEECH_PACKAGE_PATH], model_identity[65], data_dir[256];
    char paths[14][SPEECH_PACKAGE_PATH];
    SpeechPackageFile files[SPEECH_PACKAGE_FILES];
    SpeechPackageDirectory directories[SPEECH_PACKAGE_DIRECTORIES];
    size_t count, directory_count;
    uint64_t total_bytes;
} SpeechPackageReceipt;
const char *speech_package_platform(void);
void speech_package_identity(const char *manifest_sha256, char out[65]);
const char *speech_package_verify(int root_fd, const char *root_path,
    const char *manifest_sha256, const char *(*control)(void *), void *,
    SpeechPackageReceipt *, int recheck);
/* Uses only paths derived from a successfully verified package, never caller
 * paths. Pocket references are returned separately from runtime model options. */
const char *speech_package_options(const SpeechPackageReceipt *, SpeechRuntimeOptions *,
                                  const char **caro, const char **stuart);
#endif
