#ifndef SAMOSA_INFERENCE_WHISPER_LIFECYCLE_H
#define SAMOSA_INFERENCE_WHISPER_LIFECYCLE_H

#include <samosa_inference/whisper_process.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SamosaWhisperLifecycle SamosaWhisperLifecycle;
typedef struct {
    SamosaWhisperProcessConfig process;
    const char *state, *engine_sha256, *model_sha256;
    size_t engine_size, model_size;
    const char *qualification_wav, *qualification_sha256, *expected_text;
    size_t qualification_size;
} SamosaWhisperLifecycleConfig;

/* Trusted local configuration. String storage lives through destroy.
 * Start verifies the pinned inputs and runs a 60-second-bounded speech probe.
 * Status returns NULL for qualified on-demand execution with unchanged assets.
 * A qualified idle owner holds its leases and has no child engine. */
const char *samosa_whisper_lifecycle_create(const SamosaWhisperLifecycleConfig *, SamosaWhisperLifecycle **);
const char *samosa_whisper_lifecycle_start(SamosaWhisperLifecycle *);
const char *samosa_whisper_lifecycle_status(SamosaWhisperLifecycle *);
/* Stop cancels current work and waits for confirmed guardian cleanup.
 * Call control callbacks without reentering start/stop/destroy. Unknown guardian
 * cleanup keeps the owner and its fence. Serialize destroy with all users. */
const char *samosa_whisper_lifecycle_stop(SamosaWhisperLifecycle *);
const char *samosa_whisper_lifecycle_destroy(SamosaWhisperLifecycle *);
/* Run the original text/SRT contract. A supplied expected identity ties a
 * window to its caller's initial attachment snapshot. */
int samosa_whisper_lifecycle_run(SamosaWhisperLifecycle *, const SamosaWhisperProcessIdentity *,
    const SamosaWhisperProcessControl *, const char *wav_path, int timestamped,
    int preserve_cancel, int own_input, char **output);

#ifdef __cplusplus
}
#endif
#endif
