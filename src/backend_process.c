#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/backend_process.h>
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* Existing primary-backend implementation, with explicit trusted owner inputs. */
static int backend_write_small_file(const char *path, const char *text) {
    char temp[PATH_MAX];
    if (snprintf(temp, sizeof(temp), "%s.tmp.%ld", path, (long)getpid()) >=
        (int)sizeof(temp)) return 0;
    int out = open(temp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (out < 0) return 0;
    size_t length = strlen(text), written = 0;
    int ok = 1;
    while (ok && written < length) {
        ssize_t n = write(out, text + written, length - written);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { ok = 0; break; }
        written += (size_t)n;
    }
    if (ok) ok = fsync(out) == 0;
    if (close(out)) ok = 0;
    if (ok) ok = rename(temp, path) == 0;
    if (!ok) unlink(temp);
    return ok;
}

static int backend_tcp_connect(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (struct sockaddr *)&address, sizeof(address))) {
        close(fd);
        return -1;
    }
    return fd;
}

static int backend_send_all(int fd, const void *data, size_t size) {
    const char *cursor=(const char *)data;
    while (size) {
#ifdef MSG_NOSIGNAL
        ssize_t n=send(fd,cursor,size,MSG_NOSIGNAL);
#else
        ssize_t n=send(fd,cursor,size,0);
#endif
        if (n<0 && errno==EINTR) continue;
        if (n<=0) return 0;
        cursor+=n; size-=(size_t)n;
    }
    return 1;
}

/* Reuse EngineProcess's WNOWAIT ownership pattern: keep the direct child
 * unreaped until its group is finished, so the group identity cannot be reused.
 * The old early return after waitpid left TERM-resistant helpers alive. */
static void backend_finish_child(pid_t pid, pid_t pgid) {
    if (pgid > 1 && pgid != getpgrp()) (void)kill(-pgid, SIGKILL);
    (void)kill(pid, SIGKILL);
    while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {}
}

pid_t samosa_backend_process_pid(const SamosaBackendOwner *owner) {
    pthread_mutex_lock(owner->mutex);
    pid_t pid = owner->process->pid;
    pthread_mutex_unlock(owner->mutex);
    return pid;
}

int samosa_backend_process_running(const SamosaBackendOwner *owner) {
    pthread_mutex_lock(owner->mutex);
    pid_t pid = owner->process->pid;
    if (pid <= 0) {
        pthread_mutex_unlock(owner->mutex);
        return 0;
    }
    siginfo_t info = {0};
    int observed = waitid(P_PID, (id_t)pid, &info, WEXITED | WNOHANG | WNOWAIT);
    if ((!observed && info.si_pid == pid) || (observed < 0 && errno == ECHILD)) {
        if (!observed) backend_finish_child(pid, owner->process->pgid);
        owner->process->pid = 0;
        owner->process->pgid = 0;
        unlink(owner->pid_file);
        pthread_mutex_unlock(owner->mutex);
        return 0;
    }
    pthread_mutex_unlock(owner->mutex);
    return observed == 0;
}

int samosa_backend_process_probe(const SamosaBackendOwner *owner, const char *backend, int port) {
    if (!samosa_backend_process_running(owner)) return 0;
    int fd = backend_tcp_connect(port);
    if (fd < 0) return 0;
    /* A process can accept the TCP connection before its HTTP health handler
       is usable (or accept and then wedge). Without socket deadlines the
       selection watchdog blocks forever inside recv(), so the UI remains on
       "Starting..." and its own readiness deadline is never observed. Keep
       each probe bounded; the watchdog will retry until its overall deadline. */
    struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    const char *path = !strcmp(backend, "qwen") ? "/healthz" :
                       !strcmp(backend, "maple") ? "/healthz" : "/health";
    char request[256];
    int n = snprintf(request, sizeof(request),
                     "GET %s HTTP/1.1\r\nHost: 127.0.0.1:%d\r\nConnection: close\r\n\r\n",
                     path, port);
    char response[64] = {0};
    int ok = n > 0 && backend_send_all(fd, request, (size_t)n) &&
             recv(fd, response, sizeof(response) - 1, 0) > 0 &&
             strstr(response, " 200 ") != NULL;
    close(fd);
    return ok;
}

void samosa_backend_process_stop(const SamosaBackendOwner *owner, const SamosaBackendStopControl *control) {
    pthread_mutex_lock(owner->mutex);
    pid_t pid = owner->process->pid;
    pid_t pgid = owner->process->pgid;
    owner->process->pid = 0;
    owner->process->pgid = 0;
    unlink(owner->pid_file);
    if (control && control->detach) control->detach(control->context);
    pthread_mutex_unlock(owner->mutex);
    if (control && control->interrupted) control->interrupted(control->context);
    if (pid <= 0) return;
    siginfo_t info = {0};
    if (waitid(P_PID, (id_t)pid, &info, WEXITED | WNOHANG | WNOWAIT) < 0 && errno == ECHILD) return;
    /* llama-server may create helper processes (for example Metal/runtime
       helpers). Give every backend its own process group so closing the app
       cannot leave one of those children behind after the tracked server is
       gone. The guard prevents a failed setpgid() from ever targeting the
       gateway's own group. */
    if (pgid > 1 && pgid != getpgrp()) (void)kill(-pgid, SIGTERM);
    kill(pid, SIGTERM);
    for (int i = 0; i < 80; ++i) {
        memset(&info, 0, sizeof(info));
        int observed = waitid(P_PID, (id_t)pid, &info, WEXITED | WNOHANG | WNOWAIT);
        if (observed < 0 && errno == ECHILD) return;
        if (!observed && info.si_pid == pid) break;
        struct timespec pause = {.tv_sec = 0, .tv_nsec = 100000000};
        nanosleep(&pause, NULL);
    }
    backend_finish_child(pid, pgid);
}

