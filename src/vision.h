#ifndef SAMOSA_INFERENCE_VISION_H
#define SAMOSA_INFERENCE_VISION_H
#include "llama.h"
#include "json.h"
struct VisionConfig {
    const char *self,*executable,*model,*executable_sha256;
    const char *config_sha256,*tokenizer_sha256,*weights_sha256,*model_sha256;
};
/* Existing native VisionPsy inspect protocol; all paths/pins are host config. */
void vision_model_identity(const VisionConfig *,char [65]);
int vision_dispatch(const InferenceConfig *,const Json *,FILE *);
char *vision_normalized(const InferenceConfig *,const Json *);
int vision_bridge_dispatch(int,char **);
#endif
