#ifndef SAMOSA_TEMPORAL_PACKAGE_H
#define SAMOSA_TEMPORAL_PACKAGE_H
#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
typedef struct {
    char name[256], sha256[65];
    uint64_t bytes;
    struct stat identity;
} TemporalPackageFile;
typedef struct {
    struct stat directory, manifest;
    TemporalPackageFile files[64];
    size_t count;
    uint64_t total_bytes;
} TemporalPackageReceipt;
/* Host-owned directory and manifest pin. The caller retains the bound directory
 * descriptor through execution. No request supplies paths or model identity. */
const char *temporal_package_verify(int, const char *, const char *,
                                    const char *(*)(void *), void *,
                                    TemporalPackageReceipt *, int);
void temporal_package_identity(const char *, char[65]);
#endif
