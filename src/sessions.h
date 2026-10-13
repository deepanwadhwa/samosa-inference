#ifndef SAMOSA_INFERENCE_SESSIONS_H
#define SAMOSA_INFERENCE_SESSIONS_H
#include "llama.h"
#include <stdio.h>
/* Kernel admission primitive works for fake/native drivers alike. */
const char *inference_admit(const char *,int *);
int session_public(const InferenceConfig *,const char *,const char *,FILE *);
const char *session_begin(const InferenceConfig *,const char *,size_t,const char *);
const char *session_finish(const InferenceConfig *,const char *,size_t,const char *,const char *);
#endif
