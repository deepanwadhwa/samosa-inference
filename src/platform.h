#ifndef SAMOSA_INFERENCE_PLATFORM_H
#define SAMOSA_INFERENCE_PLATFORM_H
#include <stddef.h>
#include <string.h>
/* Executing binary's OS/architecture, consistent with package/catalog pins.
 * This is a compatibility target, not a model qualification or readiness check. */
static inline const char *inference_host_platform(void) {
#if defined(__APPLE__) && (defined(__aarch64__) || defined(__arm64__))
    return "macos-arm64";
#elif defined(__APPLE__) && defined(__x86_64__)
    return "macos-x86_64";
#elif defined(__linux__) && defined(__aarch64__)
    return "linux-arm64";
#elif defined(__linux__) && defined(__x86_64__)
    return "linux-x86_64";
#else
    return NULL;
#endif
}
static inline int inference_platform_valid(const char *platform) {
    return !platform || !strcmp(platform,"macos-arm64") || !strcmp(platform,"macos-x86_64") ||
        !strcmp(platform,"linux-arm64") || !strcmp(platform,"linux-x86_64");
}
static inline int inference_platform_allowed(const char *platform) {
    const char *host=inference_host_platform();
    return !platform || (host&&!strcmp(platform,host));
}
#endif
