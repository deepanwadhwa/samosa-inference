#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "sha256.h"
#include "temporal.h"
#include "temporal_package.h"
static int sha(const char *s) {
    return s && strlen(s) == 64 && strspn(s, "0123456789abcdef") == 64;
}
int temporal_configured(const InferenceConfig *cfg) {
    if (!cfg || !cfg->provider_id || !cfg->temporal || !cfg->admission_state)
        return 0;
    const TemporalConfig *v = cfg->temporal;
    char identity[65];
    temporal_package_identity(v->manifest_sha256, identity);
    return v->self && v->self[0] == '/' && v->executable &&
           v->executable[0] == '/' && v->model && v->model[0] == '/' &&
           sha(v->executable_sha256) && sha(v->model_sha256) && *identity &&
           !strcmp(identity, v->model_sha256);
}
static int fields(const Json *j, int object, const char *names) {
    if (object < 0 || j->tokens[object].type != JT_OBJECT)
        return 0;
    for (int t = object + 1; t < j->tokens[object].next;
         t = j->tokens[j->tokens[t].next].next) {
        char *key = json_string(j, t), match[128];
        int n = snprintf(match, sizeof(match), "|%s|", key ? key : "");
        free(key);
        if (n < 0 || (size_t)n >= sizeof(match) || !strstr(names, match))
            return 0;
    }
    return 1;
}
static int base64(unsigned char c) {
    return c >= 'A' && c <= 'Z'   ? c - 'A'
           : c >= 'a' && c <= 'z' ? c - 'a' + 26
           : c >= '0' && c <= '9' ? c - '0' + 52
           : c == '+'             ? 62
           : c == '/'             ? 63
                                  : -1;
}
static unsigned char *decode(const char *s, size_t bytes) {
    size_t n = strlen(s);
    if (!bytes || bytes > 512u * 512u * 4u || n != ((bytes + 2) / 3) * 4)
        return NULL;
    unsigned char *out = malloc(n / 4 * 3);
    if (!out)
        return NULL;
    size_t used = 0;
    for (size_t i = 0; i < n; i += 4) {
        int a = base64((unsigned char)s[i]),
            b = base64((unsigned char)s[i + 1]),
            c = base64((unsigned char)s[i + 2]),
            d = base64((unsigned char)s[i + 3]), pc = s[i + 2] == '=',
            pd = s[i + 3] == '=';
        if (a < 0 || b < 0 || (!pc && c < 0) || (!pd && d < 0) || (pc && !pd) ||
            ((pc || pd) && i + 4 != n) || (pc && (b & 15)) ||
            (!pc && pd && (c & 3))) {
            free(out);
            return NULL;
        }
        out[used++] = (unsigned char)((a << 2) | (b >> 4));
        if (!pc)
            out[used++] = (unsigned char)((b << 4) | (c >> 2));
        if (!pd)
            out[used++] = (unsigned char)((c << 6) | d);
    }
    if (used != bytes) {
        free(out);
        return NULL;
    }
    return out;
}
char *temporal_ppm(const TemporalFrame *frame, size_t *bytes) {
    char *body = NULL;
    FILE *out = open_memstream(&body, bytes);
    if (!out)
        return NULL;
    fprintf(out, "P6\n%zu %zu\n255\n", frame->width, frame->height);
    for (size_t i = 0; i < frame->width * frame->height; i++)
        for (size_t c = 0; c < 3; c++) {
            unsigned alpha = frame->rgba[i * 4 + 3],
                     value = ((unsigned)frame->rgba[i * 4 + c] * alpha +
                              255u * (255u - alpha) + 127u) /
                             255u;
            fputc((int)value, out);
        }
    if (fclose(out)) {
        free(body);
        return NULL;
    }
    return body;
}
void temporal_input_free(TemporalInput *input) {
    for (size_t k = 0; k < 16; k++)
        free(input->frames[k].rgba);
    free(input->provider);
    free(input->prompt);
    memset(input, 0, sizeof(*input));
}
const char *temporal_input(const InferenceConfig *cfg, const Json *j,
                           TemporalInput *input) {
    memset(input, 0, sizeof(*input));
    if (!temporal_configured(cfg))
        return "invalid_configuration";
    if (!fields(
            j, 0,
            "|contract_version|op|provider_id|frames|start_us|end_us|prompt|"
            "max_output_tokens|operation_id|parent_operation_id|timeout_ms|"))
        return "unknown_field";
    input->provider = json_string(j, json_get(j, 0, "provider_id"));
    input->prompt = json_string(j, json_get(j, 0, "prompt"));
    int frames = json_get(j, 0, "frames");
    const char *error = NULL;
    if (!input->provider || !input->prompt || !*input->prompt ||
        strlen(input->prompt) > 4096 ||
        !json_uint(j, json_get(j, 0, "start_us"), &input->start_us) ||
        !json_uint(j, json_get(j, 0, "end_us"), &input->end_us) ||
        input->start_us >= input->end_us ||
        input->end_us > UINT64_C(14400000000) ||
        !json_uint(j, json_get(j, 0, "max_output_tokens"), &input->tokens) ||
        input->tokens < 32 || input->tokens > 1024 || frames < 0 ||
        j->tokens[frames].type != JT_ARRAY || !j->tokens[frames].count ||
        j->tokens[frames].count > 16)
        error = "invalid_request";
    else if (strcmp(input->provider, cfg->provider_id))
        error = "provider_unavailable";
    for (int t = frames + 1; !error && t < j->tokens[frames].next;
         t = j->tokens[t].next) {
        TemporalFrame *frame = &input->frames[input->count];
        if (!fields(j, t, "|timestamp_us|width|height|rgba_base64|"))
            error = "unknown_field";
        if (!error &&
            (!json_uint(j, json_get(j, t, "timestamp_us"),
                        &frame->timestamp_us) ||
             frame->timestamp_us < input->start_us ||
             frame->timestamp_us >= input->end_us ||
             (input->count &&
              frame->timestamp_us <=
                  input->frames[input->count - 1].timestamp_us) ||
             !json_uint(j, json_get(j, t, "width"), &frame->width) ||
             !frame->width || frame->width > 512 ||
             !json_uint(j, json_get(j, t, "height"), &frame->height) ||
             !frame->height || frame->height > 512))
            error = "invalid_request";
        char *encoded =
            error ? NULL : json_string(j, json_get(j, t, "rgba_base64"));
        if (!error && !encoded)
            error = "invalid_request";
        if (!error) {
            frame->rgba = decode(encoded, frame->width * frame->height * 4);
            if (!frame->rgba)
                error = "invalid_image";
        }
        free(encoded);
        if (!error) {
            hash_bytes(frame->rgba, frame->width * frame->height * 4,
                       frame->rgba_sha256);
            size_t bytes = 0;
            char *ppm = temporal_ppm(frame, &bytes);
            if (!ppm)
                error = "resource_limit";
            else
                hash_bytes(ppm, bytes, frame->prepared_sha256);
            free(ppm);
            input->count++;
        }
    }
    char *canonical = NULL, *prepared = NULL;
    size_t n = 0, pn = 0;
    FILE *out = NULL, *prep = NULL;
    if (!error) {
        out = open_memstream(&canonical, &n);
        prep = open_memstream(&prepared, &pn);
        if (!out || !prep)
            error = "resource_limit";
    }
    if (!error) {
        fprintf(out,
                "{\"recipe\":\"molmo2-temporal-rgba8-v1\",\"start_us\":%zu,"
                "\"end_us\":%zu,\"frames\":[",
                input->start_us, input->end_us);
        fputs("[", prep);
        for (size_t k = 0; k < input->count; k++) {
            TemporalFrame *frame = &input->frames[k];
            if (k) {
                fputc(',', out);
                fputc(',', prep);
            }
            fprintf(out,
                    "{\"timestamp_us\":%zu,\"width\":%zu,\"height\":%zu,\"rgba_"
                    "sha256\":\"%s\"}",
                    frame->timestamp_us, frame->width, frame->height,
                    frame->rgba_sha256);
            fprintf(prep, "{\"timestamp_us\":%zu,\"ppm_sha256\":\"%s\"}",
                    frame->timestamp_us, frame->prepared_sha256);
        }
        fputs("]}", out);
        fputs("]", prep);
    }
    if (out && fclose(out))
        error = "resource_limit";
    if (prep && fclose(prep))
        error = "resource_limit";
    if (!error) {
        hash_bytes(canonical, n, input->input_sha256);
        hash_bytes(prepared, pn, input->prepared_sha256);
    }
    free(canonical);
    free(prepared);
    if (error)
        temporal_input_free(input);
    return error;
}
char *temporal_normalized(const InferenceConfig *cfg, const Json *j) {
    TemporalInput input;
    const char *error = temporal_input(cfg, j, &input);
    if (error)
        return NULL;
    char *parent = json_string(j, json_get(j, 0, "parent_operation_id"));
    size_t budget = 0;
    int timeout = json_get(j, 0, "timeout_ms"),
        parent_token = json_get(j, 0, "parent_operation_id");
    if ((timeout >= 0 &&
         (!json_uint(j, timeout, &budget) || !budget || budget > 120000)) ||
        (parent_token >= 0 && (!parent || !*parent || strlen(parent) > 128)))
        error = "invalid_request";
    if (parent)
        for (const unsigned char *p = (const unsigned char *)parent; *p; p++)
            if (!isalnum(*p) && !strchr("-_:.", *p))
                error = "invalid_request";
    char *body = NULL;
    size_t n = 0;
    FILE *out = error ? NULL : open_memstream(&body, &n);
    if (out) {
        fputs("{\"op\":\"analyze_video_frames\",\"provider_id\":", out);
        json_quote(out, input.provider, strlen(input.provider));
        fputs(",\"configured_provider\":", out);
        json_quote(out, cfg->provider_id, strlen(cfg->provider_id));
        fprintf(out,
                ",\"input_sha256\":\"%s\",\"engine_sha256\":\"%s\",\"model_"
                "sha256\":\"%s\",\"max_output_tokens\":%zu,\"prompt\":",
                input.input_sha256, cfg->temporal->executable_sha256,
                cfg->temporal->model_sha256, input.tokens);
        json_quote(out, input.prompt, strlen(input.prompt));
        if (parent) {
            fputs(",\"parent_operation_id\":", out);
            json_quote(out, parent, strlen(parent));
        }
        if (timeout >= 0)
            fprintf(out, ",\"timeout_ms\":%zu", budget);
        fputc('}', out);
        if (fclose(out)) {
            free(body);
            body = NULL;
        }
    }
    free(parent);
    temporal_input_free(&input);
    return body;
}
