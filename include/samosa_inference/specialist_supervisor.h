#ifndef SAMOSA_MULTIMODAL_H
#define SAMOSA_MULTIMODAL_H

#include <pthread.h>
#include <stddef.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Private pipe protocols supported by the specialist supervisor.  The legacy
   newline protocol keeps VisionPsy compatible while new helpers use the
   length-prefixed protocol so an embedded newline cannot desynchronise IPC. */
typedef enum {
    SAMOSA_MM_PROTOCOL_JSON_LINE = 0,
    SAMOSA_MM_PROTOCOL_JSON_FRAME_V1 = 1
} SamosaMmProtocol;

enum {
    SAMOSA_MM_PROVIDER_NAME_MAX = 48,
    SAMOSA_MM_ERROR_MAX = 96,
    SAMOSA_MM_DEFAULT_MAX_FRAME = 1024 * 1024
};

typedef struct {
    const char *provider;
    const char *executable;
    const char *model_dir;
    SamosaMmProtocol protocol;
    int ready_timeout_ms;
    int request_timeout_ms;
    int shutdown_grace_ms;
    size_t max_frame_bytes;
} SamosaMmProvider;

/* One instance belongs to the gateway.  A reservation is made before fork,
   so concurrent visual turns cannot race into launching two Metal models. */
typedef struct {
    pthread_mutex_t mutex;
    pid_t active_pid;
    pid_t active_pgid;
    uint64_t generation;
    int reserved;
    char active_provider[SAMOSA_MM_PROVIDER_NAME_MAX];
} SamosaMmSupervisor;

/* Optional caller control, checked during both writes and reads. deadline_ms
   uses CLOCK_MONOTONIC milliseconds; zero uses the configured transport timeout.
   check returns NULL or a stable error string. Only the owning thread uses a
   session. A failed exchange invalidates it: close must reap before reuse. */
typedef struct {
    int64_t deadline_ms;
    const char *(*check)(void *context);
    void *context;
} SamosaMmControl;
/* Trusted lifecycle owner retains verified artifacts, process guardian, residency
   lease and descriptors. The transport only borrows the returned O_NONBLOCK
   endpoints. open runs after specialist reservation and must obey control.
   close returns NULL only after confirmed cleanup; errors keep ownership for
   retry. Neither callback may reenter this session/supervisor. */
typedef struct {
    const char *(*open)(void *, const SamosaMmProvider *, const SamosaMmControl *, int *, int *);
    const char *(*close)(void *);
    void *context;
} SamosaMmLifecycle;

typedef struct {
    SamosaMmSupervisor *supervisor;
    SamosaMmProtocol protocol;
    pid_t pid;
    pid_t pgid;
    int in_fd;
    int out_fd;
    int active;
    int request_timeout_ms;
    int shutdown_grace_ms;
    size_t max_frame_bytes;
    uint64_t generation;
    char provider[SAMOSA_MM_PROVIDER_NAME_MAX];
    SamosaMmLifecycle lifecycle;
} SamosaMmSession;

int samosa_mm_supervisor_init(SamosaMmSupervisor *supervisor);
/* All session closes must be confirmed before destroying their supervisor. */
void samosa_mm_supervisor_destroy(SamosaMmSupervisor *supervisor);

/* Starts a process group, negotiates the configured private protocol, and
   holds the global specialist lease until session_close. */
int samosa_mm_session_start(SamosaMmSupervisor *supervisor,
                            const SamosaMmProvider *provider,
                            SamosaMmSession *session,
                            char *error_code, size_t error_cap);

/* Additive lifecycle entry point. session must be zero-initialized or confirmed
   closed. Never call either start on an owned session. A failed start can retain cleanup ownership: always call close_owned
   and retain session/context until it returns NULL. The inherited ping/hello
   handshake proves transport only; the host must qualify real generation.
   Global supervisor_cancel signals only the original launcher; external owners
   cancel through their scoped controls/guardian and close_owned instead. */
int samosa_mm_session_start_owned(SamosaMmSupervisor *, const SamosaMmProvider *,
                                const SamosaMmLifecycle *, const SamosaMmControl *,
                                SamosaMmSession *, char *, size_t);
const char *samosa_mm_session_close_owned(SamosaMmSession *);

/* Sends one bounded JSON object and returns a malloc-owned, NUL-terminated
   reply.  The caller frees *reply.  Binary pixels are intentionally outside
   this protocol and are passed by validated private paths. */
int samosa_mm_session_request(SamosaMmSession *session,
                              const char *json, size_t json_len,
                              char **reply, size_t *reply_len,
                              char *error_code, size_t error_cap);

int samosa_mm_session_request_controlled(SamosaMmSession *session,
                              const SamosaMmControl *control,
                              const char *json, size_t json_len,
                              char **reply, size_t *reply_len,
                              char *error_code, size_t error_cap);

/* Graceful quit, bounded wait, SIGTERM, then SIGKILL.  Safe on partially
   started sessions with the default launcher. External owners must use the
   typed close_owned and retain their context on unconfirmed cleanup. */
void samosa_mm_session_close(SamosaMmSession *session);

/* Used by the authenticated cancel and shutdown paths.  The owning request
   thread still performs the wait/reap and releases the lease. */
int samosa_mm_supervisor_cancel(SamosaMmSupervisor *supervisor);
pid_t samosa_mm_supervisor_active_pgid(SamosaMmSupervisor *supervisor);
int samosa_mm_supervisor_is_active(SamosaMmSupervisor *supervisor,
                                   char *provider, size_t provider_cap);

#ifdef __cplusplus
}
#endif

#endif
