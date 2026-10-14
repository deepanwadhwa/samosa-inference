#ifndef SAMOSA_INFERENCE_PACKAGE_TREE_H
#define SAMOSA_INFERENCE_PACKAGE_TREE_H
#include "package_index.h"
typedef struct {size_t files,directories;uint64_t payload_bytes;char manifest_sha256[65];} PackageTreeResult;
/* Verify the held read-only payload-root descriptor against a parsed pinned
 * index. Complete private regular single-link files only, no unlisted entries.
 * Controls remain authoritative between directory entries and bounded hash
 * reads. Output is untouched on failure. This proves observed bytes/identities,
 * not future immutability, named-root binding, loader compatibility or readiness. */
const char *package_tree_verify(int,const PackageIndex *,const char *(*)(void *),void *,PackageTreeResult *);
/* Installer variant syncs observed payload files/directories before returning. */
const char *package_tree_sync(int,const PackageIndex *,const char *(*)(void *),void *,PackageTreeResult *);
#endif
