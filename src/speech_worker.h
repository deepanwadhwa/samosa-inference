#ifndef SAMOSA_INFERENCE_SPEECH_WORKER_H
#define SAMOSA_INFERENCE_SPEECH_WORKER_H
#include "speech_runtime.h"
#include "engine_process.h"
#define SPEECH_PACKET_HEADER 80
#define SPEECH_PACKET_LIMIT 16777216
/* Hidden native worker accepts only text/voice through a bound regular input
 * descriptor. Package path and pin are trusted host argv, root read-only fd5.
 * Returns -1 for unrelated command dispatch, otherwise the worker exit code. */
int speech_worker_dispatch(int argc, char **argv);
/* Decode only a completed owned worker's retained output descriptor. Reads
 * bounded PCM chunks, verifies exact metadata/hash/identity, exposes no paths.
 * Does not turn partial/cancelled worker output into a successful receipt. */
const char *speech_worker_result(int output_fd, const char *(*control)(void *),
                                void *, SpeechRuntimeResult *);
typedef struct {
    const char *self, *package_path, *manifest_sha256, *workspace_path;
    int residency_fd, package_fd, workspace_fd, input_fd, output_fd;
    double lifetime_seconds;
    size_t log_limit;
    EngineProcess *process; /* Retained by caller, including shutdown pending. */
} SpeechProcessConfig;
/* Reuses guardian process-group ownership, residency retention and workspace
 * cleanup. Caller owns unopened/failed-start workspaces; a started guardian
 * owns cleanup. Control errors stop/reap before returning, and remain typed.
 * Initialize process with guardian/control/response=-1. On shutdown pending,
 * keep that record and retry engine_process_stop; never lose a live guardian.
 * Owner/auth/admission/artifact publication belong to the public adapter. */
const char *speech_process_run(const SpeechProcessConfig *,
    const char *(*control)(void *), void *, SpeechRuntimeResult *);
#endif
