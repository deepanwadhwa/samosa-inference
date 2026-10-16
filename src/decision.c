#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "decision.h"
#include "../engines/decision/tokenizer/request.h"
#include "admission.h"
#include "budget.h"
#include "engine_process.h"
#include "rgba_input.h"
#include "sha256.h"
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>

#define DECISION_LIMIT 1048576u
typedef struct {
  unsigned char *rgba;
  size_t width, height;
  char sha256[65];
} DecisionFrame;
static const char *frame_read(const Json *j, DecisionFrame *frame) {
  if (!json_uint(j, json_get(j, 0, "width"), &frame->width) ||
      !json_uint(j, json_get(j, 0, "height"), &frame->height) ||
      !frame->width || frame->width > 512 || !frame->height || frame->height > 512)
    return "invalid_request";
  char *encoded = json_string(j, json_get(j, 0, "image_rgba_base64"));
  if (!encoded)
    return "invalid_request";
  frame->rgba = rgba_decode(encoded, frame->width * frame->height * 4);
  free(encoded);
  if (!frame->rgba)
    return "invalid_image";
  hash_bytes(frame->rgba, frame->width * frame->height * 4, frame->sha256);
  return NULL;
}
static int image_mode(const InferenceConfig *cfg) {
  return cfg->decision->profile == DECISION_ASSETS_IMAGE;
}
static const char *work_op(const InferenceConfig *cfg) {
  return image_mode(cfg) ? "decide_image" : "decide_text";
}
static int fail(FILE *out, const char *error) {
  fprintf(out, "{\"contract_version\":1,\"error\":{\"code\":\"%s\"}}\n", error);
  return 1;
}
static int fields(const Json *j, const char *allowed) {
  for (int t = 1; t < j->tokens[0].next;
       t = j->tokens[j->tokens[t].next].next) {
    char *key = json_string(j, t), match[128];
    int n = snprintf(match, sizeof(match), "|%s|", key ? key : "");
    free(key);
    if (n < 0 || (size_t)n >= sizeof(match) || !strstr(allowed, match))
      return 0;
  }
  return 1;
}
static int configured(const InferenceConfig *cfg) {
  if (!cfg || !cfg->decision || !cfg->provider_id || !cfg->admission_state)
    return 0;
  const DecisionConfig *d = cfg->decision;
  if (!d->self || d->self[0] != '/' || !d->assets || d->assets[0] != '/' ||
      d->profile > DECISION_ASSETS_IMAGE)
    return 0;
  for (unsigned i = 0; i < DECISION_ASSET_COUNT; i++)
    if (!d->pins[i] || strlen(d->pins[i]) != 64 ||
        strspn(d->pins[i], "0123456789abcdef") != 64)
      return 0;
  return 1;
}
const char *decision_payload(const Json *j, char **out) {
  if (!j || !out || *out)
    return "invalid_request";
  char *op = json_string(j, json_get(j, 0, "op"));
  int image = op && !strcmp(op, "decide_image");
  free(op);
  const char *allowed = image ?
      "|contract_version|op|provider_id|request|operation_id|parent_operation_id|timeout_ms|image_rgba_base64|width|height|" :
      "|contract_version|op|provider_id|request|operation_id|parent_operation_id|timeout_ms|";
  if (!fields(j, allowed))
    return "invalid_request";
  int t = json_get(j, 0, "request");
  if (t < 0 || j->tokens[t].type != JT_OBJECT)
    return "invalid_request";
  void *handle = NULL;
  SmTokenizerOutput buffer = {0};
  int code =
      sm_decision_request_load((const uint8_t *)j->text + j->tokens[t].begin,
                               j->tokens[t].end - j->tokens[t].begin, &handle);
  if (!code)
    code = sm_decision_request_normalized(handle, &buffer);
  if (!code && image) {
    SmDecisionRequest view = {0};
    code = sm_decision_request_view(handle, &view);
    if (!code && view.kind == SM_DECISION_RELATIONS)
      code = SM_TOKENIZER_INVALID_INPUT;
  }
  const char *error = code == SM_TOKENIZER_CONTEXT_LIMIT ? "context_limit"
                      : code                             ? "invalid_request"
                                                         : NULL;
  if (!error) {
    *out = malloc(buffer.length + 1);
    if (!*out)
      error = "resource_limit";
    else {
      memcpy(*out, buffer.data, buffer.length);
      (*out)[buffer.length] = 0;
    }
  }
  sm_tokenizer_output_free(&buffer);
  sm_decision_request_free(handle);
  return error;
}
char *decision_normalized(const InferenceConfig *cfg, const Json *j) {
  if (!configured(cfg))
    return NULL;
  char *payload = NULL, *op = json_string(j, json_get(j, 0, "op")),
       *provider = json_string(j, json_get(j, 0, "provider_id"));
  char *parent = json_string(j, json_get(j, 0, "parent_operation_id"));
  size_t budget = 0;
  int timeout = json_get(j, 0, "timeout_ms"),
      has_parent = json_get(j, 0, "parent_operation_id") >= 0;
  int valid = op && !strcmp(op, work_op(cfg)) && provider &&
              !strcmp(provider, cfg->provider_id) &&
              (!has_parent || (parent && *parent && strlen(parent) <= 128)) &&
              (timeout < 0 ||
               (json_uint(j, timeout, &budget) && budget && budget <= 120000));
  if (parent)
    for (const char *p = parent; *p; p++)
      if (!isalnum((unsigned char)*p) && !strchr("-_:.", *p))
        valid = 0;
  char *body = NULL;
  size_t bytes = 0;
  DecisionFrame frame = {0};
  if (valid && image_mode(cfg) && frame_read(j, &frame))
    valid = 0;
  if (valid && !decision_payload(j, &payload)) {
    FILE *f = open_memstream(&body, &bytes);
    if (f) {
      fputs("{\"op\":", f);
      json_quote(f, op, strlen(op));
      fputs(",\"provider_id\":", f);
      json_quote(f, provider, strlen(provider));
      fputs(",\"request\":", f);
      fputs(payload, f);
      fputs(",\"assets\":[", f);
      for (unsigned i = 0; i < DECISION_ASSET_COUNT; i++) {
        if (i)
          fputc(',', f);
        json_quote(f, cfg->decision->pins[i], 64);
      }
      fputc(']', f);
      if (image_mode(cfg))
        fprintf(f, ",\"width\":%zu,\"height\":%zu,\"image_sha256\":\"%s\"",
                frame.width, frame.height, frame.sha256);
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
  }
  free(payload);
  free(op);
  free(provider);
  free(parent);
  free(frame.rgba);
  return body;
}
static const char *validated_result(const InferenceConfig *cfg, const char *payload,
                                    const uint8_t *bytes, size_t length) {
  void *request = NULL;
  int code = sm_decision_request_load((const uint8_t *)payload, strlen(payload),
                                      &request);
  if (!code)
    code = sm_decision_result_validate_profile(request, bytes, length, cfg->decision->profile);
  sm_decision_request_free(request);
  if (!code)
    return NULL;
  Json error;
  if (json_parse(&error, (const char *)bytes, length) &&
      error.tokens[0].type == JT_OBJECT && error.tokens[0].count == 1) {
    char *name = json_string(&error, json_get(&error, 0, "error"));
    const char *result =
        name && !strcmp(name, "context_limit") ? "context_limit"
        : name && (!strcmp(name, "decision_request_limit") ||
                   !strcmp(name, "decision_input_limit") ||
                   !strcmp(name, "decision_result_limit"))
            ? "context_limit"
        : name && !strcmp(name, "engine_failed") ? "provider_process_failed"
                                                 : "invalid_provider_response";
    free(name);
    return result;
  }
  return "invalid_provider_response";
}
static const char *control(void *cfg) { return inference_interrupted(cfg); }
static int transfer(int fd, void *data, size_t bytes, int writing) {
  size_t at = 0;
  while (at < bytes) {
    ssize_t n = writing ? write(fd, (char *)data + at, bytes - at)
                        : pread(fd, (char *)data + at, bytes - at, (off_t)at);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      return 0;
    at += (size_t)n;
  }
  return 1;
}
/* Use the existing guardian's bounded framed-file transport and workspace
 * cleanup. The worker processes one frame then clean EOF; persistent engine
 * ownership is a subsequent lifecycle requirement. */
static const char *run(const InferenceConfig *cfg, const char *payload, const DecisionFrame *frame,
                       char **result) {
  const DecisionConfig *d = cfg->decision;
  DecisionAssets assets = {0};
  int lease = -1, input = -1, output = -1, directory = -1, created = 0;
  const char *error = inference_schedule(cfg, &lease);
  if (!error)
    error =
        decision_assets_open_profile(d->assets, d->pins, d->profile, control, (void *)cfg, &assets);
  char workspace[] = "/tmp/samosa-inference-decision-XXXXXX", inpath[256] = {0},
       outpath[256] = {0}, imagepath[256] = {0};
  if (!error) {
    created = mkdtemp(workspace) != NULL;
    if (!created)
      error = "engine_io_failed";
  }
  if (!error) {
    snprintf(inpath, sizeof(inpath), "%s/request.frame", workspace);
    snprintf(outpath, sizeof(outpath), "%s/result.frame", workspace);
    directory =
        open(workspace, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    input = open(inpath, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    output = open(outpath, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (directory < 0 || input < 0 || output < 0)
      error = "engine_io_failed";
  }
  size_t n = strlen(payload);
  unsigned char header[4] = {(unsigned char)(n >> 24), (unsigned char)(n >> 16),
                             (unsigned char)(n >> 8), (unsigned char)n};
  if (!error && (!transfer(input, header, 4, 1) ||
                 !transfer(input, (void *)payload, n, 1) ||
                 lseek(input, 0, SEEK_SET) < 0))
    error = "engine_io_failed";
  if (input >= 0) {
    close(input);
    input = -1;
  }
  if (!error && image_mode(cfg)) {
    snprintf(imagepath, sizeof(imagepath), "%s/input.rgba", workspace);
    int pixels = open(imagepath, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
    if (pixels < 0)
      error = "engine_io_failed";
    else {
      if (!transfer(pixels, frame->rgba, frame->width * frame->height * 4, 1))
        error = "engine_io_failed";
      if (close(pixels) && !error)
        error = "engine_io_failed";
    }
  }
  if (!error) {
    input = open(inpath, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (input < 0)
      error = "engine_io_failed";
  }
  char *argv = NULL;
  size_t bytes = 0;
  FILE *f = error ? NULL : open_memstream(&argv, &bytes);
  if (!error && !f)
    error = "resource_limit";
  if (f) {
    char paths[DECISION_ASSET_COUNT][8192];
    for (unsigned i = 0; i < DECISION_ASSET_COUNT; i++)
      snprintf(paths[i], sizeof(paths[i]), "%s/%s", d->assets,
               decision_asset_name(i));
    char width[16], height[16];
    snprintf(width, sizeof(width), "%zu", frame->width);
    snprintf(height, sizeof(height), "%zu", frame->height);
    const char *args[] = {paths[DECISION_ENGINE], "--model",
                          paths[DECISION_MODEL], "--tokenizer",
                          paths[DECISION_TOKENIZER], "--image-rgba", imagepath,
                          "--width", width, "--height", height};
    fputc('[', f);
    for (unsigned i = 0; i < (image_mode(cfg) ? 11u : 5u); i++) {
      if (i)
        fputc(',', f);
      json_quote(f, args[i], strlen(args[i]));
    }
    fputc(']', f);
    if (fclose(f))
      error = "resource_limit";
  }
  EngineProcess process = {.guardian = -1, .control = -1, .response = -1};
  if (!error)
    error = decision_assets_recheck(&assets, control, (void *)cfg);
  if (!error) {
    double remaining = cfg->deadline_monotonic
                           ? cfg->deadline_monotonic - inference_now()
                           : 60;
    error = remaining <= 0
                ? "deadline_exceeded"
                : engine_process_start_bound_protocol_sized(
                      d->self, argv, remaining, 262144, lease,
                      assets.files[DECISION_MODEL], workspace, directory, input,
                      output, DECISION_LIMIT + 4, &process);
  }
  while (!error && process.guardian > 0) {
    error = inference_interrupted(cfg);
    if (error)
      break;
    struct pollfd watch = {process.response, POLLIN | POLLHUP, 0};
    int ready = poll(&watch, 1, 10);
    if (ready < 0 && errno == EINTR)
      continue;
    if (ready < 0) {
      error = "engine_io_failed";
      break;
    }
    if (ready > 0) {
      const char *terminal = engine_process_stop(&process);
      if (terminal)
        error = !strcmp(terminal, "lifetime_exceeded") ? "deadline_exceeded"
                : !strcmp(terminal, "protocol_limit")
                    ? "invalid_provider_response"
                    : "provider_process_failed";
      if (!error && strcmp(process.reason, "completed"))
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
    error = decision_assets_recheck(&assets, control, (void *)cfg);
  if (!error) {
    struct stat s;
    if (fstat(output, &s) || !S_ISREG(s.st_mode) || s.st_uid != geteuid() ||
        s.st_nlink != 0 || s.st_size < 5 || s.st_size > DECISION_LIMIT + 4)
      error = "invalid_provider_response";
    else {
      unsigned char *frame = malloc((size_t)s.st_size + 1);
      if (!frame)
        error = "resource_limit";
      else {
        if (!transfer(output, frame, (size_t)s.st_size, 0))
          error = "invalid_provider_response";
        else {
          size_t length = ((size_t)frame[0] << 24) | ((size_t)frame[1] << 16) |
                          ((size_t)frame[2] << 8) | frame[3];
          if (length != (size_t)s.st_size - 4)
            error = "invalid_provider_response";
          else {
            frame[s.st_size] = 0;
            error = validated_result(cfg, payload, frame + 4, length);
            if (!error) {
              *result = strndup((char *)frame + 4, length);
              if (!*result)
                error = "resource_limit";
            }
          }
        }
        free(frame);
      }
    }
  }
  free(argv);
  if (created) {
    unlink(inpath);
    unlink(outpath);
    if (*imagepath)
      unlink(imagepath);
    rmdir(workspace);
  }
  if (input >= 0)
    close(input);
  if (output >= 0)
    close(output);
  if (directory >= 0)
    close(directory);
  decision_assets_close(&assets);
  if (lease >= 0)
    close(lease);
  return error;
}
int decision_dispatch(const InferenceConfig *cfg, const Json *j, FILE *out) {
  if (!configured(cfg))
    return fail(out, "invalid_configuration");
  char *op = json_string(j, json_get(j, 0, "op"));
  int describe = op && !strcmp(op, "describe"),
      work = op && !strcmp(op, work_op(cfg));
  free(op);
  if (describe) {
    if (!fields(j, "|contract_version|op|"))
      return fail(out, "unknown_field");
    fputs("{\"contract_version\":1,\"component\":\"samosa-inference\","
          "\"provider_id\":",
          out);
    json_quote(out, cfg->provider_id, strlen(cfg->provider_id));
    if (image_mode(cfg))
      fputs(",\"capabilities\":[\"image_decisions\",\"operations\",\"cancellation\",\"request_budgets\"],"
            "\"methods\":[\"choice\",\"score\",\"noul\",\"relation\"],\"input\":\"rgba8_base64\","
            "\"max_axis\":512,\"alpha_policy\":\"discard\",\"max_decision_request_bytes\":1048576,\"max_image_bytes\":1048576,"
            "\"default_timeout_ms\":60000,\"max_timeout_ms\":120000,\"context_tokens\":64,\"quality\":\"native_siglip\"}\n", out);
    else fputs(",\"capabilities\":[\"text_decisions\",\"operations\","
          "\"cancellation\",\"request_budgets\"],\"methods\":[\"choice\","
          "\"score\",\"noul\",\"relation\",\"relations\"],\"max_request_"
          "bytes\":1048576,\"default_timeout_ms\":60000,\"max_timeout_ms\":"
          "120000,\"context_tokens\":512,\"quality\":\"native_deberta\"}\n",
          out);
    return 0;
  }
  if (!work)
    return fail(out, "unsupported_operation");
  char *normalized = decision_normalized(cfg, j);
  if (!normalized)
    return fail(out, "invalid_request");
  free(normalized);
  char *payload = NULL, *result = NULL;
  DecisionFrame frame = {0};
  const char *error = decision_payload(j, &payload);
  if (!error && image_mode(cfg))
    error = frame_read(j, &frame);
  if (!error)
    error = run(cfg, payload, &frame, &result);
  if (!error) {
    char digest[65];
    hash_bytes(payload, strlen(payload), digest);
    if (image_mode(cfg)) {
      char material[256];
      int length = snprintf(material, sizeof(material), "samosa-image-decision-v1:%zu:%zu:%s:%s",
                            frame.width, frame.height, frame.sha256, digest);
      hash_bytes(material, (size_t)length, digest);
    }
    fputs("{\"contract_version\":1,\"provider_id\":", out);
    json_quote(out, cfg->provider_id, strlen(cfg->provider_id));
    fprintf(out, ",\"input_sha256\":\"%s\",\"result\":%s", digest, result);
    if (image_mode(cfg))
      fprintf(out, ",\"image_sha256\":\"%s\",\"width\":%zu,\"height\":%zu", frame.sha256, frame.width, frame.height);
    fprintf(out, ",\"provenance\":{\"engine\":\"%s\"",
            image_mode(cfg) ? "samosa-native-siglip" : "samosa-native-deberta");
    if (image_mode(cfg))
      fputs(",\"alpha_policy\":\"discard\",\"processor_profile\":\"siglip224-pil-bilinear-v1\"", out);
    fputs(",\"asset_sha256\":[", out);
    for (unsigned i = 0; i < DECISION_ASSET_COUNT; i++) {
      if (i)
        fputc(',', out);
      json_quote(out, cfg->decision->pins[i], 64);
    }
    fputs("]}}\n", out);
  }
  free(payload);
  free(result);
  free(frame.rgba);
  return error ? fail(out, error) : 0;
}
