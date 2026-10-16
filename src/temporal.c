#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "temporal.h"
#include "admission.h"
#include "artifact.h"
#include "budget.h"
#include "engine_process.h"
#include "temporal_package.h"
#include <arpa/inet.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <unistd.h>
#define RESULT_MAX ENGINE_VISUAL_RESULT_MAX
static int fail(FILE *out, const char *error) {
    fprintf(out, "{\"contract_version\":1,\"error\":{\"code\":\"%s\"}}\n",
            error);
    return 1;
}
static const char *interrupted(void *context) {
    return inference_interrupted(context);
}
static int same(const struct stat *a, const struct stat *b) {
    if (a->st_dev != b->st_dev || a->st_ino != b->st_ino ||
        a->st_size != b->st_size || a->st_mode != b->st_mode ||
        a->st_uid != b->st_uid || a->st_nlink != b->st_nlink)
        return 0;
#ifdef __APPLE__
    return a->st_mtimespec.tv_sec == b->st_mtimespec.tv_sec &&
           a->st_mtimespec.tv_nsec == b->st_mtimespec.tv_nsec &&
           a->st_ctimespec.tv_sec == b->st_ctimespec.tv_sec &&
           a->st_ctimespec.tv_nsec == b->st_ctimespec.tv_nsec;
#else
    return a->st_mtim.tv_sec == b->st_mtim.tv_sec &&
           a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
           a->st_ctim.tv_sec == b->st_ctim.tv_sec &&
           a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
#endif
}
static const char *engine_verify(const InferenceConfig *cfg, int fd,
                                 struct stat *receipt, int recheck) {
    struct stat before, after;
    const char *error = fstat(fd, &before) ? "artifact_unavailable" : NULL;
    if (!error && (!S_ISREG(before.st_mode) || before.st_size <= 0 ||
                   before.st_uid != geteuid() || before.st_nlink != 1 ||
                   (before.st_mode & 022) || !(before.st_mode & 0111)))
        error = "unsupported_artifact";
    if (!error && recheck && !same(&before, receipt))
        error = "artifact_changed";
    if (!error)
        error = artifact_verify_controlled(
            fd, (uint64_t)before.st_size, 67108864u,
            cfg->temporal->executable_sha256, interrupted, (void *)cfg);
    if (!error && (fstat(fd, &after) || !same(&before, &after)))
        error = "artifact_changed";
    if (!error && !recheck)
        *receipt = before;
    return error;
}
static const char *write_owned(const InferenceConfig *cfg, int directory,
                               const char *name, const void *body,
                               size_t bytes) {
    int fd =
        openat(directory, name, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (fd < 0)
        return "engine_io_failed";
    const char *error = NULL;
    size_t used = 0;
    while (!error && used < bytes) {
        error = inference_interrupted(cfg);
        if (error)
            break;
        size_t want = bytes - used;
        if (want > 65536)
            want = 65536;
        ssize_t n = write(fd, (const char *)body + used, want);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            error = "engine_io_failed";
            break;
        }
        used += (size_t)n;
    }
    if (close(fd) && !error)
        error = "engine_io_failed";
    return error;
}
static char *framed_reply(const unsigned char *body, size_t bytes,
                          size_t *offset) {
    if (*offset > bytes || bytes - *offset < 4)
        return NULL;
    uint32_t encoded;
    memcpy(&encoded, body + *offset, 4);
    size_t length = ntohl(encoded);
    *offset += 4;
    if (!length || length > 30000 || length > bytes - *offset ||
        memchr(body + *offset, 0, length))
        return NULL;
    char *reply = malloc(length + 1);
    if (!reply)
        return NULL;
    memcpy(reply, body + *offset, length);
    reply[length] = 0;
    *offset += length;
    return reply;
}
static int equal(const Json *j, int object, const char *key,
                 const char *expected) {
    char *s = json_string(j, json_get(j, object, key));
    int ok = s && !strcmp(s, expected);
    free(s);
    return ok;
}
static int timestamp(const Json *j, int token, size_t expected) {
    if (token < 0 || j->tokens[token].type != JT_NUMBER)
        return 0;
    size_t n = j->tokens[token].end - j->tokens[token].begin;
    if (!n || n >= 64)
        return 0;
    char number[64];
    memcpy(number, j->text + j->tokens[token].begin, n);
    number[n] = 0;
    char *end;
    errno = 0;
    double value = strtod(number, &end);
    return !errno && !*end && isfinite(value) &&
           fabs(value * 1000000.0 - (double)expected) < 0.5;
}
static const char *run(const InferenceConfig *cfg, const TemporalInput *input,
                       char **text, size_t *prompt_tokens,
                       size_t *generated_tokens) {
    int lease = -1, model = -1, engine = -1, directory = -1, result_fd = -1,
        created = 0;
    const TemporalConfig *v = cfg->temporal;
    const char *error = inference_interrupted(cfg);
    TemporalPackageReceipt *package = NULL;
    struct stat engine_receipt = {0};
    char workspace[] = "/tmp/samosa-inference-temporal-XXXXXX",
         command_path[256] = {0}, result_path[256] = {0};
    char *command = NULL, *arguments = NULL;
    size_t command_bytes = 0;
    if (!error)
        error = inference_schedule(cfg, &lease);
    if (!error) {
        model = open(v->model, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (model < 0)
            error = "artifact_unavailable";
    }
    if (!error) {
        package = calloc(1, sizeof(*package));
        if (!package)
            error = "resource_limit";
    }
    if (!error)
        error = temporal_package_verify(model, v->model, v->manifest_sha256,
                                        interrupted, (void *)cfg, package, 0);
    if (!error) {
        engine =
            open(v->executable, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
        error = engine < 0 ? "artifact_unavailable"
                           : engine_verify(cfg, engine, &engine_receipt, 0);
    }
    if (!error) {
        created = mkdtemp(workspace) != NULL;
        if (!created)
            error = "engine_io_failed";
        else {
            directory = open(workspace,
                             O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            if (directory < 0)
                error = "engine_io_failed";
        }
    }
    for (size_t k = 0; !error && k < input->count; k++) {
        char name[32];
        snprintf(name, sizeof(name), "frame-%02zu.ppm", k);
        size_t bytes = 0;
        char *ppm = temporal_ppm(&input->frames[k], &bytes);
        if (!ppm)
            error = "resource_limit";
        else
            error = write_owned(cfg, directory, name, ppm, bytes);
        free(ppm);
    }
    if (!error) {
        FILE *out = open_memstream(&command, &command_bytes);
        if (!out)
            error = "resource_limit";
        else {
            fputs("{\"command\":\"analyze\",\"media_kind\":\"video_frames\","
                  "\"id\":",
                  out);
            json_quote(out, input->input_sha256, 64);
            fputs(",\"media_paths\":[", out);
            for (size_t k = 0; k < input->count; k++) {
                char path[256];
                snprintf(path, sizeof(path), "%s/frame-%02zu.ppm", workspace,
                         k);
                if (k)
                    fputc(',', out);
                json_quote(out, path, strlen(path));
            }
            fputs("],\"timestamps_seconds\":[", out);
            for (size_t k = 0; k < input->count; k++) {
                if (k)
                    fputc(',', out);
                fprintf(out, "%zu.%06zu",
                        input->frames[k].timestamp_us / 1000000,
                        input->frames[k].timestamp_us % 1000000);
            }
            fputs("],\"prompt\":", out);
            json_quote(out, input->prompt, strlen(input->prompt));
            fprintf(out, ",\"max_tokens\":%zu}", input->tokens);
            if (fclose(out))
                error = "resource_limit";
        }
    }
    if (!error) {
        const char *quit = "{\"command\":\"quit\"}";
        size_t quit_bytes = strlen(quit),
               total = 8 + command_bytes + quit_bytes;
        unsigned char *framed = malloc(total);
        if (!framed)
            error = "resource_limit";
        else {
            uint32_t n = htonl((uint32_t)command_bytes);
            memcpy(framed, &n, 4);
            memcpy(framed + 4, command, command_bytes);
            n = htonl((uint32_t)quit_bytes);
            memcpy(framed + 4 + command_bytes, &n, 4);
            memcpy(framed + 8 + command_bytes, quit, quit_bytes);
            error = write_owned(cfg, directory, "request.json", framed, total);
            free(framed);
        }
    }
    if (!error) {
        result_fd = openat(directory, "result.json",
                           O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (result_fd < 0)
            error = "engine_io_failed";
    }
    if (!error) {
        snprintf(command_path, sizeof(command_path), "%s/request.json",
                 workspace);
        snprintf(result_path, sizeof(result_path), "%s/result.json", workspace);
        size_t bytes = 0;
        FILE *out = open_memstream(&arguments, &bytes);
        if (!out)
            error = "resource_limit";
        else {
            const char *args[] = {v->self,       "--vision-engine-bounded-bridge",
                                  v->executable, v->executable_sha256,
                                  command_path,  result_path};
            fputc('[', out);
            for (size_t k = 0; k < 6; k++) {
                if (k)
                    fputc(',', out);
                json_quote(out, args[k], strlen(args[k]));
            }
            fputc(']', out);
            if (fclose(out))
                error = "resource_limit";
        }
    }
    EngineProcess process = {.guardian = -1, .control = -1, .response = -1};
    if (!error) {
        double remaining = cfg->deadline_monotonic
                               ? cfg->deadline_monotonic - inference_now()
                               : 120;
        if (remaining <= 0)
            error = "deadline_exceeded";
        else
            error = engine_process_start_bound_workspace_captured(
                v->self, arguments, remaining, 262144, lease, model, workspace,
                directory, result_fd, &process);
    }
    while (!error && process.guardian > 0) {
        error = inference_interrupted(cfg);
        struct stat result;
        if (!error &&
            (fstat(result_fd, &result) || result.st_size > RESULT_MAX))
            error = "invalid_provider_response";
        if (error)
            break;
        struct pollfd port = {process.response, POLLIN | POLLHUP, 0};
        int ready = poll(&port, 1, 10);
        if (ready < 0 && errno == EINTR)
            continue;
        if (ready < 0) {
            error = "engine_io_failed";
            break;
        }
        if (ready) {
            const char *terminal = engine_process_stop(&process);
            if (terminal)
                error = !strcmp(terminal, "lifetime_exceeded")
                            ? "deadline_exceeded"
                            : !strcmp(terminal, "protocol_limit") ? "invalid_provider_response"
                            : "provider_process_failed";
            else if (strcmp(process.reason, "completed"))
                error = "provider_process_failed";
            break;
        }
    }
    if (process.guardian > 0) {
        const char *stopped = engine_process_stop(&process);
        if (stopped && !strcmp(stopped, "engine_shutdown_pending"))
            error = stopped;
    }
    if (!error)
        error = inference_interrupted(cfg);
    if (!error)
        error = temporal_package_verify(model, v->model, v->manifest_sha256,
                                        interrupted, (void *)cfg, package, 1);
    if (!error) {
        struct stat named;
        if (lstat(v->executable, &named) || !same(&named, &engine_receipt))
            error = "artifact_changed";
        else
            error = engine_verify(cfg, engine, &engine_receipt, 1);
    }
    unsigned char *body = NULL;
    size_t bytes = 0;
    if (!error) {
        struct stat result;
        if (fstat(result_fd, &result) || !S_ISREG(result.st_mode) ||
            result.st_nlink != 0 || result.st_size <= 0 ||
            result.st_size > RESULT_MAX)
            error = "invalid_provider_response";
        else {
            bytes = (size_t)result.st_size;
            body = malloc(bytes);
            if (!body)
                error = "resource_limit";
            else {
                size_t used = 0;
                while (used < bytes) {
                    ssize_t n = pread(result_fd, body + used, bytes - used,
                                      (off_t)used);
                    if (n < 0 && errno == EINTR)
                        continue;
                    if (n <= 0) {
                        error = "invalid_provider_response";
                        break;
                    }
                    used += (size_t)n;
                }
            }
        }
    }
    if (!error) {
        size_t offset = 0;
        char *reply = framed_reply(body, bytes, &offset),
             *quit = framed_reply(body, bytes, &offset);
        Json j, q;
        if (!reply || !quit || offset != bytes ||
            !json_parse(&j, reply, strlen(reply)) ||
            !json_parse(&q, quit, strlen(quit)) ||
            !equal(&j, 0, "status", "ok") ||
            !equal(&j, 0, "id", input->input_sha256) ||
            !equal(&q, 0, "status", "ok"))
            error = "invalid_provider_response";
        if (!error) {
            size_t frames = 0, images = 1;
            int times = json_get(&j, 0, "sample_times_seconds");
            *text = json_string(&j, json_get(&j, 0, "observation"));
            if (!*text || !**text || strlen(*text) > 16384 ||
                !json_uint(&j, json_get(&j, 0, "frames"), &frames) ||
                frames != input->count ||
                !json_uint(&j, json_get(&j, 0, "images"), &images) || images ||
                !json_uint(&j, json_get(&j, 0, "prompt_tokens"),
                           prompt_tokens) ||
                !*prompt_tokens || *prompt_tokens > 2048 ||
                !json_uint(&j, json_get(&j, 0, "generated_tokens"),
                           generated_tokens) ||
                !*generated_tokens || *generated_tokens > input->tokens ||
                times < 0 || j.tokens[times].type != JT_ARRAY ||
                j.tokens[times].count != (int)input->count)
                error = "invalid_provider_response";
            size_t k = 0;
            for (int t = times + 1; !error && t < j.tokens[times].next;
                 t = j.tokens[t].next, k++)
                if (!timestamp(&j, t, input->frames[k].timestamp_us))
                    error = "invalid_provider_response";
        }
        free(reply);
        free(quit);
    }
    free(body);
    free(package);
    free(command);
    free(arguments);
    if (directory >= 0) {
        for (size_t k = 0; k < 16; k++) {
            char name[32];
            snprintf(name, sizeof(name), "frame-%02zu.ppm", k);
            unlinkat(directory, name, 0);
        }
        unlinkat(directory, "request.json", 0);
        unlinkat(directory, "result.json", 0);
    }
    if (created)
        rmdir(workspace);
    if (result_fd >= 0)
        close(result_fd);
    if (directory >= 0)
        close(directory);
    if (engine >= 0)
        close(engine);
    if (model >= 0)
        close(model);
    if (lease >= 0)
        close(lease);
    return error;
}
int temporal_dispatch(const InferenceConfig *cfg, const Json *j, FILE *out) {
    if (!temporal_configured(cfg))
        return fail(out, "invalid_configuration");
    char *op = json_string(j, json_get(j, 0, "op"));
    if (!op)
        return fail(out, "invalid_request");
    if (!strcmp(op, "describe")) {
        free(op);
        if (j->tokens[0].count != 2)
            return fail(out, "unknown_field");
        fprintf(
            out,
            "{\"contract_version\":1,\"component\":\"samosa-inference\","
            "\"provider_id\":\"%s\",\"capabilities\":[\"temporal_visual_"
            "analysis\",\"operations\",\"cancellation\",\"request_budgets\"],"
            "\"input\":\"timestamped_rgba8_frames\",\"max_frames\":16,\"max_"
            "axis\":512,\"quality\":\"native_molmo2_sampled_uncertain\"}\n",
            cfg->provider_id);
        return 0;
    }
    if (strcmp(op, "analyze_video_frames")) {
        free(op);
        return fail(out, "unsupported_operation");
    }
    free(op);
    TemporalInput input;
    const char *error = temporal_input(cfg, j, &input);
    char *text = NULL;
    size_t in = 0, generated = 0;
    if (!error)
        error = run(cfg, &input, &text, &in, &generated);
    if (!error) {
        fprintf(out,
                "{\"contract_version\":1,\"provider_id\":\"%s\",\"kind\":"
                "\"derived\",\"artifact_type\":\"video_observation\",\"input_"
                "sha256\":\"%s\",\"text\":",
                cfg->provider_id, input.input_sha256);
        json_quote(out, text, strlen(text));
        fprintf(out,
                ",\"range\":{\"start_us\":%zu,\"end_us\":%zu,\"coordinate_"
                "space\":\"input_video_us\"},\"frames\":[",
                input.start_us, input.end_us);
        for (size_t k = 0; k < input.count; k++) {
            const TemporalFrame *frame = &input.frames[k];
            if (k)
                fputc(',', out);
            fprintf(out,
                    "{\"timestamp_us\":%zu,\"width\":%zu,\"height\":%zu,\"rgba_"
                    "sha256\":\"%s\",\"prepared_sha256\":\"%s\"}",
                    frame->timestamp_us, frame->width, frame->height,
                    frame->rgba_sha256, frame->prepared_sha256);
        }
        fprintf(out,
                "],\"usage\":{\"input_tokens\":%zu,\"output_tokens\":%zu},"
                "\"coverage\":{\"status\":\"partial\",\"reason\":\"sampled_"
                "video_analysis_uncertain\"},\"provenance\":{\"engine\":"
                "\"molmo2\",\"engine_sha256\":\"%s\",\"model_sha256\":\"%s\","
                "\"manifest_sha256\":\"%s\",\"prepared_frames_sha256\":\"%s\","
                "\"alpha_policy\":\"rgba8_composited_on_white\",\"timestamp_"
                "policy\":\"molmo2_one_decimal_seconds\",\"coordinate_space\":"
                "\"input_video_us\"}}\n",
                in, generated, cfg->temporal->executable_sha256,
                cfg->temporal->model_sha256, cfg->temporal->manifest_sha256,
                input.prepared_sha256);
    }
    free(text);
    temporal_input_free(&input);
    return error ? fail(out, error) : 0;
}
