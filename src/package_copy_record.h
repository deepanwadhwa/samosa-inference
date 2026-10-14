#ifndef SAMOSA_INFERENCE_PACKAGE_COPY_RECORD_H
#define SAMOSA_INFERENCE_PACKAGE_COPY_RECORD_H
#include "package_index.h"
/* Private journal outside payload/, under caller's package lease. Binds planned
 * bytes/index and held root, parent and copy inode before any payload write.
 * Actions: 0 verify, 1 register, 2 forget a verified matching record. */
const char *package_copy_record(int,int,int,int,const PackageIndex *,size_t,int);
#endif
