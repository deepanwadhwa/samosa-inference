#ifndef SAMOSA_INFERENCE_DECISION_H
#define SAMOSA_INFERENCE_DECISION_H
#include "decision_assets.h"
#include "inference.h"
#include "json.h"
struct DecisionConfig {
  const char *self, *assets;
  const char *pins[DECISION_ASSET_COUNT];
  unsigned profile;
};
/* Native typed worker request, validated and normalized by the existing Rust
 * decoder. The caller owns returned buffers. No model is loaded here. */
const char *decision_payload(const Json *, char **);
char *decision_normalized(const InferenceConfig *, const Json *);
int decision_dispatch(const InferenceConfig *, const Json *, FILE *);
#endif
