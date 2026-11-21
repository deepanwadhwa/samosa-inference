#ifndef SAMOSA_DECISION_HOST_H
#define SAMOSA_DECISION_HOST_H
#include "activation.h"
#include "decision.h"
typedef struct {
    Activation activation;
    DecisionAssets assets;
    InferenceConfig inference;
    struct stat engine_before,model_before;
    char state[4096],engine[4096],model[4096],tokenizer[4096],identity[65],bundle[512];
    size_t engine_size,model_size;
    int input,output,initialized,attempted,qualified,activating;
} DecisionHost;
const char *decision_host_init(DecisionHost *,const InferenceConfig *,size_t,size_t);
const char *decision_host_start(DecisionHost *,const InferenceConfig *);
const char *decision_host_status(void *);
const char *decision_host_stop(DecisionHost *);
const char *decision_host_run(void *,const InferenceConfig *,const char *,char **);
const char *decision_host_run_image(void *,const InferenceConfig *,const char *,const uint8_t *,size_t,size_t,char **);
#endif
