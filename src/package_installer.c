#define _POSIX_C_SOURCE 200809L
#include "package_installer.h"
#include "package_reserve.h"
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
typedef struct {const PackageInstaller *config;const char *reason;} InstallerControl;
static int interrupted(void *opaque){InstallerControl *c=opaque;const PackageInstaller *config=c->config;
    c->reason=config->assembly.control?config->assembly.control(config->assembly.context):NULL;
    if(c->reason)return 1;
    return config->store.cancelled&&config->store.cancelled(config->store.cancellation);
}
const char *package_install_catalog(const PackageInstaller *c,PackageTreeResult *out){
    if(!c||!c->entry||!out||!c->assembly.reservation_ceiling||!c->assembly.reservation_capacity||c->assembly.reservation_capacity>PACKAGE_RESERVATION_CAPACITY||!c->entry->size||c->entry->size>PACKAGE_INDEX_BYTES||c->store.acquire||
        !memchr(c->entry->platform,0,sizeof(c->entry->platform))||!memchr(c->entry->minimum_os,0,sizeof(c->entry->minimum_os))||
        !memchr(c->entry->recipe,0,sizeof(c->entry->recipe))||!memchr(c->entry->runtime,0,sizeof(c->entry->runtime))||!memchr(c->entry->sha256,0,sizeof(c->entry->sha256))||
        !memchr(c->entry->url,0,sizeof(c->entry->url)))return "invalid_package_installer";
    const char *e=c->assembly.control?c->assembly.control(c->assembly.context):NULL;if(e)return e;
    char platform[65],os[65];e=provider_catalog_host(platform,os);
    if(!e&&strcmp(c->entry->platform,platform))e="unsupported_provider_platform";
    if(!e&&strcmp(c->entry->runtime,"system-runtime-v1"))e="unsupported_provider_runtime";
    if(!e)e=provider_catalog_qualify(c->entry,os);if(e)return e;
    ArtifactConfig artifact=c->store;artifact.sha256=c->entry->sha256;artifact.size=c->entry->size;
    int index_fd=-1;e=artifact_open_verified(&artifact,c->assembly.control,c->assembly.context,&index_fd);
    if(e&&!strcmp(e,"artifact_absent")&&c->index_source){
        InstallerControl controls={.config=c};artifact.cancelled=interrupted;artifact.cancellation=&controls;
        RemoteArtifactSource source=*c->index_source;source.url=c->entry->url;
        e=artifact_install_remote(&artifact,&source);if(controls.reason)e=controls.reason;
        if(!e)e=artifact_open_verified(&artifact,c->assembly.control,c->assembly.context,&index_fd);
    }
    PackageIndex *index=NULL;
    if(!e){index=calloc(1,sizeof(*index));if(!index)e="allocation_failed";}
    if(!e)e=package_catalog_read_index(c->entry,index_fd,c->assembly.control,c->assembly.context,index);
    if(index_fd>=0)close(index_fd);
    if(!e&&c->file_sources&&c->file_count!=index->count)e="invalid_package_remote_source";
    if(!e){PackageRemoteCacheSource files={.store=c->store,.index=index,.files=c->file_sources,.count=index->count};
        PackageAssembly assembly=c->assembly;assembly.index=index;assembly.source=-1;assembly.source_open=package_remote_cache_open;assembly.source_context=&files;
        e=package_assemble(&assembly,out);}
    free(index);return e;
}

const char *package_install_selected(const ProviderCatalog *catalog,const char *id,const PackageInstaller *config,PackageTreeResult *out){
    if(!config||!out)return "invalid_package_installer";
    char platform[65],os[65];const char *e=provider_catalog_host(platform,os);if(e)return e;
    const ProviderEntry *entry=NULL;e=provider_catalog_select(catalog,id,platform,&entry);if(e)return e;
    PackageInstaller selected=*config;selected.entry=entry;return package_install_catalog(&selected,out);
}
