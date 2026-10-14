#ifndef SAMOSA_INFERENCE_PACKAGE_LEASE_H
#define SAMOSA_INFERENCE_PACKAGE_LEASE_H
#include "package_index.h"
/* Acquire a nonblocking shared consumer lease on a registered package. Never
 * creates/repairs state. Caller closes the owned descriptor only when no worker
 * can use package files. Writers use the existing exclusive assemble.lock.
 * Matching identity is not package integrity/completion/readiness. Caller keeps
 * named-root bindings and verifies publication while retaining this lease.
 * Output untouched on failure. Descriptor is close-on-exec. */
const char *package_lease_acquire(int,const PackageIndex *,const char *(*)(void *),void *,int *);
#endif
