#ifndef SAMOSA_MANAGED_PROVIDER_H
#define SAMOSA_MANAGED_PROVIDER_H
#include "activation.h"
#include "llama.h"
#include <sys/stat.h>
typedef struct {
    Activation activation;
    InferenceConfig inference;
    struct stat engine,model;
    const char *engine_path,*model_path,*state_path;
    int qualified;
} ManagedProvider;
/* Config and strings live for the supervisor lifetime. This is the text adapter
 * for the declared pinned static-system llama package, not arbitrary executables. */
const char *managed_provider_start(const ActivationConfig *,const InferenceConfig *,ManagedProvider *);
const char *managed_provider_stop(ManagedProvider *);
const char *managed_provider_status(void *);
const char *managed_provider_identity(const ActivationConfig *,const InferenceConfig *,char identity[100]);
#endif
