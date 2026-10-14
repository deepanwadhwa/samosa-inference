#ifndef SAMOSA_INFERENCE_PACKAGE_REMOVAL_RECORD_H
#define SAMOSA_INFERENCE_PACKAGE_REMOVAL_RECORD_H
#include "package_tree.h"
#define PACKAGE_REMOVAL_BYTES 2097152u
/* Explicit portable fields: device, inode, uid, mode, link count, size,
 * mtime seconds/nanoseconds, ctime seconds/nanoseconds. Signed seconds use
 * modulo-2^64 encoding. No native struct layout is persisted. */
typedef struct {uint64_t value[10];} PackageRemovalIdentity;
typedef struct {char name[256];PackageRemovalIdentity identity;} PackageRemovalDirectory;
typedef struct {char index_sha256[65];size_t file_count,directory_count;
    PackageRemovalIdentity package,payload,completion,files[PACKAGE_INDEX_FILES];
    PackageRemovalDirectory directories[PACKAGE_TREE_DIRECTORIES];} PackageRemovalRecord;
/* Caller supplies verified inventory/registered completion under its exclusive
 * package lease. This encodes evidence, never grants removal authority itself. */
const char *package_removal_record_capture(const PackageIndex *,const PackageTreeSnapshot *,const struct stat *,const struct stat *,PackageRemovalRecord *);
/* Owned private immutable journal, with checksum and bounded explicit encoding.
 * Store does not replace an existing or foreign record. Load is read-only.
 * Caller owns lease/named-root bindings. Outputs untouched on failure. */
const char *package_removal_record_store(int,const PackageIndex *,const PackageRemovalRecord *,const char *(*)(void *),void *);
const char *package_removal_record_load(int,const PackageIndex *,const char *(*)(void *),void *,PackageRemovalRecord *);
#endif
