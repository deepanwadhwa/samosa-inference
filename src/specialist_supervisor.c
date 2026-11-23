#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE

#include <samosa_inference/specialist_supervisor.h>
#include "specialist_supervisor_internal.h"

#include "controlled_pipe.h"
#include "engine_process.h"
#include "json.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void mm_error(char *out, size_t cap, const char *code) {
    if (!out || !cap) return;
    snprintf(out, cap, "%s", code ? code : "multimodal_error");
}

static long long mm_now_ms(void) {
    struct timespec ts;
    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0) return 0;
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}

static int mm_regular_executable(const char *path) {
    struct stat st;
    return path && path[0] == '/' && lstat(path, &st) == 0 &&
           S_ISREG(st.st_mode) && !(st.st_mode & S_IWOTH) &&
           access(path, X_OK) == 0;
}

static int mm_directory_no_symlink(const char *path) {
    struct stat st;
    return path && path[0] == '/' && lstat(path, &st) == 0 && S_ISDIR(st.st_mode);
}

static int mm_set_nonblock(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    return flags >= 0 && fcntl(fd, F_SETFL, flags | O_NONBLOCK) == 0;
}

typedef struct {
    const SamosaMmControl *control;
    const char *error;
} MmRequestControl;

static int mm_check(MmRequestControl *request) {
    if (!request || !request->control) return 1;
    const SamosaMmControl *control = request->control;
    if (control->check) request->error = control->check(control->context);
    if (!request->error && control->deadline_ms > 0 &&
        mm_now_ms() >= control->deadline_ms)
        request->error = "multimodal_deadline_exceeded";
    return request->error == NULL;
}

static int mm_wait_fd(int fd, short events, long long deadline, MmRequestControl *request) {
    for (;;) {
        if (!mm_check(request)) return 0;
        long long left = deadline - mm_now_ms();
        if (left <= 0) return 0;
        struct pollfd pfd = {.fd = fd, .events = events};
        int wait_ms = left > INT_MAX ? INT_MAX : (int)left;
        if (request && request->control && wait_ms > 10) wait_ms = 10;
        int rc = poll(&pfd, 1, wait_ms);
        if (rc < 0 && errno == EINTR) continue;
        if (!mm_check(request)) return 0;
        if (rc == 0 && request && request->control) continue;
        if (rc <= 0) return 0;
        if (pfd.revents & (POLLERR | POLLNVAL)) return 0;
        if ((events & POLLOUT) && (pfd.revents & POLLHUP)) return 0;
        if (pfd.revents & events) return 1;
        if ((events & POLLIN) && (pfd.revents & POLLHUP)) return 1;
    }
}

static int mm_write_deadline(int fd, const void *bytes_, size_t len,
                             long long deadline, MmRequestControl *request) {
    const unsigned char *bytes = (const unsigned char *)bytes_;
    size_t at = 0;
    while (at < len) {
        if (!mm_wait_fd(fd, POLLOUT, deadline, request)) return 0;
        ssize_t n = controlled_pipe_write(fd, bytes + at, len - at);
        if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (n <= 0) return 0;
        at += (size_t)n;
    }
    return 1;
}

static int mm_read_exact_deadline(int fd, void *bytes_, size_t len,
                                  long long deadline, MmRequestControl *request) {
    unsigned char *bytes = (unsigned char *)bytes_;
    size_t at = 0;
    while (at < len) {
        if (!mm_wait_fd(fd, POLLIN, deadline, request)) return 0;
        ssize_t n = read(fd, bytes + at, len - at);
        if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
        if (n <= 0) return 0;
        at += (size_t)n;
    }
    return 1;
}

