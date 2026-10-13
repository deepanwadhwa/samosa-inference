#ifndef SAMOSA_MANAGED_STREAM_H
#define SAMOSA_MANAGED_STREAM_H
#include <stdatomic.h>
#include <stdio.h>
typedef struct {
    int input,output;
    int (*dispatch)(void *,const char *,size_t,FILE *);void *context;
    int (*interrupted)(void *);void *interruption;
    atomic_int *stop;
} ManagedStreamConfig;
/* 0: EOF, 1: transport failure (all workers joined), 2: bounded shutdown expired.
 * On2 the owning process must exit immediately; worker references remain live. */
int managed_stream_run(const ManagedStreamConfig *);
#endif
