#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "visual_assets.h"
#include "artifact.h"
#include "budget.h"
#include "sha256.h"
#include <fcntl.h>
#include <math.h>
#include <unistd.h>
static const char *verify_control(void *context) { return inference_interrupted(context); }
const char *vision_file_verify(const InferenceConfig *cfg, int fd, const char *digest, uint64_t ceiling,
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
int visual_same_receipt(const struct stat *a, const struct stat *b) {
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
const char *vision_model_verify(const InferenceConfig *cfg, int directory, VisionModelReceipt *receipt,
                                int recheck) {
    const char *names[] = {"config.json", "tokenizer.json", "model.safetensors"},
               *pins[] = {cfg->vision->config_sha256, cfg->vision->tokenizer_sha256,
                          cfg->vision->weights_sha256};
    const uint64_t limits[] = {1048576u, 8388608u, 8589934592ull};
    struct stat before, after, named;
    if (fstat(directory, &before) || lstat(cfg->vision->model, &named))
        return "artifact_unavailable";
    if (!visual_same_receipt(&before, &named) || (recheck && !visual_same_receipt(&before, &receipt->directory)))
        return "artifact_changed";
    for (size_t i = 0; i < 3; i++) {
        int fd = openat(directory, names[i], O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
        if (fd < 0)
            return "artifact_unavailable";
        struct stat file;
        const char *error = fstat(fd, &file) ? "artifact_unavailable" : NULL;
        if (!error && recheck && !visual_same_receipt(&file, &receipt->files[i]))
            error = "artifact_changed";
        if (!error)
            error = vision_file_verify(cfg, fd, pins[i], limits[i], 0);
        if (!error && (fstat(fd, &after) || !visual_same_receipt(&file, &after)))
            error = "artifact_changed";
        close(fd);
        if (error)
            return error;
        if (!recheck)
            receipt->files[i] = file;
    }
    if (fstat(directory, &after) || !visual_same_receipt(&before, &after))
        return "artifact_changed";
    if (!recheck)
        receipt->directory = before;
    return NULL;
}
const char *temporal_engine_verify(const InferenceConfig *cfg, int fd,
                                 struct stat *receipt, int recheck) {
    struct stat before, after;
    const char *error = fstat(fd, &before) ? "artifact_unavailable" : NULL;
    if (!error && (!S_ISREG(before.st_mode) || before.st_size <= 0 ||
                   before.st_uid != geteuid() || before.st_nlink != 1 ||
                   (before.st_mode & 022) || !(before.st_mode & 0111)))
        error = "unsupported_artifact";
    if (!error && recheck && !visual_same_receipt(&before, receipt))
        error = "artifact_changed";
    if (!error)
        error = artifact_verify_controlled(
            fd, (uint64_t)before.st_size, 67108864u,
            cfg->temporal->executable_sha256, verify_control, (void *)cfg);
    if (!error && (fstat(fd, &after) || !visual_same_receipt(&before, &after)))
        error = "artifact_changed";
    if (!error && !recheck)
        *receipt = before;
    return error;
}
const char *visual_write_owned(const InferenceConfig *cfg, int directory,
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
char *vision_ppm(const unsigned char *rgba, size_t width, size_t height, size_t *bytes, char prepared[65]) {
    char *ppm = NULL;
    FILE *f = open_memstream(&ppm, bytes);
    if (!f) return NULL;
    fprintf(f, "P6\n%zu %zu\n255\n", width, height);
    for (size_t i = 0; i < width * height; i++)
        for (size_t c = 0; c < 3; c++) {
            unsigned alpha = rgba[i * 4 + 3], value = ((unsigned)rgba[i * 4 + c] * alpha +
                                                       255u * (255u - alpha) + 127u) / 255u;
            fputc((int)value, f);
        }
    if (fclose(f)) { free(ppm); return NULL; }
    hash_bytes(ppm, *bytes, prepared);
    return ppm;
}
char *vision_command(const char *image, const char *input_digest, const char *prompt, size_t tokens, size_t *bytes) {
    char *command = NULL;
    FILE *f = open_memstream(&command, bytes);
    if (!f) return NULL;
    fprintf(f, "{\"command\":\"inspect\",\"id\":\"%s\",\"image_path\":", input_digest);
    json_quote(f, image, strlen(image));
    fputs(",\"prompt\":", f);
    json_quote(f, prompt, strlen(prompt));
    fprintf(f, ",\"max_tokens\":%zu,\"max_side_len\":512}", tokens);
    if (fclose(f)) { free(command); return NULL; }
    return command;
}
char *temporal_command(const char *workspace, const TemporalInput *input, size_t *bytes) {
    char *command = NULL;
    FILE *out = open_memstream(&command, bytes);
    if (!out) return NULL;
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

    if (fclose(out)) { free(command); return NULL; }
    return command;
}
const char *vision_result(const char *body, const char *input_digest, size_t tokens,
                         char **observation, size_t *input_tokens, size_t *output_tokens) {
    Json j;
    if (!json_parse(&j, body, strlen(body))) return "invalid_provider_response";
    char *status = json_string(&j, json_get(&j, 0, "status"));
    char *id = json_string(&j, json_get(&j, 0, "id"));
    *observation = json_string(&j, json_get(&j, 0, "observation"));
    const char *error = NULL;
    if (!status || strcmp(status, "ok") || !id || strcmp(id, input_digest) ||
        !*observation || !**observation || strlen(*observation) > 16384 ||
        !json_uint(&j, json_get(&j, 0, "prompt_tokens"), input_tokens) ||
        !json_uint(&j, json_get(&j, 0, "generated_tokens"), output_tokens) ||
        *input_tokens > 131072 || *output_tokens > tokens)
        error = "invalid_provider_response";
    free(status); free(id);
    return error;
}
const char *temporal_result(const char *reply, const TemporalInput *input,
                           char **text, size_t *prompt_tokens, size_t *generated_tokens) {
    Json j;
    if (strlen(reply) > 30000 || !json_parse(&j, reply, strlen(reply)) ||
        !equal(&j, 0, "status", "ok") || !equal(&j, 0, "id", input->input_sha256))
        return "invalid_provider_response";
    const char *error = NULL;
    size_t frames = 0, images = 1;
    int times = json_get(&j, 0, "sample_times_seconds");
    *text = json_string(&j, json_get(&j, 0, "observation"));
    if (!*text || !**text || strlen(*text) > 16384 ||
        !json_uint(&j, json_get(&j, 0, "frames"), &frames) || frames != input->count ||
        !json_uint(&j, json_get(&j, 0, "images"), &images) || images ||
        !json_uint(&j, json_get(&j, 0, "prompt_tokens"), prompt_tokens) ||
        !*prompt_tokens || *prompt_tokens > 2048 ||
        !json_uint(&j, json_get(&j, 0, "generated_tokens"), generated_tokens) ||
        !*generated_tokens || *generated_tokens > input->tokens ||
        times < 0 || j.tokens[times].type != JT_ARRAY || j.tokens[times].count != (int)input->count)
        error = "invalid_provider_response";
    size_t k = 0;
    for (int token = times + 1; !error && token < j.tokens[times].next; token = j.tokens[token].next, k++)
        if (!timestamp(&j, token, input->frames[k].timestamp_us)) error = "invalid_provider_response";
    return error;
}