static int mm_send(SamosaMmSession *session, const char *json, size_t len,
                   int timeout_ms, MmRequestControl *request) {
    long long deadline = mm_now_ms() + timeout_ms;
    if (request && request->control && request->control->deadline_ms > 0 &&
        request->control->deadline_ms < deadline) deadline = request->control->deadline_ms;
    if (session->protocol == SAMOSA_MM_PROTOCOL_JSON_FRAME_V1) {
        if (len == 0 || len > UINT32_MAX || len > session->max_frame_bytes) return 0;
        uint32_t encoded = htonl((uint32_t)len);
        return mm_write_deadline(session->in_fd, &encoded, sizeof(encoded), deadline, request) &&
               mm_write_deadline(session->in_fd, json, len, deadline, request);
    }
    if (!len || len + 1 > session->max_frame_bytes || memchr(json, '\n', len)) return 0;
    return mm_write_deadline(session->in_fd, json, len, deadline, request) &&
           mm_write_deadline(session->in_fd, "\n", 1, deadline, request);
}

static int mm_receive(SamosaMmSession *session, char **reply, size_t *reply_len,
                      int timeout_ms, MmRequestControl *request) {
    long long deadline = mm_now_ms() + timeout_ms;
    if (request && request->control && request->control->deadline_ms > 0 &&
        request->control->deadline_ms < deadline) deadline = request->control->deadline_ms;
    size_t len = 0;
    char *result = NULL;
    if (session->protocol == SAMOSA_MM_PROTOCOL_JSON_FRAME_V1) {
        uint32_t encoded = 0;
        if (!mm_read_exact_deadline(session->out_fd, &encoded, sizeof(encoded), deadline, request)) return 0;
        len = ntohl(encoded);
        if (!len || len > session->max_frame_bytes) return 0;
        result = (char *)malloc(len + 1);
        if (!result) return 0;
        if (!mm_read_exact_deadline(session->out_fd, result, len, deadline, request)) {
            free(result);
            return 0;
        }
    } else {
        size_t cap = session->max_frame_bytes;
        result = (char *)malloc(cap + 1);
        if (!result) return 0;
        while (len < cap) {
            if (!mm_wait_fd(session->out_fd, POLLIN, deadline, request)) {
                free(result);
                return 0;
            }
            unsigned char c = 0;
            ssize_t n = read(session->out_fd, &c, 1);
            if (n < 0 && (errno == EINTR || errno == EAGAIN || errno == EWOULDBLOCK)) continue;
            if (n <= 0) { free(result); return 0; }
            if (c == '\n') break;
            result[len++] = (char)c;
        }
        if (len == cap) { free(result); return 0; }
    }
    if (!mm_check(request)) { free(result); return 0; }
    result[len] = 0;
    *reply = result;
    if (reply_len) *reply_len = len;
    return 1;
}

static int mm_reply_ok(const char *json, size_t len) {
    /* Handshake replies are helper-owned, bounded JSON.  Full request replies
       are parsed by the gateway.  Keep this check deliberately strict enough
       not to accept an error containing the word "ok" in a message. */
    static const char needle[] = "\"status\":\"ok\"";
    if (!json || len < sizeof(needle) - 1) return 0;
    for (size_t i = 0; i + sizeof(needle) - 1 <= len; ++i)
        if (!memcmp(json + i, needle, sizeof(needle) - 1)) return 1;
    return 0;
}

static void mm_supervisor_release(SamosaMmSession *session) {
    if (!session || !session->supervisor) return;
    SamosaMmSupervisor *supervisor = session->supervisor;
    pthread_mutex_lock(&supervisor->mutex);
    if (supervisor->generation == session->generation &&
        (supervisor->active_pid == session->pid || supervisor->reserved)) {
        supervisor->active_pid = 0;
        supervisor->active_pgid = 0;
        supervisor->reserved = 0;
        supervisor->active_provider[0] = 0;
    }
    pthread_mutex_unlock(&supervisor->mutex);
}

void mm_session_publish(SamosaMmSession *session) {
    SamosaMmSupervisor *supervisor=session->supervisor;
    pthread_mutex_lock(&supervisor->mutex);
    if (supervisor->generation==session->generation) {
        supervisor->active_pid=session->pid;
        supervisor->active_pgid=session->pgid;
        supervisor->reserved=0;
    }
    pthread_mutex_unlock(&supervisor->mutex);
}

