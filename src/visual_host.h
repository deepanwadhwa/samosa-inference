#ifndef SAMOSA_INFERENCE_VISUAL_HOST_H
#define SAMOSA_INFERENCE_VISUAL_HOST_H
#include "activation.h"
#include "visual_assets.h"
#include "temporal_package.h"
#include <samosa_inference/specialist_supervisor.h>
#include <stdatomic.h>
typedef struct {
    Activation activation;
    InferenceConfig inference;
    SamosaMmSupervisor supervisor;
    SamosaMmSession session;
    VisionModelReceipt image_receipt;
    TemporalPackageReceipt temporal_receipt;
    struct stat engine_before,anchor_before;
    pthread_mutex_t mutex;
    atomic_int attempted,qualified;
    char state[4096],anchor[4096],workspace[256],bundle[512],identity[65];
    size_t engine_size,anchor_size;
    int initialized,temporal,activating,model,directory,input,output;
    const char *terminal;
} VisualHost;
const char *visual_host_init(VisualHost *,const InferenceConfig *,size_t,size_t);
const char *visual_host_start(VisualHost *,const InferenceConfig *);
const char *visual_host_status(void *);
const char *visual_host_stop(VisualHost *);
const char *visual_host_close(VisualHost *);
const char *visual_host_image(void *,const InferenceConfig *,const unsigned char *,size_t,size_t,
                              const char *,size_t,char **,size_t *,size_t *,char [65]);
const char *visual_host_temporal(void *,const InferenceConfig *,const TemporalInput *,char **,size_t *,size_t *);
#endif
