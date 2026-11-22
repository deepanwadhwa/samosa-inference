#ifndef SAMOSA_BACKEND_PROCESS_INTERNAL_H
#define SAMOSA_BACKEND_PROCESS_INTERNAL_H
#include <samosa_inference/backend_process.h>
#include "engine_process.h"
/* Existing owner/guardian launch into stable adapter-owned storage, for the
 * Activation boundary. Never copy a live record or use a second guardian.
 * The adapter joins users and withdraws/finishes the backend owner before
 * releasing this storage. Outside code must not independently poll/stop it
 * while the backend owner is published. All launch inputs remain borrowed. */
const char *backend_process_start_delegated(const SamosaBackendOwner *,const SamosaBackendLaunch *,EngineProcess *);
/* Recoverable JSON projection of the existing private bootstrap serialization.
 * All fields/strings come from configuration_write, not rebuilt launch rules. */
const char *backend_process_launch_snapshot(const SamosaBackendLaunch *,char **);
#endif