int samosa_backend_process_start(const SamosaBackendOwner *owner, const SamosaBackendLaunch *config) {
    /* Never launch into a port already owned by an untracked process. Before
       this guard, the new child would fail its bind while backend_probe()
       happily accepted the old process's /health response. That made every
       model switch fail and then falsely reported Qwen as healthy. */
    int occupied = backend_tcp_connect(config->port);
    if (occupied >= 0) {
        close(occupied);
        fprintf(stderr, "samosa-gateway: refusing to start %s: private backend port %d "
                        "is already occupied by an untracked process\n",
                config->backend, config->port);
        return 0;
    }
    const SamosaBackendRuntime *runtime = &config->runtime;
    pid_t pid = fork();
    if (pid < 0) return 0;
    if (pid == 0) {
        /* Keep llama-server and any helpers it creates in a group owned by
           this gateway. The parent repeats this call below to close the
           small fork/exec race before it records the PID. */
        (void)setpgid(0, 0);
        /* SAMOSA_BIND controls only the authenticated public gateway. Native
           qwen/maple backends also use samosa_http.h and inherit the gateway
           environment, so pin the child back to loopback before exec. Without
           this, `--lan` would accidentally expose the raw model port + 1. */
        setenv("SAMOSA_BIND", "127.0.0.1", 1);
        unsetenv("SAMOSA_LAN");
        unsetenv("SAMOSA_LAN_PASSWORD");
        int log = open(config->log, O_WRONLY | O_CREAT | O_APPEND, 0600);
        if (log >= 0) { dup2(log, STDOUT_FILENO); dup2(log, STDERR_FILENO); close(log); }
        char port[16];
        snprintf(port, sizeof(port), "%d", config->port);
        if (!strcmp(config->backend, "qwen")) {
            setenv("SNAP", config->model, 1);
            setenv("TOKENIZER", config->tokenizer, 1);
            setenv("SAMOSA_CHATS_DIR", config->chats, 1);
            char threads[16], context[16], threshold[16];
            if (!runtime->cpu_locked && !runtime->cpu_auto) {
                snprintf(threads, sizeof(threads), "%d", runtime->cpu_effective);
                setenv("OMP_NUM_THREADS", threads, 1);
            }
            if (!runtime->context_locked) {
                if (runtime->context_auto) setenv("SAMOSA_CONTEXT_TOKENS", "auto", 1);
                else {
                    snprintf(context, sizeof(context), "%d", runtime->context_effective);
                    setenv("SAMOSA_CONTEXT_TOKENS", context, 1);
                }
            }
            snprintf(threshold, sizeof(threshold), "%d", runtime->compact_threshold_percent);
            setenv("SAMOSA_AUTO_COMPACT", runtime->auto_compact ? "1" : "0", 1);
            setenv("SAMOSA_COMPACT_THRESHOLD", threshold, 1);
            fprintf(stderr, "[samosa] runtime: qwen threads=%d (%s) context=%d (%s) compact=%s@%d%%\n",
                    runtime->cpu_effective, runtime->cpu_source,
                    runtime->context_effective, runtime->context_source,
                    runtime->auto_compact ? "on" : "off", runtime->compact_threshold_percent);
            execl(config->engine, config->engine, "--serve", "--port", port,
                  "--tokenizer", config->tokenizer, (char *)NULL);
        } else if (!strcmp(config->backend, "maple")) {
            fprintf(stderr, "[samosa] runtime: starting maple backend on port %s\n", port);
            execl(config->engine, config->engine, "--model-dir", config->model, "--port", port, (char *)NULL);
        } else {
            SamosaLlamaLaunch launch = config->llama;
            (void)samosa_llama_launch(&launch);
        }
        _Exit(127);
    }
    int grouped = setpgid(pid, pid) == 0;
    if (!grouped) {
        pid_t current = getpgid(pid);
        grouped = current == pid;
    }
    pthread_mutex_lock(owner->mutex);
    owner->process->pid = pid;
    owner->process->pgid = grouped ? pid : 0;
    char pid_text[32];
    snprintf(pid_text, sizeof(pid_text), "%ld\n", (long)pid);
    if (!backend_write_small_file(owner->pid_file, pid_text))
        fprintf(stderr, "samosa-gateway: could not persist backend pid marker %s\n",
                owner->pid_file);
    pthread_mutex_unlock(owner->mutex);
    return 1;
}
