#ifndef SAMOSA_INFERENCE_SPEECH_PACKAGE_LOADER_H
#define SAMOSA_INFERENCE_SPEECH_PACKAGE_LOADER_H
#include "speech_package.h"
#include "library_runtime.h"
/* Rechecks the package around native image/graph inspection. OS libraries are
 * trusted; all non-OS direct/transitive edges must resolve to pinned sibling
 * library/dependency entries. Dedicated guardian execution and an actual load
 * gate remain necessary: this does not constrain arbitrary native code dlopen. */
const char *speech_package_loader_verify(int root_fd, const char *root_path,
    const char *manifest_sha256, const LibraryRuntimePolicy *,
    const char *(*control)(void *), void *, SpeechPackageReceipt *);
#endif
