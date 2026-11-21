#ifndef SAMOSA_INFERENCE_SUMMARY_PROCESS_H
#define SAMOSA_INFERENCE_SUMMARY_PROCESS_H
#include <pthread.h>
#include <stddef.h>
#include <sys/types.h>
#ifdef __cplusplus
extern "C" {
#endif
typedef struct SamosaSummaryProcessConfig SamosaSummaryProcessConfig;
typedef struct {
    /* Absolute CLOCK_MONOTONIC milliseconds; zero uses the transport budget.
     * check returns NULL or a stable typed error such as cancelled. */
    long long deadline_ms;
    const char *(*check)(void *);
    void *context;
} SamosaSummaryControl;
typedef struct {
    /* Under the transport mutex; never reenter it. Endpoints are borrowed.
     * Host retains pins/guardian/lease and obeys the supplied controls. */
    const char *(*open)(void *,const SamosaSummaryProcessConfig *,const SamosaSummaryControl *,int *,int *);
    /* Return an error only if cleanup is unconfirmed. Retain context/ownership
     * in that case; close is retryable and new work is refused meanwhile. */
    const char *(*close)(void *);
    void *context;
} SamosaSummaryLifecycle;
/* Resident native T5 process, extracted from the existing Chat supervisor.
 * Initialize once before sharing; hold mutex across start and protocol I/O.
 * Pipe descriptors are borrowed by that serialized transport, never closed by it.
 * Stop clears pipes/residency; destroy only after all users have joined. */
typedef struct {
    pthread_mutex_t mutex;
    pid_t pid;
    int write_fd, read_fd, warmed;
    SamosaSummaryLifecycle lifecycle;
    const char *error,*cleanup_error;
} SamosaSummaryProcess;
struct SamosaSummaryProcessConfig {
    const char *engine, *model, *log_path, *gpu_layers;
};
int samosa_summary_process_init(SamosaSummaryProcess *);
/* Bind once on a fresh process before sharing. Its original launcher remains
 * the default. Lifecycle context survives until confirmed close/destroy. */
int samosa_summary_process_bind(SamosaSummaryProcess *,const SamosaSummaryLifecycle *);
const char *samosa_summary_process_close(SamosaSummaryProcess *);
/* One trusted preframed request/reply for the public summary codec. Preserves
 * reply bytes (no legacy punctuation/minimum-length policy), at most8192 UTF-8
 * bytes. Same lifecycle, scoped controls and invalidation as other calls. */
char *samosa_summary_process_frame_controlled(SamosaSummaryProcess *,const SamosaSummaryProcessConfig *,const unsigned char *,size_t,const SamosaSummaryControl *,const char **);
int samosa_summary_process_start_locked(SamosaSummaryProcess *, const SamosaSummaryProcessConfig *);
void samosa_summary_process_stop_locked(SamosaSummaryProcess *);
void samosa_summary_process_stop(SamosaSummaryProcess *);
void samosa_summary_process_destroy(SamosaSummaryProcess *);
/* External owners must check close before destroy; unconfirmed close keeps
 * the mutex/context usable for retry. Error strings have stable lifetime. */
char *samosa_summary_process_once_controlled(SamosaSummaryProcess *,const SamosaSummaryProcessConfig *,const char *,size_t,const SamosaSummaryControl *,const char **);
int samosa_summary_process_batch_controlled(SamosaSummaryProcess *,const SamosaSummaryProcessConfig *,char **,int,char **,int,const SamosaSummaryControl *,const char **);
int samosa_summary_process_coalesce_controlled(SamosaSummaryProcess *,const SamosaSummaryProcessConfig *,char **,int,char **,int,
                                              void (*)(void *,const char *),void *,const SamosaSummaryControl *,const char **);
/* Serialized inherited scalar/batch protocol. Results are malloc-owned.
 * Batch reply slots must initially be NULL; failed batches clear all slots.
 * Configuration is trusted host input, fixed for the resident process lifetime.
 * These calls acquire mutex themselves; do not hold it when calling them. */
char *samosa_summary_process_once(SamosaSummaryProcess *, const SamosaSummaryProcessConfig *, const char *, size_t);
int samosa_summary_process_batch(SamosaSummaryProcess *, const SamosaSummaryProcessConfig *, char **, int, char **, int);
/* Existing 4ms/64-frame coalescing of compatible concurrent batches. Callers
 * retain arrays/config strings until return and must not destroy a process with
 * pending users. Activity callbacks receive summary_queued/summarizing outside
 * the queue lock and must not synchronously reenter the coalescer. This local
 * optimization is not global admission or a bounded service queue. */
int samosa_summary_process_coalesce(SamosaSummaryProcess *, const SamosaSummaryProcessConfig *, char **, int, char **, int,
                                   void (*)(void *, const char *), void *);
#ifdef __cplusplus
}
#endif
#endif
