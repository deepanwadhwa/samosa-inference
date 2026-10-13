#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "registry_host.h"
#include "engine_process.h"
#include <limits.h>
#include <signal.h>

int main(int argc,char **argv) {
    int bridge=summary_bridge_dispatch(argc,argv);if(bridge>=0)return bridge;
    bridge=vision_bridge_dispatch(argc,argv);if(bridge>=0)return bridge;
    int guardian=engine_guardian_dispatch(argc,argv);if(guardian>=0)return guardian;
    if(argc!=3||strcmp(argv[1],"--config")){fputs("usage: samosa-inference-providers --config ABSOLUTE_PRIVATE_JSON\n",stderr);return 2;}
    char self[PATH_MAX];if(!realpath(argv[0],self))return 2;
    InferenceRegistryHost host;const char *error=inference_registry_host_load(argv[2],self,&host);
    if(error){fprintf(stdout,"{\"contract_version\":1,\"error\":{\"code\":\"%s\"}}\n",error);return 2;}
    signal(SIGPIPE,SIG_IGN);
    char *request=malloc(INFERENCE_REQUEST_MAX+1);if(!request){inference_registry_host_free(&host);return 2;}
    size_t bytes=fread(request,1,INFERENCE_REQUEST_MAX+1,stdin);
    int rc=ferror(stdin)?2:inference_registry_dispatch(&host.registry,request,bytes,stdout);
    free(request);inference_registry_host_free(&host);return rc;
}
