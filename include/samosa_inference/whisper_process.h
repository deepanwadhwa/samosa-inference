#ifndef SAMOSA_INFERENCE_WHISPER_PROCESS_H
#define SAMOSA_INFERENCE_WHISPER_PROCESS_H
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
enum {
    SAMOSA_WHISPER_OK = 1,
    SAMOSA_WHISPER_NOT_READY = -1,
    SAMOSA_WHISPER_BUSY = -2,
    SAMOSA_WHISPER_PREPARE_FAILED = -3,
    SAMOSA_WHISPER_PROCESS_FAILED = -4,
    SAMOSA_WHISPER_OUTPUT_INVALID = -5,
    SAMOSA_WHISPER_CANCELLED = -6
};
typedef struct {
    const char *engine, *model, *temporary_directory;
} SamosaWhisperProcessConfig;
/* Borrowed synchronous host coordination. A supplied control has all callbacks.
 * acquire returns1 after reserving this operation,0 for busy; it preserves or
 * clears the existing cancellation scope according to preserve_cancel.
 * started publishes the exact child under the host's cancellation lock, signals
 * pending cancellation and registers tracking. reaped removes that tracking;
 * release clears the reservation/PID after cleanup. Callbacks must not reenter
 * this runner. Context/configuration survive until return. NULL control is for
 * an already serialized caller without a cancellation scope. */
typedef struct {
    int (*acquire)(void *, int preserve_cancel);
    void (*started)(void *, pid_t);
    void (*reaped)(void *, pid_t);
    void (*release)(void *);
    int (*cancelled)(void *);
    void *context;
} SamosaWhisperProcessControl;
/* Original on-demand native CLI runner: English/-nt and txt or SRT, bounded
 *16-MiB nonempty UTF-8 output, malloc-owned on success. Host resolves/validates
 * engine/model and prepared audio before calling. No30-second clip limit is
 * introduced here. These paths are trusted configuration, never service DTOs.
 * Return codes preserve the original gateway behavior. This legacy runner is
 * distinct from the verified short-clip/guardian public adapter. */
int samosa_whisper_process_run(const SamosaWhisperProcessConfig *,
    const SamosaWhisperProcessControl *, const char *wav_path, int timestamped,
    int preserve_cancel, char **output);
#ifdef __cplusplus
}
#endif
#endif
