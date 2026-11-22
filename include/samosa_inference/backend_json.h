#ifndef SAMOSA_INFERENCE_BACKEND_JSON_H
#define SAMOSA_INFERENCE_BACKEND_JSON_H
#include <stddef.h>
#include <pthread.h>
#include <samosa_inference/turn_control.h>
#ifdef __cplusplus
extern "C" {
#endif
#define SAMOSA_BACKEND_JSON_MAX_BODY (4u << 20)
#define SAMOSA_BACKEND_JSON_MAX_HEADER (64u << 10)
/* Original internal primary-model JSON transport. Trusted host inputs only;
 * no paths, hooks or ports come from a public model request. The caller keeps
 * the gate/mutex/upstream slot/context alive throughout this synchronous call.
 * probe retains existing selected-provider/liveness policy. Trace hooks are
 * optional and must not reenter this request. Cancellation uses the original
 * mutex-protected upstream socket shutdown; all exits release their own slot
 * and generation contribution. Payloads/results are borrowed/owned respectively.
 * This preserves the existing HTTP/body/deadline behavior; it does not claim
 * pinned identity, successful-generation readiness or global scheduling. */
typedef struct {
    int port, timeout_seconds;
    SamosaTurnGate *gate;
    pthread_mutex_t *mutex;
    int *upstream_fd;
    int (*probe)(void *);
    void (*trace_payload)(void *,const char *,const char *,const char *,size_t);
    void (*trace_event)(void *,const char *,const char *);
    void *context;
    /* Qualification requires a complete successful HTTP response. Ordinary
     * callers keep the original partial-response behavior. */
    int require_complete;
    /* Qualification can be cancelled before the socket is published. The
     * callback runs under mutex and must only observe the cancellation flag. */
    int (*cancelled)(void *);
} SamosaBackendJson;
/* Returns the existing raw HTTP body or NULL. Caller frees a non-NULL result.
 * Original result ceiling permits a final read chunk beyond body+header limit;
 * status/header parsing remains the original behavior, not strict HTTP parsing. */
char *samosa_backend_json(const SamosaBackendJson *,const char *payload);
#ifdef __cplusplus
}
#endif
#endif
