#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/specialist_protocol.h>
#include "specialist_supervisor_internal.h"
/* Reuse the existing engine's original Chat JSON parser. */
#include "../engines/json.h"
#include <math.h>
#include <time.h>

#define MOLMO2_MAX_IMAGES_PER_REQUEST SAMOSA_MM_MOLMO2_MAX_IMAGES
#define MOLMO2_MAX_FRAMES_PER_REQUEST SAMOSA_MM_MOLMO2_MAX_FRAMES

static long long protocol_now_ms(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (long long)ts.tv_sec * 1000LL + ts.tv_nsec / 1000000LL;
}
static void protocol_trace_event(const SamosaMmTrace *trace, const char *event, const char *fields) {
    if (trace && trace->event) trace->event(trace->context, event, fields);
}
static void protocol_trace_payload(const SamosaMmTrace *trace, const char *event, const char *provider, const char *bytes, size_t size) {
    if (trace && trace->payload) trace->payload(trace->context, event, provider, bytes, size);
}

static int path_copy(char *out, size_t cap, const char *value) {
    int n = snprintf(out, cap, "%s", value ? value : "");
    return n >= 0 && (size_t)n < cap;
}

static int json_escape_to(char *out, size_t cap, size_t *used, const char *text) {
    static const char hex[] = "0123456789abcdef";
    for (const unsigned char *p = (const unsigned char *)text; *p; ) {
        char encoded[7]; const char *part = encoded; size_t length;
        unsigned char c = *p;
        if (c == '"' || c == '\\') { encoded[0] = '\\'; encoded[1] = (char)c; length = 2; ++p; }
        else if (c == '\n') { part = "\\n"; length = 2; ++p; }
        else if (c == '\r') { part = "\\r"; length = 2; ++p; }
        else if (c == '\t') { part = "\\t"; length = 2; ++p; }
        else if (c < 0x20) { memcpy(encoded, "\\u00", 4); encoded[4] = hex[c >> 4]; encoded[5] = hex[c & 15]; length = 6; ++p; }
        else if (c < 0x80) { encoded[0] = (char)c; length = 1; ++p; }
        else {
            /* JSON strings are UTF-8 on the wire. Replace malformed input
               from fetched pages/files instead of emitting invalid JSON. */
            size_t width = 0;
            unsigned int codepoint = 0;
            if (c >= 0xc2 && c <= 0xdf) { width = 2; codepoint = c & 0x1f; }
            else if (c >= 0xe0 && c <= 0xef) { width = 3; codepoint = c & 0x0f; }
            else if (c >= 0xf0 && c <= 0xf4) { width = 4; codepoint = c & 0x07; }
            int valid = width != 0;
            for (size_t i = 1; valid && i < width; ++i)
                valid = p[i] && (p[i] & 0xc0) == 0x80;
            if (valid) {
                for (size_t i = 1; i < width; ++i) codepoint = (codepoint << 6) | (p[i] & 0x3f);
                valid = (width == 2 && codepoint >= 0x80) ||
                        (width == 3 && codepoint >= 0x800 && !(codepoint >= 0xd800 && codepoint <= 0xdfff)) ||
                        (width == 4 && codepoint >= 0x10000 && codepoint <= 0x10ffff);
            }
            if (valid) { part = (const char *)p; length = width; ++p; for (size_t i = 1; i < width; ++i) ++p; }
            else { encoded[0] = (char)0xef; encoded[1] = (char)0xbf; encoded[2] = (char)0xbd; length = 3; ++p; }
        }
        if (*used + length >= cap) return 0;
        memcpy(out + *used, part, length); *used += length;
    }
    out[*used] = 0; return 1;
}

typedef struct {
    char *data;
    size_t len;
    size_t cap;
} TextBuffer;

static int text_reserve(TextBuffer *buffer, size_t extra) {
    if (extra > SIZE_MAX - buffer->len - 1) return 0;
    size_t needed = buffer->len + extra + 1;
    if (needed <= buffer->cap) return 1;
    size_t cap = buffer->cap ? buffer->cap : 4096;
    while (cap < needed) {
        if (cap > SIZE_MAX / 2) { cap = needed; break; }
        cap *= 2;
    }
    char *next = realloc(buffer->data, cap);
    if (!next) return 0;
    buffer->data = next; buffer->cap = cap;
    return 1;
}

