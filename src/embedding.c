#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "admission.h"
#include "budget.h"
#include "embedding.h"
#include "sha256.h"
#include <fcntl.h>
#include <locale.h>
#ifdef __APPLE__
/* Older macOS SDKs declare per-thread POSIX locales in xlocale.h. */
#include <xlocale.h>
#endif
#include <sys/stat.h>
#include <unistd.h>
static int failure(FILE *out, const char *error) {
  fprintf(out, "{\"contract_version\":1,\"error\":{\"code\":\"%s\"}}\n", error);
  return 1;
}
static int digest(const char *s) {
  if (!s || strlen(s) != 64)
    return 0;
  for (size_t i = 0; i < 64; i++)
    if (!strchr("0123456789abcdef", s[i]))
      return 0;
  return 1;
}
static int string(const char *s) { return s && *s && strlen(s) <= 256; }
static int configured(const InferenceConfig *cfg) {
  const EmbeddingConfig *e = cfg ? cfg->embedding : NULL;
  return e && string(e->model_id) && string(e->model_revision) &&
         string(e->recipe) && e->query_prefix &&
         strlen(e->query_prefix) <= 256 && e->document_prefix &&
         strlen(e->document_prefix) <= 256 && digest(e->model_sha256) &&
         digest(e->engine_sha256) && digest(e->tokenizer_hash) &&
         e->dimensions && e->dimensions <= 1024;
}
static void profile(FILE *out, const EmbeddingConfig *e) {
  Sha256 identity;
  unsigned char sum[32];
  char revision[65];
  sha256_init(&identity);
  const char *parts[] = {"samosa-embedding-profile-v1",
                         e->model_revision,
                         e->model_sha256,
                         e->engine_sha256,
                         e->tokenizer_hash,
                         e->recipe,
                         e->query_prefix,
                         e->document_prefix,
                         "pooled-l2-f32"};
  for (size_t i = 0; i < sizeof(parts) / sizeof(parts[0]); i++)
    sha256_update(&identity, parts[i], strlen(parts[i]) + 1);
  char dimension[32];
  snprintf(dimension, sizeof(dimension), "%zu", e->dimensions);
  sha256_update(&identity, dimension, strlen(dimension));
  sha256_final(&identity, sum);
  sha256_hex(sum, revision);

  fputs("{\"representation_kind\":\"text_embedding\",\"model_id\":", out);
  json_quote(out, e->model_id, strlen(e->model_id));
  fputs(",\"model_revision\":", out);
  json_quote(out, revision, 64);
  fprintf(out,
          ",\"dimensions\":%zu,\"dtype\":\"f32\",\"normalization\":\"l2\","
          "\"tokenizer_hash\":",
          e->dimensions);
  json_quote(out, e->tokenizer_hash, 64);
  fputs(",\"projector_hash\":null}", out);
}
int embedding_describe(const InferenceConfig *cfg,const Json *j,FILE *out) {
  if(!configured(cfg))return failure(out,"invalid_configuration");
  if(j->tokens[0].count!=2)return failure(out,"unknown_field");
  fputs("{\"contract_version\":1,\"component\":\"samosa-inference\",\"provider_id\":",out);
  json_quote(out,cfg->provider_id,strlen(cfg->provider_id));
  fprintf(out,",\"capabilities\":[\"text_embeddings\",\"operations\",\"cancellation\",\"request_budgets\"],\"max_title_bytes\":1024,\"context_tokens\":%zu,\"representation\":",cfg->context_tokens);
  profile(out,cfg->embedding);fputs("}\n",out);return 0;
}
static int request(const Json *j, char **text, char **role, char **title) {
  *text = *role = *title = NULL;
  const char *fields[] = {"contract_version",
                          "op",
                          "text",
                          "role",
                          "title",
                          "operation_id",
                          "parent_operation_id",
                          "timeout_ms"};
  unsigned seen = 0;
  for (int t = 1; t < j->tokens[0].next;
       t = j->tokens[j->tokens[t].next].next) {
    char *key = json_string(j, t);
    size_t field = 0;
    if (!key)
      return 0;
    for (; field < sizeof(fields) / sizeof(fields[0]); field++)
      if (!strcmp(key, fields[field]))
        break;
    free(key);
    if (field == sizeof(fields) / sizeof(fields[0]) || (seen & (1u << field)))
      return 0;
    seen |= 1u << field;
  }
  *text = json_string(j, json_get(j, 0, "text"));
  *role = json_string(j, json_get(j, 0, "role"));
  *title = json_string(j, json_get(j, 0, "title"));
  return *text && **text && *role &&
         (!strcmp(*role, "query") || !strcmp(*role, "document")) &&
         (json_get(j, 0, "title") < 0 ||
          (*title && strlen(*title) <= 1024 && !strcmp(*role, "document")));
}
char *embedding_normalized(const InferenceConfig *cfg, const Json *j) {
  if (!configured(cfg))
    return NULL;
  char *text = NULL, *role = NULL, *title = NULL, *body = NULL;
  size_t n = 0;
  int valid = request(j, &text, &role, &title);
  FILE *out = valid ? open_memstream(&body, &n) : NULL;
  if (out) {
    const EmbeddingConfig *e = cfg->embedding;
    fputs("{\"op\":\"embed_text\",\"provider_id\":", out);
    json_quote(out, cfg->provider_id, strlen(cfg->provider_id));
    fputs(",\"model_alias\":", out);
    if (cfg->model_alias)
      json_quote(out, cfg->model_alias, strlen(cfg->model_alias));
    else
      fputs("null", out);
    fputs(",\"text\":", out);
    json_quote(out, text, strlen(text));
    fputs(",\"role\":", out);
    json_quote(out, role, strlen(role));
    fputs(",\"title\":", out);
    json_quote(out, title ? title : "none", strlen(title ? title : "none"));
    fputs(",\"profile\":", out);
    profile(out, e);
    fputs(",\"recipe\":", out);
    json_quote(out, e->recipe, strlen(e->recipe));
    fprintf(out,
            ",\"model_sha256\":\"%s\",\"engine_sha256\":\"%s\",\"context\":%zu,"
            "\"query_prefix\":",
            e->model_sha256, e->engine_sha256, cfg->context_tokens);
    json_quote(out, e->query_prefix, strlen(e->query_prefix));
    fputs(",\"document_prefix\":", out);
    json_quote(out, e->document_prefix, strlen(e->document_prefix));
    for (size_t i = 0; i < 2; i++) {
      const char *key = i ? "timeout_ms" : "parent_operation_id";
      int token = json_get(j, 0, key);
      if (token >= 0) {
        fprintf(out, ",\"%s\":", key);
        fwrite(j->text + j->tokens[token].begin, 1,
               j->tokens[token].end - j->tokens[token].begin, out);
      }
    }
    fputc('}', out);
    if (fclose(out)) {
      free(body);
      body = NULL;
    }
  }
  free(text);
  free(role);
  free(title);
  return body;
}
int embedding_dispatch(const InferenceConfig *cfg, const Json *j, FILE *out) {
  if (!configured(cfg))
    return failure(out, "invalid_configuration");
  char *text = NULL, *role = NULL, *title = NULL;
  if (!request(j, &text, &role, &title)) {
    free(text);
    free(role);
    free(title);
    return failure(out, "invalid_request");
  }
  const EmbeddingConfig *e = cfg->embedding;
  char *prepared = NULL;
  size_t n = 0;
  FILE *stream = open_memstream(&prepared, &n);
  const char *error = stream ? NULL : "resource_limit";
  if (stream) {
    fputs(!strcmp(role, "query") ? e->query_prefix : e->document_prefix,
          stream);
    if (!strcmp(role, "document"))
      fprintf(stream, "title: %s | text: ", title ? title : "none");
    fputs(text, stream);
    if (fclose(stream))
      error = "resource_limit";
  }
  float *vector = NULL;
  size_t tokens = 0;
  int lease = -1;
  if (!error)
    error = inference_interrupted(cfg);
  if (!error)
    error = inference_schedule(cfg, &lease);
#ifdef SAMOSA_WITH_LLAMA
  if (!error)
    error = llama_embed(cfg, prepared, e->dimensions, &vector, &tokens);
#else
  if (!error)
    error = "provider_unavailable";
#endif
  if (lease >= 0)
    close(lease);
  if (!error)
    error = inference_interrupted(cfg);
  locale_t numeric = (locale_t)0, previous = (locale_t)0;
  if (!error) {
    numeric = newlocale(LC_NUMERIC_MASK, "C", (locale_t)0);
    if (!numeric)
      error = "resource_limit";
    else {
      previous = uselocale(numeric);
      if (!previous)
        error = "resource_limit";
    }
  }
  if (!error) {
    Sha256 hash;
    unsigned char sum[32];
    char input[65];
    sha256_init(&hash);
    sha256_update(&hash, text, strlen(text));
    sha256_final(&hash, sum);
    sha256_hex(sum, input);
    fputs("{\"contract_version\":1,\"kind\":\"derived\",\"artifact_type\":"
          "\"embedding\",\"provider_id\":",
          out);
    json_quote(out, cfg->provider_id, strlen(cfg->provider_id));
    fprintf(out,
            ",\"input_sha256\":\"%s\",\"input_bytes\":%zu,\"input_tokens\":%zu,"
            "\"role\":",
            input, strlen(text), tokens);
    json_quote(out, role, strlen(role));
    fputs(",\"title\":", out);
    if (title)
      json_quote(out, title, strlen(title));
    else
      fputs("null", out);
    sha256_init(&hash);
    sha256_update(&hash, prepared, strlen(prepared));
    sha256_final(&hash, sum);
    sha256_hex(sum, input);
    fprintf(out,
            ",\"prepared_input_sha256\":\"%s\",\"prepared_input_bytes\":%zu",
            input, strlen(prepared));
    fputs(",\"profile\":", out);
    profile(out, e);
    fputs(",\"vector\":[", out);

    for (size_t i = 0; i < e->dimensions; i++)
      fprintf(out, "%s%.9g", i ? "," : "", (double)vector[i]);
    fprintf(out,
            "],\"provenance\":{\"model_sha256\":\"%s\",\"engine_sha256\":\"%"
            "s\",\"recipe\":",
            e->model_sha256, e->engine_sha256);
    json_quote(out, e->recipe, strlen(e->recipe));
    fputs("}}\n", out);
  }
  if (previous)
    uselocale(previous);
  if (numeric)
    freelocale(numeric);
  free(vector);
  free(prepared);
  free(text);
  free(role);
  free(title);
  return error ? failure(out, error) : 0;
}

