#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "admission.h"
#include "artifact.h"
#include "budget.h"
#include "engine_process.h"
#include "sha256.h"
#include "summary.h"
#include "summary_protocol.h"
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>

static const char *allowed =
    "|contract_version|op|provider_id|text|max_output_tokens|operation_id|"
    "parent_operation_id|timeout_ms|";
static int fail(FILE *out, const char *error) {
  fprintf(out, "{\"contract_version\":1,\"error\":{\"code\":\"%s\"}}\n", error);
  return 1;
}
static int fields(const Json *j, const char *list) {
  for (int t = 1; t < j->tokens[0].next;
       t = j->tokens[j->tokens[t].next].next) {
    char *key = json_string(j, t), field[128];
    int n = snprintf(field, sizeof(field), "|%s|", key ? key : "");
    free(key);
    if (n < 0 || (size_t)n >= sizeof(field) || !strstr(list, field))
      return 0;
  }
  return 1;
}
static int sha(const char *s) {
  if (!s || strlen(s) != 64)
    return 0;
  for (size_t i = 0; i < 64; i++)
    if (!strchr("0123456789abcdef", s[i]))
      return 0;
  return 1;
}
static int options(const Json *j, size_t *tokens) {
  return json_uint(j, json_get(j, 0, "max_output_tokens"), tokens) &&
         *tokens >= 16 && *tokens <= 256;
}
char *summary_normalized(const InferenceConfig *cfg, const Json *j) {
  if (!cfg || !cfg->summary || !cfg->provider_id ||
      !sha(cfg->summary->model_sha256) ||
      !sha(cfg->summary->executable_sha256) || !fields(j, allowed))
    return NULL;
  char *op = json_string(j, json_get(j, 0, "op"));
  char *provider = json_string(j, json_get(j, 0, "provider_id"));
  char *text = json_string(j, json_get(j, 0, "text"));
  char *parent = json_string(j, json_get(j, 0, "parent_operation_id"));
  size_t tokens = 0, budget = 0;
  int timeout = json_get(j, 0, "timeout_ms"),
      has_parent = json_get(j, 0, "parent_operation_id") >= 0;
  int valid = op &&
              (!strcmp(op, "summarize") || !strcmp(op, "summarize_opening")) &&
              provider && text && *text && strlen(text) <= 60000 &&
              options(j, &tokens) &&
              (!has_parent || (parent && *parent && strlen(parent) <= 128)) &&
              (timeout < 0 ||
               (json_uint(j, timeout, &budget) && budget && budget <= 60000));
  if (parent)
    for (const char *p = parent; *p; p++)
      if (!isalnum((unsigned char)*p) && !strchr("-_:.", *p))
        valid = 0;
  char *body = NULL;
  size_t bytes = 0;
  FILE *out = valid ? open_memstream(&body, &bytes) : NULL;
  if (out) {
    fputs("{\"op\":", out);
    json_quote(out, op, strlen(op));
    fputs(",\"provider_id\":", out);
    json_quote(out, provider, strlen(provider));
    fputs(",\"text\":", out);
    json_quote(out, text, strlen(text));
    fprintf(out, ",\"max_output_tokens\":%zu,\"configured_provider\":", tokens);
    json_quote(out, cfg->provider_id, strlen(cfg->provider_id));
    fprintf(out, ",\"engine_sha256\":\"%s\",\"model_sha256\":\"%s\"",
            cfg->summary->executable_sha256, cfg->summary->model_sha256);
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
  free(op);
  free(provider);
  free(text);
  free(parent);
  return body;
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
static const char *control(void *context) {
  return inference_interrupted(context);
}
static const char *verify(const InferenceConfig *cfg, int fd,
                          const char *digest, uint64_t limit, int executable) {
  const char *error = inference_interrupted(cfg);
  struct stat s;
  if (!error && (fstat(fd, &s) || !S_ISREG(s.st_mode) || s.st_size <= 0 ||
                 s.st_nlink != 1 || s.st_uid != geteuid() ||
                 (s.st_mode & 022) || (executable && !(s.st_mode & 0111))))
    error = "unsupported_artifact";
  if (!error)
    error = artifact_verify_controlled(fd, (uint64_t)s.st_size, limit, digest,
                                       cfg ? control : NULL, (void *)cfg);
  if (!error)
    error = inference_interrupted(cfg);
  return error;
}
int summary_bridge_dispatch(int argc, char **argv) {
  if (argc < 2 || strcmp(argv[1], "--summary-engine-bridge"))
    return -1;
  if (argc != 5 || argv[2][0] != '/' || !sha(argv[3]))
    return 2;
  char *end;
  errno = 0;
  unsigned long tokens = strtoul(argv[4], &end, 10);
  if (errno || end == argv[4] || *end || tokens < 16 || tokens > 256)
    return 2;
  int executable = open(argv[2], O_RDONLY | O_NOFOLLOW | O_NONBLOCK);
  if (executable < 0 || verify(NULL, executable, argv[3], 67108864u, 1))
    return 2;
  char model_path[PATH_MAX], executable_path[PATH_MAX];
#ifdef __APPLE__
  struct stat retained, named;
  if (fstat(executable, &retained) || lstat(argv[2], &named) ||
      !same(&retained, &named) || fcntl(5, F_GETPATH, model_path) < 0 ||
      fstat(5, &retained) || lstat(model_path, &named) ||
      !same(&retained, &named))
    return 2;
  snprintf(executable_path, sizeof(executable_path), "%s", argv[2]);
#else
  snprintf(model_path, sizeof(model_path), "/dev/fd/5");
  snprintf(executable_path, sizeof(executable_path), "/dev/fd/%d", executable);
#endif
  char *args[] = {executable_path,
                  "--model",
                  model_path,
                  "--max-tokens",
                  argv[4],
                  "--batch-size",
                  "1",
                  "--threads",
                  "2",
                  "--gpu-layers",
                  "0",
                  NULL};
  execv(executable_path, args);
  return 2;
}
static const char *run(const InferenceConfig *cfg, const char *text,
                       size_t tokens, int opening, char **result) {
  const SummaryConfig *s = cfg->summary;
  int lease = -1, model = -1, executable = -1, directory = -1, input = -1,
      output = -1;
  const char *error = inference_interrupted(cfg);
  if (!error)
    error = inference_schedule(cfg, &lease);
  struct stat model_receipt = {0}, executable_receipt = {0};
  if (!error) {
    model = open(s->model, O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    error = model < 0 ? "artifact_unavailable"
                      : verify(cfg, model, s->model_sha256, 536870912u, 0);
  }
  if (!error && fstat(model, &model_receipt))
    error = "artifact_unavailable";
  if (!error) {
    executable =
        open(s->executable, O_RDONLY | O_NOFOLLOW | O_CLOEXEC | O_NONBLOCK);
    error = executable < 0
                ? "artifact_unavailable"
                : verify(cfg, executable, s->executable_sha256, 67108864u, 1);
  }
  if (!error && fstat(executable, &executable_receipt))
    error = "artifact_unavailable";
  char workspace[] = "/tmp/samosa-inference-summary-XXXXXX",
       input_path[256] = {0}, output_path[256] = {0};
  int created = 0;
  if (!error) {
    created = mkdtemp(workspace) != NULL;
    if (!created)
      error = "engine_io_failed";
    else {
      snprintf(input_path, sizeof(input_path), "%s/request.frame", workspace);
      snprintf(output_path, sizeof(output_path), "%s/result.frame", workspace);
      directory =
          open(workspace, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
      input = open(input_path, O_WRONLY | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
      output = open(output_path, O_RDWR | O_CREAT | O_EXCL | O_CLOEXEC, 0600);
      if (directory < 0 || input < 0 || output < 0)
        error = "engine_io_failed";
    }
  }
  size_t frame_bytes = 0;
  unsigned char *frame =
      error     ? NULL
      : opening ? summary_opening_frame(text, strlen(text), &frame_bytes)
                : summary_prompt_frame(text, strlen(text), &frame_bytes);
  if (!error && !frame)
    error = "invalid_request";
  size_t used = 0;
  while (!error && used < frame_bytes) {
    error = inference_interrupted(cfg);
    if (error)
      break;
    ssize_t n = write(input, frame + used, frame_bytes - used);
    if (n < 0 && errno == EINTR)
      continue;
    if (n <= 0)
      error = "engine_io_failed";
    else
      used += (size_t)n;
  }
  free(frame);
  if (input >= 0) {
    close(input);
    input = -1;
  }
  if (!error) {
    input = open(input_path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    if (input < 0)
      error = "engine_io_failed";
  }
  char token_text[32];
  snprintf(token_text, sizeof(token_text), "%zu", tokens);
  char *argv = NULL;
  size_t argv_bytes = 0;
  FILE *f = error ? NULL : open_memstream(&argv, &argv_bytes);
  if (!error && !f)
    error = "resource_limit";
  if (f) {
    const char *args[] = {s->self, "--summary-engine-bridge", s->executable,
                          s->executable_sha256, token_text};
    fputc('[', f);
    for (size_t i = 0; i < sizeof(args) / sizeof(args[0]); i++) {
      if (i)
        fputc(',', f);
      json_quote(f, args[i], strlen(args[i]));
    }
    fputc(']', f);
    if (fclose(f))
      error = "resource_limit";
  }
  EngineProcess process = {.guardian = -1, .control = -1, .response = -1};
  if (!error) {
    double remaining = cfg->deadline_monotonic
                           ? cfg->deadline_monotonic - inference_now()
                           : 60;
    if (remaining <= 0)
      error = "deadline_exceeded";
    else
      error = engine_process_start_bound_protocol(
          s->self, argv, remaining, 262144, lease, model, workspace, directory,
          input, output, &process);
  }
  while (!error && process.guardian > 0) {
    error = inference_interrupted(cfg);
    if (error)
      break;
    struct stat size;
    if (fstat(output, &size) || size.st_size > 8196) {
      error = "invalid_provider_response";
      break;
    }
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
    error = inference_interrupted(cfg);
  int fds[] = {model, executable};
  const char *paths[] = {s->model, s->executable};
  const char *pins[] = {s->model_sha256, s->executable_sha256};
  const struct stat *receipts[] = {&model_receipt, &executable_receipt};
  for (int i = 0; !error && i < 2; i++) {
    struct stat retained, named;
    if (fstat(fds[i], &retained) || lstat(paths[i], &named) ||
        !same(&retained, receipts[i]) || !same(&retained, &named))
      error = "artifact_changed";
    if (!error)
      error = verify(cfg, fds[i], pins[i], i ? 67108864u : 536870912u, i);
  }
  if (!error) {
    struct stat stat;
    unsigned char reply[8196];
    if (fstat(output, &stat) || !S_ISREG(stat.st_mode) ||
        stat.st_uid != geteuid() || stat.st_nlink != 0 || stat.st_size < 5 ||
        stat.st_size > (off_t)sizeof(reply))
      error = "invalid_provider_response";
    else {
      size_t read_bytes = 0;
      while (!error && read_bytes < (size_t)stat.st_size) {
        ssize_t n = pread(output, reply + read_bytes,
                          (size_t)stat.st_size - read_bytes, (off_t)read_bytes);
        if (n < 0 && errno == EINTR)
          continue;
        if (n <= 0)
          error = "invalid_provider_response";
        else
          read_bytes += (size_t)n;
      }
      if (!error && !(*result = summary_reply_frame(reply, read_bytes, &used)))
        error = "invalid_provider_response";
    }
  }
  free(argv);
  if (created) {
    unlink(input_path);
    unlink(output_path);
    rmdir(workspace);
  }
  if (input >= 0)
    close(input);
  if (output >= 0)
    close(output);
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
/* Ported from native_summary_chunk_length/native_summarize_text_impl in the
 * monolith: sentence-aware1400-byte reduction, at most4 shrinking rounds and
 * a UTF-8-safe1800-byte final cap. Every mapped chunk enters the reducer. */
static size_t reduction_chunk(const char *text, size_t remaining) {
  if (remaining <= 1400)
    return remaining;
  size_t end = 1400;
  for (size_t i = end; i > end / 2; --i) {
    unsigned char c = (unsigned char)text[i - 1];
    if (c == '\n' || c == '.' || c == '!' || c == '?')
      return i;
  }
  while (end && ((unsigned char)text[end] & 0xc0) == 0x80)
    end--;
  return end ? end : 1400;
}
static const char *run_opening(const InferenceConfig *cfg, const char *text,
                               size_t tokens, char **result,
                               size_t *mapped_bytes, size_t *rounds,
                               int *truncated) {
  const char *error = run(cfg, text, tokens, 1, result);
  *rounds = 0;
  *truncated = 0;
  *mapped_bytes = error ? 0 : strlen(*result);
  for (size_t round = 0; !error && strlen(*result) > 1800 && round < 4;
       round++) {
    char *working = *result, *reduced = NULL;
    size_t reduced_bytes = 0, at = 0, count = 0;
    FILE *out = open_memstream(&reduced, &reduced_bytes);
    if (!out) {
      error = "resource_limit";
      break;
    }
    while (!error && at < strlen(working)) {
      while (at < strlen(working) && isspace((unsigned char)working[at]))
        at++;
      if (at == strlen(working))
        break;
      size_t n = reduction_chunk(working + at, strlen(working) - at);
      char *part = strndup(working + at, n), *reply = NULL;
      if (!part) {
        error = "resource_limit";
        break;
      }
      error = run(cfg, part, tokens, 0, &reply);
      free(part);
      if (!error) {
        if (count++)
          fputc('\n', out);
        fwrite(reply, 1, strlen(reply), out);
        at += n;
      }
      free(reply);
    }
    if (fclose(out) && !error)
      error = "resource_limit";
    if (error) {
      free(reduced);
      break;
    }
    if (!count || !reduced || reduced_bytes >= strlen(working)) {
      free(reduced);
      break;
    }
    free(working);
    *result = reduced;
    (*rounds)++;
  }
  if (!error && strlen(*result) > 1800) {
    size_t keep = 1800;
    while (keep && ((unsigned char)(*result)[keep] & 0xc0) == 0x80)
      keep--;
    (*result)[keep] = 0;
    *truncated = 1;
  }
  return error;
}
int summary_dispatch(const InferenceConfig *cfg, const Json *j, FILE *out) {
  const SummaryConfig *s = cfg ? cfg->summary : NULL;
  if (!s || !s->self || s->self[0] != '/' || !s->model || s->model[0] != '/' ||
      !s->executable || s->executable[0] != '/' || !sha(s->model_sha256) ||
      !sha(s->executable_sha256) || !cfg->admission_state)
    return fail(out, "invalid_configuration");
  char *op = json_string(j, json_get(j, 0, "op"));
  if (!op)
    return fail(out, "invalid_request");
  if (!strcmp(op, "describe")) {
    free(op);
    if (!fields(j, "|contract_version|op|"))
      return fail(out, "unknown_field");
    fprintf(
        out,
        "{\"contract_version\":1,\"component\":\"samosa-inference\",\"provider_"
        "id\":\"%s\",\"capabilities\":[\"summarization\",\"opening_sample_summarization\",\"operations\","
        "\"cancellation\",\"request_budgets\"],\"max_input_bytes\":60000,\"max_"
        "output_tokens\":256,\"quality\":\"native_t5\"}\n",
        cfg->provider_id);
    return 0;
  }
  int opening = !strcmp(op, "summarize_opening");
  int supported = !strcmp(op, "summarize") || opening;
  free(op);
  if (!supported)
    return fail(out, "unsupported_operation");
  if (!fields(j, allowed))
    return fail(out, "unknown_field");
  char *provider = json_string(j, json_get(j, 0, "provider_id")),
       *text = json_string(j, json_get(j, 0, "text"));
  size_t tokens = 0;
  const char *error = !provider || !text || !*text || strlen(text) > 60000 ||
                              !options(j, &tokens)
                          ? "invalid_request"
                      : strcmp(provider, cfg->provider_id)
                          ? "provider_unavailable"
                          : NULL;
  char *result = NULL;
  size_t mapped_bytes = 0, reduction_rounds = 0;
  int output_truncated = 0;
  if (!error)
    error = opening ? run_opening(cfg, text, tokens, &result, &mapped_bytes,
                                  &reduction_rounds, &output_truncated)
                    : run(cfg, text, tokens, 0, &result);
  if (!error) {
    char digest[65];
    hash_bytes(text, strlen(text), digest);
    fprintf(out,
            "{\"contract_version\":1,\"provider_id\":\"%s\",\"kind\":"
            "\"derived\",\"artifact_type\":\"summary\",\"input_sha256\":\"%s\","
            "\"input_bytes\":%zu,\"text\":",
            cfg->provider_id, digest, strlen(text));
    json_quote(out, result, strlen(result));
    if (opening) {
      fprintf(
          out,
          ",\"coverage\":{\"status\":\"partial\",\"reason\":\"native_"
          "partitioning_and_generated_summary_uncertain\"},\"provenance\":{"
          "\"engine\":\"samosa-native-t5\",\"engine_sha256\":\"%s\",\"model_"
          "sha256\":\"%s\",\"recipe\":\"samosa-native-summary-opening-map-"
          "reduce-v1\","
          "\"max_output_tokens\":%zu,\"chunk_max_output_tokens\":%zu,\"encoder_"
          "context_tokens\":512,\"input_partitioning\":\"native_token_chunks\","
          "\"processed_input_tokens\":null,\"map_output_bytes\":%zu,"
          "\"reduction_rounds\":%zu,\"max_output_bytes\":1800,\"output_"
          "truncated\":%s}}\n",
          s->executable_sha256, s->model_sha256, tokens,
          tokens < 64 ? tokens : 64, mapped_bytes, reduction_rounds,
          output_truncated ? "true" : "false");
    } else {
      fprintf(
          out,
          ",\"coverage\":{\"status\":\"partial\",\"reason\":\"bounded_encoder_"
          "and_generated_summary_uncertain\"},\"provenance\":{\"engine\":"
          "\"samosa-native-t5\",\"engine_sha256\":\"%s\",\"model_sha256\":\"%"
          "s\","
          "\"recipe\":\"samosa-native-summary-scalar-v1\",\"max_output_"
          "tokens\":%"
          "zu,\"encoder_context_tokens\":512,\"processed_input_tokens\":null}}"
          "\n",
          s->executable_sha256, s->model_sha256, tokens);
    }
  }
  free(provider);
  free(text);
  free(result);
  return error ? fail(out, error) : 0;
}
