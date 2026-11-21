#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/specialist_supervisor.h>
#include "summary_owned_fixture.h"
#include <assert.h>
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <time.h>

extern char **environ;

typedef struct {
    SummaryOwnedFixture guardian;
    char model[1024];
    int pending, fail_open;
} VisualOwner;
static const char *visual_open(void *context, const SamosaMmProvider *provider,
                              const SamosaMmControl *control, int *input, int *output) {
    VisualOwner *host = context;
    SamosaSummaryProcessConfig config = {.engine = provider->executable,
        .model = host->model, .gpu_layers = "0"};
    SamosaSummaryControl converted = {0};
    if (control) converted = (SamosaSummaryControl){control->deadline_ms, control->check, control->context};
    /* Reuse the already qualified guardian fixture/duplex launcher. The native
       multimodal fixture ignores argv; its original protocol loop is unchanged. */
    assert(!mkdir(host->guardian.workspace, 0700));
    char prepared[1200]; snprintf(prepared, sizeof(prepared), "%s/input.ppm", host->guardian.workspace);
    int image = open(prepared, O_CREAT | O_EXCL | O_WRONLY, 0600); assert(image >= 0); close(image);
    const char *error = owned_open(&host->guardian, &config, control ? &converted : NULL, input, output);
    return error ? error : host->fail_open ? "fixture_open_failed" : NULL;
}
static const char *visual_close(void *context) {
    VisualOwner *host = context;
    if (host->pending) { host->pending--; return "engine_shutdown_pending"; }
    return owned_close(&host->guardian);
}
static int64_t now_ms(void) {
    struct timespec now; assert(!clock_gettime(CLOCK_MONOTONIC, &now));
    return (int64_t)now.tv_sec * 1000 + now.tv_nsec / 1000000;
}
static void healthy(SamosaMmSession *session) {
    const char *request = "{\"command\":\"inspect\"}";
    char *reply = NULL, error[96]; size_t size = 0;
    assert(samosa_mm_session_request(session, request, strlen(request), &reply, &size, error, sizeof(error)));
    assert(size && strstr(reply, "\"status\":\"ok\"")); free(reply);
}
int main(int argc, char **argv) {
    int dispatch = engine_guardian_dispatch(argc, argv);
    if (dispatch >= 0) return dispatch;
    int owner_child = argc == 4 && !strcmp(argv[2], "--parent-owner");
    assert((argc == 2 || owner_child) && argv[1][0] == '/');
    char temporary[] = "/tmp/samosa-visual-owned-XXXXXX";
    const char *root = owner_child ? argv[3] : mkdtemp(temporary); assert(root);
    VisualOwner host = {0}; owned_init(&host.guardian, argv[0], root);
    snprintf(host.model, sizeof(host.model), "%s/model", root);
    snprintf(host.guardian.workspace, sizeof(host.guardian.workspace), "%s/workspace", root);
    int model = open(host.model, owner_child ? O_RDONLY : O_CREAT | O_WRONLY | O_EXCL, 0600); assert(model >= 0); close(model);
    char cache[1200], marker[1200];
    snprintf(cache, sizeof(cache), "%s/metal-cache-probe", root);
    snprintf(marker, sizeof(marker), "%s/owner-ready", root);
    setenv("SAMOSA_FAKE_MM_CACHE_PROBE", cache, 1);
    SamosaMmLifecycle lifecycle = {visual_open, visual_close, &host};
    SamosaMmSupervisor supervisor; assert(samosa_mm_supervisor_init(&supervisor));
    char error[96];
    if (owner_child) {
        unsetenv("SAMOSA_FAKE_MM_FRAMED");
        SamosaMmProvider provider = {.provider = "owned_visual", .executable = argv[1], .model_dir = root,
            .ready_timeout_ms = 3000, .max_frame_bytes = 4096};
        SamosaMmSession session = {0};
        assert(samosa_mm_session_start_owned(&supervisor, &provider, &lifecycle, NULL, &session, error, sizeof(error)));
        healthy(&session);
        int ready = open(marker, O_CREAT | O_EXCL | O_WRONLY, 0600); assert(ready >= 0); close(ready);
        for (;;) pause();
    }
    for (int framed = 0; framed < 2; framed++) {
        if (framed) setenv("SAMOSA_FAKE_MM_FRAMED", "1", 1);
        else unsetenv("SAMOSA_FAKE_MM_FRAMED");
        SamosaMmProvider provider = {.provider = "owned_visual", .executable = argv[1], .model_dir = root,
            .protocol = framed ? SAMOSA_MM_PROTOCOL_JSON_FRAME_V1 : SAMOSA_MM_PROTOCOL_JSON_LINE,
            .ready_timeout_ms = 3000, .request_timeout_ms = 3000, .max_frame_bytes = 4096};
        SamosaMmSession session = {0};
        assert(samosa_mm_session_start_owned(&supervisor, &provider, &lifecycle, NULL, &session, error, sizeof(error)));
        int guardian = host.guardian.process.guardian, input = session.in_fd, output = session.out_fd;
        assert(guardian > 0 && !owned_released(&host.guardian));
        healthy(&session); healthy(&session);
        struct stat cache_stat; assert(!stat(cache, &cache_stat) && cache_stat.st_size == 67108865);
        assert(host.guardian.process.guardian == guardian);
        assert(!samosa_mm_supervisor_cancel(&supervisor));
        healthy(&session); /* No direct signal of a PID owned by another owner. */
        SamosaMmSession refused = {0};
        assert(!samosa_mm_session_start(&supervisor, &provider, &refused, error, sizeof(error)));
        assert(!strcmp(error, "multimodal_specialist_busy"));
        host.pending = framed ? 1 : 0;
        if (!framed) {
            assert(!kill(guardian, SIGSTOP)); int stopped;
            assert(waitpid(guardian, &stopped, WUNTRACED) == guardian && WIFSTOPPED(stopped));
        }
        assert(!strcmp(samosa_mm_session_close_owned(&session), "engine_shutdown_pending"));
        assert(!session.active && session.supervisor == &supervisor && session.lifecycle.context == &host);
        assert(fcntl(input, F_GETFD) >= 0 && fcntl(output, F_GETFD) >= 0);
        assert(!access(host.guardian.workspace, F_OK));
        assert(!owned_released(&host.guardian) && samosa_mm_supervisor_is_active(&supervisor, NULL, 0));
        assert(!samosa_mm_session_start_owned(&supervisor, &provider, &lifecycle, NULL, &session, error, sizeof(error)));
        assert(!strcmp(error, "multimodal_session_owned"));
        if (!framed) assert(!kill(guardian, SIGCONT));
        assert(!samosa_mm_session_close_owned(&session));
        assert(!samosa_mm_session_close_owned(&session));
        assert(!session.supervisor && !session.lifecycle.close && session.in_fd == -1 && session.out_fd == -1);
        assert(access(host.guardian.workspace, F_OK) == -1 && errno == ENOENT);
        assert(owned_released(&host.guardian) && !samosa_mm_supervisor_is_active(&supervisor, NULL, 0));
        assert(waitpid(guardian, NULL, WNOHANG) == -1 && errno == ECHILD);

        /* Partial startup failure must retain owner state until confirmed close. */
        host.fail_open = 1; host.pending = 2;
        assert(!samosa_mm_session_start_owned(&supervisor, &provider, &lifecycle, NULL, &session, error, sizeof(error)));
        assert(!strcmp(error, "fixture_open_failed") && session.lifecycle.close);
        assert(!owned_released(&host.guardian));
        assert(!strcmp(samosa_mm_session_close_owned(&session), "engine_shutdown_pending"));
        assert(session.lifecycle.context == &host && samosa_mm_supervisor_is_active(&supervisor, NULL, 0));
        assert(!samosa_mm_session_close_owned(&session)); host.fail_open = 0;
        assert(owned_released(&host.guardian));

        /* Request deadline invalidates transport; typed close still owns reap. */
        assert(samosa_mm_session_start_owned(&supervisor, &provider, &lifecycle, NULL, &session, error, sizeof(error)));
        SamosaMmControl control = {.deadline_ms = now_ms() + 100};
        const char *hang = "{\"command\":\"hang\"}"; char *reply = NULL; size_t size = 0;
        assert(!samosa_mm_session_request_controlled(&session, &control, hang, strlen(hang), &reply, &size, error, sizeof(error)));
        assert(!strcmp(error, "multimodal_deadline_exceeded") && !reply && !size);
        assert(!owned_released(&host.guardian));
        assert(!samosa_mm_session_close_owned(&session) && owned_released(&host.guardian));

        /* A guardian-started helper exiting mid-request cannot be reused. */
        assert(samosa_mm_session_start_owned(&supervisor, &provider, &lifecycle, NULL, &session, error, sizeof(error)));
        const char *crash = "{\"command\":\"crash\"}";
        assert(!samosa_mm_session_request(&session, crash, strlen(crash), &reply, &size, error, sizeof(error)));
        assert(!session.active);
        assert(!samosa_mm_session_close_owned(&session) && owned_released(&host.guardian));
    }
    /* Owner death must trigger the reused guardian lifeline: reap the worker,
       clean the workspace, then release residency, leaving an unrelated peer. */
    pid_t peer = fork(); assert(peer >= 0); if (!peer) { for (;;) pause(); }
    char *arguments[] = {argv[0], argv[1], "--parent-owner", (char *)root, NULL};
    pid_t parent; assert(!posix_spawn(&parent, argv[0], NULL, NULL, arguments, environ));
    int64_t until = now_ms() + 5000;
    struct timespec tick = {0, 1000000};
    while (access(marker, F_OK) && now_ms() < until) nanosleep(&tick, NULL);
    assert(!access(marker, F_OK) && !owned_released(&host.guardian));
    assert(!kill(parent, SIGKILL)); int status; assert(waitpid(parent, &status, 0) == parent);
    until = now_ms() + 5000;
    while (!owned_released(&host.guardian) && now_ms() < until) nanosleep(&tick, NULL);
    assert(owned_released(&host.guardian));
    assert(access(host.guardian.workspace, F_OK) == -1 && errno == ENOENT);
    assert(!kill(peer, 0) && !kill(peer, SIGKILL) && waitpid(peer, &status, 0) == peer);
    assert(!unlink(marker) && !unlink(cache));
    samosa_mm_supervisor_destroy(&supervisor);
    assert(!unlink(host.model)); assert(!unlink(host.guardian.lease)); assert(!rmdir(root));
    puts("specialist owned guardian: PASS (both protocols, reuse, reservation, pending close, failed open, deadline, crash/reap, workspace, cache cap, owner death)");
    return 0;
}
