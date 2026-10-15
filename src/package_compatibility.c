#include "package_compatibility.h"
#include "speech_package_loader.h"
#include <stdlib.h>
#include <string.h>
int package_recipe_supported(const char *recipe){return recipe&&(!strcmp(recipe,"sherpa-speech-package-v1")||!strcmp(recipe,"llama-gguf-package-v1")||!strcmp(recipe,"whisper-ggml-package-v1")||!strcmp(recipe,"visionpsy-package-v1")||!strcmp(recipe,"molmo2-mlx-package-v1"));}
const char *package_backend_compatible(int root,const PackageIndex *index,void *context){
    PackageCompatibilityContext *c=context;
    if(!index||!c||!c->payload_path||c->payload_path[0]!='/'||!index->count||index->count>PACKAGE_INDEX_FILES||
        !memchr(index->recipe,0,sizeof(index->recipe)))return "invalid_package_compatibility";
    if(!package_recipe_supported(index->recipe))return "unsupported_package_recipe";
    if(!strcmp(index->recipe,"llama-gguf-package-v1")){
        if(!c->llama_policy)return "unqualified_llama_package_runtime";
        LlamaPackageDescriptor observed;return llama_package_verify(root,index,c->llama_policy,c->control,c->context,&observed);
    }
    if(!strcmp(index->recipe,"whisper-ggml-package-v1")){
        if(!c->whisper_policy)return "unqualified_whisper_package_runtime";
        WhisperPackageDescriptor observed;return whisper_package_verify(root,index,c->whisper_policy,c->control,c->context,&observed);
    }
    if(!strcmp(index->recipe,"visionpsy-package-v1")){
        if(!c->vision_policy)return "unqualified_vision_package_runtime";
        VisionPackageDescriptor observed;return vision_package_verify(root,index,c->vision_policy,c->control,c->context,&observed);
    }
    if(!strcmp(index->recipe,"molmo2-mlx-package-v1")){
        if(!c->molmo_policy)return "unqualified_molmo_package_runtime";
        MolmoPackageDescriptor observed;return molmo_package_verify(root,c->payload_path,index,c->molmo_policy,c->control,c->context,&observed);
    }
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
