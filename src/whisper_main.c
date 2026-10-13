#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "inference.h"
#include "whisper.h"
#include "engine_process.h"
#include <limits.h>
#include <errno.h>
static int sha(const char *s){if(!s||strlen(s)!=64)return 0;for(size_t i=0;i<64;i++)if(!strchr("0123456789abcdef",s[i]))return 0;return 1;}
int main(int argc,char **argv){
    int guardian=engine_guardian_dispatch(argc,argv);if(guardian>=0)return guardian;
    char self[PATH_MAX];if(!realpath(argv[0],self))return 2;
    WhisperConfig whisper={.self=self};InferenceConfig cfg={.provider_id="whisper-base-en-v1",.context_tokens=131072,.owner="local",.whisper=&whisper};
    for(int i=1;i<argc;i++){
        if(i+1>=argc)return 2;
        const char *flag=argv[i],*value=argv[++i];
        if(!strcmp(flag,"--whisper-cli"))whisper.executable=value;
        else if(!strcmp(flag,"--whisper-model"))whisper.model=value;
        else if(!strcmp(flag,"--whisper-cli-sha256"))whisper.executable_sha256=value;
        else if(!strcmp(flag,"--whisper-model-sha256"))whisper.model_sha256=value;
        else if(!strcmp(flag,"--provider-id"))cfg.provider_id=value;
        else if(!strcmp(flag,"--admission-state"))cfg.admission_state=value;
        else if(!strcmp(flag,"--owner"))cfg.owner=value;
        else if(!strcmp(flag,"--queue-capacity")||!strcmp(flag,"--admission-wait-ms")){char *end;errno=0;unsigned long n=strtoul(value,&end,10);int queue=!strcmp(flag,"--queue-capacity");if(errno||end==value||*end||n>(queue?8u:60000u)||(!queue&&!n))return 2;if(queue)cfg.queue_capacity=(size_t)n;else cfg.admission_wait_ms=(size_t)n;}
        else return 2;
    }
    if(!whisper.executable||whisper.executable[0]!='/'||!whisper.model||whisper.model[0]!='/'||!sha(whisper.executable_sha256)||!sha(whisper.model_sha256)||!cfg.admission_state||cfg.admission_state[0]!='/')return 2;
    char *request=malloc(INFERENCE_REQUEST_MAX+1);if(!request)return 2;size_t n=fread(request,1,INFERENCE_REQUEST_MAX+1,stdin);
    int rc=ferror(stdin)?2:inference_dispatch_config(&cfg,request,n,stdout);free(request);return rc;
}
