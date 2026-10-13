#ifndef SAMOSA_INFERENCE_SUMMARY_H
#define SAMOSA_INFERENCE_SUMMARY_H
#include "llama.h"
#include "json.h"
struct SummaryConfig {
    const char *self, *executable, *model, *executable_sha256, *model_sha256;
};
int summary_bridge_dispatch(int, char **);
int summary_dispatch(const InferenceConfig *, const Json *, FILE *);
char *summary_normalized(const InferenceConfig *, const Json *);
#endif