void embedding_config_free(EmbeddingConfig *e) {
  if (!e)
    return;
  free((void *)e->model_id);
  free((void *)e->model_revision);
  free((void *)e->model_sha256);
  free((void *)e->engine_sha256);
  free((void *)e->tokenizer_hash);
  free((void *)e->query_prefix);
  free((void *)e->document_prefix);
  free((void *)e->recipe);
  memset(e, 0, sizeof(*e));
}
static int config_stable(const struct stat *a,const struct stat *b){
 if(a->st_dev!=b->st_dev||a->st_ino!=b->st_ino||a->st_size!=b->st_size||a->st_mode!=b->st_mode||a->st_uid!=b->st_uid||a->st_nlink!=b->st_nlink)return 0;
#ifdef __APPLE__
 return a->st_mtimespec.tv_sec==b->st_mtimespec.tv_sec&&a->st_mtimespec.tv_nsec==b->st_mtimespec.tv_nsec&&a->st_ctimespec.tv_sec==b->st_ctimespec.tv_sec&&a->st_ctimespec.tv_nsec==b->st_ctimespec.tv_nsec;
#else
 return a->st_mtim.tv_sec==b->st_mtim.tv_sec&&a->st_mtim.tv_nsec==b->st_mtim.tv_nsec&&a->st_ctim.tv_sec==b->st_ctim.tv_sec&&a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
#endif
}
int embedding_config_load(const char *path, EmbeddingConfig *e) {
  memset(e, 0, sizeof(*e));
  if (!path || path[0] != '/')
    return 0;
  int fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
  if (fd < 0)
    return 0;
  struct stat before, after, named;
  char body[8193];
  size_t used = 0;
  int valid = !fstat(fd, &before) && S_ISREG(before.st_mode) &&
              before.st_uid == geteuid() && !(before.st_mode & 077) &&
              before.st_nlink == 1 && before.st_size > 0 &&
              before.st_size <= 8192;
  while (valid && used < (size_t)before.st_size) {
    ssize_t got = read(fd, body + used, (size_t)before.st_size - used);
    if (got < 0 && errno == EINTR)
      continue;
    if (got <= 0) {
      valid = 0;
      break;
    }
    used += (size_t)got;
  }
  if (valid)
    valid = !fstat(fd, &after) && !lstat(path, &named) && config_stable(&before,&after) && config_stable(&before,&named);
  close(fd);
  if (!valid)
    return 0;
  Json j;
  valid = json_parse(&j, body, used) && j.tokens[0].type == JT_OBJECT;
  const char *names[] = {"model_id",        "model_revision", "model_sha256",
                         "engine_sha256",   "tokenizer_hash", "query_prefix",
                         "document_prefix", "recipe",         "dimensions"};
  unsigned seen = 0;
  for (int t = 1; valid && t < j.tokens[0].next;
       t = j.tokens[j.tokens[t].next].next) {
    char *key = json_string(&j, t);
    size_t i = 0;
    for (; key && i < 9; i++)
      if (!strcmp(key, names[i]))
        break;
    int has_key=key!=NULL;free(key);
    if (i == 9 || !has_key || (seen & (1u << i))) {
      valid = 0;
      break;
    }
    seen |= 1u << i;
  }
  if (valid && seen == 511) {
    e->model_id = json_string(&j, json_get(&j, 0, names[0]));
    e->model_revision = json_string(&j, json_get(&j, 0, names[1]));
    e->model_sha256 = json_string(&j, json_get(&j, 0, names[2]));
    e->engine_sha256 = json_string(&j, json_get(&j, 0, names[3]));
    e->tokenizer_hash = json_string(&j, json_get(&j, 0, names[4]));
    e->query_prefix = json_string(&j, json_get(&j, 0, names[5]));
    e->document_prefix = json_string(&j, json_get(&j, 0, names[6]));
    e->recipe = json_string(&j, json_get(&j, 0, names[7]));
    valid = json_uint(&j, json_get(&j, 0, names[8]), &e->dimensions);
    InferenceConfig cfg = {.embedding = e};
    valid = valid && configured(&cfg);
  } else
    valid = 0;
  if (!valid)
    embedding_config_free(e);
  return valid;
}
