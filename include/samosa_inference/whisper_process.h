#ifndef SAMOSA_INFERENCE_WHISPER_PROCESS_H
#define SAMOSA_INFERENCE_WHISPER_PROCESS_H
#include <sys/stat.h>
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
    SAMOSA_WHISPER_CANCELLED = -6,
    SAMOSA_WHISPER_GUARDIAN_FAILED = -7,
    SAMOSA_WHISPER_DEADLINE_EXCEEDED = -8
};
typedef struct {
    const char *engine, *model, *temporary_directory;
} SamosaWhisperProcessConfig;
typedef struct {
    struct stat engine, model;
} SamosaWhisperProcessIdentity;

/* Observe the installed input files. A positive model size requires an exact
 * match. This is a presence and identity check; it does not execute a probe. */
int samosa_whisper_process_inspect(const SamosaWhisperProcessConfig *,
    off_t expected_model_size, SamosaWhisperProcessIdentity *);
/* Borrowed synchronous host coordination. A supplied control has all callbacks.
 * acquire returns1 after reserving this operation,0 for busy; it preserves or
 * clears the existing cancellation scope according to preserve_cancel.
 * started publishes the exact child under the host's cancellation lock, signals
 * pending cancellation and registers tracking. reaped withdraws PID/tracking
 * while the guardian still reserves identity, before actual reap;
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
 * distinct from the verified short-clip public adapter. Both reuse the existing
 * Inference guardian. Caller audio/model are borrowed and never removed. */
/* Call before normal configuration. Returns -1 for ordinary command dispatch. */
int samosa_whisper_process_dispatch(int argc,char **argv);
int samosa_whisper_process_run(const SamosaWhisperProcessConfig *,
    const SamosaWhisperProcessControl *, const char *wav_path, int timestamped,
    int preserve_cancel, char **output);
/* Explicit handoff of a caller-created regular, singly linked temporary WAV
 * on the workspace filesystem. After admission/preflight it is renamed into
 * the guardian-owned workspace and made0600; normal return, cancellation and owner death
 * clean it with the results. Borrowed/durable sources use run() above.
 * On busy/preflight/handoff failure the caller retains its existing cleanup. */
int samosa_whisper_process_run_owned(const SamosaWhisperProcessConfig *,
    const SamosaWhisperProcessControl *,const char *wav_path,int timestamped,
    int preserve_cancel,char **output);
/* Run against the identity already observed by the host. Changed inputs return
 * NOT_READY with no transcript, including changes during execution or cleanup.
 * own_input selects the same prepared-input handoff as run_owned(). */
int samosa_whisper_process_run_checked(const SamosaWhisperProcessConfig *,
    const SamosaWhisperProcessIdentity *, const SamosaWhisperProcessControl *,
    const char *wav_path, int timestamped, int preserve_cancel, int own_input, char **output);
#ifdef __cplusplus
}
#endif
#endif
