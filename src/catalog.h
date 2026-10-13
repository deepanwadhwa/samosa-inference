#ifndef SAMOSA_CATALOG_H
#define SAMOSA_CATALOG_H
#include "artifact.h"
#include <stddef.h>
#define PROVIDER_CATALOG_MAX 16
#define PROVIDER_CATALOG_BYTES 65536
typedef struct {
    char id[65],version[65],url[2049],license[2049],platform[65];
    char minimum_os[65],runtime[65],recipe[129],sha256[65];
    uint64_t size;
} ProviderEntry;
typedef struct { char revision[65]; size_t count; ProviderEntry entries[PROVIDER_CATALOG_MAX]; } ProviderCatalog;
/* Host-owned bytes only. On failure output is untouched. */
const char *provider_catalog_parse(const char *,size_t,ProviderCatalog *);
const char *provider_catalog_load(const char *,ProviderCatalog *);
const char *provider_catalog_select(const ProviderCatalog *,const char *,const char *,const ProviderEntry **);
int provider_os_version(const char *,uint32_t [3]);
const char *provider_catalog_qualify(const ProviderEntry *,const char *);
const char *provider_catalog_host(char [65],char [65]);
void provider_catalog_artifact(const ProviderEntry *,ArtifactConfig *);
#endif
