#ifndef SAMOSA_INFERENCE_DECISION_H
#define SAMOSA_INFERENCE_DECISION_H
#include "decision_assets.h"
#include "inference.h"
#include "json.h"
#include <stdint.h>
struct DecisionConfig {
  const char *self, *assets;
  const char *pins[DECISION_ASSET_COUNT];
  unsigned profile;
  /* Trusted resident text execution; keeps typed requests/results and public
   * provenance/receipts in this existing adapter. Image execution stays separate. */
  const char *(*run)(void *,const InferenceConfig *,const char *,char **);
  void *context;
  const char *(*run_image)(void *,const InferenceConfig *,const char *,const uint8_t *,size_t,size_t,char **);
};
/* Native typed worker request, validated and normalized by the existing Rust
 * decoder. The caller owns returned buffers. No model is loaded here. */
const char *decision_payload(const Json *, char **);
const char *decision_result_validate(const InferenceConfig *,const char *,const uint8_t *,size_t);
char *decision_normalized(const InferenceConfig *, const Json *);
int decision_dispatch(const InferenceConfig *, const Json *, FILE *);
#endif
