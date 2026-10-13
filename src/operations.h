#ifndef SAMOSA_INFERENCE_OPERATIONS_H
#define SAMOSA_INFERENCE_OPERATIONS_H
#include "llama.h"
#include <stdio.h>
#include <sqlite3.h>
typedef struct {sqlite3 *db;int directory,lease,cancel;char *id;const char *owner;
    int (*parent_cancelled)(void *);void *parent_cancellation;
} InferenceOperation;
const char *operation_begin(const InferenceConfig *,const char *,const char *,InferenceOperation *,char **);
const char *operation_begin_parent(const InferenceConfig *,const char *,const char *,const char *,InferenceOperation *,char **);
const char *operation_finish(InferenceOperation *,const char *,int);
void operation_close(InferenceOperation *);
int operation_cancelled(void *);
int operation_public(const InferenceConfig *,const char *,const char *,FILE *);
#endif
