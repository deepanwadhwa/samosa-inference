#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "vision_fixture.h"
static void absent(const char *path) {
    for (int i = 0; i < 500; i++) {
        if (access(path, F_OK) && errno == ENOENT)
            return;
        delay();
    }
    CHECK(0);
}
static char *ready(const char *marker, pid_t *pid) {
    for (int i = 0; i < 500; i++) {
        if (!access(marker, F_OK))
            break;
        delay();
    }
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
static int fixture(const char *model) {
    struct stat bound, named;
    CHECK(!fstat(5, &bound) && !stat(model, &named) && bound.st_dev == named.st_dev &&
          bound.st_ino == named.st_ino);
    char config[1024];
    snprintf(config, sizeof(config), "%s/config.json", model);
    char *body = file_read(config, 4096);
    Json j;
    parse(body, &j);
    char *marker = json_string(&j, json_get(&j, 0, "marker"));
    CHECK(marker);
    free(body);
    char *line = NULL;
    size_t capacity = 0;
    CHECK(getline(&line, &capacity, stdin) > 0);
    parse(line, &j);
    char *command = json_string(&j, json_get(&j, 0, "command")),
         *id = json_string(&j, json_get(&j, 0, "id")),
         *image = json_string(&j, json_get(&j, 0, "image_path")),
         *prompt = json_string(&j, json_get(&j, 0, "prompt"));
    CHECK(command && !strcmp(command, "inspect") && id && image && prompt);
    size_t tokens = 0, side = 0;
    CHECK(json_uint(&j, json_get(&j, 0, "max_tokens"), &tokens) && tokens == 32);
    CHECK(json_uint(&j, json_get(&j, 0, "max_side_len"), &side) && side == 512);
    FILE *ppm = fopen(image, "rb");
    CHECK(ppm);
    size_t width = 0, height = 0;
    CHECK(fscanf(ppm, "P6\n%zu %zu\n255", &width, &height) == 2);
    CHECK(fgetc(ppm) == '\n');
    if (width == 2 && height == 1) {
        unsigned char actual[6], expected[] = {255, 0, 0, 255, 255, 255};
        CHECK(fread(actual, 1, 6, ppm) == 6 && !memcmp(actual, expected, 6));
    } else {
        CHECK(width == 512 && height == 512);
        for (size_t i = 0; i < width * height * 3; i++)
            CHECK(fgetc(ppm) == 255);
    }
    CHECK(fgetc(ppm) == EOF);
    CHECK(!fclose(ppm));
    char *workspace = strdup(image), *slash = strrchr(workspace, '/');
    CHECK(slash);
    *slash = 0;
    /* Publish readiness only after the complete JSON is closed. The observing
     * parent must never see a newly created but partially written marker. */
    char temporary[1200];
    CHECK(snprintf(temporary, sizeof(temporary), "%s.pending", marker) > 0);
    FILE *f = fopen(temporary, "w");
    CHECK(f);
    fprintf(f, "{\"pid\":%ld,\"workspace\":", (long)getpid());
    json_quote(f, workspace, strlen(workspace));
    fputc('}', f);
    CHECK(!fclose(f));
    CHECK(!rename(temporary, marker));
    free(workspace);
    free(marker);
    if (!strcmp(prompt, "stall")) {
        signal(SIGTERM, SIG_IGN);
        for (;;)
            pause();
    }
    if (!strcmp(prompt, "mutate")) {
        struct timespec t = {0, 500000000};
        nanosleep(&t, NULL);
    }
    int result = 0;
    if (!strcmp(prompt, "fail")) {
        result = 9;
        goto fixture_done;
    }
    if (!strcmp(prompt, "big")) {
        char bytes[4096];
        memset(bytes, 'x', sizeof(bytes));
        for (size_t i = 0; i < 9; i++)
            write_all(STDOUT_FILENO, bytes, sizeof(bytes));
        goto fixture_done;
    }
    if (!strcmp(prompt, "malformed")) {
        puts("broken json");
        goto fixture_done;
    }
    if (!strcmp(prompt, "nul")) {
        const char invalid[] = "broken\0json\n";
        write_all(STDOUT_FILENO, invalid, sizeof(invalid) - 1);
        goto fixture_done;
    }
    fputs("{\"status\":\"ok\",\"id\":", stdout);
    json_quote(stdout, !strcmp(prompt, "wrong-id") ? "forged" : id,
               strlen(!strcmp(prompt, "wrong-id") ? "forged" : id));
    fprintf(stdout,
            ",\"observation\":\"%s\",\"prompt_tokens\":12,\"generated_tokens\":%u,\"n_w\":1,\"n_"
            "h\":1,\"has_global\":false,\"prefill_ms\":1,\"decode_ms\":1}\n",
            !strcmp(prompt, "empty") ? "" : "Red pixel and white pixel.",
            !strcmp(prompt, "over-budget") ? 33u : 8u);
    CHECK(getline(&line, &capacity, stdin) > 0);
    parse(line, &j);
    char *quit = json_string(&j, json_get(&j, 0, "command"));
    CHECK(quit && !strcmp(quit, "quit"));
    puts(!strcmp(prompt, "bad-quit") ? "{\"status\":\"error\",\"message\":\"bye\"}"
                                     : "{\"status\":\"ok\",\"message\":\"bye\"}");
    free(quit);
fixture_done:
    free(line);
    free(command);
    free(id);
    free(image);
    free(prompt);
    return result;
}
int main(int argc, char **argv) {
    if (argc == 2 && (!strcmp(argv[1], "/dev/fd/5") || strstr(argv[1], "/model")))
        return fixture(argv[1]);
    CHECK(argc == 2 && argv[0][0] == '/' && argv[1][0] == '/');
    alarm(90);
    char root[] = "/tmp/samosa-vision-native-XXXXXX";
    CHECK(mkdtemp(root));
    char engine[512], model[512], marker[512], state[512], config[512], tokenizer[512],
        weights[512], saved_engine[512], saved_model[512];
    snprintf(engine, sizeof(engine), "%s/engine", root);
    snprintf(model, sizeof(model), "%s/model", root);
    snprintf(marker, sizeof(marker), "%s/ready", root);
    snprintf(state, sizeof(state), "%s/state", root);
    snprintf(config, sizeof(config), "%s/model/config.json", root);
    snprintf(tokenizer, sizeof(tokenizer), "%s/model/tokenizer.json", root);
    snprintf(weights, sizeof(weights), "%s/model/model.safetensors", root);
    snprintf(saved_engine, sizeof(saved_engine), "%s/saved-engine", root);
    snprintf(saved_model, sizeof(saved_model), "%s/saved-model", root);
    VisionFixture cfg = {.base = {.cli = argv[1],
                                  .engine = engine,
                                  .model = model,
                                  .state = state,
                                  .owner = "vision-owner"}};
    CHECK(!mkdir(model, 0700));
    file_copy(argv[0], engine);
    file_sha(engine, cfg.base.engine_sha);
    char metadata[1024];
    snprintf(metadata, sizeof(metadata), "{\"marker\":\"%s\"}", marker);
    file_write(config, metadata);
    file_write(tokenizer, "{}");
    file_write(weights, "fixture weights");
    file_sha(config, cfg.config);
    file_sha(tokenizer, cfg.tokenizer);
    file_sha(weights, cfg.weights);
    char *body = vinvoke(&cfg, "{\"contract_version\":1,\"op\":\"describe\"}", NULL);
    CHECK(strstr(body, "visual_analysis") && strstr(body, "rgba8_base64"));
    free(body);
    unsigned char rgba[] = {255, 0, 0, 255, 0, 0, 0, 0};
    char *pixels = encode(rgba, sizeof(rgba));
    char *req = request(pixels, 2, 1, "describe", "visual-one", 10000),
         *saved = vinvoke(&cfg, req, NULL);
    CHECK(strstr(saved, "Red pixel and white pixel.") &&
          strstr(saved, "visual_analysis_uncertain") && strstr(saved, "rgba8_composited_on_white"));
    char digest[65];
    hash_bytes(rgba, sizeof(rgba), digest);
    CHECK(strstr(saved, digest));
    pid_t child;
    char *workspace = ready(marker, &child);
    absent(workspace);
    free(workspace);
    CHECK(!rename(engine, saved_engine) && !rename(model, saved_model));
    body = vinvoke(&cfg, req, NULL);
    CHECK(!strcmp(body, saved));
    free(body);
    CHECK(!rename(saved_engine, engine) && !rename(saved_model, model));
    free(saved);
    free(req);
    req = request(pixels, 2, 1, "different", "visual-one", 10000);
    body = vinvoke(&cfg, req, "idempotency_conflict");
    free(body);
    free(req);
    cfg.base.owner = "foreign";
    req = control("operation_get", "visual-one");
    body = vinvoke(&cfg, req, "unknown_operation");
    free(body);
    free(req);
    cfg.base.owner = "vision-owner";
    const char *invalid[] = {"!!!!", "/wAA/wAAAAB=", "/wAA/wAAAAA"};
    for (size_t i = 0; i < 3; i++) {
        req = request(invalid[i], 2, 1, "describe", NULL, 0);
        body = vinvoke(&cfg, req, "invalid_image");
        free(body);
        free(req);
    }
    req = request(pixels, 513, 1, "describe", NULL, 0);
    body = vinvoke(&cfg, req, "invalid_request");
    free(body);
    free(req);
    req = request(pixels, 2, 1, "describe", NULL, 0);
    char pin[65];
    memcpy(pin, cfg.weights, 65);
    memset(cfg.weights, '0', 64);
    body = vinvoke(&cfg, req, "artifact_checksum_mismatch");
    free(body);
    memcpy(cfg.weights, pin, 65);
    free(req);
    const char *modes[] = {"malformed",   "big",  "wrong-id", "empty",
                           "over-budget", "fail", "bad-quit", "nul"};
    for (size_t i = 0; i < sizeof(modes) / sizeof(*modes); i++) {
        req = request(pixels, 2, 1, modes[i], NULL, 10000);
        body = vinvoke(&cfg, req, i == 5 ? "provider_process_failed" : "invalid_provider_response");
        free(body);
        free(req);
        workspace = ready(marker, &child);
        absent(workspace);
        free(workspace);
    }
    unsigned char *large = malloc(512u * 512u * 4u);
    CHECK(large);
    memset(large, 255, 512u * 512u * 4u);
    char *encoded = encode(large, 512u * 512u * 4u);
    free(large);
    req = request(encoded, 512, 512, "describe", NULL, 10000);
    free(encoded);
    body = vinvoke(&cfg, req, NULL);
    CHECK(strstr(body, "\"width\":512"));
    free(body);
    free(req);
    req = request(pixels, 2, 1, "stall", NULL, 500);
    body = vinvoke(&cfg, req, "deadline_exceeded");
    free(body);
    free(req);
    workspace = ready(marker, &child);
    absent(workspace);
    free(workspace);
    CHECK(kill(child, 0) < 0 && errno == ESRCH);
    CHECK(!unlink(marker));
    req = request(pixels, 2, 1, "mutate", "mutated-model", 10000);
    Invocation pending = vlaunch(&cfg, req);
    free(req);
    workspace = ready(marker, &child);
    file_write(tokenizer, "{}");
    body = finish(pending, 1);
    error_check(body, "artifact_changed");
    free(body);
    absent(workspace);
    free(workspace);
    CHECK(!unlink(marker));
    req = request(pixels, 2, 1, "mutate", "replaced-engine", 10000);
    pending = vlaunch(&cfg, req);
    free(req);
    workspace = ready(marker, &child);
    CHECK(!rename(engine, saved_engine));
    file_copy(saved_engine, engine);
    CHECK(!unlink(engine) && !rename(saved_engine, engine));
    body = finish(pending, 1);
    error_check(body, "artifact_changed");
    free(body);
    absent(workspace);
    free(workspace);
    CHECK(!unlink(marker));
    req = request(pixels, 2, 1, "mutate", "replaced-model", 10000);
    pending = vlaunch(&cfg, req);
    free(req);
    workspace = ready(marker, &child);
    CHECK(!rename(model, saved_model) && !mkdir(model, 0700) && !rmdir(model) &&
          !rename(saved_model, model));
    body = finish(pending, 1);
    error_check(body, "artifact_changed");
    free(body);
    absent(workspace);
    free(workspace);
    CHECK(!unlink(marker));
    req = request(pixels, 2, 1, "stall", "cancel-visual", 10000);
    pending = vlaunch(&cfg, req);
    free(req);
    workspace = ready(marker, &child);
    req = control("operation_cancel", "cancel-visual");
    body = vinvoke(&cfg, req, NULL);
    free(body);
    free(req);
    body = finish(pending, 1);
    error_check(body, "cancelled");
    free(body);
    absent(workspace);
    free(workspace);
    CHECK(!unlink(marker));
    req = request(pixels, 2, 1, "stall", "kill-visual", 10000);
    pending = vlaunch(&cfg, req);
    free(req);
    workspace = ready(marker, &child);
    CHECK(!kill(pending.pid, SIGKILL));
    int status;
    CHECK(waitpid(pending.pid, &status, 0) == pending.pid && WIFSIGNALED(status));
    CHECK(!close(pending.output));
    absent(workspace);
    free(workspace);
    CHECK(kill(child, 0) < 0 && errno == ESRCH);
    req = control("operation_get", "kill-visual");
    body = vinvoke(&cfg, req, NULL);
    CHECK(strstr(body, "interrupted"));
    free(body);
    free(req);
    req = request(pixels, 2, 1, "describe", NULL, 10000);
    body = vinvoke(&cfg, req, NULL);
    free(body);
    free(req);
    free(pixels);
    cleanup(root);
    printf(
        "PASS native public visual adapter: %u checks; inspect/quit, alpha, input bounds, replay, "
        "identity/mutation, failure/deadline/cancel/parent death cleanup; fixture engine only\n",
        checks);
    return 0;
}