typedef struct {
    EngineProcess local_process, *process;
    int closing,directory;
    char workspace[PATH_MAX];
    const char *(*verify)(void *);
    void *verify_context;
    SamosaMmControl probe_control;
} MmGuard;
static void mm_wait_engine(MmGuard *guard,long long deadline) {
    while(mm_now_ms()<deadline&&engine_process_running(guard->process)){
        struct timespec pause={0,20000000};nanosleep(&pause,NULL);
    }
}
static const char *mm_guard_close(SamosaMmSession *session) {
    MmGuard *guard=session->guardian;
    if(!guard->closing){
        guard->closing=1;
        int graceful=session->active;
        if(graceful&&session->in_fd>=0){const char *quit="{\"command\":\"quit\"}";
            (void)mm_send(session,quit,strlen(quit),250,NULL);}
        if(session->in_fd>=0)close(session->in_fd);
        if(session->out_fd>=0)close(session->out_fd);
        session->in_fd=session->out_fd=-1;
        if(graceful)mm_wait_engine(guard,mm_now_ms()+session->shutdown_grace_ms);
        /* Guardian reserves the leader until acknowledgement. Keep the original
         * grace/TERM750ms/KILL sequence and finish helpers before releasing it. */
        if(session->pid>1&&engine_process_running(guard->process)){
            (void)kill(-session->pid,SIGTERM);mm_wait_engine(guard,mm_now_ms()+750);
            if(engine_process_running(guard->process))(void)kill(-session->pid,SIGKILL);
        }
        session->active=0;
        /* Withdraw signal publication while identity is still reserved, but
         * retain reservation/context until guardian cleanup is confirmed. */
        SamosaMmSupervisor *supervisor=session->supervisor;
        pthread_mutex_lock(&supervisor->mutex);
        if(supervisor->generation==session->generation){supervisor->active_pid=supervisor->active_pgid=0;supervisor->reserved=1;}
        pthread_mutex_unlock(&supervisor->mutex);
    }
    const char *error=engine_process_stop(guard->process);
    if(error&&!strcmp(error,"engine_shutdown_pending"))return error;
    if(error&&!strcmp(error,"engine_guardian_failed")&&
       (session->pid>0||guard->process!=&guard->local_process))return error;
    if(guard->directory>=0){
        if(!error||strcmp(error,"engine_guardian_failed"))
            engine_workspace_discard_preparation(ENGINE_WORKSPACE_SPECIALIST,guard->directory,guard->workspace);
        close(guard->directory);
    }
    free(guard);session->guardian=NULL;return NULL;
}

int samosa_mm_supervisor_init(SamosaMmSupervisor *supervisor) {
    if (!supervisor) return 0;
    memset(supervisor, 0, sizeof(*supervisor));
    return pthread_mutex_init(&supervisor->mutex, NULL) == 0;
}

void samosa_mm_supervisor_destroy(SamosaMmSupervisor *supervisor) {
    if (!supervisor) return;
    (void)samosa_mm_supervisor_cancel(supervisor);
    pthread_mutex_destroy(&supervisor->mutex);
}

