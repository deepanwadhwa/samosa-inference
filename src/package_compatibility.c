#include "package_compatibility.h"
#include "speech_package_loader.h"
#include <stdlib.h>
#include <string.h>
const char *package_backend_compatible(int root,const PackageIndex *index,void *context){
    PackageCompatibilityContext *c=context;
    if(!index||!c||!c->payload_path||c->payload_path[0]!='/'||!index->count||index->count>PACKAGE_INDEX_FILES||
        !memchr(index->recipe,0,sizeof(index->recipe)))return "invalid_package_compatibility";
    if(strcmp(index->recipe,"sherpa-speech-package-v1"))return "unsupported_package_recipe";
    const PackageFile *manifest=NULL;
    for(size_t i=0;i<index->count;i++){
        if(!memchr(index->files[i].name,0,sizeof(index->files[i].name)))return "invalid_package_index";
        if(!strcmp(index->files[i].name,"manifest.json"))manifest=&index->files[i];
    }
    if(!manifest||manifest->role!=PACKAGE_METADATA||!memchr(manifest->sha256,0,sizeof(manifest->sha256)))return "invalid_speech_package_index";
    const char *e=c->control?c->control(c->context):NULL;if(e)return e;
    LibraryRuntimePolicy policy;e=library_runtime_host_policy(&policy);if(e)return e;
    SpeechPackageReceipt *receipt=calloc(1,sizeof(*receipt));if(!receipt)return "allocation_failed";
    /* Reuse the existing complete native manifest and transitive loader graph
     * checks: host architecture/OS, ABI declaration, path closure, dependency
     * versions and unsafe loader environments. No library is loaded here. */
    e=speech_package_verify(root,c->payload_path,manifest->sha256,c->control,c->context,receipt,0);
    if(!e)e=speech_package_loader_verify(root,c->payload_path,manifest->sha256,&policy,c->control,c->context,receipt);
    free(receipt);return e;
}
