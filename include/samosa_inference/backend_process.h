#ifndef SAMOSA_INFERENCE_BACKEND_PROCESS_H
#define SAMOSA_INFERENCE_BACKEND_PROCESS_H
#include <pthread.h>
#include <sys/types.h>
#include <samosa_inference/llama_launch.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Zero-initialized owner state. Only this module mutates the tracked child.
 * Borrow the original host mutex so detach/marker/upstream state stays atomic
 * with existing caller coordination. Strings/mutex/state survive each call.
 * No outside waitpid/reaping or PID adoption is supported. Trusted host inputs,
 * never public request fields. Host retains original serialization of whole
 * launch/stop operations; the borrowed mutex coordinates state publication. */
typedef struct { pid_t pid, pgid; } SamosaBackendProcess;
typedef struct {
    SamosaBackendProcess *process;
    pthread_mutex_t *mutex;
    const char *pid_file;
} SamosaBackendOwner;
typedef struct {
    int cpu_auto, context_auto, auto_compact, compact_threshold_percent;
    int cpu_locked, context_locked, cpu_effective, context_effective;
    const char *cpu_source, *context_source;
} SamosaBackendRuntime;
typedef struct {
    const char *backend, *engine, *model, *tokenizer, *chats, *log;
    int port;
    SamosaBackendRuntime runtime;
    SamosaLlamaLaunch llama;
} SamosaBackendLaunch;
/* detach runs under the borrowed mutex, after child/marker state is cleared.
 * interrupted runs after unlock, before termination/reaping, even with no child.
 * Preserve existing upstream shutdown/shared-specialist cancellation here.
 * Callbacks must not reenter this owner; context survives until stop returns. */
typedef struct {
    void (*detach)(void *);
    void (*interrupted)(void *);
    void *context;
} SamosaBackendStopControl;
pid_t samosa_backend_process_pid(const SamosaBackendOwner *);
int samosa_backend_process_running(const SamosaBackendOwner *);
/* The original bounded HTTP health probe is transport availability, not actual
 * model-generation qualification or common lifecycle readiness. */
int samosa_backend_process_probe(const SamosaBackendOwner *, const char *backend, int port);
/* Preserves fork-success semantics; exec/readiness failures are observed by
 * running/probe and the caller's original selection/rollback watchdog. Host
 * resolves availability, virtual backends, model selection and chats directory.
 * Legacy process launch is not guardian/parent-death ownership. */
int samosa_backend_process_start(const SamosaBackendOwner *, const SamosaBackendLaunch *);
void samosa_backend_process_stop(const SamosaBackendOwner *, const SamosaBackendStopControl *);
#ifdef __cplusplus
}
#endif
#endif
