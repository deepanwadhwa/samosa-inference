#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "budget.h"
#include "json.h"
#include "llama.h"
#include "sessions.h"
#include <curl/curl.h>
#include <fcntl.h>
#include <locale.h>
#ifdef __APPLE__
/* Older macOS SDKs declare per-thread POSIX locales in xlocale.h. */
#include <xlocale.h>
#endif
#include <math.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>

#define HTTP_MAX 1048576u
/* Only trusted loopback endpoints for this first optional adapter. */
static int endpoint_valid(const char *url) {
  const char *p = NULL;
  if (!url)
    return 0;
  if (!strncmp(url, "http://", 7))
    p = url + 7;
  else if (!strncmp(url, "https://", 8))
    p = url + 8;
  else
    return 0;
  if (!strncmp(p, "127.0.0.1:", 10))
    p += 10;
  else if (!strncmp(p, "localhost:", 10))
    p += 10;
  else if (!strncmp(p, "[::1]:", 6))
    p += 6;
  else
    return 0;
  unsigned port = 0;
  size_t digits = 0;
  while (*p >= '0' && *p <= '9') {
    if (++digits > 5)
      return 0;
    port = port * 10 + (unsigned)(*p++ - '0');
  }
  return digits && port > 0 && port <= 65535 && !*p;
}
typedef struct {
  char *bytes;
  size_t used;
  int overflow;
} Response;
static size_t receive(char *data, size_t size, size_t count, void *opaque) {
  Response *r = opaque;
  if (size && count > SIZE_MAX / size) {
    r->overflow = 1;
    return 0;
  }
  size_t n = size * count;
  if (n > HTTP_MAX - r->used) {
    r->overflow = 1;
    return 0;
  }
  memcpy(r->bytes + r->used, data, n);
  r->used += n;
  return n;
}
static int progress(void *opaque, curl_off_t total, curl_off_t current,
                    curl_off_t upload_total, curl_off_t uploaded) {
  const InferenceConfig *cfg = opaque;
  (void)total;
  (void)current;
  (void)upload_total;
  (void)uploaded;
  return inference_interrupted(cfg) != NULL;
}
static const char *post(const InferenceConfig *cfg, const char *path,
                        const char *body, char **response, size_t *n) {
  const char *endpoint = cfg->endpoint;
  *response = NULL;
  *n = 0;
  const char *interruption = inference_interrupted(cfg);
  if (interruption)
    return interruption;
  if (!endpoint_valid(endpoint) || strlen(endpoint) > 128 ||
      (body && strlen(body) > HTTP_MAX))
    return "invalid_configuration";
  if (cfg->transfer_timeout_ms > 60000)
    return "invalid_configuration";
  if (cfg->api_key) {
    size_t bytes = strlen(cfg->api_key);
    if (!bytes || bytes > 128)
      return "invalid_configuration";
    for (size_t i = 0; i < bytes; i++)
      if (!isalnum((unsigned char)cfg->api_key[i]) &&
          !strchr("-_.", cfg->api_key[i]))
        return "invalid_configuration";
  }
  if (curl_global_init(CURL_GLOBAL_DEFAULT) != CURLE_OK)
    return "provider_unavailable";
  CURL *curl = curl_easy_init();
  if (!curl) {
    curl_global_cleanup();
    return "provider_unavailable";
  }
  char url[192];
  snprintf(url, sizeof(url), "%s%s", endpoint, path);
  Response result = {.bytes = malloc(HTTP_MAX + 1)};
  if (!result.bytes) {
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return "resource_limit";
  }
  struct curl_slist *headers =
      curl_slist_append(NULL, "Content-Type: application/json");
  if (!headers) {
    free(result.bytes);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return "resource_limit";
  }
  if (cfg->api_key) {
    char authorization[160];
    snprintf(authorization, sizeof(authorization), "Authorization: Bearer %s",
             cfg->api_key);
    struct curl_slist *with_auth = curl_slist_append(headers, authorization);
    if (!with_auth) {
      curl_slist_free_all(headers);
      free(result.bytes);
      curl_easy_cleanup(curl);
      curl_global_cleanup();
      return "resource_limit";
    }
    headers = with_auth;
  }
  curl_easy_setopt(curl, CURLOPT_URL, url);
  curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
  if (body) {
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE_LARGE,
                     (curl_off_t)strlen(body));
  }
  long timeout = inference_transfer_ms(cfg);
  if (timeout <= 0) {
    curl_slist_free_all(headers);
    free(result.bytes);
    curl_easy_cleanup(curl);
    curl_global_cleanup();
    return "deadline_exceeded";
  }
  curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT_MS,
                   timeout < 2000 ? timeout : 2000L);
  curl_easy_setopt(curl, CURLOPT_TIMEOUT_MS, timeout);
  curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 0L);
  curl_easy_setopt(curl, CURLOPT_PROXY, "");
  curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
  curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, receive);
  curl_easy_setopt(curl, CURLOPT_WRITEDATA, &result);
  if (cfg->cancelled || cfg->deadline_monotonic) {
    curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
    curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, progress);
    curl_easy_setopt(curl, CURLOPT_XFERINFODATA, cfg);
  }
  CURLcode rc = curl_easy_perform(curl);
  long status = 0;
  curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
  curl_slist_free_all(headers);
  curl_easy_cleanup(curl);
  curl_global_cleanup();
  const char *error = rc == CURLE_ABORTED_BY_CALLBACK  ? "cancelled"
                      : result.overflow                ? "response_too_large"
                      : rc == CURLE_OPERATION_TIMEDOUT ? "deadline_exceeded"
                      : rc != CURLE_OK                 ? "provider_unavailable"
                      : status == 401 || status == 403 ? "provider_auth_failed"
                      : status != 200                  ? "provider_http_error"
                                                       : NULL;
  if (rc == CURLE_ABORTED_BY_CALLBACK) {
    const char *reason = inference_interrupted(cfg);
    if (reason)
      error = reason;
  }
  if (error) {
    free(result.bytes);
    return error;
  }
  result.bytes[result.used] = 0;
  *response = result.bytes;
  *n = result.used;
  return NULL;
}
/* Tokenize's published response is one numeric token array. Count it without
 * allocating a tree node per token; bounded body/131072 IDs still apply. */
