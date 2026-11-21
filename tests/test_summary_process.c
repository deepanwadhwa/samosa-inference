#define _POSIX_C_SOURCE 200809L
#include <samosa_inference/summary_process.h>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <poll.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void exact_read(int fd, void *data, size_t length) {
    unsigned char *p = data;
    while (length) {
        struct pollfd ready = {.fd = fd, .events = POLLIN};
        assert(poll(&ready, 1, 5000) > 0);
        ssize_t count = read(fd, p, length);
        assert(count > 0);
        p += count; length -= (size_t)count;
    }
}
static void request(SamosaSummaryProcess *process) {
    const char prompt[] = "summarize: Retained resident engine answers the next request.";
    uint32_t size = htonl(sizeof(prompt) - 1);
    assert(write(process->write_fd, &size, sizeof(size)) == sizeof(size));
    assert(write(process->write_fd, prompt, sizeof(prompt)-1) == sizeof(prompt)-1);
    exact_read(process->read_fd, &size, sizeof(size));
    size = ntohl(size);
    char output[256] = {0};
    assert(size > 0 && size < sizeof(output));
    exact_read(process->read_fd, output, size);
    assert(!strcmp(output, prompt + 11));
}
typedef struct {
    pthread_mutex_t mutex;
    pthread_cond_t changed;
    int ready, go;
} StartGate;
typedef struct {
    SamosaSummaryProcess *process;
    SamosaSummaryProcessConfig config;
    StartGate *gate;
    int index, queued, running;
} CoalescedRequest;
static void activity(void *context, const char *event) {
    CoalescedRequest *request = context;
    if (!strcmp(event, "summary_queued")) request->queued++;
    else if (!strcmp(event, "summarizing")) request->running++;
    else assert(0);
}
static void *coalesced_request(void *context) {
    CoalescedRequest *request = context;
    pthread_mutex_lock(&request->gate->mutex);
    request->gate->ready++;
    pthread_cond_broadcast(&request->gate->changed);
    while (!request->gate->go) pthread_cond_wait(&request->gate->changed, &request->gate->mutex);
    pthread_mutex_unlock(&request->gate->mutex);
    char source[128];
    snprintf(source, sizeof(source), "Concurrent source number %d keeps its distinct answer.", request->index);
    char *parts[] = {source}, *replies[] = {NULL};
    assert(samosa_summary_process_coalesce(request->process, &request->config, parts, 1, replies,
                                         request->index % 2, activity, request));
    assert(replies[0] && !strcmp(replies[0], source));
    assert(request->queued == 1 && request->running == 1);
    free(replies[0]);
    return NULL;
}
int main(int argc, char **argv) {
    assert(argc == 2);
    SamosaSummaryProcess process;
    assert(samosa_summary_process_init(&process));
    assert(process.pid == 0 && process.read_fd == -1 && process.write_fd == -1);
    char model[] = "/tmp/samosa-summary-process-model.XXXXXX";
    int model_fd = mkstemp(model); assert(model_fd >= 0); close(model_fd);
    char log[] = "/tmp/samosa-summary-process-log.XXXXXX";
    int log_fd = mkstemp(log); assert(log_fd >= 0); close(log_fd);
    SamosaSummaryProcessConfig config = {argv[1], model, log, "0"};
    pthread_mutex_lock(&process.mutex);
    SamosaSummaryProcessConfig invalid = {"/missing-summary-engine", model, log, "0"};
    assert(!samosa_summary_process_start_locked(&process, &invalid));
    int inherited[2]; assert(pipe(inherited) == 0);
    assert(samosa_summary_process_start_locked(&process, &config));
    pid_t resident = process.pid;
    assert(resident > 0);
    close(inherited[1]);
    /* Exec must not retain a caller socket/pipe and postpone EOF. */
    struct pollfd eof = {.fd = inherited[0], .events = POLLIN};
    assert(poll(&eof, 1, 5000) > 0);
    char byte; assert(read(inherited[0], &byte, 1) == 0); close(inherited[0]);
    for (int i = 0; i < 8; i++) {
        assert(samosa_summary_process_start_locked(&process, &config));
        assert(process.pid == resident); request(&process);
    }
    assert(kill(resident, SIGKILL) == 0);
    /* Observe exit without reaping: the process owner performs the reap. */
    siginfo_t info; memset(&info, 0, sizeof(info));
    assert(waitid(P_PID, (id_t)resident, &info, WEXITED | WNOWAIT) == 0);
    assert(samosa_summary_process_start_locked(&process, &config));
    assert(process.pid != resident); request(&process);
    pid_t replacement = process.pid;
    samosa_summary_process_stop_locked(&process);
    assert(process.pid == 0 && process.read_fd == -1 && process.write_fd == -1);
    assert(waitpid(replacement, NULL, WNOHANG) == -1 && errno == ECHILD);
    samosa_summary_process_stop_locked(&process);
    pthread_mutex_unlock(&process.mutex);

    const char text[] = "A resident summary preserves the source punctuation .";
    char *summary = samosa_summary_process_once(&process, &config, text, strlen(text));
    assert(summary && !strcmp(summary, "A resident summary preserves the source punctuation."));
    free(summary);
    resident = process.pid;
    assert(resident > 0 && process.warmed);
    assert(!samosa_summary_process_once(&process, &config, text, 60001));
    assert(process.pid == resident);
    char *parts[] = {"The first independently framed summary chunk.",
                     "The second independently framed summary chunk."};
    char *replies[2] = {NULL, NULL};
    for (int concise = 0; concise < 2; concise++) {
        assert(samosa_summary_process_batch(&process, &config, parts, 2, replies, concise));
        assert(process.pid == resident);
        for (int i = 0; i < 2; i++) {
            assert(!strcmp(replies[i], parts[i])); free(replies[i]); replies[i] = NULL;
        }
    }
    assert(!samosa_summary_process_batch(&process, &config, parts, 0, replies, 0));
    assert(process.pid == resident);
    char *bad_parts[] = {parts[0], "tiny"};
    assert(!samosa_summary_process_batch(&process, &config, bad_parts, 2, replies, 0));
    assert(!replies[0] && !replies[1] && process.pid == 0);
    assert(!samosa_summary_process_once(&process, &config, "tiny", 4));
    assert(process.pid == 0 && process.write_fd == -1 && process.read_fd == -1);
    summary = samosa_summary_process_once(&process, &config, text, strlen(text));
    assert(summary); free(summary);
    SamosaSummaryProcess peer;
    assert(samosa_summary_process_init(&peer));
    StartGate gate = {PTHREAD_MUTEX_INITIALIZER, PTHREAD_COND_INITIALIZER, 0, 0};
    CoalescedRequest jobs[8]; pthread_t threads[8];
    for (int i = 0; i < 8; i++) {
        jobs[i] = (CoalescedRequest){i < 4 ? &process : &peer, config, &gate, i, 0, 0};
        assert(!pthread_create(&threads[i], NULL, coalesced_request, &jobs[i]));
    }
    pthread_mutex_lock(&gate.mutex);
    while (gate.ready < 8) pthread_cond_wait(&gate.changed, &gate.mutex);
    gate.go = 1; pthread_cond_broadcast(&gate.changed);
    pthread_mutex_unlock(&gate.mutex);
    for (int i = 0; i < 8; i++) assert(!pthread_join(threads[i], NULL));
    assert(process.pid > 0 && peer.pid > 0 && process.pid != peer.pid);
    assert(!samosa_summary_process_coalesce(&process, &config, parts, 65, replies, 0, NULL, NULL));
    samosa_summary_process_destroy(&peer);
    pthread_cond_destroy(&gate.changed); pthread_mutex_destroy(&gate.mutex);
    samosa_summary_process_destroy(&process);
    unlink(model); unlink(log);
    puts("summary process: PASS (reuse, descriptors, crash/restart, scalar/batch, failure recovery, stop/reap)");
    return 0;
}