static int text_add_n(TextBuffer *buffer, const char *text, size_t length) {
    if (!text_reserve(buffer, length)) return 0;
    memcpy(buffer->data + buffer->len, text, length);
    buffer->len += length; buffer->data[buffer->len] = 0;
    return 1;
}

static int text_add(TextBuffer *buffer, const char *text) {
    return text_add_n(buffer, text, strlen(text));
}

static int text_json_string(TextBuffer *buffer, const char *value) {
    if (!text_add(buffer, "\"")) return 0;
    size_t source_len = strlen(value);
    size_t cap = source_len * 6 + 1;
    char *escaped = malloc(cap);
    size_t used = 0;
    if (!escaped || !json_escape_to(escaped, cap, &used, value)) {
        free(escaped); return 0;
    }
    int ok = text_add_n(buffer, escaped, used) && text_add(buffer, "\"");
    free(escaped); return ok;
}

static int vision_json_bounded_integer(jval *value, int minimum, int maximum,
                                       int *output) {
    if (!value || value->t != J_NUM || !isfinite(value->num) ||
        floor(value->num) != value->num || value->num < minimum ||
        value->num > maximum) return 0;
    if (output) *output = (int)value->num;
    return 1;
}

int samosa_mm_molmo2_analyze_request(
                                  SamosaMmSession *s, const SamosaMmTrace *trace,
                                  const char *media_path,
                                  const char *const *media_paths, int image_count,
                                  const char *media_kind, const char *prompt,
                                  int max_tokens, double start_seconds,
                                  double end_seconds, int max_frames,
                                  char *out_obs, size_t out_cap,
                                  int *out_prompt_tokens, int *out_gen_tokens,
                                  double *out_duration_seconds, int *out_frames,
                                  int *out_images,
                                  char *out_err_code, size_t err_cap) {
    if (!s || !s->active) {
        path_copy(out_err_code, err_cap, "vision_not_ready"); return 0;
    }
    TextBuffer cmd = {0}; char numbers[256];
    int ok = text_add(&cmd, "{\"command\":\"analyze\",\"id\":\"gateway\",");
    if (image_count > 0) {
        ok = ok && image_count <= MOLMO2_MAX_IMAGES_PER_REQUEST &&
             media_paths && text_add(&cmd, "\"media_paths\":[");
        for (int index = 0; ok && index < image_count; ++index) {
            if (index) ok = text_add(&cmd, ",");
            ok = ok && media_paths[index] && text_json_string(&cmd, media_paths[index]);
        }
        ok = ok && text_add(&cmd, "],\"media_kind\":\"images\",\"prompt\":");
    } else {
        ok = ok && media_path && text_add(&cmd, "\"media_path\":") &&
             text_json_string(&cmd, media_path) && text_add(&cmd, ",\"media_kind\":") &&
             text_json_string(&cmd, media_kind) && text_add(&cmd, ",\"prompt\":");
    }
    ok = ok && text_json_string(
        &cmd, prompt && *prompt ? prompt : "Describe the visual content in detail.");
    snprintf(numbers, sizeof(numbers),
             ",\"max_tokens\":%d,\"start_seconds\":%.3f,\"end_seconds\":%.3f,\"max_frames\":%d}",
             max_tokens >= 32 && max_tokens <= 1024 ? max_tokens : 256,
             start_seconds, end_seconds,
             max_frames >= 1 && max_frames <= MOLMO2_MAX_FRAMES_PER_REQUEST
                 ? max_frames : MOLMO2_MAX_FRAMES_PER_REQUEST);
    ok = ok && text_add(&cmd, numbers);
    if (!ok) { free(cmd.data); path_copy(out_err_code, err_cap, "vision_request_encode_failed"); return 0; }
    protocol_trace_payload(trace, "molmo2_request", "samosa-molmo2", cmd.data, cmd.len);
    char *reply = NULL; size_t reply_len = 0; char transport[SAMOSA_MM_ERROR_MAX] = {0};
    int request_ok = samosa_mm_session_request(s, cmd.data, cmd.len, &reply, &reply_len,
                                               transport, sizeof(transport));
    free(cmd.data);
    if (!request_ok) {
        path_copy(out_err_code, err_cap, strstr(transport, "timeout") ? "vision_timeout" : "vision_pipe_error");
        free(reply); return 0;
    }
    protocol_trace_payload(trace, "molmo2_raw_response", "samosa-molmo2", reply, reply_len);
    char *arena = NULL; jval *root = json_parse(reply, &arena);
    jval *status = root && root->t == J_OBJ ? json_get(root, "status") : NULL;
    jval *reply_id = root && root->t == J_OBJ ? json_get(root, "id") : NULL;
    if (!reply_id || reply_id->t != J_STR || strcmp(reply_id->str, "gateway")) {
        path_copy(out_err_code, err_cap, "vision_malformed_response");
        json_free(root); free(arena); free(reply); return 0;
    }
    if (!status || status->t != J_STR || strcmp(status->str, "ok")) {
        jval *code = root && root->t == J_OBJ ? json_get(root, "code") : NULL;
        path_copy(out_err_code, err_cap,
                  code && code->t == J_STR ? code->str : "vision_inference_failed");
        json_free(root); free(arena); free(reply); return 0;
    }
    jval *observation = json_get(root, "observation");
    if (!observation || observation->t != J_STR) {
        path_copy(out_err_code, err_cap, "vision_malformed_response");
        json_free(root); free(arena); free(reply); return 0;
    }
    path_copy(out_obs, out_cap, observation->str);
    jval *pt = json_get(root, "prompt_tokens"), *gt = json_get(root, "generated_tokens");
    jval *duration = json_get(root, "duration_seconds"), *frames = json_get(root, "frames");
    jval *images = json_get(root, "images");
    int prompt_tokens = 0, generated_tokens = 0, decoded_frames = 0, decoded_images = 0;
    int numeric_ok = vision_json_bounded_integer(pt, 0, 36864, &prompt_tokens) &&
                     vision_json_bounded_integer(gt, 0, 1024, &generated_tokens) &&
                     vision_json_bounded_integer(frames, 0,
                                                 MOLMO2_MAX_FRAMES_PER_REQUEST,
                                                 &decoded_frames) &&
                     vision_json_bounded_integer(images, 0,
                                                 MOLMO2_MAX_IMAGES_PER_REQUEST,
                                                 &decoded_images) &&
                     duration && duration->t == J_NUM && isfinite(duration->num) &&
                     duration->num >= 0.0 &&
                     (!image_count || decoded_images == image_count);
    if (!numeric_ok) {
        path_copy(out_err_code, err_cap, "vision_malformed_response");
        json_free(root); free(arena); free(reply); return 0;
    }
    if (out_prompt_tokens) *out_prompt_tokens = prompt_tokens;
    if (out_gen_tokens) *out_gen_tokens = generated_tokens;
    if (out_duration_seconds) *out_duration_seconds = duration->num;
    if (out_frames) *out_frames = decoded_frames;
    if (out_images) *out_images = decoded_images;
    json_free(root); free(arena); free(reply);
    if (out_err_code && err_cap) out_err_code[0] = 0;
    return 1;
}

