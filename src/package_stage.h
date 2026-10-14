#ifndef SAMOSA_INFERENCE_PACKAGE_STAGE_H
#define SAMOSA_INFERENCE_PACKAGE_STAGE_H
#include "package_index.h"
/* Caller retains its package coordination lease and held private staging root.
 * Input is a held verified artifact descriptor (cache publication hardlinks are
 * allowed). Copy uses fixed buffers and preserves the input seek position.
 * Publishes a private read-only, single-link payload without replacing a name.
 * This is per-file staging, not complete package publication or crash recovery. */
const char *package_stage_file(int,const PackageIndex *,size_t,int,const char *(*)(void *),void *);
/* Registered copies may resume only the bound inode and a prefix matching the
 * current fully verified source. Journal must be private and outside payload/. */
const char *package_stage_file_registered(int,int,const PackageIndex *,size_t,int,const char *(*)(void *),void *);
#endif
