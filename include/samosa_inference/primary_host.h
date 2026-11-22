#ifndef SAMOSA_INFERENCE_PRIMARY_HOST_H
#define SAMOSA_INFERENCE_PRIMARY_HOST_H

#include <stdint.h>
#include <stdio.h>
#include <samosa_inference/backend_process.h>
#include <samosa_inference/turn_control.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct SamosaPrimaryHost SamosaPrimaryHost;
typedef struct {
    const char *path, *sha256;
    uint64_t size;
} SamosaPrimaryInput;

typedef struct {
    SamosaBackendOwner owner;
    SamosaBackendLaunch launch;
    /* Trusted deployment data. The model anchor is the manifest for a model
     * directory and the GGUF for a file model. Inputs contain the remaining
     * weights, configuration, tokenizer, and projector used by launch. */
    const char *state;
    SamosaPrimaryInput engine, model;
    const SamosaPrimaryInput *inputs, *libraries;
    size_t input_count, library_count;
    SamosaTurnGate *gate;
    int *upstream_fd;
    int startup_seconds;
    int generation_seconds;
    int (*cancelled)(void *);
    void *cancellation;
} SamosaPrimaryConfig;

/* Initialize *host to NULL. The host config and its pointed-to data remain
 * immutable while retained. The caller serializes controls and joins request
 * users before stop. No other observer may stop or reap the process record.
 * An error may retain *host until physical cleanup is confirmed. */
const char *samosa_primary_start(SamosaPrimaryHost **host, const SamosaPrimaryConfig *);
const char *samosa_primary_status(SamosaPrimaryHost *, pid_t *pid, unsigned *revision);
/* Visual reclaim stops the worker while retaining Activation and verified
 * inputs. The caller owns the visual reservation and serializes pause/resume.
 * Resume checks retained identity, starts a fresh worker and probes generation.
 * On refusal, stop releases the retained state after confirmed cleanup. */
const char *samosa_primary_pause(SamosaPrimaryHost *, const SamosaBackendStopControl *);
const char *samosa_primary_resume(SamosaPrimaryHost *);
const char *samosa_primary_stop(SamosaPrimaryHost **host, const SamosaBackendStopControl *);
const char *samosa_primary_last_good(const char *state, FILE *);

#ifdef __cplusplus
}
#endif
#endif