int samosa_mm_visionpsy_inspect(SamosaMmSession *s, const SamosaMmTrace *trace, const char *image_path, const char *prompt,
                                     int max_side_len, int max_tokens,
                                     char *out_obs, size_t out_cap,
                                     int *out_prompt_tokens, int *out_gen_tokens,
                                     char *out_err_code, size_t err_cap) {
    if (!s || !s->active || s->in_fd < 0 || s->out_fd < 0) {
        if (out_err_code) path_copy(out_err_code, err_cap, "vision_not_ready");
        return 0;
    }
    TextBuffer cmd = {0};
    text_add(&cmd, "{\"command\":\"inspect\",\"image_path\":");
    text_json_string(&cmd, image_path);
    text_add(&cmd, ",\"prompt\":");
    text_json_string(&cmd, prompt && *prompt ? prompt : "Describe what is visible in this image in detail.");
    char token_buf[96];
    snprintf(token_buf, sizeof(token_buf), ",\"max_tokens\":%d,\"max_side_len\":",
             max_tokens >= 32 && max_tokens <= 1024 ? max_tokens : 256);
    text_add(&cmd, token_buf);
    char len_buf[32]; snprintf(len_buf, sizeof(len_buf), "%d}", max_side_len > 0 ? max_side_len : 2048);
    text_add(&cmd, len_buf);

    long long started = protocol_now_ms();
    protocol_trace_payload(trace, "visionpsy_request", "samosa-visionpsy",
                            cmd.data, cmd.len);
    char *line = NULL; size_t line_len = 0;
    char transport_error[SAMOSA_MM_ERROR_MAX] = {0};
    int request_ok = samosa_mm_session_request(s, cmd.data, cmd.len,
                                               &line, &line_len,
                                               transport_error, sizeof(transport_error));
    free(cmd.data);
    if (!request_ok) {
        const char *vision_code = strstr(transport_error, "timeout")
                                ? "vision_timeout" : "vision_pipe_error";
        TextBuffer fields = {0};
        if (text_add(&fields, "\"code\":") && text_json_string(&fields, vision_code) &&
            text_add(&fields, ",\"transport_code\":") && text_json_string(&fields, transport_error))
            protocol_trace_event(trace, "visionpsy_error", fields.data);
        free(fields.data);
        if (out_err_code) path_copy(out_err_code, err_cap, vision_code);
        free(line);
        return 0;
    }
    protocol_trace_payload(trace, "visionpsy_raw_response", "samosa-visionpsy",
                            line, line_len);

    char *arena = NULL;
    jval *root = json_parse(line, &arena);
    if (!root || root->t != J_OBJ) {
        protocol_trace_event(trace, "visionpsy_error", "\"code\":\"vision_malformed_response\"");
        if (root) json_free(root);
        if (arena) free(arena);
        if (out_err_code) path_copy(out_err_code, err_cap, "vision_malformed_response");
        free(line);
        return 0;
    }

    jval *status_v = json_get(root, "status");
    if (!status_v || status_v->t != J_STR || strcmp(status_v->str, "ok")) {
        jval *code_v = json_get(root, "code");
        const char *failure = (code_v && code_v->t == J_STR) ? code_v->str : "vision_inference_failed";
        if (out_err_code) path_copy(out_err_code, err_cap, failure);
        TextBuffer failure_fields = {0};
        if (text_add(&failure_fields, "\"code\":") && text_json_string(&failure_fields, failure))
            protocol_trace_event(trace, "visionpsy_error", failure_fields.data);
        free(failure_fields.data);
        json_free(root); free(arena);
        free(line);
        return 0;
    }

    jval *obs_v = json_get(root, "observation");
    if (obs_v && obs_v->t == J_STR && out_obs && out_cap > 0) {
        path_copy(out_obs, out_cap, obs_v->str);
    }
    jval *pt_v = json_get(root, "prompt_tokens");
    if (pt_v && pt_v->t == J_NUM && out_prompt_tokens) *out_prompt_tokens = (int)pt_v->num;
    jval *gt_v = json_get(root, "generated_tokens");
    if (gt_v && gt_v->t == J_NUM && out_gen_tokens) *out_gen_tokens = (int)gt_v->num;

    {
        char fields[192];
        snprintf(fields, sizeof(fields),
                 "\"max_side_len\":%d,\"prompt_tokens\":%d,\"generated_tokens\":%d,"
                 "\"observation_chars\":%zu,\"duration_ms\":%lld",
                 max_side_len, out_prompt_tokens ? *out_prompt_tokens : 0,
                 out_gen_tokens ? *out_gen_tokens : 0,
                 obs_v && obs_v->t == J_STR ? strlen(obs_v->str) : 0,
                 protocol_now_ms() - started);
        protocol_trace_event(trace, "visionpsy_complete", fields);
    }

    json_free(root); free(arena); free(line);
    return 1;
}

