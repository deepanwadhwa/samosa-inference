#ifndef VISION_FIXTURE_H
#define VISION_FIXTURE_H
#include "whisper_fixture.h"
/* Reuse the native process/file harness, not the Whisper engine protocol. */
typedef struct {
    FixtureConfig base;
    char config[65], tokenizer[65], weights[65];
} VisionFixture;
static inline Invocation vlaunch(const VisionFixture *c, const char *request) {
    char *args[] = {(char *)c->base.cli,        "--vision-engine",
                    (char *)c->base.engine,     "--vision-model",
                    (char *)c->base.model,      "--vision-engine-sha256",
                    (char *)c->base.engine_sha, "--vision-config-sha256",
                    (char *)c->config,          "--vision-tokenizer-sha256",
                    (char *)c->tokenizer,       "--vision-weights-sha256",
                    (char *)c->weights,         "--admission-state",
                    (char *)c->base.state,      "--owner",
                    (char *)c->base.owner,      NULL};
    return spawn_request(args, request);
}
static inline char *vinvoke(const VisionFixture *c, const char *request, const char *error) {
    char *body = finish(vlaunch(c, request), error ? 1 : 0);
    if (error) {
        Json j;
        parse(body, &j);
        char *actual = json_string(&j, json_get(&j, json_get(&j, 0, "error"), "code"));
        if (!actual || strcmp(actual, error))
            fprintf(stderr, "request: %.1024s; expected: %s\n", request, error);
        free(actual);
        error_check(body, error);
    }
    return body;
}
static inline char *request(const char *pixels, size_t width, size_t height, const char *prompt,
                            const char *id, size_t timeout) {
    char *body = NULL;
    size_t n = 0;
    FILE *f = open_memstream(&body, &n);
    CHECK(f);
    fputs("{\"contract_version\":1,\"op\":\"analyze_image\",\"provider_id\":\"visionpsy-native-"
          "v1\",\"image_rgba_base64\":",
          f);
    json_quote(f, pixels, strlen(pixels));
    fprintf(f, ",\"width\":%zu,\"height\":%zu,\"max_output_tokens\":32,\"prompt\":", width, height);
    json_quote(f, prompt, strlen(prompt));
    if (id) {
        fputs(",\"operation_id\":", f);
        json_quote(f, id, strlen(id));
        fputs(",\"parent_operation_id\":\"sources-parent\"", f);
    }
    if (timeout)
        fprintf(f, ",\"timeout_ms\":%zu", timeout);
    fputc('}', f);
    CHECK(!fclose(f));
    return body;
}
#endif
