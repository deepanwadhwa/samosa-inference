#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "inference.h"
#include "embedding.h"
#include "engine_process.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <ctype.h>
static int key_file(const char *path,char key[130]) {
    if(!path||path[0]!='/')return 0;
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);if(fd<0)return 0;
    struct stat s;int valid=!fstat(fd,&s)&&S_ISREG(s.st_mode)&&s.st_uid==geteuid()&&!(s.st_mode&077)&&s.st_size>0&&s.st_size<=129;
    ssize_t n=valid?read(fd,key,129):-1;close(fd);if(n<1)return 0;
    if(key[n-1]=='\n')n--;
    if(n<1||n>128)return 0;
    key[n]=0;
    for(ssize_t i=0;i<n;i++)if(!key[i]||(!isalnum((unsigned char)key[i])&&!strchr("-_.",key[i])))return 0;
    return 1;
}
int main(int argc,char **argv) {
    int guardian=engine_guardian_dispatch(argc,argv);if(guardian>=0)return guardian;
    InferenceConfig cfg={.context_tokens=131072,.provider_id="fake-text-v1",.owner="local"};
    int configured=0,context_given=0,provider_given=0;
    EmbeddingConfig embedding={0};const char *embedding_path=NULL;
    char key[130];const char *key_path=NULL;
    for(int i=1;i<argc;i++) {
        if(!strcmp(argv[i],"--llama-endpoint")&&i+1<argc){cfg.endpoint=argv[++i];configured=1;}
        else if(!strcmp(argv[i],"--embedding-config-file")&&i+1<argc){embedding_path=argv[++i];configured=1;}
        else if(!strcmp(argv[i],"--api-key-file")&&i+1<argc)key_path=argv[++i];
        else if(!strcmp(argv[i],"--provider-id")&&i+1<argc){cfg.provider_id=argv[++i];provider_given=1;configured=1;}
        else if(!strcmp(argv[i],"--admission-state")&&i+1<argc){cfg.admission_state=argv[++i];configured=1;}
        else if(!strcmp(argv[i],"--model-alias")&&i+1<argc)cfg.model_alias=argv[++i];
        else if(!strcmp(argv[i],"--owner")&&i+1<argc){cfg.owner=argv[++i];configured=1;}
        else if((!strcmp(argv[i],"--queue-capacity")||!strcmp(argv[i],"--admission-wait-ms"))&&i+1<argc){
            int capacity=!strcmp(argv[i],"--queue-capacity");char *end;errno=0;
            unsigned long n=strtoul(argv[++i],&end,10);if(errno||*end||end==argv[i]||n>(capacity?8u:60000u)||(!capacity&&!n))return 2;
            if(capacity)cfg.queue_capacity=(size_t)n;else cfg.admission_wait_ms=(size_t)n;configured=1;
        }
        else if(!strcmp(argv[i],"--context-tokens")&&i+1<argc) {
            char *end;errno=0;unsigned long n=strtoul(argv[++i],&end,10);if(errno||*end||n<128||n>131072)return 2;cfg.context_tokens=(size_t)n;context_given=1;configured=1;
        } else {fputs("usage: samosa-inference [--admission-state PRIVATE_DIR --owner ID] [--llama-endpoint LOOPBACK_URL --provider-id ID --model-alias ALIAS --context-tokens N]\n",stderr);return 2;}
    }
    if(cfg.endpoint&&(!provider_given||!cfg.admission_state||!cfg.model_alias))return 2;
    if(!cfg.endpoint&&cfg.model_alias)return 2;
    if(cfg.endpoint&&!context_given)cfg.context_tokens=4096;
    if(key_path){if(!cfg.endpoint||!key_file(key_path,key))return 2;cfg.api_key=key;}
    if(embedding_path){if(!cfg.endpoint||!cfg.admission_state||!embedding_config_load(embedding_path,&embedding))return 2;cfg.embedding=&embedding;}
    char *request=malloc(INFERENCE_REQUEST_MAX+1);if(!request)return 2;
    size_t n=fread(request,1,INFERENCE_REQUEST_MAX+1,stdin);
    int rc=ferror(stdin)?2:inference_dispatch_config(configured?&cfg:NULL,request,n,stdout);free(request);embedding_config_free(&embedding);return rc;
}