static void whitespace(const char *s, size_t n, size_t *p) {
  while (*p < n && strchr(" \t\r\n", s[*p]))
    (*p)++;
}
static int token_array(const char *s, size_t n, size_t *begin, size_t *end,
                       size_t *count) {
  size_t p = 0;
  *count = 0;
  whitespace(s, n, &p);
  if (p == n || s[p++] != '{')
    return 0;
  whitespace(s, n, &p);
  if (n - p < 8 || memcmp(s + p, "\"tokens\"", 8))
    return 0;
  p += 8;
  whitespace(s, n, &p);
  if (p == n || s[p++] != ':')
    return 0;
  whitespace(s, n, &p);
  *begin = p;
  if (p == n || s[p++] != '[')
    return 0;
  whitespace(s, n, &p);
  if (p < n && s[p] == ']')
    return 0;
  for (;;) {
    if (p == n || s[p] < '0' || s[p] > '9')
      return 0;
    uint64_t value = 0;
    size_t digits = 0;
    while (p < n && s[p] >= '0' && s[p] <= '9') {
      if (++digits > 10)
        return 0;
      value = value * 10 + (unsigned)(s[p++] - '0');
      if (value > 2147483647u)
        return 0;
    }
    if (digits > 1 && s[p - digits] == '0')
      return 0;
    if (++*count > 131072)
      return 0;
    whitespace(s, n, &p);
    if (p == n)
      return 0;
    char next = s[p++];
    if (next == ']')
      break;
    if (next != ',')
      return 0;
    whitespace(s, n, &p);
  }
  *end = p;
  whitespace(s, n, &p);
  if (p == n || s[p++] != '}')
    return 0;
  whitespace(s, n, &p);
  return p == n;
}
const char *llama_properties(const InferenceConfig *cfg) {
  if (cfg->owned_status) {
    const char *observed = cfg->owned_status(cfg->owned_context);
    if (observed)
      return observed;
  }
  char *reply = NULL;
  size_t reply_length = 0;
  if (!cfg->model_alias || !*cfg->model_alias || strlen(cfg->model_alias) > 256)
    return "invalid_configuration";
  const char *error = post(cfg, "/props", NULL, &reply, &reply_length);
  Json j;
  if (!error) {
    if (!json_parse(&j, reply, reply_length) || j.tokens[0].type != JT_OBJECT)
      error = "invalid_provider_response";
    else {
      char *alias = json_string(&j, json_get(&j, 0, "model_alias"));
      int defaults = json_get(&j, 0, "default_generation_settings");
      size_t context;
      if (!alias || strcmp(alias, cfg->model_alias) ||
          !json_uint(&j, json_get(&j, defaults, "n_ctx"), &context) ||
          context != cfg->context_tokens)
        error = "provider_changed";
      free(alias);
    }
  }
  free(reply);
  return error;
}
static const char *prepare(const InferenceConfig *cfg, const char *prompt,
                           char **tokens, size_t *count) {
  *tokens = NULL;
  *count = 0;
  char *body = NULL, *reply = NULL, *rendered = NULL;
  size_t n = 0, reply_length = 0;
  const char *error = llama_properties(cfg);
  Json j;
  if (error)
    return error;
  FILE *stream = open_memstream(&body, &n);
  if (!stream)
    return "resource_limit";
  fputs("{\"messages\":[{\"role\":\"user\",\"content\":", stream);
  json_quote(stream, prompt, strlen(prompt));
  fputs("}],\"add_generation_prompt\":true,\"enable_thinking\":false}", stream);
  error = fclose(stream) ? "resource_limit" : NULL;
  if (!error)
    error = post(cfg, "/apply-template", body, &reply, &reply_length);
  if (!error) {
    if (!json_parse(&j, reply, reply_length) || j.tokens[0].type != JT_OBJECT)
      error = "invalid_provider_response";
    else {
      rendered = json_string(&j, json_get(&j, 0, "prompt"));
      if (!rendered)
        error = "invalid_provider_response";
    }
  }
  free(body);
  body = NULL;
  free(reply);
  reply = NULL;
  if (!error) {
    stream = open_memstream(&body, &n);
    if (!stream)
      error = "resource_limit";
    else {
      fputs("{\"content\":", stream);
      json_quote(stream, rendered, strlen(rendered));
      fputs(",\"add_special\":true,\"parse_special\":true}", stream);
      if (fclose(stream))
        error = "resource_limit";
    }
  }
  if (!error)
    error = post(cfg, "/tokenize", body, &reply, &reply_length);
  if (!error) {
    size_t begin, end;
    if (!token_array(reply, reply_length, &begin, &end, count))
      error = "invalid_provider_response";
    else {
      size_t bytes = end - begin;
      *tokens = malloc(bytes + 1);
      if (!*tokens)
        error = "resource_limit";
      else {
        memcpy(*tokens, reply + begin, bytes);
        (*tokens)[bytes] = 0;
      }
    }
  }
  free(rendered);
  free(body);
  free(reply);
  return error;
}
const char *llama_count(const InferenceConfig *cfg, const char *prompt,
                        size_t *count) {
  char *tokens = NULL;
  const char *err = prepare(cfg, prompt, &tokens, count);
  free(tokens);
  return err;
}
const char *llama_generate(const InferenceConfig *cfg, const char *prompt,
                           size_t reserve, char **answer, size_t *input,
                           size_t *output, size_t *processed) {
  *answer = NULL;
  *input = *output = *processed = 0;
  char *tokens = NULL, *body = NULL, *reply = NULL;
  size_t n = 0, length = 0;
  const char *error = prepare(cfg, prompt, &tokens, input);
  if (!error &&
      (reserve > cfg->context_tokens || *input > cfg->context_tokens - reserve))
    error = "context_limit";
  if (!error) {
    FILE *stream = open_memstream(&body, &n);
    if (!stream)
      error = "resource_limit";
    else {
      fprintf(stream,
              "{\"prompt\":%s,\"n_predict\":%zu,\"temperature\":0,\"cache_"
              "prompt\":true,\"stream\":false}",
              tokens, reserve);
      if (fclose(stream))
        error = "resource_limit";
    }
  }
  if (!error)
    error = post(cfg, "/completion", body, &reply, &length);
  Json j;
  if (!error) {
    if (!json_parse(&j, reply, length) || j.tokens[0].type != JT_OBJECT)
      error = "invalid_provider_response";
    else {
      int timings = json_get(&j, 0, "timings");
      *answer = json_string(&j, json_get(&j, 0, "content"));
      if (!*answer ||
          !json_uint(&j, json_get(&j, 0, "tokens_predicted"), output) ||
          *output > reserve ||
          !json_uint(&j, json_get(&j, timings, "prompt_n"), processed) ||
          *processed > *input)
        error = "invalid_provider_response";
    }
  }
  if (error) {
    free(*answer);
    *answer = NULL;
  }
  free(tokens);
  free(body);
  free(reply);
  return error;
}
const char *llama_admit(const char *path, int *lock) {
  return inference_admit(path, lock);
}

