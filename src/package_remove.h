#ifndef SAMOSA_INFERENCE_PACKAGE_REMOVE_H
#define SAMOSA_INFERENCE_PACKAGE_REMOVE_H
#include "package_removal_record.h"
/* Resume deletion of a registered completed payload using its durable record.
 * Acquires the exclusive assembly lease; refuses readers, foreign nodes and
 * substituted inodes. Leaves payload root, completion, journal and quota charge
 * for subsequent verified retirement. No paths come from public requests. */
const char *package_remove_payload(int,const PackageIndex *,const char *(*)(void *),void *);
/* Retire only the empty recorded payload root and exact completion marker.
 * Resumes their already-absent states under the exclusive lease. Journal and
 * reservation remain until the following quota/metadata retirement phase. */
const char *package_remove_retire(int,const PackageIndex *,const char *(*)(void *),void *);
/* Release the exact charge only after recorded retirement and namespace checks.
 * Keeps the removal journal/barrier and identity metadata for later cleanup. */
const char *package_remove_release(int,int,const PackageIndex *,const char *(*)(void *),void *,int *);
/* Settle exact charge absence, remove only known empty housekeeping metadata,
 * then retire the journal last. Identity and assembly lock remain for safe
 * same-index reuse and idempotent recovery. Outputs untouched on failure. */
const char *package_remove_finish(int,int,const PackageIndex *,const char *(*)(void *),void *,int *);
#endif
