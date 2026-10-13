#ifndef SAMOSA_ACTIVATION_H
#define SAMOSA_ACTIVATION_H
#include "engine_process.h"
#include <stdint.h>
#include <stdio.h>
typedef struct {
    const char *state,*self,*engine,*model,*engine_sha,*model_sha,*argv_template;
    uint64_t engine_size,model_size,byte_ceiling;
    unsigned expected_revision;
    /* Host-selected serialized reactivation of these exact pinned inputs.
     * The current revision is read under the residency lease. Public inference
     * requests cannot enable this; strict revision checking remains default. */
    int advance_current;
    double lifetime;size_t log_limit;
    int require_system_runtime;
    /* Adapter-owned readiness qualification; never supplied by a public caller. */
    const char *(*qualify)(void *);void *qualification;
#ifdef SAMOSA_INFERENCE_TEST_FAULTS
    void (*checkpoint)(const char *,void *);void *checkpoint_context;
#endif
} ActivationConfig;
typedef struct {EngineProcess process;int root,lease,engine_fd,model_fd;unsigned revision;} Activation;
const char *activation_start(const ActivationConfig *,Activation *);
const char *activation_stop(Activation *);
/* Reads only committed last-good configuration; it never claims a live engine. */
const char *activation_get(const char *state,FILE *);
#endif