static int mm_session_start(SamosaMmSupervisor *supervisor,
                            const SamosaMmProvider *provider,
                            SamosaMmSession *session,
                            const SamosaMmLifecycle *lifecycle, const SamosaMmControl *control,
                            EngineProcess *admitted, int lease, const char *workspace_root,
                            char *error_code, size_t error_cap) {
    if (!supervisor || !provider || !session || !provider->provider ||
        !provider->executable || !provider->model_dir) {
        mm_error(error_code, error_cap, "multimodal_invalid_provider");
        return 0;
    }
    memset(session, 0, sizeof(*session));
    session->in_fd = session->out_fd = -1;
    if (!mm_regular_executable(provider->executable)) {
        mm_error(error_code, error_cap, "multimodal_engine_unavailable");
        return 0;
    }
    if (!mm_directory_no_symlink(provider->model_dir)) {
        mm_error(error_code, error_cap, "multimodal_model_unavailable");
        return 0;
    }

    pthread_mutex_lock(&supervisor->mutex);
    if (supervisor->reserved || supervisor->active_pid > 0) {
        pthread_mutex_unlock(&supervisor->mutex);
        mm_error(error_code, error_cap, "multimodal_specialist_busy");
        return 0;
    }
    supervisor->reserved = 1;
    supervisor->generation++;
    session->generation = supervisor->generation;
    snprintf(supervisor->active_provider, sizeof(supervisor->active_provider),
             "%s", provider->provider);
    pthread_mutex_unlock(&supervisor->mutex);

    session->supervisor = supervisor;
    session->protocol = provider->protocol;
    session->request_timeout_ms = provider->request_timeout_ms > 0
                                ? provider->request_timeout_ms : 120000;
    session->shutdown_grace_ms = provider->shutdown_grace_ms > 0
                               ? provider->shutdown_grace_ms : 2000;
    session->max_frame_bytes = provider->max_frame_bytes > 0
                             ? provider->max_frame_bytes : SAMOSA_MM_DEFAULT_MAX_FRAME;
    snprintf(session->provider, sizeof(session->provider), "%s", provider->provider);

    MmRequestControl request = {.control = control};
    if (lifecycle) {
        session->lifecycle = *lifecycle;
        const char *error = NULL;
        if (!mm_check(&request)) error = request.error;
        else error = lifecycle->open(lifecycle->context, provider, control,
                                     &session->in_fd, &session->out_fd);
        if (!error && (!mm_set_nonblock(session->in_fd) || !mm_set_nonblock(session->out_fd)))
            error = "multimodal_pipe_failed";
        if (error) {
            mm_error(error_code, error_cap, error);
            (void)samosa_mm_session_close_owned(session);
            return 0;
        }
    } else {
        /* Capture inherited diagnostics before allocating pipes: a closed fd2
           must not be mistaken for a newly allocated protocol reader. */
        int inherited_error=fcntl(STDERR_FILENO,F_DUPFD_CLOEXEC,20);
        if(inherited_error<0&&errno==EBADF)inherited_error=open("/dev/null",O_WRONLY|O_CLOEXEC);
        if(inherited_error<0){mm_error(error_code,error_cap,"multimodal_pipe_failed");mm_supervisor_release(session);return 0;}
        int in_pipe[2] = {-1, -1}, out_pipe[2] = {-1, -1};
        if (pipe(in_pipe) != 0 || pipe(out_pipe) != 0) {
            close(inherited_error);
            if (in_pipe[0] >= 0) { close(in_pipe[0]); close(in_pipe[1]); }
            if (out_pipe[0] >= 0) { close(out_pipe[0]); close(out_pipe[1]); }
            mm_error(error_code, error_cap, "multimodal_pipe_failed");
            mm_supervisor_release(session);
            return 0;
        }
        int all[]={in_pipe[0],in_pipe[1],out_pipe[0],out_pipe[1]};int prepared=1;
        for(size_t i=0;i<4;i++)if(fcntl(all[i],F_SETFD,FD_CLOEXEC))prepared=0;
        MmGuard *guard=prepared?calloc(1,sizeof(*guard)):NULL;
        if(guard){
            guard->process=admitted?admitted:&guard->local_process;
            guard->directory=engine_workspace_prepare(workspace_root,ENGINE_WORKSPACE_SPECIALIST,
                guard->workspace,sizeof(guard->workspace),NULL);
        }
        if(!guard||guard->directory<0)prepared=0;
        char self[PATH_MAX],*command=NULL;size_t bytes=0;
        FILE *encoded=prepared&&engine_process_executable(self)?open_memstream(&command,&bytes):NULL;
        if(encoded){fputc('[',encoded);json_quote(encoded,provider->executable,strlen(provider->executable));fputc(',',encoded);json_quote(encoded,provider->model_dir,strlen(provider->model_dir));fputc(']',encoded);prepared=!fclose(encoded);}else prepared=0;
        if(guard)*guard->process=(EngineProcess){.guardian=-1,.control=-1,.response=-1};
        const char *error="engine_io_failed";
        if(prepared)
            error=mm_check(&request)?engine_process_start_specialist(self,command,in_pipe[0],out_pipe[1],
                inherited_error,guard->workspace,guard->directory,lease,guard->process):request.error;
        free(command);close(in_pipe[0]);close(out_pipe[1]);close(inherited_error);
        session->in_fd=in_pipe[1];session->out_fd=out_pipe[0];session->guardian=guard;
        if(error){
            if(!guard){close(session->in_fd);close(session->out_fd);session->in_fd=session->out_fd=-1;mm_supervisor_release(session);session->supervisor=NULL;}
            else if(admitted)(void)samosa_mm_session_close_owned(session);
            else samosa_mm_session_close(session);
            mm_error(error_code,error_cap,request.error?request.error:
                !strcmp(error,"engine_exec_failed")?"multimodal_readiness_failed":"multimodal_fork_failed");return 0;
        }
        session->pid=session->pgid=guard->process->engine;
        (void)mm_set_nonblock(session->in_fd);(void)mm_set_nonblock(session->out_fd);
        /* Activation can roll back and reap before its caller returns.
         * Publish the PID only after a verified turn is committed. */
        if (!admitted) mm_session_publish(session);
    }

    const char *hello = provider->protocol == SAMOSA_MM_PROTOCOL_JSON_FRAME_V1
        ? "{\"command\":\"hello\",\"protocol\":\"samosa.multimodal.v1\"}"
        : "{\"command\":\"ping\"}";
    int ready_timeout = provider->ready_timeout_ms > 0 ? provider->ready_timeout_ms : 20000;
    char *reply = NULL; size_t reply_len = 0;
    if (!mm_send(session, hello, strlen(hello), ready_timeout, &request) ||
        !mm_receive(session, &reply, &reply_len, ready_timeout, &request) ||
        !mm_reply_ok(reply, reply_len)) {
        free(reply);
        mm_error(error_code, error_cap, request.error ? request.error : "multimodal_readiness_failed");
        if(lifecycle||admitted)(void)samosa_mm_session_close_owned(session);else samosa_mm_session_close(session);
        return 0;
    }
    free(reply);
    session->active = 1;
    mm_error(error_code, error_cap, "");
    return 1;
}

