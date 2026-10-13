#ifndef SAMOSA_SUMMARY_FIXTURE_H
#define SAMOSA_SUMMARY_FIXTURE_H
#define launch whisper_launch
#define invoke whisper_invoke
#include "whisper_fixture.h"
#undef launch
#undef invoke
static inline Invocation launch(const FixtureConfig *c, const char *request) {
    const char *registry=getenv("SAMOSA_TEST_REGISTRY_CONFIG");
    if(registry){char *args[]={(char*)c->cli,"--config",(char*)registry,NULL};return spawn_request(args,request);}
    char *args[] = {(char *)c->cli, "--summary-cli", (char *)c->engine, "--summary-model", (char *)c->model,
        "--summary-cli-sha256", (char *)c->engine_sha, "--summary-model-sha256", (char *)c->model_sha,
        "--admission-state", (char *)c->state, "--owner", (char *)c->owner, NULL};
    return spawn_request(args, request);
}
static inline char *invoke(const FixtureConfig *c, const char *request, const char *error) {
    char *body = finish(launch(c, request), error ? 1 : 0);
    if (error) error_check(body, error);
    return body;
}
static inline char *request_mode(const char *text, const char *id, size_t timeout, int opening) {
    char *body = NULL; size_t n; FILE *f = open_memstream(&body, &n); CHECK(f);
fprintf(f,"{\"contract_version\":1,\"op\":\"%s\",\"provider_id\":\"native-t5-summary-v1\",\"text\":",opening?"summarize_opening":"summarize");
    json_quote(f, text, strlen(text)); fputs(",\"max_output_tokens\":64", f);
    if (id) { fputs(",\"operation_id\":", f); json_quote(f, id, strlen(id)); }
    if (timeout) fprintf(f, ",\"timeout_ms\":%zu", timeout);
    fputc('}', f); CHECK(!fclose(f)); return body;
}
static inline char *request(const char *text,const char *id,size_t timeout){return request_mode(text,id,timeout,0);}
static inline char *request_opening(const char *text,const char *id,size_t timeout){return request_mode(text,id,timeout,1);}
#endif
