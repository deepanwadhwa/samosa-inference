#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE

#include <samosa_inference/specialist_supervisor.h>

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

typedef struct {
    SamosaMmSession *session;
    int result;
    char error[96];
} HangingRequest;

static void *request_hang(void *opaque) {
    HangingRequest *request = opaque;
    char *reply = NULL; size_t reply_len = 0;
    request->result = samosa_mm_session_request(
        request->session, "{\"command\":\"hang\"}", 18,
        &reply, &reply_len, request->error, sizeof(request->error));
    free(reply);
    return NULL;
}

static SamosaMmProvider provider(const char *engine, const char *model,
                                 SamosaMmProtocol protocol) {
    SamosaMmProvider value = {
        .provider = protocol == SAMOSA_MM_PROTOCOL_JSON_LINE ? "fixture_line" : "fixture_frame",
        .executable = engine,
        .model_dir = model,
        .protocol = protocol,
        .ready_timeout_ms = 2000,
        .request_timeout_ms = 2000,
        .shutdown_grace_ms = 100,
        .max_frame_bytes = 4096
    };
    return value;
}

static int64_t now_ms(void) {
    struct timespec now;
    assert(clock_gettime(CLOCK_MONOTONIC, &now) == 0);
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}
static const char *cancel_after(void *context) {
    return now_ms() >= *(int64_t *)context ? "fixture_request_cancelled" : NULL;
}

static void controlled_cases(SamosaMmSupervisor *supervisor,
                             const SamosaMmProvider *configuration) {
    for (int cancel = 0; cancel < 2; ++cancel) {
        SamosaMmSession session = {0}; char error[96];
        assert(samosa_mm_session_start(supervisor, configuration, &session, error, sizeof(error)));
        /* Cancellation before any write leaves the stream synchronized. */
        int64_t expired = now_ms() - 1;
        SamosaMmControl before = {.deadline_ms = expired};
        char *early_reply = NULL; size_t early_length = 0;
        assert(!samosa_mm_session_request_controlled(&session, &before, "{}", 2,
            &early_reply, &early_length, error, sizeof(error)));
        assert(!strcmp(error, "multimodal_deadline_exceeded") && session.active);
        const char *echo = "{\"command\":\"inspect\"}";
        assert(samosa_mm_session_request(&session, echo, strlen(echo),
            &early_reply, &early_length, error, sizeof(error)));
        free(early_reply);
        int64_t started = now_ms(), stop = started + 100;
        SamosaMmControl control = {.deadline_ms = cancel ? 0 : stop,
            .check = cancel ? cancel_after : NULL, .context = &stop};
        char *reply = NULL; size_t length = 0;
        const char *hang = "{\"command\":\"hang\"}";
        assert(!samosa_mm_session_request_controlled(&session, &control, hang, strlen(hang),
            &reply, &length, error, sizeof(error)));
        assert(!strcmp(error, cancel ? "fixture_request_cancelled" : "multimodal_deadline_exceeded"));
        assert(now_ms() - started < 1000);
        assert(!reply && !length && !session.active);
        /* Lease remains owned until the existing close path reaps the helper.
           A late response must never become another request's response. */
        assert(samosa_mm_supervisor_is_active(supervisor, NULL, 0));
        assert(!samosa_mm_session_request(&session, hang, strlen(hang),
            &reply, &length, error, sizeof(error)));
        assert(!strcmp(error, "multimodal_not_ready"));
        samosa_mm_session_close(&session);
        assert(!samosa_mm_supervisor_is_active(supervisor, NULL, 0));
    }
}

static void blocked_write_case(SamosaMmSupervisor *supervisor,
                               SamosaMmProvider configuration) {
    configuration.max_frame_bytes = 1024 * 1024;
    setenv("SAMOSA_FAKE_MM_STOP_READING", "1", 1);
    SamosaMmSession session = {0}; char error[96];
    assert(samosa_mm_session_start(supervisor, &configuration, &session, error, sizeof(error)));
    unsetenv("SAMOSA_FAKE_MM_STOP_READING");
    size_t size = 512 * 1024;
    char *message = malloc(size); assert(message); memset(message, 'x', size);
    char *reply = NULL; size_t length = 0;
    int64_t start = now_ms(), stop = start + 100;
    SamosaMmControl control = {.check = cancel_after, .context = &stop};
    assert(!samosa_mm_session_request_controlled(&session, &control, message, size,
        &reply, &length, error, sizeof(error)));
    assert(!strcmp(error, "fixture_request_cancelled"));
    assert(now_ms() - start < 1000 && !reply && !length && !session.active);
    free(message);
    samosa_mm_session_close(&session);
    assert(!samosa_mm_supervisor_is_active(supervisor, NULL, 0));
}

