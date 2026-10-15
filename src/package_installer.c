#define _POSIX_C_SOURCE 200809L
#include "package_installer.h"
#include "package_reserve.h"
#include "package_compatibility.h"
#include "package_remove.h"
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
    if(!c||!c->entry||!out||(c->file_sources&&c->digest_source)||!c->assembly.reservation_ceiling||!c->assembly.reservation_capacity||c->assembly.reservation_capacity>PACKAGE_RESERVATION_CAPACITY||!c->entry->size||c->entry->size>PACKAGE_INDEX_BYTES||c->store.acquire||
        !memchr(c->entry->platform,0,sizeof(c->entry->platform))||!memchr(c->entry->minimum_os,0,sizeof(c->entry->minimum_os))||
        !memchr(c->entry->recipe,0,sizeof(c->entry->recipe))||!memchr(c->entry->runtime,0,sizeof(c->entry->runtime))||!memchr(c->entry->sha256,0,sizeof(c->entry->sha256))||
        !memchr(c->entry->url,0,sizeof(c->entry->url)))return "invalid_package_installer";
    const char *e=c->assembly.control?c->assembly.control(c->assembly.context):NULL;if(e)return e;
    char platform[65],os[65];e=provider_catalog_host(platform,os);
    if(!e&&strcmp(c->entry->platform,platform))e="unsupported_provider_platform";
    if(!e&&strcmp(c->entry->runtime,"system-runtime-v1"))e="unsupported_provider_runtime";
    if(!e)e=provider_catalog_qualify(c->entry,os);if(e)return e;
    if(!package_recipe_supported(c->entry->recipe))return "unsupported_package_recipe";
    if(!strcmp(c->entry->recipe,"llama-gguf-package-v1")&&!c->assembly.llama_policy)return "unqualified_llama_package_runtime";
    if(!strcmp(c->entry->recipe,"llama-gguf-package-v1")&&!llama_package_policy_valid(c->assembly.llama_policy))return "invalid_llama_package_policy";
    if(!strcmp(c->entry->recipe,"whisper-ggml-package-v1")&&!c->assembly.whisper_policy)return "unqualified_whisper_package_runtime";
    if(!strcmp(c->entry->recipe,"whisper-ggml-package-v1")&&!whisper_package_policy_valid(c->assembly.whisper_policy))return "invalid_whisper_package_policy";
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
    if(!e){PackageRemoteCacheSource files={.store=c->store,.index=index,.files=c->file_sources,.count=index->count,.digest_source=c->digest_source};
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

const char *package_install_from_file(const char *path,const char *id,const PackageInstaller *config,PackageTreeResult *out){
    if(!config||!out)return "invalid_package_installer";
    const char *e=config->assembly.control?config->assembly.control(config->assembly.context):NULL;if(e)return e;
    ProviderCatalog *catalog=calloc(1,sizeof(*catalog));if(!catalog)return "allocation_failed";
    e=provider_catalog_load(path,catalog);
    if(!e)e=config->assembly.control?config->assembly.control(config->assembly.context):NULL;
    if(!e)e=package_install_selected(catalog,id,config,out);
    free(catalog);return e;
}

static const char *cached_catalog_operation(const char *path,const char *id,const PackageInstaller *config,PackageAssemblyStatus *out,int *released,int remove){
    if(!config||(!out&&!released))return "invalid_package_installer";
    const char *e=config->assembly.control?config->assembly.control(config->assembly.context):NULL;if(e)return e;
    ProviderCatalog *catalog=calloc(1,sizeof(*catalog));PackageIndex *index=calloc(1,sizeof(*index));
    if(!catalog||!index){free(catalog);free(index);return "allocation_failed";}
    const ProviderEntry *entry=NULL;char platform[65],os[65];int fd=-1;
    e=provider_catalog_load(path,catalog);
    if(!e)e=provider_catalog_host(platform,os);
    if(!e)e=provider_catalog_select(catalog,id,platform,&entry);
    if(!e)e=provider_catalog_qualify(entry,os);
    if(!e){ArtifactConfig artifact=config->store;artifact.sha256=entry->sha256;artifact.size=entry->size;
        e=artifact_open_verified(&artifact,config->assembly.control,config->assembly.context,&fd);}
    if(!e)e=package_catalog_read_index(entry,fd,config->assembly.control,config->assembly.context,index);
    if(fd>=0)close(fd);
    if(!e){PackageAssembly assembly=config->assembly;assembly.index=index;
        if(out)e=package_assembly_status(&assembly,out);
        else if(remove)e=package_remove_run(assembly.reservation_directory,assembly.directory,index,assembly.control,assembly.context,released);
        else e=package_reservation_release_empty(assembly.reservation_directory,assembly.directory,index,assembly.control,assembly.context,released);}
    free(index);free(catalog);return e;
}
const char *package_status_from_file(const char *path,const char *id,const PackageInstaller *config,PackageAssemblyStatus *out){return cached_catalog_operation(path,id,config,out,NULL,0);}
const char *package_release_empty_from_file(const char *path,const char *id,const PackageInstaller *config,int *out){return cached_catalog_operation(path,id,config,NULL,out,0);}
const char *package_remove_from_file(const char *path,const char *id,const PackageInstaller *config,int *out){return cached_catalog_operation(path,id,config,NULL,out,1);}
