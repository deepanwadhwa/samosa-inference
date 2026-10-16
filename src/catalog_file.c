#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "catalog.h"
#include "private_config.h"
const char *provider_catalog_load(const char *path,ProviderCatalog *out) {
    if(!out)return "invalid_catalog_file";
    char *bytes=NULL;size_t length=0;
    const char *error=private_config_read(path,PROVIDER_CATALOG_BYTES,&bytes,&length);
    if(error)return !strcmp(error,"allocation_failed")?error:"invalid_catalog_file";
    error=provider_catalog_parse(bytes,length,out);free(bytes);return error;
}
