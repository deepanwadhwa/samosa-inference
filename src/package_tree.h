#ifndef SAMOSA_INFERENCE_PACKAGE_TREE_H
#define SAMOSA_INFERENCE_PACKAGE_TREE_H
#include "package_index.h"
#include <sys/stat.h>
#define PACKAGE_TREE_DIRECTORIES (PACKAGE_INDEX_FILES * 7u)
typedef struct {char name[256];struct stat identity;} PackageTreeDirectory;
/* Native in-memory ownership evidence, not an on-disk ABI. File identities
 * follow pinned index order; directory paths are recorded with their inodes.
 * Caller allocates this bounded large object on the heap and holds its lease. */
typedef struct {struct stat root,files[PACKAGE_INDEX_FILES];
    PackageTreeDirectory directories[PACKAGE_TREE_DIRECTORIES];size_t directory_count,file_count;
    char manifest_sha256[65];} PackageTreeSnapshot;
typedef struct {size_t files,directories;uint64_t payload_bytes;char manifest_sha256[65];} PackageTreeResult;
/* Verify the held read-only payload-root descriptor against a parsed pinned
 * index. Complete private regular single-link files only, no unlisted entries.
 * Controls remain authoritative between directory entries and bounded hash
 * reads. Output is untouched on failure. This proves observed bytes/identities,
 * not future immutability, named-root binding, loader compatibility or readiness. */
const char *package_tree_verify(int,const PackageIndex *,const char *(*)(void *),void *,PackageTreeResult *);
/* Installer variant syncs observed payload files/directories before returning. */
const char *package_tree_sync(int,const PackageIndex *,const char *(*)(void *),void *,PackageTreeResult *);
/* Full pin/closed-tree verification plus exact observed inode inventory. Output
 * untouched on failure. Does not authorize deletion or future inode adoption. */
const char *package_tree_snapshot(int,const PackageIndex *,const char *(*)(void *),void *,PackageTreeSnapshot *);
#endif
