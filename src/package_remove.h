#ifndef SAMOSA_INFERENCE_PACKAGE_REMOVE_H
#define SAMOSA_INFERENCE_PACKAGE_REMOVE_H
#include "package_removal_record.h"
/* Resume deletion of a registered completed payload using its durable record.
 * Acquires the exclusive assembly lease; refuses readers, foreign nodes and
 * substituted inodes. Leaves payload root, completion, journal and quota charge
 * for subsequent verified retirement. No paths come from public requests. */
const char *package_remove_payload(int,const PackageIndex *,const char *(*)(void *),void *);
#endif
