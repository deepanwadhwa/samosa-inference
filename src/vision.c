#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "admission.h"
#include "artifact.h"
#include "budget.h"
#include "engine_process.h"
#include "sha256.h"
#include "vision.h"
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>
#define IMAGE_MAX (512u * 512u * 4u)
#define RESULT_MAX ENGINE_VISUAL_RESULT_MAX
static const char *allowed =
    "|contract_version|op|provider_id|image_rgba_base64|width|height|prompt|max_output_tokens|"
    "operation_id|parent_operation_id|timeout_ms|";
static int fail(FILE *out, const char *error) {
    fprintf(out, "{\"contract_version\":1,\"error\":{\"code\":\"%s\"}}\n", error);
    return 1;
}
static int sha(const char *s) {
    return s && strlen(s) == 64 && strspn(s, "0123456789abcdef") == 64;
}
static int fields(const Json *j, const char *names) {
    for (int t = 1; t < j->tokens[0].next; t = j->tokens[j->tokens[t].next].next) {
        char *key = json_string(j, t), match[128];
        int n = snprintf(match, sizeof(match), "|%s|", key ? key : "");
        free(key);
        if (n < 0 || (size_t)n >= sizeof(match) || !strstr(names, match))
            return 0;
    }
    return 1;
}
void vision_model_identity(const VisionConfig *cfg, char out[65]) {
    out[0] = 0;
    if (!cfg || !sha(cfg->config_sha256) || !sha(cfg->tokenizer_sha256) ||
        !sha(cfg->weights_sha256))
        return;
    char material[256];
    int n = snprintf(material, sizeof(material), "visionpsy-model-v1:%s:%s:%s", cfg->config_sha256,
                     cfg->tokenizer_sha256, cfg->weights_sha256);
    if (n > 0 && (size_t)n < sizeof(material))
        hash_bytes(material, (size_t)n, out);
}
static int configured(const InferenceConfig *cfg) {
    if (!cfg || !cfg->vision || !cfg->admission_state)
        return 0;
    const VisionConfig *v = cfg->vision;
    char identity[65];
    if (!v->self || v->self[0] != '/' || !v->executable || v->executable[0] != '/' || !v->model ||
        v->model[0] != '/' || !sha(v->executable_sha256) || !sha(v->config_sha256) ||
        !sha(v->tokenizer_sha256) || !sha(v->weights_sha256) || !sha(v->model_sha256))
        return 0;
    vision_model_identity(v, identity);
    return !strcmp(identity, v->model_sha256);
}
char *vision_normalized(const InferenceConfig *cfg, const Json *j) {
    if (!configured(cfg) || !fields(j, allowed))
        return NULL;
    char *provider = json_string(j, json_get(j, 0, "provider_id")),
         *image = json_string(j, json_get(j, 0, "image_rgba_base64")),
         *prompt = json_string(j, json_get(j, 0, "prompt")),
         *parent = json_string(j, json_get(j, 0, "parent_operation_id"));
    size_t width = 0, height = 0, tokens = 0, budget = 0;
    int timeout = json_get(j, 0, "timeout_ms"),
        has_parent = json_get(j, 0, "parent_operation_id") >= 0;
    int valid = provider && image && prompt && strlen(prompt) <= 4096 &&
                json_uint(j, json_get(j, 0, "width"), &width) && width && width <= 512 &&
                json_uint(j, json_get(j, 0, "height"), &height) && height && height <= 512 &&
                json_uint(j, json_get(j, 0, "max_output_tokens"), &tokens) && tokens >= 32 &&
                tokens <= 1024 &&
                (timeout < 0 || (json_uint(j, timeout, &budget) && budget && budget <= 60000)) &&
                (!has_parent || (parent && *parent && strlen(parent) <= 128));
    if (parent)
        for (const char *p = parent; *p; p++)
            if (!isalnum((unsigned char)*p) && !strchr("-_:.", *p))
                valid = 0;
    char *body = NULL;
    size_t n = 0;
    FILE *f = valid ? open_memstream(&body, &n) : NULL;
    if (f) {
        fputs("{\"op\":\"analyze_image\",\"provider_id\":", f);
        json_quote(f, provider, strlen(provider));
        fputs(",\"configured_provider\":", f);
        json_quote(f, cfg->provider_id, strlen(cfg->provider_id));
        fputs(",\"image\":", f);
        json_quote(f, image, strlen(image));
        fputs(",\"prompt\":", f);
        json_quote(f, prompt, strlen(prompt));
        fprintf(f,
                ",\"width\":%zu,\"height\":%zu,\"max_output_tokens\":%zu,\"engine_sha256\":\"%s\","
                "\"model_sha256\":\"%s\"",
                width, height, tokens, cfg->vision->executable_sha256, cfg->vision->model_sha256);
        if (parent) {
            fputs(",\"parent_operation_id\":", f);
            json_quote(f, parent, strlen(parent));
        }
        if (timeout >= 0)
            fprintf(f, ",\"timeout_ms\":%zu", budget);
        fputc('}', f);
        if (fclose(f)) {
            free(body);
            body = NULL;
        }
    }
    free(provider);
    free(image);
    free(prompt);
    free(parent);
    return body;
}
static int b64(unsigned char c) {
    if (c >= 'A' && c <= 'Z')
        return c - 'A';
    if (c >= 'a' && c <= 'z')
        return c - 'a' + 26;
    if (c >= '0' && c <= '9')
        return c - '0' + 52;
    return c == '+' ? 62 : c == '/' ? 63 : -1;
}
static unsigned char *decode(const char *s, size_t expected) {
    size_t n = strlen(s);
    if (n != ((expected + 2) / 3) * 4 || expected > IMAGE_MAX)
        return NULL;
    unsigned char *bytes = malloc(n / 4 * 3);
    if (!bytes)
        return NULL;
    size_t used = 0;
    for (size_t i = 0; i < n; i += 4) {
        int a = b64((unsigned char)s[i]), b = b64((unsigned char)s[i + 1]),
            c = b64((unsigned char)s[i + 2]), d = b64((unsigned char)s[i + 3]),
            pc = s[i + 2] == '=', pd = s[i + 3] == '=';
        if (a < 0 || b < 0 || (!pc && c < 0) || (!pd && d < 0) || (pc && !pd) ||
            ((pc || pd) && i + 4 != n) || (pc && (b & 15)) || (!pc && pd && (c & 3))) {
            free(bytes);
            return NULL;
        }
        bytes[used++] = (unsigned char)((a << 2) | (b >> 4));
        if (!pc)
            bytes[used++] = (unsigned char)((b << 4) | (c >> 2));
        if (!pd)
            bytes[used++] = (unsigned char)((c << 6) | d);
    }
    if (used != expected) {
        free(bytes);
        return NULL;
    }
    return bytes;
}
static const char *verify_control(void *context) { return inference_interrupted(context); }
static const char *verify(const InferenceConfig *cfg, int fd, const char *digest, uint64_t ceiling,
                          int executable) {
    const char *error = inference_interrupted(cfg);
    struct stat s;
    if (!error &&
        (fstat(fd, &s) || !S_ISREG(s.st_mode) || s.st_size <= 0 || s.st_nlink != 1 ||
         s.st_uid != geteuid() || (s.st_mode & 022) || (executable && !(s.st_mode & 0111))))
        error = "unsupported_artifact";
    if (!error)
        error = artifact_verify_controlled(fd, (uint64_t)s.st_size, ceiling, digest,
                                           cfg ? verify_control : NULL, (void *)cfg);
    if (!error)
        error = inference_interrupted(cfg);
    return error;
}
typedef struct {
    struct stat directory, files[3];
} ModelReceipt;
static int same_receipt(const struct stat *a, const struct stat *b) {
    if (a->st_dev != b->st_dev || a->st_ino != b->st_ino || a->st_size != b->st_size ||
        a->st_mode != b->st_mode || a->st_uid != b->st_uid || a->st_nlink != b->st_nlink)
        return 0;
#ifdef __APPLE__
    return a->st_mtimespec.tv_sec == b->st_mtimespec.tv_sec &&
           a->st_mtimespec.tv_nsec == b->st_mtimespec.tv_nsec &&
           a->st_ctimespec.tv_sec == b->st_ctimespec.tv_sec &&
           a->st_ctimespec.tv_nsec == b->st_ctimespec.tv_nsec;
#else
    return a->st_mtim.tv_sec == b->st_mtim.tv_sec && a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
           a->st_ctim.tv_sec == b->st_ctim.tv_sec && a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
#endif
}
static const char *model_verify(const InferenceConfig *cfg, int directory, ModelReceipt *receipt,
                                int recheck) {
    const char *names[] = {"config.json", "tokenizer.json", "model.safetensors"},
               *pins[] = {cfg->vision->config_sha256, cfg->vision->tokenizer_sha256,
                          cfg->vision->weights_sha256};
    const uint64_t limits[] = {1048576u, 8388608u, 8589934592ull};
    struct stat before, after, named;
    if (fstat(directory, &before) || lstat(cfg->vision->model, &named))
        return "artifact_unavailable";
    if (!same_receipt(&before, &named) || (recheck && !same_receipt(&before, &receipt->directory)))
        return "artifact_changed";
    for (size_t i = 0; i < 3; i++) {
        int fd = openat(directory, names[i], O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
        if (fd < 0)
            return "artifact_unavailable";
        struct stat file;
        const char *error = fstat(fd, &file) ? "artifact_unavailable" : NULL;
        if (!error && recheck && !same_receipt(&file, &receipt->files[i]))
            error = "artifact_changed";
        if (!error)
            error = verify(cfg, fd, pins[i], limits[i], 0);
        if (!error && (fstat(fd, &after) || !same_receipt(&file, &after)))
            error = "artifact_changed";
        close(fd);
        if (error)
            return error;
        if (!recheck)
            receipt->files[i] = file;
    }
    if (fstat(directory, &after) || !same_receipt(&before, &after))
        return "artifact_changed";
    if (!recheck)
        receipt->directory = before;
    return NULL;
}
static const char *run(const InferenceConfig *cfg, const unsigned char *rgba, size_t width,
                       size_t height, const char *prompt, size_t tokens, char **observation,
                       size_t *input_tokens, size_t *output_tokens, char prepared[65]) {
    const VisionConfig *v = cfg->vision;
    int lease = -1, model = -1, executable = -1, directory = -1, result_fd = -1;
    const char *error = inference_interrupted(cfg);
    if (!error)
        error = inference_schedule(cfg, &lease);
    if (!error) {
        model = open(v->model, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        struct stat s;
        if (model < 0)
            error = "artifact_unavailable";
        else if (fstat(model, &s) || s.st_uid != geteuid() || (s.st_mode & 022))
            error = "unsupported_artifact";
    }
    ModelReceipt receipt = {0};
    if (!error)
        error = model_verify(cfg, model, &receipt, 0);
    struct stat executable_receipt = {0};
    if (!error) {
        executable = open(v->executable, O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
        error = executable < 0 ? "artifact_unavailable"
                               : verify(cfg, executable, v->executable_sha256, 67108864u, 1);
    }
    if (!error && fstat(executable, &executable_receipt))
        error = "artifact_unavailable";
    char workspace[] = "/tmp/samosa-inference-vision-XXXXXX", image[256] = {0}, command[256] = {0},
         result[256] = {0};
    int created = 0;
    char *ppm = NULL, *arguments = NULL;
    size_t ppm_bytes = 0;
    if (!error) {
        FILE *f = open_memstream(&ppm, &ppm_bytes);
        if (!f)
            error = "resource_limit";
        else {
            fprintf(f, "P6\n%zu %zu\n255\n", width, height);
            for (size_t i = 0; i < width * height; i++)
                for (size_t c = 0; c < 3; c++) {
                    unsigned alpha = rgba[i * 4 + 3], value = ((unsigned)rgba[i * 4 + c] * alpha +
                                                               255u * (255u - alpha) + 127u) /
                                                              255u;
                    fputc((int)value, f);
                }
            if (fclose(f))
                error = "resource_limit";
            else
                hash_bytes(ppm, ppm_bytes, prepared);
        }
    }
    if (!error) {
        created = mkdtemp(workspace) != NULL;
        if (!created)
            error = "engine_io_failed";
        else {
            snprintf(image, sizeof(image), "%s/input.ppm", workspace);
            snprintf(command, sizeof(command), "%s/request.json", workspace);
            snprintf(result, sizeof(result), "%s/result.json", workspace);
            directory = open(workspace, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            if (directory < 0)
                error = "engine_io_failed";
        }
    }
    if (!error) {
        int fd = open(image, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (fd < 0)
            error = "engine_io_failed";
        else {
            size_t used = 0;
            while (used < ppm_bytes) {
                ssize_t n = write(fd, ppm + used, ppm_bytes - used);
                if (n < 0 && errno == EINTR)
                    continue;
                if (n <= 0) {
                    error = "engine_io_failed";
                    break;
                }
                used += (size_t)n;
            }
            close(fd);
        }
    }
    char input_digest[65];
    hash_bytes(rgba, width * height * 4, input_digest);
    if (!error) {
        int fd = open(command, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        FILE *f = fd < 0 ? NULL : fdopen(fd, "w");
        if (!f) {
            if (fd >= 0)
                close(fd);
            error = "engine_io_failed";
        } else {
            fprintf(f, "{\"command\":\"inspect\",\"id\":\"%s\",\"image_path\":", input_digest);
            json_quote(f, image, strlen(image));
            fputs(",\"prompt\":", f);
            json_quote(f, prompt, strlen(prompt));
            fprintf(f, ",\"max_tokens\":%zu,\"max_side_len\":512}\n{\"command\":\"quit\"}\n",
                    tokens);
            if (fclose(f))
                error = "engine_io_failed";
        }
    }
    if (!error) {
        result_fd = open(result, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
        if (result_fd < 0)
            error = "engine_io_failed";
    }
    if (!error) {
        size_t n = 0;
        FILE *f = open_memstream(&arguments, &n);
        if (!f)
            error = "resource_limit";
        else {
            const char *args[] = {v->self,       "--vision-engine-bounded-bridge",
                                  v->executable, v->executable_sha256,
                                  command,       result};
            fputc('[', f);
            for (size_t i = 0; i < 6; i++) {
                if (i)
                    fputc(',', f);
                json_quote(f, args[i], strlen(args[i]));
            }
            fputc(']', f);
            if (fclose(f))
                error = "resource_limit";
        }
    }
    EngineProcess process = {.guardian = -1, .control = -1, .response = -1};
    if (!error) {
        double remaining = cfg->deadline_monotonic ? cfg->deadline_monotonic - inference_now() : 60;
        if (remaining <= 0)
            error = "deadline_exceeded";
        else
            error =
                engine_process_start_bound_workspace_captured(v->self, arguments, remaining, 262144, lease,
                                                     model, workspace, directory, result_fd, &process);
    }
    while (!error && process.guardian > 0) {
        error = inference_interrupted(cfg);
        struct stat s;
        if (!error && (fstat(result_fd, &s) || s.st_size > RESULT_MAX))
            error = "invalid_provider_response";
        if (error)
            break;
        struct pollfd p = {process.response, POLLIN | POLLHUP, 0};
        int ready = poll(&p, 1, 10);
        if (ready < 0 && errno == EINTR)
            continue;
        if (ready < 0) {
            error = "engine_io_failed";
            break;
        }
        if (ready) {
            const char *terminal = engine_process_stop(&process);
            if (terminal)
                error = !strcmp(terminal, "lifetime_exceeded") ? "deadline_exceeded"
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
        error = model_verify(cfg, model, &receipt, 1);
    if (!error) {
        struct stat retained, named;
        if (fstat(executable, &retained) || lstat(v->executable, &named) ||
            !same_receipt(&retained, &executable_receipt) || !same_receipt(&retained, &named))
            error = "artifact_changed";
    }
    if (!error)
        error = verify(cfg, executable, v->executable_sha256, 67108864u, 1);
    char *body = NULL;
    if (!error) {
        struct stat s;
        if (fstat(result_fd, &s) || !S_ISREG(s.st_mode) || s.st_nlink != 0 || s.st_size <= 0 ||
            s.st_size > RESULT_MAX)
            error = "invalid_provider_response";
        else {
            body = malloc((size_t)s.st_size + 1);
            if (!body)
                error = "resource_limit";
            else {
                size_t used = 0;
                while (used < (size_t)s.st_size) {
                    ssize_t n = read(result_fd, body + used, (size_t)s.st_size - used);
                    if (n < 0 && errno == EINTR)
                        continue;
                    if (n <= 0) {
                        error = "invalid_provider_response";
                        break;
                    }
                    used += (size_t)n;
                }
                body[used] = 0;
                if (memchr(body, 0, used))
                    error = "invalid_provider_response";
            }
        }
    }
    if (!error) {
        char *line = strchr(body, '\n');
        if (!line)
            error = "invalid_provider_response";
        else {
            *line = 0;
            Json j, q;
            char *status = NULL, *id = NULL, *bye = NULL, *quit_status = NULL;
            if (!json_parse(&j, body, strlen(body)) || !json_parse(&q, line + 1, strlen(line + 1)))
                error = "invalid_provider_response";
            else {
                status = json_string(&j, json_get(&j, 0, "status"));
                id = json_string(&j, json_get(&j, 0, "id"));
                bye = json_string(&q, json_get(&q, 0, "message"));
                quit_status = json_string(&q, json_get(&q, 0, "status"));
                *observation = json_string(&j, json_get(&j, 0, "observation"));
                if (!status || strcmp(status, "ok") || !id || strcmp(id, input_digest) || !bye ||
                    strcmp(bye, "bye") || !quit_status || strcmp(quit_status, "ok") ||
                    !*observation || !**observation || strlen(*observation) > 16384 ||
                    !json_uint(&j, json_get(&j, 0, "prompt_tokens"), input_tokens) ||
                    !json_uint(&j, json_get(&j, 0, "generated_tokens"), output_tokens) ||
                    *input_tokens > 131072 || *output_tokens > tokens)
                    error = "invalid_provider_response";
            }
            free(status);
            free(id);
            free(bye);
            free(quit_status);
        }
    }
    free(body);
    free(ppm);
    free(arguments);
    if (created) {
        unlink(image);
        unlink(command);
        unlink(result);
        rmdir(workspace);
    }
    if (result_fd >= 0)
        close(result_fd);
    if (directory >= 0)
        close(directory);
    if (model >= 0)
        close(model);
    if (executable >= 0)
        close(executable);
    if (lease >= 0)
        close(lease);
    return error;
}
int vision_dispatch(const InferenceConfig *cfg, const Json *j, FILE *out) {
    if (!configured(cfg))
        return fail(out, "invalid_configuration");
    char *op = json_string(j, json_get(j, 0, "op"));
    if (!op)
        return fail(out, "invalid_request");
    if (!strcmp(op, "describe")) {
        free(op);
        if (!fields(j, "|contract_version|op|"))
            return fail(out, "unknown_field");
        fprintf(out,
                "{\"contract_version\":1,\"component\":\"samosa-inference\",\"provider_id\":\"%s\","
                "\"capabilities\":[\"visual_analysis\",\"operations\",\"cancellation\",\"request_"
                "budgets\"],\"input\":\"rgba8_base64\",\"max_axis\":512,\"max_output_tokens\":1024,"
                "\"quality\":\"native_visionpsy\"}\n",
                cfg->provider_id);
        return 0;
    }
    if (strcmp(op, "analyze_image")) {
        free(op);
        return fail(out, "unsupported_operation");
    }
    free(op);
    if (!fields(j, allowed))
        return fail(out, "unknown_field");
    char *provider = json_string(j, json_get(j, 0, "provider_id")),
         *encoded = json_string(j, json_get(j, 0, "image_rgba_base64")),
         *prompt = json_string(j, json_get(j, 0, "prompt"));
    size_t width = 0, height = 0, tokens = 0;
    const char *error = NULL;
    if (!provider || !encoded || !prompt || !*prompt || strlen(prompt) > 4096 ||
        !json_uint(j, json_get(j, 0, "width"), &width) || !width || width > 512 ||
        !json_uint(j, json_get(j, 0, "height"), &height) || !height || height > 512 ||
        !json_uint(j, json_get(j, 0, "max_output_tokens"), &tokens) || tokens < 32 || tokens > 1024)
        error = "invalid_request";
    else if (strcmp(provider, cfg->provider_id))
        error = "provider_unavailable";
    unsigned char *rgba = error ? NULL : decode(encoded, width * height * 4);
    if (!error && !rgba)
        error = "invalid_image";
    char *observation = NULL, digest[65], prepared[65];
    size_t input_tokens = 0, output_tokens = 0;
    if (!error) {
        hash_bytes(rgba, width * height * 4, digest);
        error = run(cfg, rgba, width, height, prompt, tokens, &observation, &input_tokens,
                    &output_tokens, prepared);
    }
    if (!error) {
        fprintf(
            out,
            "{\"contract_version\":1,\"provider_id\":\"%s\",\"kind\":\"derived\",\"artifact_type\":"
            "\"visual_observation\",\"input_sha256\":\"%s\",\"width\":%zu,\"height\":%zu,\"text\":",
            cfg->provider_id, digest, width, height);
        json_quote(out, observation, strlen(observation));
        fprintf(out,
                ",\"usage\":{\"input_tokens\":%zu,\"output_tokens\":%zu},\"coverage\":{\"status\":"
                "\"partial\",\"reason\":\"visual_analysis_uncertain\"},\"provenance\":{\"engine\":"
                "\"visionpsy\",\"engine_sha256\":\"%s\",\"model_sha256\":\"%s\",\"prepared_image_"
                "sha256\":\"%s\",\"alpha_policy\":\"rgba8_composited_on_white\",\"coordinate_"
                "space\":\"input_pixels\"}}\n",
                input_tokens, output_tokens, cfg->vision->executable_sha256,
                cfg->vision->model_sha256, prepared);
    }
    free(provider);
    free(encoded);
    free(prompt);
    free(rgba);
    free(observation);
    return error ? fail(out, error) : 0;
}