/* Existing gateway visual launch profiles; caller supplies selection policy. */
SamosaMmProvider mm_visual_profile(int molmo, const char *engine, const char *model) {
    return (SamosaMmProvider){
        .provider = molmo ? "molmo2_4b" : "visionpsy_nano_460m",
        .executable = engine,
        .model_dir = model,
        .protocol = molmo ? SAMOSA_MM_PROTOCOL_JSON_FRAME_V1 : SAMOSA_MM_PROTOCOL_JSON_LINE,
        .ready_timeout_ms = molmo ? 90000 : 20000,
        .request_timeout_ms = molmo ? 600000 : 120000,
        .shutdown_grace_ms = molmo ? 3000 : 2000,
        .max_frame_bytes = molmo ? 1024 * 1024 : 65536
    };
}

int samosa_mm_visionpsy_start(SamosaMmSupervisor *supervisor, SamosaMmSession *s,
                             const SamosaMmTrace *trace, const char *engine,
                             const char *model, int available) {
    if (!s) return 0;
    if (!available) {
        protocol_trace_event(trace, "visionpsy_start_failed", "\"code\":\"model_unavailable\"");
        return 0;
    }
    TextBuffer start_fields = {0};
    if (text_add(&start_fields, "\"engine\":") && text_json_string(&start_fields, engine) &&
        text_add(&start_fields, ",\"model_path\":") && text_json_string(&start_fields, model))
        protocol_trace_event(trace, "visionpsy_starting", start_fields.data);
    free(start_fields.data);
    SamosaMmProvider provider = mm_visual_profile(0, engine, model);
    char error_code[SAMOSA_MM_ERROR_MAX] = {0};
    if (!samosa_mm_session_start(supervisor, &provider, s,
                                 error_code, sizeof(error_code))) {
        TextBuffer fields = {0};
        if (text_add(&fields, "\"code\":") &&
            text_json_string(&fields, error_code[0] ? error_code : "start_failed"))
            protocol_trace_event(trace, "visionpsy_start_failed", fields.data);
        free(fields.data);
        return 0;
    }
    char fields[96];
    snprintf(fields, sizeof(fields), "\"pid\":%ld,\"supervisor\":\"multimodal_v1\"",
             (long)s->pid);
    protocol_trace_event(trace, "visionpsy_process_started", fields);
    protocol_trace_event(trace, "visionpsy_ready", NULL);
    return 1;
}