int samosa_mm_session_start(SamosaMmSupervisor *supervisor,
                            const SamosaMmProvider *provider, SamosaMmSession *session,
                            char *error_code, size_t error_cap) {
    return mm_session_start(supervisor, provider, session, NULL, NULL, NULL, -1, NULL, error_code, error_cap);
}

int mm_session_start_admitted(SamosaMmSupervisor *supervisor, const SamosaMmProvider *provider,
        const SamosaMmControl *control, SamosaMmSession *session, EngineProcess *process, int lease,
        const char *workspace_root, char *error_code, size_t error_cap) {
    if (!session || session->supervisor || session->active || session->guardian || !process || lease<0) {
        mm_error(error_code,error_cap,"multimodal_invalid_lifecycle");
        return 0;
    }
    return mm_session_start(supervisor,provider,session,NULL,control,process,lease,workspace_root,error_code,error_cap);
}

int samosa_mm_session_start_owned(SamosaMmSupervisor *supervisor,
                            const SamosaMmProvider *provider, const SamosaMmLifecycle *lifecycle,
                            const SamosaMmControl *control, SamosaMmSession *session,
                            char *error_code, size_t error_cap) {
    if (!lifecycle || !lifecycle->open || !lifecycle->close || !session) {
        mm_error(error_code, error_cap, "multimodal_invalid_lifecycle"); return 0;
    }
    if (session->supervisor || session->active || session->pid || session->lifecycle.close || session->guardian) {
        mm_error(error_code, error_cap, "multimodal_session_owned"); return 0;
    }
    return mm_session_start(supervisor, provider, session, lifecycle, control, NULL, -1, NULL, error_code, error_cap);
}

