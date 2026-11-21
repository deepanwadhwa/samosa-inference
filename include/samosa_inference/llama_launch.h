#ifndef SAMOSA_INFERENCE_LLAMA_LAUNCH_H
#define SAMOSA_INFERENCE_LLAMA_LAUNCH_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Trusted host launch inputs, never public request fields. Called only in an
 * already owned child process; replaces that process on success, returns -1
 * with exec errno on failure. Group/log/socket ownership remains with caller.
 * Override strings preserve the existing native environment interpretation. */
typedef struct {
    const char *engine, *model, *alias, *projector;
    int port, context_tokens, threads;
    const char *context_override, *gpu_override, *threads_override;
    uint64_t ram_bytes;
    int perf_cores;
} SamosaLlamaLaunch;
int samosa_llama_launch(const SamosaLlamaLaunch *);
#ifdef __cplusplus
}
#endif
#endif
