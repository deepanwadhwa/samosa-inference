#ifndef SAMOSA_INFERENCE_PACKAGE_NATIVE_RUNTIME_H
#define SAMOSA_INFERENCE_PACKAGE_NATIVE_RUNTIME_H
#include "package_tree.h"
/* Pinned closed-tree + actual-host executable/library inspection + dependency
 * closure. Exactly one engine is required. Models remain opaque pinned payloads;
 * backend/model compatibility and loaded readiness require their own gates. */
const char *package_native_runtime_verify(int,const PackageIndex *,const char *(*)(void *),void *);
/* Observe held/named directory identity. Caller retains the existing package
 * lease and repeats this around verification; this does not create a lease. */
int package_native_runtime_bound(int,const char *);
#endif