/* Embedding input already carries its task/document prefix. Chat templating
 * would alter the representation recipe, so tokenize it directly. */
const char *llama_embed(const InferenceConfig *cfg, const char *text,
                        size_t dimensions, float **vector,
                        size_t *input_tokens) {
  *vector = NULL;
  *input_tokens = 0;
  if (!cfg || !text || !*text || !dimensions || dimensions > 1024)
    return "invalid_request";
  const char *error = llama_properties(cfg);
  char *body = NULL, *reply = NULL, *tokens = NULL;
  size_t bytes = 0, length = 0;
  if (!error) {
    FILE *out = open_memstream(&body, &bytes);
    if (!out)
      error = "resource_limit";
    else {
      fputs("{\"content\":", out);
      json_quote(out, text, strlen(text));
      fputs(",\"add_special\":true,\"parse_special\":false}", out);
      if (fclose(out))
        error = "resource_limit";
    }
  }
  if (!error)
    error = post(cfg, "/tokenize", body, &reply, &length);
  if (!error) {
    size_t begin, end;
    if (!token_array(reply, length, &begin, &end, input_tokens))
      error = "invalid_provider_response";
    else if (*input_tokens > cfg->context_tokens)
      error = "context_limit";
    else {
      tokens = strndup(reply + begin, end - begin);
      if (!tokens)
        error = "resource_limit";
    }
  }
  free(body);
  free(reply);
  body = reply = NULL;
  if (!error) {
    FILE *out = open_memstream(&body, &bytes);
    if (!out)
      error = "resource_limit";
    else {
      fprintf(out,
              "{\"input\":%s,\"encoding_format\":\"float\",\"model\":", tokens);
      json_quote(out, cfg->model_alias, strlen(cfg->model_alias));
      fputs("}", out);
      if (fclose(out))
        error = "resource_limit";
    }
  }
  if (!error)
    error = post(cfg, "/v1/embeddings", body, &reply, &length);
  locale_t numeric = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0),
           previous = (locale_t)0;
  if (!numeric && !error)
    error = "resource_limit";
  if (numeric) {
    previous = uselocale(numeric);
    if (!previous && !error)
      error = "resource_limit";
  }
  Json j;
  if (!error) {
    if (!json_parse(&j, reply, length) || j.tokens[0].type != JT_OBJECT)
      error = "invalid_provider_response";
    else {
      int data = json_get(&j, 0, "data"), usage = json_get(&j, 0, "usage");
      size_t consumed, index;
      if (data < 0 || j.tokens[data].type != JT_ARRAY ||
          j.tokens[data].count != 1 ||
          !json_uint(&j, json_get(&j, usage, "prompt_tokens"), &consumed) ||
          consumed != *input_tokens ||
          !json_uint(&j, json_get(&j, data + 1, "index"), &index) || index)
        error = "invalid_provider_response";
      else {
        int values = json_get(&j, data + 1, "embedding");
        if (values < 0 || j.tokens[values].type != JT_ARRAY ||
            (size_t)j.tokens[values].count != dimensions)
          error = "invalid_provider_response";
        else {
          *vector = malloc(dimensions * sizeof(float));
          if (!*vector)
            error = "resource_limit";
          double norm = 0;
          size_t i = 0;
          for (int t = values + 1; !error && t < j.tokens[values].next;
               t = j.tokens[t].next) {
            char number[128];
            size_t n = j.tokens[t].end - j.tokens[t].begin;
            if (j.tokens[t].type != JT_NUMBER || n >= sizeof(number)) {
              error = "invalid_provider_response";
              break;
            }
            memcpy(number, j.text + j.tokens[t].begin, n);
            number[n] = 0;
            char *end;
            float value = strtof(number, &end);
            if (*end || !isfinite(value)) {
              error = "invalid_provider_response";
              break;
            }
            (*vector)[i++] = value;
            norm += (double)value * value;
          }
          if (!error && (!isfinite(norm) || norm < 0.999 || norm > 1.001))
            error = "invalid_provider_response";
        }
      }
    }
  }
  if (previous)
    uselocale(previous);
  if (numeric)
    freelocale(numeric);
  if (!error)
    error = inference_interrupted(cfg);
  if (error) {
    free(*vector);
    *vector = NULL;
  }
  free(tokens);
  free(body);
  free(reply);
  return error;
}