static void broken_pipe_case(void) {
    /* A caller need not change the application's process-wide SIGPIPE policy. */
    assert(signal(SIGPIPE, SIG_DFL) != SIG_ERR);
    int endpoints[2]; assert(pipe(endpoints) == 0);
    close(endpoints[0]);
    assert(fcntl(endpoints[1], F_SETFL, O_NONBLOCK) == 0);
    SamosaMmSession session = {.active = 1, .in_fd = endpoints[1],
        .out_fd = -1, .request_timeout_ms = 100, .max_frame_bytes = 4096};
    char *reply = NULL; size_t length = 0; char error[96];
    assert(!samosa_mm_session_request(&session, "{}", 2, &reply, &length, error, sizeof(error)));
    assert(!session.active && !reply && !length);
    close(endpoints[1]);
}

int main(int argc, char **argv) {
    int dispatch=samosa_mm_process_dispatch(argc,argv);if(dispatch>=0)return dispatch;
    assert(argc == 2 && argv[1][0] == '/');
    char model_template[] = "/tmp/samosa-mm-model-XXXXXX";
    char *model_dir = mkdtemp(model_template);
    assert(model_dir);

    SamosaMmSupervisor supervisor;
    assert(samosa_mm_supervisor_init(&supervisor));
    char error[96] = {0};

    unsetenv("SAMOSA_FAKE_MM_FRAMED");
    SamosaMmProvider line_provider = provider(argv[1], model_dir, SAMOSA_MM_PROTOCOL_JSON_LINE);
    SamosaMmSession line = {0};
    assert(samosa_mm_session_start(&supervisor, &line_provider, &line, error, sizeof(error)));
    assert(samosa_mm_supervisor_is_active(&supervisor, NULL, 0));

    SamosaMmSession refused = {0};
    assert(!samosa_mm_session_start(&supervisor, &line_provider, &refused, error, sizeof(error)));
    assert(!strcmp(error, "multimodal_specialist_busy"));

    char *reply = NULL; size_t reply_len = 0;
    const char *echo = "{\"command\":\"inspect\"}";
    assert(samosa_mm_session_request(&line, echo, strlen(echo), &reply, &reply_len,
                                     error, sizeof(error)));
    assert(reply_len == strlen(reply));
    assert(strstr(reply, "\"observation\":\"fixture"));
    free(reply);
    samosa_mm_session_close(&line);
    assert(!samosa_mm_supervisor_is_active(&supervisor, NULL, 0));

    setenv("SAMOSA_FAKE_MM_FRAMED", "1", 1);
    SamosaMmProvider frame_provider = provider(argv[1], model_dir, SAMOSA_MM_PROTOCOL_JSON_FRAME_V1);
    SamosaMmSession frame = {0};
    assert(samosa_mm_session_start(&supervisor, &frame_provider, &frame, error, sizeof(error)));
    const char *with_newline = "{\"command\":\"inspect\",\"prompt\":\"a\\nb\"}";
    assert(samosa_mm_session_request(&frame, with_newline, strlen(with_newline), &reply, &reply_len,
                                     error, sizeof(error)));
    free(reply);
    char oversized[5000]; memset(oversized, 'x', sizeof(oversized));
    assert(!samosa_mm_session_request(&frame, oversized, sizeof(oversized), &reply, &reply_len,
                                      error, sizeof(error)));
    assert(!strcmp(error, "multimodal_request_too_large"));
    samosa_mm_session_close(&frame);
    controlled_cases(&supervisor, &frame_provider);
    blocked_write_case(&supervisor, frame_provider);

    /* Cancellation must break a wedged request without waiting for its
       ordinary request timeout, after which close owns reap/lease release. */
    unsetenv("SAMOSA_FAKE_MM_FRAMED");
    SamosaMmSession hanging = {0};
    assert(samosa_mm_session_start(&supervisor, &line_provider, &hanging, error, sizeof(error)));
    HangingRequest request = {.session = &hanging};
    pthread_t thread;
    assert(pthread_create(&thread, NULL, request_hang, &request) == 0);
    struct timespec delay = {.tv_sec = 0, .tv_nsec = 100000000};
    while (nanosleep(&delay, &delay) != 0) {
        assert(errno == EINTR);
    }
    assert(samosa_mm_supervisor_cancel(&supervisor));
    pthread_join(thread, NULL);
    assert(!request.result);
    samosa_mm_session_close(&hanging);
    assert(!samosa_mm_supervisor_is_active(&supervisor, NULL, 0));

    controlled_cases(&supervisor, &line_provider);
    blocked_write_case(&supervisor, line_provider);
    broken_pipe_case();
    samosa_mm_supervisor_destroy(&supervisor);
    assert(rmdir(model_dir) == 0);
    puts("multimodal supervisor: PASS");
    return 0;
}
