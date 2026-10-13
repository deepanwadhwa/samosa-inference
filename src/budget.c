#define _POSIX_C_SOURCE 200809L
#include "budget.h"
#include <math.h>
#include <time.h>
double inference_now(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return -1;return (double)t.tv_sec+(double)t.tv_nsec/1e9;}
const char *inference_interrupted(const InferenceConfig *cfg){
    if(!cfg)return NULL;
    if(!isfinite(cfg->deadline_monotonic)||cfg->deadline_monotonic<0)return "invalid_configuration";
    if(cfg->cancelled&&cfg->cancelled(cfg->cancellation))return "cancelled";
    if(cfg->deadline_monotonic){double now=inference_now();if(now<0)return "state_unavailable";if(now>=cfg->deadline_monotonic)return "deadline_exceeded";}
    return NULL;
}
long inference_transfer_ms(const InferenceConfig *cfg){long limit=cfg->transfer_timeout_ms?(long)cfg->transfer_timeout_ms:60000L;if(cfg->deadline_monotonic){double remaining=cfg->deadline_monotonic-inference_now();if(!isfinite(remaining)||remaining<=0)return 0;if(remaining<(double)limit/1000){long ms=(long)(remaining*1000);if(ms<1)ms=1;limit=ms;}}return limit;}
