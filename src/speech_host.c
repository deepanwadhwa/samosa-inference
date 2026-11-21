#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "speech_host.h"
#include "speech_package_loader.h"
#include "budget.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
static const char *interrupted(void *context){return inference_interrupted(context);}
static const char *owned(SpeechHost *h) {
    const SpeechConfig *s=h->inference.speech;
    const char *e=activation_owned_status(&h->activation,h->state,s->self,h->manifest,&h->worker_before,&h->manifest_before);
    if(!e)e=speech_package_verify(h->package_fd,s->package,s->manifest_sha256,NULL,NULL,&h->package,1);
    return e;
}
const char *speech_host_status(void *context) {
    SpeechHost *h=context;if(!h||!h->qualified)return "provider_unavailable";
    return owned(h);
}
const char *speech_host_init(SpeechHost *h,const InferenceConfig *cfg,const char *worker_pin,size_t worker_size,size_t manifest_size) {
    memset(h,0,sizeof(*h));h->package_fd=-1;speech_resident_init(&h->transport);
    h->activation=(Activation){.process={.guardian=-1,.control=-1,.response=-1},.root=-1,.lease=-1,.engine_fd=-1,.model_fd=-1};
    if(!cfg||!cfg->speech||!cfg->admission_state||!worker_pin||strlen(worker_pin)!=64||
        strspn(worker_pin,"0123456789abcdef")!=64||!worker_size||!manifest_size||
        snprintf(h->state,sizeof(h->state),"%s/activation",cfg->admission_state)>=(int)sizeof(h->state)||
        snprintf(h->manifest,sizeof(h->manifest),"%s/manifest.json",cfg->speech->package)>=(int)sizeof(h->manifest))return "invalid_speech_host_configuration";
    h->inference=*cfg;strcpy(h->worker_pin,worker_pin);h->worker_size=worker_size;h->manifest_size=manifest_size;h->initialized=1;return NULL;
}
static const char *launch(void *context,const char *self,const char *rendered,double lifetime,size_t log_limit,
        int lease,int manifest,EngineProcess *process) {
    (void)rendered;(void)manifest;SpeechHost *h=context;const SpeechConfig *s=h->inference.speech;
    h->package_fd=open(s->package,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(h->package_fd<0)return "speech_package_unavailable";
    const char *e=speech_package_verify(h->package_fd,s->package,s->manifest_sha256,interrupted,&h->inference,&h->package,0);
    LibraryRuntimePolicy policy;if(!e)e=library_runtime_host_policy(&policy);
    if(!e)e=speech_package_loader_verify(h->package_fd,s->package,s->manifest_sha256,&policy,interrupted,&h->inference,&h->package);
    if(!e){SpeechProcessConfig cfg={.self=self,.package_path=s->package,.manifest_sha256=s->manifest_sha256,
        .package_fd=h->package_fd,.residency_fd=lease,.lifetime_seconds=lifetime,.log_limit=log_limit};
        h->transport.owner=process;e=speech_resident_start(&cfg,&h->transport);}
    return e;
}
static const char *qualify(void *context) {
    SpeechHost *h=context;
    if(fstat(h->activation.engine_fd,&h->worker_before)||fstat(h->activation.model_fd,&h->manifest_before))return "provider_artifact_changed";
    FILE *spool=tmpfile();if(!spool)return "resource_limit";
    const char *request="{\"text\":\"The meeting starts at nine. Please bring your notes.\"}";
    SpeechRuntimeResult result;const char *e=speech_resident_run(&h->transport,request,strlen(request),fileno(spool),NULL,NULL,
        interrupted,&h->inference,&result);
    if(!e&&(!result.samples||result.sample_rate<8000||!result.pcm_sha256[0]))e="invalid_speech_output";
    if(!e)e=owned(h);fclose(spool);return e;
}
const char *speech_host_stop(SpeechHost *h) {
    if(!h||!h->initialized||!h->attempted)return NULL;
    h->qualified=0;const char *e=speech_resident_stop(&h->transport);
    if(e&&!strcmp(e,"engine_shutdown_pending"))return e;
    const char *released=activation_stop(&h->activation);
    if(released&&!strcmp(released,"engine_shutdown_pending"))return released;
    if(h->package_fd>=0)close(h->package_fd);h->package_fd=-1;h->attempted=0;
    return e?e:released;
}
const char *speech_host_start(SpeechHost *h,const InferenceConfig *cfg) {
    if(!h||!h->initialized)return "invalid_speech_host_configuration";
    if(h->attempted)return speech_host_status(h);
    h->inference=*cfg;double limit=inference_now()+60;
    if(h->inference.deadline_monotonic<=0||h->inference.deadline_monotonic>limit)h->inference.deadline_monotonic=limit;
    if(mkdir(cfg->admission_state,0700)&&errno!=EEXIST)return "unsafe_speech_state";
    struct stat st;if(lstat(cfg->admission_state,&st)||!S_ISDIR(st.st_mode)||st.st_uid!=geteuid()||(st.st_mode&077))return "unsafe_speech_state";
    const SpeechConfig *s=cfg->speech;char *args=NULL;size_t bytes=0;FILE *f=open_memstream(&args,&bytes);if(!f)return "resource_limit";
    fputs("[\"@ENGINE@\",\"--inference-speech-resident-worker\",\"@MODEL@\",",f);json_quote(f,s->manifest_sha256,64);fputc(']',f);
    if(fclose(f)){free(args);return "resource_limit";}
    ActivationConfig activation={.state=h->state,.self=s->self,.engine=s->self,.model=h->manifest,
        .engine_sha=h->worker_pin,.model_sha=s->manifest_sha256,.engine_size=h->worker_size,.model_size=h->manifest_size,
        .byte_ceiling=536870912,.advance_current=1,.lifetime=86400,.log_limit=1048576,.trusted_host_runtime=1,
        .argv_template=args,.model_argument=s->package,.launch=launch,.launch_context=h,.qualify=qualify,.qualification=h};
    h->attempted=1;h->activating=1;const char *e=activation_start(&activation,&h->activation);h->activating=0;free(args);
    if(!e){h->qualified=1;e=speech_host_status(h);}
    if(e){const char *cleanup=speech_host_stop(h);if(cleanup&&!strcmp(cleanup,"engine_shutdown_pending"))return cleanup;}
    return e;
}
const char *speech_host_run(void *context,const InferenceConfig *cfg,const char *request,size_t bytes,int spool,
        SpeechPcmSink sink,void *sink_context,SpeechRuntimeResult *result) {
    SpeechHost *h=context;const char *e=speech_host_status(h);if(e)return e;
    e=speech_resident_run(&h->transport,request,bytes,spool,sink,sink_context,interrupted,(void *)cfg,result);
    if(!e)e=speech_host_status(h);
    if(e){memset(result,0,sizeof(*result));const char *cleanup=speech_host_stop(h);
        if(cleanup&&!strcmp(cleanup,"engine_shutdown_pending"))return cleanup;}
    return e;
}
