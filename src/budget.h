#ifndef SAMOSA_INFERENCE_BUDGET_H
#define SAMOSA_INFERENCE_BUDGET_H
#include "llama.h"
double inference_now(void);
const char *inference_interrupted(const InferenceConfig *);
long inference_transfer_ms(const InferenceConfig *);
#endif
