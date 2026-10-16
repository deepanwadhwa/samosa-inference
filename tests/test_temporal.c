#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "whisper_fixture.h"
#include <arpa/inet.h>
typedef struct {
    FixtureConfig base;
    char manifest[65];
} TemporalFixture;
static void exact(int fd, void *buffer, size_t n) {
    size_t used = 0;
    while (used < n) {
        ssize_t got = read(fd, (char *)buffer + used, n - used);
        if (got < 0 && errno == EINTR)
            continue;
        CHECK(got > 0);
        used += (size_t)got;
    }
}
static char *frame_read(void) {
    uint32_t n;
    exact(0, &n, 4);
    n = ntohl(n);
    CHECK(n && n < 1048576);
    char *body = malloc(n + 1);
    CHECK(body);
    exact(0, body, n);
    body[n] = 0;
    return body;
}
static void frame_write(const char *body) {
    uint32_t n = htonl((uint32_t)strlen(body));
    write_all(1, &n, 4);
    write_all(1, body, strlen(body));
}
static int engine_fixture(const char *model) {
    struct stat bound, named;
    CHECK(!fstat(5, &bound) && !stat(model, &named) &&
          bound.st_dev == named.st_dev && bound.st_ino == named.st_ino);
    char *command = frame_read();
    Json j;
    parse(command, &j);
    char *id = json_string(&j, json_get(&j, 0, "id")),
         *prompt = json_string(&j, json_get(&j, 0, "prompt"));
    CHECK(id && prompt);
    int paths = json_get(&j, 0, "media_paths"),
        times = json_get(&j, 0, "timestamps_seconds");
    CHECK(paths >= 0 && times >= 0 && j.tokens[paths].count == 16 &&
          j.tokens[times].count == 16);
    char *workspace = NULL;
    for (int t = paths + 1; t < j.tokens[paths].next; t = j.tokens[t].next) {
        char *path = json_string(&j, t);
        CHECK(path);
        FILE *file = fopen(path, "rb");
        CHECK(file);
        char ppm[14];
        CHECK(fread(ppm, 1, 14, file) == 14 &&
              !memcmp(ppm, "P6\n1 1\n255\n\xff\0\0", 14) &&
              fgetc(file) == EOF && !fclose(file));
        if (!workspace) {
            workspace = strdup(path);
            CHECK(workspace);
            char *slash = strrchr(workspace, '/');
            CHECK(slash);
            *slash = 0;
        }
        free(path);
    }
    const char *marker = getenv("SAMOSA_TEMPORAL_MARKER");
    CHECK(marker);
    /* Publish the full JSON before the parent can observe readiness. */
    char temporary[4096];
    int marker_length = snprintf(temporary, sizeof(temporary), "%s.pending", marker);
    CHECK(marker_length > 0 && (size_t)marker_length < sizeof(temporary));
    FILE *mark = fopen(temporary, "w");
    CHECK(mark);
    fprintf(mark, "{\"pid\":%ld,\"workspace\":", (long)getpid());
    json_quote(mark, workspace, strlen(workspace));
    fputc('}', mark);
    CHECK(!fclose(mark));
    CHECK(!rename(temporary, marker));
    free(workspace);
    if (!strcmp(prompt, "cache-growth")) {
        char cache[4096];
        CHECK(snprintf(cache, sizeof(cache), "%s.cache", marker) < (int)sizeof(cache));
        int fd = open(cache, O_RDWR | O_NOFOLLOW);
        struct stat before;
        CHECK(fd >= 0 && !fstat(fd, &before) && before.st_size == 134217728);
        CHECK(!ftruncate(fd, 134283264) && !close(fd));
    }
    if (!strcmp(prompt, "stall")) {
        signal(SIGTERM, SIG_IGN);
        for (;;)
            pause();
    }
    if (!strcmp(prompt, "mutate")) {
        struct timespec wait = {1, 0};
        nanosleep(&wait, NULL);
    }
    if (!strcmp(prompt, "fail")) {
        free(id);
        free(prompt);
        free(command);
        return 9;
    }
    if (!strcmp(prompt, "big")) {
        char bytes[4096];
        memset(bytes, 'x', sizeof(bytes));
        for (int i = 0; i < 9; i++)
            write_all(1, bytes, sizeof(bytes));
        free(id);
        free(prompt);
        free(command);
        return 0;
    }
    char *body = NULL;
    size_t bytes = 0;
    FILE *out = open_memstream(&body, &bytes);
    CHECK(out);
    fputs("{\"status\":\"ok\",\"id\":", out);
    json_quote(out, !strcmp(prompt, "wrong-id") ? "forged" : id,
               strlen(!strcmp(prompt, "wrong-id") ? "forged" : id));
    fprintf(out,
            ",\"observation\":\"%s\",\"frames\":16,\"images\":0,\"prompt_"
            "tokens\":1600,\"generated_tokens\":%u,\"sample_times_seconds\":[",
            !strcmp(prompt, "empty") ? "" : "A controlled temporal fixture.",
            !strcmp(prompt, "over-budget") ? 33u : 8u);
    size_t k = 0;
    for (int t = times + 1; t < j.tokens[times].next;
         t = j.tokens[t].next, k++) {
        if (k)
            fputc(',', out);
        if (!k && !strcmp(prompt, "wrong-time"))
            fputs("99", out);
        else
            fwrite(j.text + j.tokens[t].begin, 1,
                   j.tokens[t].end - j.tokens[t].begin, out);
    }
    fputs("]}", out);
    CHECK(!fclose(out));
    if (!strcmp(prompt, "malformed"))
        frame_write("broken json");
    else
        frame_write(body);
    free(body);
    char *quit = frame_read();
    free(quit);
    frame_write(!strcmp(prompt, "bad-quit") ? "{\"status\":\"error\"}"
                                            : "{\"status\":\"ok\"}");
    free(command);
    free(id);
    free(prompt);
    return 0;
}
static Invocation tlaunch(const TemporalFixture *c, const char *request) {
    char *args[] = {(char *)c->base.cli,        "--temporal-engine",
                    (char *)c->base.engine,     "--temporal-model",
                    (char *)c->base.model,      "--temporal-engine-sha256",
                    (char *)c->base.engine_sha, "--temporal-manifest-sha256",
                    (char *)c->manifest,        "--admission-state",
                    (char *)c->base.state,      "--owner",
                    (char *)c->base.owner,      NULL};
    return spawn_request(args, request);
}
static char *tinvoke(const TemporalFixture *c, const char *request,
                     const char *error) {
    char *body = finish(tlaunch(c, request), error ? 1 : 0);
    if (error)
        error_check(body, error);
    return body;
}
static char *request(const char *prompt, const char *id, size_t timeout) {
    char *body = NULL;
    size_t bytes = 0;
    FILE *out = open_memstream(&body, &bytes);
    CHECK(out);
    fputs("{\"contract_version\":1,\"op\":\"analyze_video_frames\",\"provider_"
          "id\":\"molmo2-native-v1\",\"start_us\":100000,\"end_us\":2000000,"
          "\"max_output_tokens\":32,\"prompt\":",
          out);
    json_quote(out, prompt, strlen(prompt));
    if (id) {
        fputs(",\"operation_id\":", out);
        json_quote(out, id, strlen(id));
        fputs(",\"parent_operation_id\":\"temporal-parent\"", out);
    }
    if (timeout)
        fprintf(out, ",\"timeout_ms\":%zu", timeout);
    fputs(",\"frames\":[", out);
    for (size_t k = 0; k < 16; k++)
        fprintf(out,
                "%s{\"timestamp_us\":%zu,\"width\":1,\"height\":1,\"rgba_"
                "base64\":\"/wAA/w==\"}",
                k ? "," : "", 100000 + k * 100000);
    fputs("]}", out);
    CHECK(!fclose(out));
    return body;
}
static char *ready(const char *marker, pid_t *pid) {
    for (int k = 0; k < 1000 && access(marker, F_OK); k++)
        delay();
    char *body = file_read(marker, 2048);
    Json j;
    parse(body, &j);
    size_t p = 0;
    CHECK(json_uint(&j, json_get(&j, 0, "pid"), &p) && p > 0);
    *pid = (pid_t)p;
    char *workspace = json_string(&j, json_get(&j, 0, "workspace"));
    CHECK(workspace);
    free(body);
    return workspace;
}
static void absent(const char *path, pid_t child) {
    for (int k = 0; k < 1000; k++) {
        if (access(path, F_OK) && errno == ENOENT) {
            CHECK(kill(child, 0) < 0 && errno == ESRCH);
            return;
        }
        delay();
    }
    CHECK(0);
}
int main(int argc, char **argv) {
    if (argc == 2 && getenv("SAMOSA_TEMPORAL_MARKER"))
        return engine_fixture(argv[1]);
    CHECK(argc == 3 && argv[0][0] == '/' && argv[1][0] == '/' &&
          argv[2][0] == '/');
    alarm(180);
    char root[] = "/tmp/samosa-temporal-lifecycle-XXXXXX";
    CHECK(mkdtemp(root));
    char engine[512], model[512], marker[512], state[512], manifest[512],
        path[4096], target[512], weights[512];
    snprintf(engine, sizeof(engine), "%s/engine", root);
    snprintf(model, sizeof(model), "%s/model", root);
    snprintf(marker, sizeof(marker), "%s/ready", root);
    snprintf(state, sizeof(state), "%s/state", root);
    snprintf(manifest, sizeof(manifest), "%s/model/manifest.json", root);
    snprintf(weights, sizeof(weights), "%s/model/weights.safetensors", root);
    CHECK(!mkdir(model, 0700));
    file_copy(argv[0], engine);
    CHECK(!setenv("SAMOSA_TEMPORAL_MARKER", marker, 1));
    TemporalFixture cfg = {.base = {.cli = argv[1],
                                    .engine = engine,
                                    .model = model,
                                    .state = state,
                                    .owner = "temporal-owner"}};
    file_sha(engine, cfg.base.engine_sha);
    const char *names[] = {"config.json", "tokenizer.json", "processor.json",
                           "weights.safetensors"};
    FILE *out = fopen(manifest, "w");
    CHECK(out);
    fputs("{\"format\":\"samosa.molmo2.mlx.v1\",\"package_id\":\"molmo2-4b-mlx-"
          "q4-v1\",\"model_id\":\"allenai/"
          "Molmo2-4B\",\"upstream_revision\":"
          "\"042abfa7a38879a376cec03d949eff0aefaa0600\",\"processor_"
          "fingerprint\":"
          "\"808de9add76144a557348c5f5180a8408b12ca83592c7a6a257ae69c968e51df\""
          ",\"quantization\":{\"mode\":\"affine\",\"bits\":4,\"group_size\":64}"
          ",\"estimated_resident_bytes\":3932160000,\"files\":[",
          out);
    for (size_t k = 0; k < 4; k++) {
        CHECK(snprintf(target, sizeof(target), "%s/%s", model, names[k]) <
              (int)sizeof(target));
        if (k < 3) {
            CHECK(snprintf(path, sizeof(path), "%s/%s", argv[2], names[k]) <
                  (int)sizeof(path));
            file_copy(path, target);
        } else
            file_write(target, "controlled fixture weights, not a model");
        char digest[65];
        file_sha(target, digest);
        struct stat st;
        CHECK(!stat(target, &st));
        fprintf(out,
                "%s{\"name\":\"%s\",\"role\":\"%s\",\"bytes\":%llu,\"sha256\":"
                "\"%s\"}",
                k ? "," : "", names[k],
                k == 3   ? "weights"
                : k == 1 ? "tokenizer"
                         : "metadata",
                (unsigned long long)st.st_size, digest);
    }
    fputs("]}", out);
    CHECK(!fclose(out) && !chmod(manifest, 0600));
    file_sha(manifest, cfg.manifest);
    char *req = request("describe", "temporal-first", 10000),
         *body = tinvoke(&cfg, req, NULL);
    CHECK(strstr(body, "sampled_video_analysis_uncertain") &&
          strstr(body, "Controlled") == NULL &&
          strstr(body, "controlled temporal fixture"));
    char *saved = strdup(body);
    CHECK(saved);
    free(body);
    pid_t child;
    char *workspace = ready(marker, &child);
    absent(workspace, child);
    free(workspace);
    CHECK(!unlink(marker));
    TemporalFixture offline = cfg;
    offline.base.engine = "/missing/engine";
    offline.base.model = "/missing/model";
    body = tinvoke(&offline, req, NULL);
    CHECK(!strcmp(body, saved));
    free(body);
    free(saved);
    free(req);
    TemporalFixture foreign = offline;
    foreign.base.owner = "foreign-owner";
    body = tinvoke(&foreign,
                   "{\"contract_version\":1,\"op\":\"operation_get\","
                   "\"operation_id\":\"temporal-first\"}",
                   "unknown_operation");
    free(body);
    /* Metal grows a shared cache that may already exceed 64 MiB. Result
     * flooding must still refuse independently of this legitimate file write. */
    char cache[4096];
    CHECK(snprintf(cache, sizeof(cache), "%s.cache", marker) < (int)sizeof(cache));
    int cache_fd = open(cache, O_RDWR | O_CREAT | O_EXCL, 0600);
    CHECK(cache_fd >= 0 && !ftruncate(cache_fd, 134217728) && !close(cache_fd));
    req = request("cache-growth", NULL, 10000);
    body = tinvoke(&cfg, req, NULL);
    free(body);free(req);
    struct stat cache_stat;
    CHECK(!stat(cache, &cache_stat) && cache_stat.st_size == 134283264);
    CHECK(!unlink(cache));
    workspace = ready(marker, &child);
    absent(workspace, child);
    free(workspace);
    CHECK(!unlink(marker));
    const char *modes[] = {"wrong-id", "wrong-time", "empty", "over-budget",
                           "bad-quit", "malformed",  "big",   "fail"};
    for (size_t k = 0; k < 8; k++) {
        req = request(modes[k], NULL, 10000);
        body = tinvoke(&cfg, req,
                       k == 7 ? "provider_process_failed"
                              : "invalid_provider_response");
        free(body);
        free(req);
        workspace = ready(marker, &child);
        absent(workspace, child);
        free(workspace);
        CHECK(!unlink(marker));
    }
    req = request("stall", NULL, 3000);
    body = tinvoke(&cfg, req, "deadline_exceeded");
    free(body);
    free(req);
    workspace = ready(marker, &child);
    absent(workspace, child);
    free(workspace);
    CHECK(!unlink(marker));
    req = request("stall", "cancel-temporal", 10000);
    Invocation pending = tlaunch(&cfg, req);
    free(req);
    workspace = ready(marker, &child);
    req=request("describe","blocked-temporal",10000);
    body=tinvoke(&cfg,req,"busy");free(body);free(req);
    TemporalFixture foreign_control=cfg;foreign_control.base.owner="foreign-active-owner";
    req=control("operation_cancel","cancel-temporal");
    body=tinvoke(&foreign_control,req,"unknown_operation");free(body);free(req);
    req = control("operation_cancel", "cancel-temporal");
    body = tinvoke(&cfg, req, NULL);
    free(body);
    free(req);
    body = finish(pending, 1);
    error_check(body, "cancelled");
    free(body);
    absent(workspace, child);
    free(workspace);
    CHECK(!unlink(marker));
    req = request("stall", "kill-temporal", 10000);
    pending = tlaunch(&cfg, req);
    free(req);
    workspace = ready(marker, &child);
    CHECK(!kill(pending.pid, SIGKILL));
    int status;
    CHECK(waitpid(pending.pid, &status, 0) == pending.pid &&
          WIFSIGNALED(status) && !close(pending.output));
    absent(workspace, child);
    free(workspace);
    CHECK(!unlink(marker));
    req = control("operation_get", "kill-temporal");
    body = tinvoke(&cfg, req, NULL);
    CHECK(strstr(body, "interrupted"));
    free(body);
    free(req);
    req = request("mutate", "changed-package", 10000);
    pending = tlaunch(&cfg, req);
    free(req);
    workspace = ready(marker, &child);
    file_write(weights, "modified controlled fixture weights");
    body = finish(pending, 1);
    error_check(body, "artifact_changed");
    free(body);
    absent(workspace, child);
    free(workspace);
    CHECK(!unlink(marker));
    cleanup(root);
    printf("PASS temporal native lifecycle: %u checks;16 prepared PPMs, framed "
           "bounds/bindings, owner/offline replay, malformed/failed output, "
           "deadline/cancel/requester death with complete workspace cleanup "
           "and package mutation refusal; fixture engine/weights only\n",
           checks);
    return 0;
}
