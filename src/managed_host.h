#ifndef SAMOSA_MANAGED_HOST_H
#define SAMOSA_MANAGED_HOST_H
#include "managed_provider.h"
#include "embedding.h"
/* Trusted host configuration/lifecycle, extracted from managed_main. Public
 * requests never provide this path or launch policy. Do not copy a live host:
 * provider config borrows this object's owned strings, arrays and embedding.
 * Start only a fresh/closed object; keep it stable until close and join users
 * before closing. Startup failures clean owned state unless shutdown is pending. */
typedef struct {
    ManagedProvider provider;
    EmbeddingConfig embedding;
    char *profile,*state,*engine,*engine_sha,*model,*model_sha,*alias,*owner,*embedding_path,*args;
    char self[4096],activation_state[4096],admission[4096],endpoint[128],identity[100],key[65];
    char key_path[4096],key_name[80];
    int key_root,key_fd,key_exists,attempted,prepared,embedding_mode;
    size_t engine_size,model_size,context,port,revision,gpu,threads;
} ManagedHost;
/* Configuration-only load: no state directories, credentials or engine work.
 * Activation consumes that prepared object; close releases loaded metadata too. */
const char *managed_host_load(const char *,ManagedHost *);
/* Retains parsed configuration for another activation; no request users may
 * remain. Unconfirmed shutdown retains ownership. Confirmed terminal failure
 * is reported once after releasing the owner, permitting another activation. */
const char *managed_host_deactivate(ManagedHost *);
const char *managed_host_activate(const char *,int,int (*)(void *),void *,ManagedHost *);
const char *managed_host_start(const char *config_path,const char *executable,int advance_current,
                              int (*cancelled)(void *),void *cancellation,ManagedHost *);
/* Close only after request users have joined. A pending engine shutdown retains
 * ownership/backing storage; retry close or exit to release guardian lifeline. */
const char *managed_host_close(ManagedHost *);
#endif
