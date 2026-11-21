#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "summary_host.h"
#include "summary_protocol.h"
#include "budget.h"
#include "admission.h"
#include <fcntl.h>
#include <poll.h>
#include <unistd.h>
static const char *interrupted(void *context){return inference_interrupted(context);}
const char *summary_host_status(void *context){
    SummaryHost *h=context;if(!h||!h->qualified)return "provider_unavailable";
    const SummaryConfig *s=h->inference.summary;
    return activation_owned_status(&h->activation,h->state,s->executable,s->model,&h->engine_before,&h->model_before);
}
static const char *transport_open(void *context,const SamosaSummaryProcessConfig *config,
        const SamosaSummaryControl *control,int *write_fd,int *read_fd){
    (void)config;SummaryHost *h=context;
    if(control&&control->check){const char *e=control->check(control->context);if(e)return e;}
    const SummaryConfig *s=h->inference.summary;
    const char *error=h->activating?activation_owned_status(&h->activation,h->state,s->executable,s->model,&h->engine_before,&h->model_before):summary_host_status(h);
    if(error)return error;
    *write_fd=h->input;*read_fd=h->output;return NULL;
}
static const char *transport_close(void *context){
    SummaryHost *h=context;h->qualified=0;
    if(h->attempted){
        /* Qualification still owns the activation transaction/lease. Reap a
         * broken warm worker here, leaving that store fence for activation's
         * normal failure cleanup after the qualifier returns. */
        const char *error=h->activating?engine_process_stop(&h->activation.process):activation_stop(&h->activation);
        if(error&&!strcmp(error,"engine_shutdown_pending"))return error;
        h->terminal=error;if(!h->activating)h->attempted=0;
    }
    if(h->input>=0)close(h->input);if(h->output>=0)close(h->output);
    h->input=h->output=-1;return NULL;
}
const char *summary_host_init(SummaryHost *h,const InferenceConfig *cfg,size_t engine_size,size_t model_size){
    memset(h,0,sizeof(*h));h->input=h->output=-1;
    h->activation=(Activation){.process={.guardian=-1,.control=-1,.response=-1},.root=-1,.lease=-1,.engine_fd=-1,.model_fd=-1};
    if(!cfg||!cfg->summary||!cfg->admission_state||!engine_size||!model_size||
       snprintf(h->state,sizeof(h->state),"%s/activation",cfg->admission_state)>=(int)sizeof(h->state))return "invalid_summary_host_configuration";
    h->inference=*cfg;h->engine_size=engine_size;h->model_size=model_size;
    if(!samosa_summary_process_init(&h->transport))return "summary_resource_limit";
    h->initialized=1;SamosaSummaryLifecycle lifecycle={transport_open,transport_close,h};
    return samosa_summary_process_bind(&h->transport,&lifecycle)?NULL:"summary_resource_limit";
}
static const char *launch(void *context,const char *self,const char *rendered,double lifetime,size_t limit,
                           int lease,int model,EngineProcess *process){
    (void)rendered;SummaryHost *h=context;const SummaryConfig *s=h->inference.summary;
    int input[2]={-1,-1},output[2]={-1,-1};const char *error=NULL;
    if(pipe(input)||pipe(output))error="engine_io_failed";
    int all[]={input[0],input[1],output[0],output[1]};
    for(size_t i=0;!error&&i<4;i++)if(fcntl(all[i],F_SETFD,FD_CLOEXEC))error="engine_io_failed";
    char *args=NULL;size_t bytes=0;FILE *out=!error?open_memstream(&args,&bytes):NULL;
    if(!error&&!out)error="summary_resource_limit";
    if(out){
        const char *argv[]={self,"--summary-resident-bridge",s->executable,s->executable_sha256,"256"};
        fputc('[',out);for(size_t i=0;i<5;i++){if(i)fputc(',',out);json_quote(out,argv[i],strlen(argv[i]));}
        fputc(']',out);if(fclose(out))error="summary_resource_limit";
    }
    if(!error)error=engine_process_start_bound_duplex(self,args,lifetime,limit,lease,model,input[0],output[1],process);
    free(args);if(input[0]>=0)close(input[0]);if(output[1]>=0)close(output[1]);
    h->input=input[1];h->output=output[0];return error;
}
static const char *request(SummaryHost *h,const InferenceConfig *cfg,const char *text,size_t tokens,int opening,char **result){
    const char *error=inference_interrupted(cfg);int lease=-1;
    if(!error&&!h->activating)error=inference_schedule(cfg,&lease);
    size_t bytes=0;unsigned char *frame=error?NULL:summary_budget_frame(text,strlen(text),tokens,opening,&bytes);
    if(!error&&!frame)error="invalid_request";
    if(!error){
        SamosaSummaryControl control={.deadline_ms=cfg->deadline_monotonic>0?(long long)(cfg->deadline_monotonic*1000):0,.check=interrupted,.context=(void *)cfg};
        const SummaryConfig *s=cfg->summary;SamosaSummaryProcessConfig transport={s->executable,s->model,"guardian","0"};
        *result=samosa_summary_process_frame_controlled(&h->transport,&transport,frame,bytes,&control,&error);
        if(error){
            if(!strcmp(error,"invalid_summary_reply"))error="invalid_provider_response";
            else if(!strcmp(error,"summary_io_failed")||!strcmp(error,"summary_protocol_failed")||!strcmp(error,"summary_engine_unavailable"))error="provider_process_failed";
            else if(!strcmp(error,"summary_resource_limit"))error="resource_limit";
        }
        if(!error)error=inference_interrupted(cfg);
        if(error&&*result){free(*result);*result=NULL;}
    }
    free(frame);if(lease>=0)close(lease);return error;
}
static const char *qualify(void *context){
    SummaryHost *h=context;
    if(fstat(h->activation.engine_fd,&h->engine_before)||fstat(h->activation.model_fd,&h->model_before))return "provider_artifact_changed";
    InferenceConfig cfg=h->inference;double limit=inference_now()+60;
    if(cfg.deadline_monotonic<=0||cfg.deadline_monotonic>limit)cfg.deadline_monotonic=limit;
    char *reply=NULL;const char *error=request(h,&cfg,"The pinned native summary engine must summarize this bounded readiness calibration.",16,0,&reply);
    if(!error&&(!reply||!*reply))error="invalid_provider_response";free(reply);return error;
}
const char *summary_host_start(SummaryHost *h,const InferenceConfig *cfg){
    if(!h->initialized)return "invalid_summary_host_configuration";
    if(h->attempted)return summary_host_status(h);
    h->inference=*cfg;h->terminal=NULL;
    if((mkdir(cfg->admission_state,0700)&&errno!=EEXIST))return "unsafe_summary_state";
    struct stat st;if(lstat(cfg->admission_state,&st)||!S_ISDIR(st.st_mode)||st.st_uid!=geteuid()||(st.st_mode&077))return "unsafe_summary_state";
    const SummaryConfig *s=cfg->summary;
    ActivationConfig activation={.state=h->state,.self=s->self,.engine=s->executable,.model=s->model,
        .engine_sha=s->executable_sha256,.model_sha=s->model_sha256,.engine_size=h->engine_size,.model_size=h->model_size,
        .byte_ceiling=536870912,.advance_current=1,.lifetime=86400,.log_limit=1048576,.require_system_runtime=1,
        .argv_template="[\"@ENGINE@\",\"--model\",\"@MODEL@\",\"--max-tokens\",\"256\",\"--batch-size\",\"1\",\"--threads\",\"2\",\"--gpu-layers\",\"0\"]",
        .qualify=qualify,.qualification=h,.launch=launch,.launch_context=h};
    h->attempted=1;h->activating=1;const char *error=activation_start(&activation,&h->activation);h->activating=0;
    if(!error){h->qualified=1;error=summary_host_status(h);}
    if(error){const char *cleanup=summary_host_stop(h);if(cleanup&&!strcmp(cleanup,"engine_shutdown_pending"))return cleanup;}
    return error;
}
const char *summary_host_run(void *context,const InferenceConfig *cfg,const char *text,size_t tokens,int opening,char **result){
    SummaryHost *h=context;const char *error=summary_host_status(h);if(error)return error;
    error=request(h,cfg,text,tokens,opening,result);
    if(!error)error=summary_host_status(h);
    if(error&&*result){free(*result);*result=NULL;}return error;
}
const char *summary_host_stop(SummaryHost *h){
    if(!h->initialized)return NULL;
    const char *error=samosa_summary_process_close(&h->transport);if(error)return error;
    error=h->terminal;h->terminal=NULL;return error;
}
const char *summary_host_close(SummaryHost *h){
    const char *error=summary_host_stop(h);if(error&&h->attempted)return error;
    if(h->initialized)samosa_summary_process_destroy(&h->transport);h->initialized=0;return error;
}
