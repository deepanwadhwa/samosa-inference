#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/specialist_protocol.h>
#include <arpa/inet.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    int molmo, images, tokens, side, frames;
    const char *path, *kind, *prompt;
    const char *const *paths;
    double start, end;
} Request;
typedef struct {
    char request[2048], response[4096], event[64], fields[1024];
    int payloads, events;
} Trace;
typedef struct {
    char observation[256], error[96];
    int prompt_tokens, generated_tokens, frames, images, ok;
    double duration;
    Trace trace;
} Result;
static unsigned cases;
static void event(void *context, const char *name, const char *fields) {
    Trace *trace = context; trace->events++;
    snprintf(trace->event, sizeof(trace->event), "%s", name);
    snprintf(trace->fields, sizeof(trace->fields), "%s", fields ? fields : "");
}
static void payload(void *context, const char *name, const char *provider,
                    const char *body, size_t bytes) {
    Trace *trace = context;
    assert(!strcmp(provider, "samosa-molmo2") || !strcmp(provider, "samosa-visionpsy"));
    char *out = strstr(name, "raw_response") ? trace->response : trace->request;
    size_t capacity = strstr(name, "raw_response") ? sizeof(trace->response) : sizeof(trace->request);
    assert(bytes < capacity); memcpy(out, body, bytes); out[bytes] = 0;
    trace->payloads++;
}
static int transfer(int fd, void *body, size_t bytes, int writing) {
    size_t at = 0;
    while (at < bytes) {
        ssize_t n = writing ? write(fd, (char *)body + at, bytes - at)
                            : read(fd, (char *)body + at, bytes - at);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) return 0;
        at += (size_t)n;
    }
    return 1;
}
static void worker(int fd, int framed, const char *expected, const char *reply) {
    char request[2048]; size_t bytes = 0;
    if (framed) {
        uint32_t size;
        assert(transfer(fd, &size, sizeof(size), 0)); bytes = ntohl(size);
        assert(bytes < sizeof(request) && transfer(fd, request, bytes, 0));
    } else {
        while (bytes < sizeof(request) - 1) {
            assert(transfer(fd, request + bytes, 1, 0));
            if (request[bytes] == '\n') break;
            bytes++;
        }
    }
    request[bytes] = 0;
    assert(!strcmp(request, expected));
    if (reply) {
        if (framed) {
            uint32_t size = htonl((uint32_t)strlen(reply));
            assert(transfer(fd, &size, sizeof(size), 1));
        }
        assert(transfer(fd, (void *)reply, strlen(reply), 1));
        if (!framed) assert(transfer(fd, "\n", 1, 1));
    }
    close(fd); _Exit(0);
}
static void invoke(SamosaMmSession *session, const Request *request, Result *result) {
    memset(result, 0, sizeof(*result));
    strcpy(result->observation, "unchanged"); strcpy(result->error, "retained");
    result->prompt_tokens = result->generated_tokens = result->frames = result->images = -1;
    result->duration = -1;
    SamosaMmTrace trace = {event, payload, &result->trace};
    if (request->molmo)
        result->ok = samosa_mm_molmo2_analyze_request(session, &trace,
            request->path, request->paths, request->images, request->kind,
            request->prompt, request->tokens, request->start, request->end,
            request->frames, result->observation, sizeof(result->observation),
            &result->prompt_tokens, &result->generated_tokens, &result->duration,
            &result->frames, &result->images, result->error, sizeof(result->error));
    else
        result->ok = samosa_mm_visionpsy_inspect(session, &trace, request->path,
            request->prompt, request->side, request->tokens,
            result->observation, sizeof(result->observation), &result->prompt_tokens,
            &result->generated_tokens, result->error, sizeof(result->error));
    cases++;
}
static Result exchange(const Request *request, const char *expected, const char *reply) {
    int sockets[2]; assert(!socketpair(AF_UNIX, SOCK_STREAM, 0, sockets));
    pid_t child = fork(); assert(child >= 0);
    if (!child) { close(sockets[0]); worker(sockets[1], request->molmo, expected, reply); }
    close(sockets[1]); assert(fcntl(sockets[0], F_SETFL, O_NONBLOCK) == 0);
    SamosaMmSession session = {.active = 1, .in_fd = sockets[0], .out_fd = sockets[0],
        .protocol = request->molmo ? SAMOSA_MM_PROTOCOL_JSON_FRAME_V1 : SAMOSA_MM_PROTOCOL_JSON_LINE,
        .request_timeout_ms = 1500, .max_frame_bytes = 1048576};
    Result result; invoke(&session, request, &result);
    close(sockets[0]); int status;
    assert(waitpid(child, &status, 0) == child && WIFEXITED(status) && WEXITSTATUS(status) == 0);
    assert(!strcmp(result.trace.request, expected));
    assert(result.trace.payloads == (reply ? 2 : 1));
    if (reply) assert(!strcmp(result.trace.response, reply));
    return result;
}
int main(void) {
    Request image = {.molmo = 1, .path = "/tmp/input.ppm", .kind = "image", .prompt = "identify\n", .tokens = 16};
    const char *image_wire = "{\"command\":\"analyze\",\"id\":\"gateway\",\"media_path\":\"/tmp/input.ppm\",\"media_kind\":\"image\",\"prompt\":\"identify\\n\",\"max_tokens\":256,\"start_seconds\":0.000,\"end_seconds\":0.000,\"max_frames\":16}";
    const char *image_reply = "{\"status\":\"ok\",\"id\":\"gateway\",\"observation\":\"caf\\u00e9 \\ud83d\\udc26\",\"prompt_tokens\":3.0,\"generated_tokens\":4e0,\"images\":1,\"frames\":0,\"duration_seconds\":0}";
    Result result = exchange(&image, image_wire, image_reply);
    assert(result.ok && !result.error[0] && !strcmp(result.observation, "caf\xc3\xa9 \xf0\x9f\x90\xa6"));
    assert(result.prompt_tokens == 3 && result.generated_tokens == 4 && result.images == 1 && result.frames == 0 && result.duration == 0);

    const char *paths[] = {"/tmp/one.ppm", "/tmp/two.ppm"};
    Request joint = {.molmo = 1, .images = 2, .paths = paths, .tokens = 1024, .start = 1.25, .end = 4.5, .frames = 8};
    const char *joint_wire = "{\"command\":\"analyze\",\"id\":\"gateway\",\"media_paths\":[\"/tmp/one.ppm\",\"/tmp/two.ppm\"],\"media_kind\":\"images\",\"prompt\":\"Describe the visual content in detail.\",\"max_tokens\":1024,\"start_seconds\":1.250,\"end_seconds\":4.500,\"max_frames\":8}";
    result = exchange(&joint, joint_wire, "{\"status\":\"ok\",\"id\":\"gateway\",\"observation\":\"two\",\"prompt_tokens\":36864,\"generated_tokens\":1024,\"images\":2,\"frames\":16,\"duration_seconds\":10.125}");
    assert(result.ok && result.prompt_tokens == 36864 && result.generated_tokens == 1024 && result.images == 2 && result.frames == 16 && result.duration == 10.125);

    const char *bad_replies[] = {
        "[]", "{}", "{\"status\":\"ok\",\"id\":\"other\"}",
        "{\"status\":\"ok\",\"id\":\"gateway\",\"observation\":7}",
        "{\"status\":\"ok\",\"id\":\"gateway\",\"observation\":\"x\",\"prompt_tokens\":1.5,\"generated_tokens\":1,\"images\":1,\"frames\":0,\"duration_seconds\":0}",
        "{\"status\":\"ok\",\"id\":\"gateway\",\"observation\":\"x\",\"prompt_tokens\":-1,\"generated_tokens\":1,\"images\":1,\"frames\":0,\"duration_seconds\":0}",
        "{\"status\":\"ok\",\"id\":\"gateway\",\"observation\":\"x\",\"prompt_tokens\":36865,\"generated_tokens\":1,\"images\":1,\"frames\":0,\"duration_seconds\":0}",
        "{\"status\":\"ok\",\"id\":\"gateway\",\"observation\":\"x\",\"prompt_tokens\":1,\"generated_tokens\":1025,\"images\":1,\"frames\":0,\"duration_seconds\":0}",
        "{\"status\":\"ok\",\"id\":\"gateway\",\"observation\":\"x\",\"prompt_tokens\":1,\"generated_tokens\":1,\"images\":1,\"frames\":17,\"duration_seconds\":0}",
        "{\"status\":\"ok\",\"id\":\"gateway\",\"observation\":\"x\",\"prompt_tokens\":1,\"generated_tokens\":1,\"images\":3,\"frames\":0,\"duration_seconds\":0}",
        "{\"status\":\"ok\",\"id\":\"gateway\",\"observation\":\"x\",\"prompt_tokens\":1,\"generated_tokens\":1,\"images\":1,\"frames\":0,\"duration_seconds\":-1}",
        "{\"status\":\"ok\",\"id\":\"gateway\",\"observation\":\"x\",\"prompt_tokens\":1,\"generated_tokens\":1,\"images\":1,\"frames\":0,\"duration_seconds\":1e999}"
    };
    for (size_t i = 0; i < sizeof(bad_replies) / sizeof(bad_replies[0]); i++) {
        result = exchange(&image, image_wire, bad_replies[i]);
        assert(!result.ok && !strcmp(result.error, "vision_malformed_response"));
    }
    result = exchange(&joint, joint_wire, "{\"status\":\"ok\",\"id\":\"gateway\",\"observation\":\"x\",\"prompt_tokens\":1,\"generated_tokens\":1,\"images\":1,\"frames\":0,\"duration_seconds\":0}");
    assert(!result.ok && !strcmp(result.error, "vision_malformed_response"));
    result = exchange(&image, image_wire, "{\"status\":\"error\",\"id\":\"gateway\",\"code\":\"model_failed\"}");
    assert(!result.ok && !strcmp(result.error, "model_failed"));
    result = exchange(&image, image_wire, NULL);
    assert(!result.ok && !strcmp(result.error, "vision_timeout"));

    Request vision = {.path = "/tmp/\xff.ppm"};
    const char *vision_wire = "{\"command\":\"inspect\",\"image_path\":\"/tmp/\xef\xbf\xbd.ppm\",\"prompt\":\"Describe what is visible in this image in detail.\",\"max_tokens\":256,\"max_side_len\":2048}";
    result = exchange(&vision, vision_wire, "{\"status\":\"ok\",\"observation\":\"seen\",\"prompt_tokens\":9,\"generated_tokens\":5}");
    assert(result.ok && !strcmp(result.observation, "seen") && result.prompt_tokens == 9 && result.generated_tokens == 5);
    assert(!strcmp(result.error, "retained") && !strcmp(result.trace.event, "visionpsy_complete"));
    result = exchange(&vision, vision_wire, "{\"status\":\"ok\"}");
    assert(result.ok && !strcmp(result.observation, "unchanged") && result.prompt_tokens == -1);
    result = exchange(&vision, vision_wire, "[]");
    assert(!result.ok && !strcmp(result.error, "vision_malformed_response") && !strcmp(result.trace.event, "visionpsy_error"));
    result = exchange(&vision, vision_wire, "{\"status\":\"error\",\"code\":\"vision_model_failed\"}");
    assert(!result.ok && !strcmp(result.error, "vision_model_failed"));
    result = exchange(&vision, vision_wire, NULL);
    assert(!result.ok && !strcmp(result.error, "vision_timeout") && !strcmp(result.trace.event, "visionpsy_error"));

    invoke(NULL, &image, &result); assert(!result.ok && !strcmp(result.error, "vision_not_ready"));
    invoke(NULL, &vision, &result); assert(!result.ok && !strcmp(result.error, "vision_not_ready"));
    SamosaMmSession active = {.active = 1};
    joint.images = 3; invoke(&active, &joint, &result);
    assert(!result.ok && !strcmp(result.error, "vision_request_encode_failed") && !result.trace.payloads);
    printf("specialist protocol: PASS (%u native exchanges/refusals, exact inherited wire/defaults, Unicode, bounds, errors, traces)\n", cases);
    return 0;
}
