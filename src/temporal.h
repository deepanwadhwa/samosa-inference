#ifndef SAMOSA_TEMPORAL_H
#define SAMOSA_TEMPORAL_H
#include "json.h"
#include "llama.h"
struct TemporalConfig {
    const char *self, *executable, *model, *executable_sha256;
    const char *manifest_sha256, *model_sha256;
};
typedef struct {
    size_t timestamp_us, width, height;
    unsigned char *rgba;
    char rgba_sha256[65], prepared_sha256[65];
} TemporalFrame;
typedef struct {
    TemporalFrame frames[16];
    size_t count, start_us, end_us, tokens;
    char *provider, *prompt;
    char input_sha256[65], prepared_sha256[65];
} TemporalInput;
int temporal_configured(const InferenceConfig *);
const char *temporal_input(const InferenceConfig *, const Json *,
                           TemporalInput *);
void temporal_input_free(TemporalInput *);
char *temporal_normalized(const InferenceConfig *, const Json *);
int temporal_dispatch(const InferenceConfig *, const Json *, FILE *);
/* Exact native P6 preparation, shared with request identity/response checks. */
char *temporal_ppm(const TemporalFrame *, size_t *);
#endif
