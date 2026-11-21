#ifndef SAMOSA_SUMMARY_HOST_H
#define SAMOSA_SUMMARY_HOST_H
#include "activation.h"
#include "summary.h"
#include <samosa_inference/summary_process.h>
typedef struct {
    Activation activation;
    InferenceConfig inference;
    SamosaSummaryProcess transport;
    struct stat engine_before,model_before;
    char state[4096];
    size_t engine_size,model_size;
    int initialized,attempted,qualified,activating,input,output;
    const char *terminal;
} SummaryHost;
const char *summary_host_init(SummaryHost *,const InferenceConfig *,size_t,size_t);
const char *summary_host_start(SummaryHost *,const InferenceConfig *);
const char *summary_host_status(void *);
const char *summary_host_stop(SummaryHost *);
const char *summary_host_close(SummaryHost *);
const char *summary_host_run(void *,const InferenceConfig *,const char *,size_t,int,char **);
#endif