int samosa_mm_session_request_controlled(SamosaMmSession *session,
                              const SamosaMmControl *control,
                              const char *json, size_t json_len,
                              char **reply, size_t *reply_len,
                              char *error_code, size_t error_cap) {
    if (reply) *reply = NULL;
    if (reply_len) *reply_len = 0;
    if (!session || !session->active || !reply || !json || !json_len) {
        mm_error(error_code, error_cap, "multimodal_not_ready");
        return 0;
    }
    MmGuard *guard=session->guardian;
    MmRequestControl request = {.control = control?control:guard?&guard->probe_control:NULL};
    if (json_len > session->max_frame_bytes) {
        mm_error(error_code, error_cap, "multimodal_request_too_large");
        return 0;
    }
    const char *verification = guard && guard->verify ? guard->verify(guard->verify_context) : NULL;
    if (verification) {
        session->active=0;
        mm_error(error_code,error_cap,verification);
        return 0;
    }
    if (!mm_check(&request)) {
        mm_error(error_code, error_cap, request.error);
        return 0;
    }
    if (!mm_send(session, json, json_len, session->request_timeout_ms, &request)) {
        session->active = 0;
        mm_error(error_code, error_cap, request.error ? request.error : "multimodal_pipe_error");
        return 0;
    }
    if (!mm_receive(session, reply, reply_len, session->request_timeout_ms, &request)) {
        session->active = 0;
        mm_error(error_code, error_cap, request.error ? request.error : "multimodal_timeout_or_protocol_error");
        return 0;
    }
    verification = guard && guard->verify ? guard->verify(guard->verify_context) : NULL;
    if (verification) {
        free(*reply);
        *reply=NULL;
        if (reply_len) *reply_len=0;
        session->active=0;
        mm_error(error_code,error_cap,verification);
        return 0;
    }
    mm_error(error_code, error_cap, "");
    return 1;
}

int samosa_mm_session_request(SamosaMmSession *session,
                              const char *json, size_t json_len,
                              char **reply, size_t *reply_len,
                              char *error_code, size_t error_cap) {
    return samosa_mm_session_request_controlled(session, NULL, json, json_len,
                                                reply, reply_len, error_code, error_cap);
}

const char *samosa_mm_session_close_owned(SamosaMmSession *session) {
    if (!session) return NULL;
    if (session->lifecycle.close) {
        session->active = 0;
        const char *error = session->lifecycle.close(session->lifecycle.context);
        if (error) return error;
        /* Only the lifecycle owner closes/reaps its descriptors and child. */
        session->in_fd = session->out_fd = -1;
    } else if (session->guardian) {
        const char *error=mm_guard_close(session);if(error)return error;
    }
    mm_supervisor_release(session);
    session->active = 0;
    session->pid = session->pgid = 0;
    session->supervisor = NULL;
    session->lifecycle = (SamosaMmLifecycle){0};
    return NULL;
}

void samosa_mm_session_close(SamosaMmSession *session) {
    if(!session)return;
    if(session->guardian){while(samosa_mm_session_close_owned(session)){
        struct timespec pause={0,20000000};nanosleep(&pause,NULL);}}
    else (void)samosa_mm_session_close_owned(session);
}

int samosa_mm_supervisor_cancel(SamosaMmSupervisor *supervisor) {
    if (!supervisor) return 0;
    pthread_mutex_lock(&supervisor->mutex);
    pid_t pid = supervisor->active_pid;
    pid_t pgid = supervisor->active_pgid;
    if (pid <= 0) {pthread_mutex_unlock(&supervisor->mutex);return 0;}
    if (pgid > 1 && pgid != getpgrp()) (void)kill(-pgid, SIGTERM);
    else (void)kill(pid, SIGTERM);
    pthread_mutex_unlock(&supervisor->mutex);
    return 1;
}

