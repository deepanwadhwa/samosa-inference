#ifndef SAMOSA_INFERENCE_PACKAGE_CATALOG_H
#define SAMOSA_INFERENCE_PACKAGE_CATALOG_H
#include "catalog.h"
#include "package_index.h"
/* Bind a verified catalog artifact to its complete package index. The catalog
 * entry's size/hash identify index bytes, not one weight file. Caller selects
 * the trusted catalog/host policy and owns the read-only descriptor. No URLs,
 * credentials or installation policy come from the package index. */
const char *package_catalog_read_index(const ProviderEntry *,int,const char *(*)(void *),void *,PackageIndex *);
#endif
