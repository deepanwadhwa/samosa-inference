#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "decision_host.h"
#include "controlled_pipe.h"
#include "budget.h"
#include "sha256.h"
#include <fcntl.h>
#include <string.h>
#include <unistd.h>
#define DECISION_LIMIT 1048576u
static const char *interrupted(void *context){return inference_interrupted(context);}
static const char *owned(DecisionHost *h,const InferenceConfig *cfg) {
    const char *e=activation_owned_status(&h->activation,h->state,h->engine,h->model,&h->engine_before,&h->model_before);
    if(!e)e=decision_assets_recheck(&h->assets,cfg?interrupted:NULL,(void *)cfg);
    return e;
}
const char *decision_host_status(void *context) {
    DecisionHost *h=context;return !h||!h->qualified?"provider_unavailable":owned(h,NULL);
}
const char *decision_host_init(DecisionHost *h,const InferenceConfig *cfg,size_t engine_size,size_t model_size) {
    memset(h,0,sizeof(*h));h->input=h->output=-1;
    h->activation=(Activation){.process={.guardian=-1,.control=-1,.response=-1},.root=-1,.lease=-1,.engine_fd=-1,.model_fd=-1};
    if(!cfg||!cfg->decision||!cfg->decision->self||!cfg->decision->assets||cfg->decision->profile>DECISION_ASSETS_IMAGE||!cfg->admission_state||!engine_size||!model_size||
        snprintf(h->state,sizeof(h->state),"%s/activation",cfg->admission_state)>=(int)sizeof(h->state)||
        snprintf(h->engine,sizeof(h->engine),"%s/%s",cfg->decision->assets,decision_asset_name(DECISION_ENGINE))>=(int)sizeof(h->engine)||
        snprintf(h->model,sizeof(h->model),"%s/%s",cfg->decision->assets,decision_asset_name(DECISION_MODEL))>=(int)sizeof(h->model)||
        snprintf(h->tokenizer,sizeof(h->tokenizer),"%s/%s",cfg->decision->assets,decision_asset_name(DECISION_TOKENIZER))>=(int)sizeof(h->tokenizer))return "invalid_decision_host_configuration";
    for(unsigned i=0;i<DECISION_ASSET_COUNT;i++)if(!cfg->decision->pins[i]||strlen(cfg->decision->pins[i])!=64||
        strspn(cfg->decision->pins[i],"0123456789abcdef")!=64)return "invalid_decision_host_configuration";
    int n=snprintf(h->bundle,sizeof(h->bundle),"{\"profile\":\"%s\",\"asset_sha256\":[\"%s\",\"%s\",\"%s\",\"%s\"]}",
        cfg->decision->profile==DECISION_ASSETS_IMAGE?"image_decisions":"text_decisions",cfg->decision->pins[0],cfg->decision->pins[1],cfg->decision->pins[2],cfg->decision->pins[3]);
    if(n<0||(size_t)n>=sizeof(h->bundle))return "invalid_decision_host_configuration";
    hash_bytes(h->bundle,(size_t)n,h->identity);h->inference=*cfg;h->engine_size=engine_size;h->model_size=model_size;h->initialized=1;return NULL;
}
static const char *runtime(void *context,int engine,int model) {
    DecisionHost *h=context;const DecisionConfig *d=h->inference.decision;
    const char *e=decision_assets_open_profile(d->assets,d->pins,d->profile,interrupted,&h->inference,&h->assets);
    int admitted[]={engine,model},slots[]={DECISION_ENGINE,DECISION_MODEL};
    for(size_t i=0;!e&&i<2;i++){struct stat a,b;
        if(fstat(admitted[i],&a)||fstat(h->assets.files[slots[i]],&b)||a.st_dev!=b.st_dev||a.st_ino!=b.st_ino)e="artifact_changed";}
    return e;
}
static const char *launch(void *context,const char *self,const char *args,double lifetime,size_t limit,
        int lease,int model,EngineProcess *process) {
    DecisionHost *h=context;int input[2]={-1,-1},output[2]={-1,-1};const char *e=NULL;
    if(pipe(input)||pipe(output))e="engine_io_failed";
    int all[]={input[0],input[1],output[0],output[1]};
    for(size_t i=0;!e&&i<4;i++)if(fcntl(all[i],F_SETFD,FD_CLOEXEC))e="engine_io_failed";
    if(!e)e=decision_assets_recheck(&h->assets,interrupted,&h->inference);
    if(!e)e=engine_process_start_bound_duplex(self,args,lifetime,limit,lease,model,input[0],output[1],process);
    if(input[0]>=0)close(input[0]);if(output[1]>=0)close(output[1]);h->input=input[1];h->output=output[0];
    if(!e&&(fcntl(h->input,F_SETFL,O_NONBLOCK)||fcntl(h->output,F_SETFL,O_NONBLOCK)))e="engine_io_failed";
    return e;
}
const char *decision_host_stop(DecisionHost *h) {
    if(!h||!h->initialized||!h->attempted)return NULL;
    h->qualified=0;const char *e=h->activating?engine_process_stop(&h->activation.process):activation_stop(&h->activation);
    if(e&&!strcmp(e,"engine_shutdown_pending"))return e;
    if(h->input>=0)close(h->input);if(h->output>=0)close(h->output);h->input=h->output=-1;
    if(!h->activating){decision_assets_close(&h->assets);h->attempted=0;}
    return e;
}
static const char *exchange(DecisionHost *h,const InferenceConfig *cfg,const char *payload,const uint8_t *rgba,size_t width,size_t height,char **reply) {
    *reply=NULL;size_t size=strlen(payload);if(!size||size>DECISION_LIMIT)return "invalid_request";
    int image=cfg->decision->profile==DECISION_ASSETS_IMAGE;
    if(image&&(!rgba||!width||width>512||!height||height>512))return "invalid_image";
    InferenceConfig scoped=*cfg;double limit=inference_now()+120;
    if(scoped.deadline_monotonic<=0)scoped.deadline_monotonic=inference_now()+60;
    if(scoped.deadline_monotonic>limit)scoped.deadline_monotonic=limit;
    const char *e=owned(h,&scoped);if(e)return e;
    unsigned char header[24]={0};
    if(image){memcpy(header,"SMIMG001",8);uint32_t values[]={(uint32_t)width,(uint32_t)height,(uint32_t)(width*height*4),(uint32_t)size};
        for(size_t i=0;i<4;i++)for(unsigned k=0;k<4;k++)header[8+i*4+k]=(unsigned char)(values[i]>>(24-k*8));}
    else{for(unsigned k=0;k<4;k++)header[k]=(unsigned char)(size>>(24-k*8));}
    e=controlled_pipe_transfer(h->input,header,image?sizeof(header):4,1,interrupted,&scoped,"engine_io_failed","provider_process_failed");
    if(!e&&image)e=controlled_pipe_transfer(h->input,rgba,width*height*4,1,interrupted,&scoped,"engine_io_failed","provider_process_failed");
    if(!e)e=controlled_pipe_transfer(h->input,payload,size,1,interrupted,&scoped,"engine_io_failed","provider_process_failed");
    if(!e)e=controlled_pipe_transfer(h->output,header,4,0,interrupted,&scoped,"engine_io_failed","provider_process_failed");
    if(!e){size=((size_t)header[0]<<24)|((size_t)header[1]<<16)|((size_t)header[2]<<8)|header[3];
        if(!size||size>DECISION_LIMIT)e="invalid_provider_response";}
    if(!e){*reply=malloc(size+1);if(!*reply)e="resource_limit";}
    if(!e)e=controlled_pipe_transfer(h->output,*reply,size,0,interrupted,&scoped,"engine_io_failed","provider_process_failed");
    if(!e){(*reply)[size]=0;e=decision_result_validate(cfg,payload,(const uint8_t *)*reply,size);}
    if(!e)e=owned(h,&scoped);
    if(e){free(*reply);*reply=NULL;
        /* Typed context refusal is a complete framed answer, not a broken
         * transport. The original persistent worker can serve the next frame. */
        if(strcmp(e,"context_limit")){const char *cleanup=decision_host_stop(h);
            if(cleanup&&!strcmp(cleanup,"engine_shutdown_pending"))return cleanup;}}
    return e;
}
static const char *qualify(void *context) {
    DecisionHost *h=context;
    if(fstat(h->activation.engine_fd,&h->engine_before)||fstat(h->activation.model_fd,&h->model_before))return "artifact_changed";
    const char *payload="{\"method\":\"choice\",\"state\":\"The sky is blue.\",\"instructions\":\"Choose the supported statement.\",\"criteria\":{\"blue\":\"The sky is blue.\",\"red\":\"The sky is red.\"}}";
    const uint8_t rgba[]={255,0,0,255};char *reply=NULL;const char *e=exchange(h,&h->inference,payload,rgba,1,1,&reply);free(reply);return e;
}
const char *decision_host_start(DecisionHost *h,const InferenceConfig *cfg) {
    if(!h||!h->initialized)return "invalid_decision_host_configuration";
    if(h->attempted)return decision_host_status(h);
    h->inference=*cfg;double limit=inference_now()+60;
    if(h->inference.deadline_monotonic<=0||h->inference.deadline_monotonic>limit)h->inference.deadline_monotonic=limit;
    if(mkdir(cfg->admission_state,0700)&&errno!=EEXIST)return "unsafe_decision_state";
    struct stat st;if(lstat(cfg->admission_state,&st)||!S_ISDIR(st.st_mode)||st.st_uid!=geteuid()||(st.st_mode&077))return "unsafe_decision_state";
    char *args=NULL;size_t bytes=0;FILE *f=open_memstream(&args,&bytes);if(!f)return "resource_limit";
    fputs("[\"@ENGINE@\",\"--model\",\"@MODEL@\",\"--tokenizer\",",f);json_quote(f,h->tokenizer,strlen(h->tokenizer));if(cfg->decision->profile==DECISION_ASSETS_IMAGE)fputs(",\"--resident-images-v1\"",f);fputc(']',f);
    if(fclose(f)){free(args);return "resource_limit";}
    ActivationConfig activation={.state=h->state,.self=cfg->decision->self,.engine=h->engine,.model=h->model,
        .engine_sha=cfg->decision->pins[DECISION_ENGINE],.model_sha=cfg->decision->pins[DECISION_MODEL],
        .engine_size=h->engine_size,.model_size=h->model_size,.byte_ceiling=cfg->decision->profile==DECISION_ASSETS_IMAGE?2147483648ULL:1073741824,.advance_current=1,
        .lifetime=86400,.log_limit=1048576,.argv_template=args,.launch=launch,.launch_context=h,
        .verify_runtime=runtime,.runtime_context=h,.bundle_identity=h->identity,.bundle_config=h->bundle,.qualify=qualify,.qualification=h};
    h->attempted=1;h->activating=1;const char *e=activation_start(&activation,&h->activation);h->activating=0;free(args);
    if(!e){h->qualified=1;e=decision_host_status(h);}
    if(e){const char *cleanup=decision_host_stop(h);if(cleanup&&!strcmp(cleanup,"engine_shutdown_pending"))return cleanup;}
    return e;
}
const char *decision_host_run(void *context,const InferenceConfig *cfg,const char *payload,char **reply) {
    DecisionHost *h=context;if(!h||!h->qualified)return "provider_unavailable";
    return exchange(h,cfg,payload,NULL,0,0,reply);
}

const char *decision_host_run_image(void *context,const InferenceConfig *cfg,const char *payload,
        const uint8_t *rgba,size_t width,size_t height,char **reply) {
    DecisionHost *h=context;if(!h||!h->qualified)return "provider_unavailable";
    return exchange(h,cfg,payload,rgba,width,height,reply);
}