pid_t samosa_mm_supervisor_active_pgid(SamosaMmSupervisor *supervisor) {
    if (!supervisor) return 0;
    pthread_mutex_lock(&supervisor->mutex);
    pid_t pgid = supervisor->active_pgid;
    pthread_mutex_unlock(&supervisor->mutex);
    return pgid;
}

int samosa_mm_supervisor_is_active(SamosaMmSupervisor *supervisor,
                                   char *provider, size_t provider_cap) {
    if (!supervisor) return 0;
    pthread_mutex_lock(&supervisor->mutex);
    int active = supervisor->reserved || supervisor->active_pid > 0;
    if (provider && provider_cap)
        snprintf(provider, provider_cap, "%s", supervisor->active_provider);
    pthread_mutex_unlock(&supervisor->mutex);
    return active;
}

int samosa_mm_process_dispatch(int argc,char **argv) {return engine_guardian_dispatch(argc,argv);}

void mm_session_probe_control(SamosaMmSession *session, const SamosaMmControl *control) {
    MmGuard *guard=session->guardian;
    guard->probe_control=control?*control:(SamosaMmControl){0};
}

void mm_session_set_verifier(SamosaMmSession *session, const char *(*check)(void *), void *context) {
    MmGuard *guard=session->guardian;
    guard->verify=check;
    guard->verify_context=context;
}

int mm_session_probe_file(SamosaMmSession *session, const void *data, size_t size, char *path, size_t capacity) {
    MmGuard *guard=session?session->guardian:NULL;
    if (!guard || guard->directory<0 || !data || !size || !path || !capacity ||
        snprintf(path,capacity,"%s/input.ppm",guard->workspace)>=(int)capacity) return 0;
    int fd=openat(guard->directory,"input.ppm",O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
    if (fd<0) return 0;
    size_t offset=0;
    while (offset<size) {
        ssize_t count=write(fd,(const char *)data+offset,size-offset);
        if (count<0&&errno==EINTR) continue;
        if (count<=0) break;
        offset+=(size_t)count;
    }
    int ok=!close(fd)&&offset==size;
    if (!ok) (void)unlinkat(guard->directory,"input.ppm",0);
    return ok;
}

int samosa_mm_session_take_temporary_image(SamosaMmSession *session,const char *source,char *out,size_t capacity) {
    if(!session||!session->active||!session->guardian||!source||!out)return 0;
    MmGuard *guard=session->guardian;struct stat directory,named,image;
    char path[PATH_MAX];
    if(guard->closing||guard->directory<0||fstat(guard->directory,&directory)||lstat(guard->workspace,&named)||
       directory.st_dev!=named.st_dev||directory.st_ino!=named.st_ino||lstat(source,&image)||
       !S_ISREG(image.st_mode)||image.st_uid!=geteuid()||image.st_nlink!=1||
       snprintf(path,sizeof(path),"%s/input.ppm",guard->workspace)>=(int)sizeof(path)||strlen(path)>=capacity)return 0;
    if(rename(source,path))return 0;
    if(chmod(path,0600))return 0;
    memcpy(out,path,strlen(path)+1);return 1;
}

int samosa_mm_session_return_temporary_image(SamosaMmSession *session,const char *destination) {
    if(!session||!session->guardian||!destination)return 0;
    MmGuard *guard=session->guardian;struct stat directory,named,image;
    if(guard->closing||guard->directory<0||fstat(guard->directory,&directory)||lstat(guard->workspace,&named)||
       directory.st_dev!=named.st_dev||directory.st_ino!=named.st_ino||
       fstatat(guard->directory,"input.ppm",&image,AT_SYMLINK_NOFOLLOW)||!S_ISREG(image.st_mode)||image.st_nlink!=1)return 0;
    /* Link exclusively before unlinking our name; never replace another file.
       Caller regains ownership for the existing unload/lower-side/restart retry. */
    if(linkat(guard->directory,"input.ppm",AT_FDCWD,destination,0))return 0;
    if(unlinkat(guard->directory,"input.ppm",0)){unlink(destination);return 0;}
    return 1;
}
