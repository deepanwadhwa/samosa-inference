#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "managed_provider.h"
#include "sha256.h"
#include "embedding.h"
#include <stdio.h>
#include <ctype.h>
#include <unistd.h>
#include <poll.h>
#include <time.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
static int unchanged(int fd,const char *path,const struct stat *before){
    struct stat current,named;
    if(fstat(fd,&current)||lstat(path,&named)||current.st_dev!=before->st_dev||current.st_ino!=before->st_ino||
        named.st_dev!=current.st_dev||named.st_ino!=current.st_ino||current.st_size!=before->st_size||
        current.st_mode!=before->st_mode)return 0;
#ifdef __APPLE__
    return current.st_mtimespec.tv_sec==before->st_mtimespec.tv_sec&&current.st_mtimespec.tv_nsec==before->st_mtimespec.tv_nsec&&
        current.st_ctimespec.tv_sec==before->st_ctimespec.tv_sec&&current.st_ctimespec.tv_nsec==before->st_ctimespec.tv_nsec;
#else
    return current.st_mtim.tv_sec==before->st_mtim.tv_sec&&current.st_mtim.tv_nsec==before->st_mtim.tv_nsec&&
        current.st_ctim.tv_sec==before->st_ctim.tv_sec&&current.st_ctim.tv_nsec==before->st_ctim.tv_nsec;
#endif
}
const char *managed_provider_status(void *opaque){
    ManagedProvider *p=opaque;
    if(!p||!p->qualified||p->activation.process.response<0)return "provider_unavailable";
    struct pollfd terminal={p->activation.process.response,POLLIN|POLLHUP,0};
    if(poll(&terminal,1,0)!=0)return "owned_engine_exited";
    struct stat root,named,lease,current;
    if(fstat(p->activation.root,&root)||lstat(p->state_path,&named)||root.st_dev!=named.st_dev||root.st_ino!=named.st_ino||
        root.st_uid!=geteuid()||(root.st_mode&077)||fstat(p->activation.lease,&lease)||
        fstatat(p->activation.root,"residency.lock",&current,AT_SYMLINK_NOFOLLOW)||lease.st_dev!=current.st_dev||lease.st_ino!=current.st_ino||
        lease.st_nlink!=1||lease.st_uid!=geteuid()||(lease.st_mode&077))return "provider_state_changed";
    if(!unchanged(p->activation.engine_fd,p->engine_path,&p->engine)||!unchanged(p->activation.model_fd,p->model_path,&p->model))
        return "provider_artifact_changed";
    return NULL;
}
static int monotonic(double *value){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return 0;*value=(double)t.tv_sec+(double)t.tv_nsec/1e9;return 1;}
const char *managed_provider_identity(const ActivationConfig *a,const InferenceConfig *i,char identity[100]){
    if(!a||!i||!a->require_system_runtime||!a->engine_sha||!a->model_sha||strlen(a->engine_sha)!=64||strlen(a->model_sha)!=64||
        !i->model_alias||strlen(i->model_alias)>128||!i->context_tokens)return "invalid_managed_configuration";
    for(unsigned n=0;n<64;n++)if(!((a->engine_sha[n]>='0'&&a->engine_sha[n]<='9')||(a->engine_sha[n]>='a'&&a->engine_sha[n]<='f'))||
        !((a->model_sha[n]>='0'&&a->model_sha[n]<='9')||(a->model_sha[n]>='a'&&a->model_sha[n]<='f')))return "invalid_managed_configuration";
    char binding[512],digest[65];
#ifdef __APPLE__
    const char *platform="macos-arm64";
#else
    const char *platform="linux-arm64";
#endif
    int n=snprintf(binding,sizeof(binding),"llama-static-system-v1:%s:system-runtime-v1:llama-native-template-v1:%s:%llu:%s:%llu:%zu:%s",platform,
        a->engine_sha,(unsigned long long)a->engine_size,a->model_sha,(unsigned long long)a->model_size,i->context_tokens,i->model_alias);
    if(n<0||(size_t)n>=sizeof(binding))return "invalid_managed_configuration";
    hash_bytes(binding,(size_t)n,digest);snprintf(identity,100,"managed-text-%s",digest);return NULL;
}
static const char *qualify(void *opaque){
    ManagedProvider *p=opaque;InferenceConfig probe=p->inference;probe.owned_status=NULL;probe.transfer_timeout_ms=1000;
    double start,now;if(!monotonic(&start))return "provider_clock_failed";
    for(;;){
        struct pollfd terminal={p->activation.process.response,POLLIN|POLLHUP,0};
        if(poll(&terminal,1,0)!=0)return "owned_engine_exited";
        if(probe.cancelled&&probe.cancelled(probe.cancellation))return "cancelled";
        const char *e=llama_properties(&probe);
        if(!e){InferenceConfig wrong=probe;wrong.api_key="wrong-managed-key";
            if(!strcmp(probe.api_key,wrong.api_key))wrong.api_key="another-wrong-managed-key";
            e=llama_properties(&wrong);
            if(!e||strcmp(e,"provider_auth_failed"))return "unqualified_endpoint";
            size_t tokens=0;e=llama_count(&probe,"x",&tokens);
            if(e)return e;
            if(!tokens||tokens>probe.context_tokens)return "invalid_provider_response";
            if(probe.embedding){float *vector=NULL;size_t embedded_tokens=0;
                e=llama_embed(&probe,"task: search result | query: runtime readiness",probe.embedding->dimensions,&vector,&embedded_tokens);free(vector);
                if(e)return e;
                if(!embedded_tokens)return "invalid_provider_response";}
            if(fstat(p->activation.engine_fd,&p->engine)||fstat(p->activation.model_fd,&p->model))return "provider_artifact_changed";
            return NULL;}
        if(strcmp(e,"provider_unavailable")&&strcmp(e,"provider_http_error")&&strcmp(e,"deadline_exceeded"))return e;
        if(!monotonic(&now))return "provider_clock_failed";
        if(now-start>=120)return "provider_startup_deadline";
        struct timespec pause={0,100000000};while(nanosleep(&pause,&pause)&&errno==EINTR){}
    }
}
const char *managed_provider_start(const ActivationConfig *activation,const InferenceConfig *inference,ManagedProvider *p){
    memset(p,0,sizeof(*p));p->activation=(Activation){.process={.guardian=-1,.control=-1,.response=-1},.root=-1,.lease=-1,.engine_fd=-1,.model_fd=-1};
    if(!activation||!inference||!activation->require_system_runtime||!inference->api_key||!inference->api_key[0]||
        !inference->endpoint||!inference->model_alias||!inference->provider_id||!inference->admission_state||!inference->owner)
        return "invalid_managed_configuration";
    if(inference->embedding&&(!inference->embedding->engine_sha256||!inference->embedding->model_sha256||
        !activation->engine_sha||!activation->model_sha||strcmp(inference->embedding->engine_sha256,activation->engine_sha)||
        strcmp(inference->embedding->model_sha256,activation->model_sha)))return "managed_embedding_pin_mismatch";
    char identity[100];const char *identity_error=managed_provider_identity(activation,inference,identity);
    if(identity_error)return identity_error;
    if(strcmp(identity,inference->provider_id))return "managed_provider_identity_mismatch";
    p->inference=*inference;p->inference.owned_status=NULL;p->inference.owned_context=NULL;
    p->engine_path=activation->engine;p->model_path=activation->model;p->state_path=activation->state;
    ActivationConfig candidate=*activation;candidate.qualify=qualify;candidate.qualification=p;
    const char *e=activation_start(&candidate,&p->activation);
    if(e)return e;
    p->qualified=1;p->inference.owned_status=managed_provider_status;p->inference.owned_context=p;
    e=managed_provider_status(p);
    if(e)(void)managed_provider_stop(p);
    return e;
}
const char *managed_provider_stop(ManagedProvider *p){p->qualified=0;return activation_stop(&p->activation);}
