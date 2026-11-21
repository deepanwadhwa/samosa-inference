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
    /* Optional trusted incremental sink. PCM is provisional until the final
     * result succeeds; sink failure stops/reaps the same owned worker. */
    SpeechPcmSink pcm_sink;
    void *pcm_context;
} SpeechProcessConfig;
/* Reuses guardian process-group ownership, residency retention and workspace
 * cleanup. Caller owns unopened/failed-start workspaces; a started guardian
 * owns cleanup. Control errors stop/reap before returning, and remain typed.
 * Initialize process with guardian/control/response=-1. On shutdown pending,
 * keep that record and retry engine_process_stop; never lose a live guardian.
 * Owner/auth/admission/artifact publication belong to the public adapter. */
const char *speech_process_run(const SpeechProcessConfig *,
    const char *(*control)(void *), void *, SpeechRuntimeResult *);
/* Private sequential resident transport. Initialize with speech_resident_init;
 * one caller at a time. Start uses self/package/pin, residency/package fds and
 * lifetime/log limits from SpeechProcessConfig; no per-request workspace fds.
 * Startup is ownership only, never readiness. A completed
 * warm generation and pinned host activation are still required. Spool is a
 * caller-owned private regular fd; it receives the original PCM packet format.
 * Active frame/control/sink failures invalidate and reap this same worker;
 * shutdown-pending retains descriptors/process for an explicit stop retry. */
typedef struct {EngineProcess process;int write_fd,read_fd;
    /* Optional trusted activation ownership. Bind before start; it is the only
     * authoritative process record, and must outlive this transport. */
    EngineProcess *owner;
} SpeechResident;
void speech_resident_init(SpeechResident *);
const char *speech_resident_start(const SpeechProcessConfig *,SpeechResident *);
const char *speech_resident_stop(SpeechResident *);
const char *speech_resident_run(SpeechResident *,const char *request,size_t bytes,
    int spool_fd,SpeechPcmSink,void *,const char *(*control)(void *),void *,SpeechRuntimeResult *);
#endif
