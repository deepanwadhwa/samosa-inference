#ifndef SAMOSA_INFERENCE_PACKAGE_INDEX_H
#define SAMOSA_INFERENCE_PACKAGE_INDEX_H
#include <stddef.h>
#include <stdint.h>
#define PACKAGE_INDEX_FILES 512u
#define PACKAGE_INDEX_BYTES 1048576u
#define PACKAGE_PAYLOAD_BYTES UINT64_C(68719476736)
enum PackageFileRole {PACKAGE_ENGINE=1,PACKAGE_LIBRARY,PACKAGE_MODEL,PACKAGE_DATA,PACKAGE_METADATA};
typedef struct {char name[256],sha256[65];uint64_t bytes;enum PackageFileRole role;} PackageFile;
typedef struct {
    char manifest_sha256[65],platform[65],minimum_os[65],recipe[129];
    size_t count;uint64_t payload_bytes;PackageFile files[PACKAGE_INDEX_FILES];
} PackageIndex;
/* Exact pinned bytes supplied by trusted acquisition/catalog configuration.
 * Parses no URLs and performs no installation. Output is untouched on failure. */
const char *package_index_parse(const char *,size_t,const char *,PackageIndex *);
/* Deployment/minimum-OS policy only; loader/backend compatibility and readiness
 * require their respective qualified package profiles after payload acquisition. */
const char *package_index_qualify(const PackageIndex *,const char *,const char *);
int package_file_name_valid(const char *,enum PackageFileRole);
#endif
