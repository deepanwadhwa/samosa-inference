#ifndef SAMOSA_INFERENCE_PACKAGE_IDENTITY_H
#define SAMOSA_INFERENCE_PACKAGE_IDENTITY_H
#include "package_index.h"
/* Under the package assembly lease, bind this private held directory to one
 * pinned index across restarts. This is ownership/identity metadata, not payload
 * integrity, completion, readiness or permission to recover unknown inodes. */
const char *package_identity_bind(int,const PackageIndex *,const char *(*)(void *),void *);
#endif
