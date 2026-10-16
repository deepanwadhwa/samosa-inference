#ifndef SAMOSA_INFERENCE_H
#define SAMOSA_INFERENCE_H
#include <stdio.h>
#include <stddef.h>
#include "llama.h"
#if defined(SAMOSA_WITH_TEMPORAL)
#define INFERENCE_REQUEST_MAX 25165824u
#elif defined(SAMOSA_WITH_WHISPER) || defined(SAMOSA_WITH_VISION) || defined(SAMOSA_WITH_DECISION)
#define INFERENCE_REQUEST_MAX 2097152u
#else
#define INFERENCE_REQUEST_MAX 1048576u
#endif
int inference_dispatch(const char *,size_t,FILE *);
int inference_dispatch_config(const InferenceConfig *,const char *,size_t,FILE *);
#endif
