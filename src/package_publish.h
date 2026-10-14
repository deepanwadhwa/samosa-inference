#ifndef SAMOSA_INFERENCE_PACKAGE_PUBLISH_H
#define SAMOSA_INFERENCE_PACKAGE_PUBLISH_H
#include "package_tree.h"
typedef const char *(*PackageCompatibility)(int,const PackageIndex *,void *);
/* Installer-owned package directory contains payload/ and completion metadata.
 * Caller retains coordination/quota/named-package-root ownership. Compatibility
 * is a required trusted backend validator, never a public caller callback. */
typedef struct {int directory;const PackageIndex *index;PackageCompatibility compatible;void *compatibility;
    const char *(*control)(void *);void *context;} PackagePublication;
const char *package_publish(const PackagePublication *,PackageTreeResult *);
/* A completion record alone never proves current integrity/compatibility. */
const char *package_publication_verify(const PackagePublication *,PackageTreeResult *);
#endif
