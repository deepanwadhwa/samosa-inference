#ifndef SAMOSA_ACTIVATION_H
#define SAMOSA_ACTIVATION_H
#include "engine_process.h"
#include <stdint.h>
#include <stdatomic.h>
#include <stdio.h>
#include <sys/stat.h>
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
    /* Trusted adapter launch after verified admission. Default remains the
     * existing bound engine. Adapters reuse guardian ownership, never public
     * request-selected commands. The model/residency fds are borrowed. */
    const char *(*launch)(void *,const char *,const char *,double,size_t,int,int,EngineProcess *);
    void *launch_context;
    /* Trusted bundle adapter may render its bound directory instead of the
     * default model fd argument. Requires a launch hook that binds/verifies
     * that bundle; model still names the separately pinned manifest file. */
    const char *model_argument;
    /* For an adapter executing this same pinned host executable, whose
     * dependencies are already part of the trusted application runtime.
     * Requires engine==self and a native qualifying launch hook. Never applies
     * the host dependency allowance to an external model engine. */
    int trusted_host_runtime;
    /* Trusted closed-package adapter: verify its complete native loader graph
     * before launch and bind retained assets to these admitted descriptors.
     * Mutually exclusive with the two existing runtime policies. */
    const char *(*verify_runtime)(void *,int,int);
    void *runtime_context;
    const char *bundle_identity;
    const char *bundle_config; /* Bounded JSON object, SHA256 == bundle_identity. */
    /* Trusted nonresident adapter: qualify executes the existing guarded
     * request and leaves no engine alive. No launch hook is accepted. */
    int on_demand;
    /* Native owning adapter preserves readable installed assets. Requires a
     * custom verified launch/runtime hook; private state policy is unchanged. */
    int readable_inputs;
#ifdef SAMOSA_INFERENCE_TEST_FAULTS
    void (*checkpoint)(const char *,void *);void *checkpoint_context;
#endif
} ActivationConfig;
typedef struct {EngineProcess process;int root,lease,engine_fd,model_fd;unsigned revision;int on_demand,retain_unknown_cleanup;atomic_int cleanup_failed;} Activation;
const char *activation_start(const ActivationConfig *,Activation *);
const char *activation_stop(Activation *);
/* Nonresident executor lost its guardian: keep the lifecycle fence and refuse
 * a false stopped/ready result. Recovery of that unknown group is separate. */
void activation_cleanup_failed(Activation *);
/* Shared live ownership check extracted from managed_provider_status. */
const char *activation_owned_status(const Activation *,const char *,const char *,const char *,const struct stat *,const struct stat *);
/* Shared retained-input identity check for adapter-owned qualification data. */
int activation_input_unchanged(int,const char *,const struct stat *);
const char *activation_retained_status(const Activation *,const char *,const char *,const char *,
    const struct stat *,const struct stat *);
/* Reads only committed last-good configuration; it never claims a live engine. */
const char *activation_get(const char *state,FILE *);
#endif