int samosa_mm_molmo2_start(SamosaMmSupervisor *supervisor, SamosaMmSession *s,
                           const SamosaMmTrace *trace, const char *engine,
                           const char *model, int available) {
    if (!s || !available) return 0;
    SamosaMmProvider provider = mm_visual_profile(1, engine, model);
    protocol_trace_event(trace, "molmo2_starting",
                          "\"provider\":\"molmo2_4b\",\"protocol\":\"framed_v1\"");
    char error_code[SAMOSA_MM_ERROR_MAX] = {0};
    if (!samosa_mm_session_start(supervisor, &provider, s,
                                 error_code, sizeof(error_code))) {
        TextBuffer fields = {0};
        if (text_add(&fields, "\"code\":") &&
            text_json_string(&fields, error_code[0] ? error_code : "start_failed"))
            protocol_trace_event(trace, "molmo2_start_failed", fields.data);
        free(fields.data);
        return 0;
    }
    char fields[128];
    snprintf(fields, sizeof(fields), "\"pid\":%ld,\"supervisor\":\"multimodal_v1\"", (long)s->pid);
    protocol_trace_event(trace, "molmo2_ready", fields);
    return 1;
}

void samosa_mm_visual_close(SamosaMmSession *s, const SamosaMmTrace *trace) {
    /* Transport failure invalidates active, but close still owns reap/lease. */
    if (!s || (!s->active && !s->supervisor && s->pid <= 0)) return;
    pid_t pid = s->pid;
    samosa_mm_session_close(s);
    char fields[128];
    snprintf(fields, sizeof(fields), "\"pid\":%ld,\"supervisor\":\"multimodal_v1\"",
             (long)pid);
    protocol_trace_event(trace, !strcmp(s->provider, "molmo2_4b")
                                ? "molmo2_process_stopped" : "visionpsy_process_stopped", fields);
}
