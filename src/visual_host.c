#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "visual_host.h"
#include "admission.h"
#include "budget.h"
#include "engine_runtime.h"
#include "sha256.h"
#include <errno.h>
#include <fcntl.h>
#include <time.h>
#include <unistd.h>

static const char *interrupted(void *context) { return inference_interrupted(context); }
static const char *model_path(const VisualHost *h) {
    return h->temporal ? h->inference.temporal->model : h->inference.vision->model;
}
static const char *engine_path(const VisualHost *h) {
    return h->temporal ? h->inference.temporal->executable : h->inference.vision->executable;
}
static const char *engine_pin(const VisualHost *h) {
    return h->temporal ? h->inference.temporal->executable_sha256 : h->inference.vision->executable_sha256;
}
static const char *assets(VisualHost *h,const InferenceConfig *cfg,int recheck) {
    const char *error=h->temporal ? temporal_package_verify(h->model,model_path(h),cfg->temporal->manifest_sha256,
                         interrupted,(void *)cfg,&h->temporal_receipt,recheck)
                       : vision_model_verify(cfg,h->model,&h->image_receipt,recheck);
    if(!error)error=h->temporal?temporal_engine_verify(cfg,h->activation.engine_fd,&h->engine_before,recheck)
        :vision_file_verify(cfg,h->activation.engine_fd,engine_pin(h),67108864u,1);
    return error;
}
static const char *owned(VisualHost *h) {
    const char *error=activation_owned_status(&h->activation,h->state,engine_path(h),h->anchor,
                                              &h->engine_before,&h->anchor_before);
    struct stat held,named;
    const struct stat *expected=h->temporal?&h->temporal_receipt.directory:&h->image_receipt.directory;
    if(!error&&(fstat(h->model,&held)||lstat(model_path(h),&named)||
       !visual_same_receipt(&held,expected)||!visual_same_receipt(&held,&named)))error="provider_artifact_changed";
    if(!error&&h->temporal){
        if(fstatat(h->model,"manifest.json",&named,AT_SYMLINK_NOFOLLOW)||
           !visual_same_receipt(&named,&h->temporal_receipt.manifest))error="provider_artifact_changed";
        for(size_t i=0;!error&&i<h->temporal_receipt.count;i++)
            if(fstatat(h->model,h->temporal_receipt.files[i].name,&named,AT_SYMLINK_NOFOLLOW)||
               !visual_same_receipt(&named,&h->temporal_receipt.files[i].identity))error="provider_artifact_changed";
    }else if(!error){
        const char *names[]={"config.json","tokenizer.json","model.safetensors"};
        for(size_t i=0;!error&&i<3;i++)if(fstatat(h->model,names[i],&named,AT_SYMLINK_NOFOLLOW)||
            !visual_same_receipt(&named,&h->image_receipt.files[i]))error="provider_artifact_changed";
    }
    return error;
}
const char *visual_host_status(void *context) {
    VisualHost *h=context;
    if(!h||!h->initialized||!atomic_load(&h->qualified))return "provider_unavailable";
    int locked=pthread_mutex_trylock(&h->mutex);
    /* Active calls recheck identity under the same mutex before and after the
       exchange. Do not turn another caller's queue admission into a wait here. */
    if(locked==EBUSY)return atomic_load(&h->qualified)?NULL:"provider_unavailable";
    if(locked)return "resource_limit";
    const char *error=owned(h);pthread_mutex_unlock(&h->mutex);return error;
}
static void remove_inputs(VisualHost *h) {
    if(h->directory<0)return;
    (void)unlinkat(h->directory,"input.ppm",0);
    for(size_t k=0;k<16;k++){char name[32];snprintf(name,sizeof(name),"frame-%02zu.ppm",k);(void)unlinkat(h->directory,name,0);}
}
static const char *transport_close(void *context) {
    VisualHost *h=context;atomic_store(&h->qualified,0);
    if(atomic_load(&h->attempted)){
        const char *error=h->activating?engine_process_stop(&h->activation.process):activation_stop(&h->activation);
        if(error&&!strcmp(error,"engine_shutdown_pending"))return error;
        h->terminal=error;if(!h->activating)atomic_store(&h->attempted,0);
    }
    if(h->input>=0)close(h->input);if(h->output>=0)close(h->output);h->input=h->output=-1;
    remove_inputs(h);
    if(h->directory>=0)close(h->directory);h->directory=-1;
    if(h->workspace[0]){(void)rmdir(h->workspace);h->workspace[0]=0;}
    if(h->model>=0)close(h->model);h->model=-1;
    return NULL;
}
static const char *stop_locked(VisualHost *h) {
    const char *error=samosa_mm_session_close_owned(&h->session);if(error)return error;
    if(atomic_load(&h->attempted)){error=transport_close(h);if(error)return error;}
    error=h->terminal;h->terminal=NULL;return error;
}
const char *visual_host_stop(VisualHost *h) {
    if(!h||!h->initialized)return NULL;
    if(pthread_mutex_trylock(&h->mutex))return "provider_busy";
    const char *error=stop_locked(h);pthread_mutex_unlock(&h->mutex);return error;
}
const char *visual_host_close(VisualHost *h) {
    const char *error=visual_host_stop(h);
    if(error&&h&&atomic_load(&h->attempted))return error;
    if(h&&h->initialized){samosa_mm_supervisor_destroy(&h->supervisor);pthread_mutex_destroy(&h->mutex);h->initialized=0;}
    return error;
}
static const char *transport_open(void *context,const SamosaMmProvider *provider,
                                 const SamosaMmControl *control,int *input,int *output) {
    (void)provider;VisualHost *h=context;
    const char *error=control&&control->check?control->check(control->context):NULL;
    if(!error)error=owned(h);
    if(!error){*input=h->input;*output=h->output;}return error;
}
const char *visual_host_init(VisualHost *h,const InferenceConfig *cfg,size_t engine_size,size_t anchor_size) {
    memset(h,0,sizeof(*h));h->model=h->directory=h->input=h->output=-1;
    h->activation=(Activation){.process={.guardian=-1,.control=-1,.response=-1},.root=-1,.lease=-1,.engine_fd=-1,.model_fd=-1};
    atomic_init(&h->attempted,0);atomic_init(&h->qualified,0);
    if(!cfg||!cfg->admission_state||(!cfg->vision&&!cfg->temporal)||(cfg->vision&&cfg->temporal)||!engine_size||!anchor_size)
        return "invalid_visual_host_configuration";
    h->inference=*cfg;h->temporal=cfg->temporal!=NULL;h->engine_size=engine_size;h->anchor_size=anchor_size;
    if(snprintf(h->state,sizeof(h->state),"%s/activation",cfg->admission_state)>=(int)sizeof(h->state)||
       snprintf(h->anchor,sizeof(h->anchor),"%s/%s",model_path(h),h->temporal?"manifest.json":"config.json")>=(int)sizeof(h->anchor))
        return "invalid_visual_host_configuration";
    const char *identity=h->temporal?cfg->temporal->model_sha256:cfg->vision->model_sha256;
    int bytes=h->temporal?
        snprintf(h->bundle,sizeof(h->bundle),"{\"profile\":\"resident-temporal-v1\",\"model_sha256\":\"%s\",\"manifest_sha256\":\"%s\"}",identity,cfg->temporal->manifest_sha256):
        snprintf(h->bundle,sizeof(h->bundle),"{\"profile\":\"resident-vision-v1\",\"model_sha256\":\"%s\",\"config_sha256\":\"%s\",\"tokenizer_sha256\":\"%s\",\"weights_sha256\":\"%s\"}",
            identity,cfg->vision->config_sha256,cfg->vision->tokenizer_sha256,cfg->vision->weights_sha256);
    if(bytes<0||(size_t)bytes>=sizeof(h->bundle))return "invalid_visual_host_configuration";
    hash_bytes(h->bundle,(size_t)bytes,h->identity);
    if(pthread_mutex_init(&h->mutex,NULL))return "resource_limit";
    if(!samosa_mm_supervisor_init(&h->supervisor)){pthread_mutex_destroy(&h->mutex);return "resource_limit";}
    h->initialized=1;return NULL;
}
static const char *verify_bundle(void *context,int engine,int anchor) {
    VisualHost *h=context;const char *error=engine_system_runtime(engine);
    if(error)return error;
    h->model=open(model_path(h),O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(h->model<0)error="artifact_unavailable";
    struct stat directory;
    /* Preserve the one-shot caller's directory admission before shared checks. */
    if(!error&&(fstat(h->model,&directory)||directory.st_uid!=geteuid()||(directory.st_mode&022)))error="unsupported_artifact";
    if(!error)error=assets(h,&h->inference,0);
    struct stat admitted,named;
    if(!error&&(fstat(anchor,&admitted)||fstatat(h->model,h->temporal?"manifest.json":"config.json",&named,AT_SYMLINK_NOFOLLOW)||
       !visual_same_receipt(&admitted,&named)))error="artifact_changed";
    return error;
}
static const char *launch(void *context,const char *self,const char *rendered,double lifetime,
                           size_t limit,int lease,int anchor,EngineProcess *process) {
    (void)rendered;(void)anchor;VisualHost *h=context;const char *error=NULL;
    snprintf(h->workspace,sizeof(h->workspace),"/tmp/samosa-visual-resident-XXXXXX");
    if(!error&&!mkdtemp(h->workspace))error="engine_io_failed";
    if(!error){h->directory=open(h->workspace,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(h->directory<0)error="engine_io_failed";}
    int input[2]={-1,-1},output[2]={-1,-1};
    if(!error&&(pipe(input)||pipe(output)))error="engine_io_failed";
    int descriptors[]={input[0],input[1],output[0],output[1]};
    for(size_t i=0;!error&&i<4;i++)if(fcntl(descriptors[i],F_SETFD,FD_CLOEXEC))error="engine_io_failed";
    char *args=NULL;size_t size=0;FILE *out=!error?open_memstream(&args,&size):NULL;
    if(!error&&!out)error="resource_limit";
    if(out){const char *argv[]={self,"--vision-engine-resident-bridge",engine_path(h),engine_pin(h)};
        fputc('[',out);for(size_t i=0;i<4;i++){if(i)fputc(',',out);json_quote(out,argv[i],strlen(argv[i]));}
        fputc(']',out);if(fclose(out))error="resource_limit";}
    if(!error)error=engine_process_start_bound_workspace_duplex(self,args,lifetime,limit,lease,h->model,
                                                               h->workspace,h->directory,input[0],output[1],process);
    free(args);if(input[0]>=0)close(input[0]);if(output[1]>=0)close(output[1]);
    h->input=input[1];h->output=output[0];return error;
}
static const char *lock_controlled(VisualHost *h,const InferenceConfig *cfg) {
    for(;;){const char *error=inference_interrupted(cfg);if(error)return error;
        int locked=pthread_mutex_trylock(&h->mutex);if(!locked)return NULL;
        if(locked!=EBUSY)return "resource_limit";
        struct timespec pause={0,5000000};nanosleep(&pause,NULL);}
}
static const char *request(VisualHost *h,const InferenceConfig *cfg,const unsigned char *rgba,
        size_t width,size_t height,const char *prompt,size_t tokens,const TemporalInput *input,
        char **text,size_t *prompt_tokens,size_t *generated_tokens,char prepared[65]) {
    *text=NULL;InferenceConfig scoped=*cfg;
    if(scoped.deadline_monotonic<=0)scoped.deadline_monotonic=inference_now()+(h->temporal?120:60);
    const char *error=lock_controlled(h,&scoped);if(error)return error;
    if(!h->activating&&!atomic_load(&h->qualified))error="provider_unavailable";
    if(!error)error=owned(h);
    int lease=-1;if(!error&&!h->activating)error=inference_schedule(&scoped,&lease);
    if(!error)error=assets(h,&scoped,1);
    char *command=NULL;size_t bytes=0;
    if(!error&&h->temporal){
        for(size_t k=0;!error&&k<input->count;k++){
            char name[32];snprintf(name,sizeof(name),"frame-%02zu.ppm",k);
            char *ppm=temporal_ppm(&input->frames[k],&bytes);
            if(!ppm)error="resource_limit";else error=visual_write_owned(&scoped,h->directory,name,ppm,bytes);free(ppm);
        }
        if(!error){command=temporal_command(h->workspace,input,&bytes);if(!command)error="resource_limit";}
    }else if(!error){
        char image[320],digest[65];snprintf(image,sizeof(image),"%s/input.ppm",h->workspace);
        char *ppm=vision_ppm(rgba,width,height,&bytes,prepared);
        if(!ppm)error="resource_limit";else error=visual_write_owned(&scoped,h->directory,"input.ppm",ppm,bytes);free(ppm);
        hash_bytes(rgba,width*height*4,digest);
        if(!error){command=vision_command(image,digest,prompt,tokens,&bytes);if(!command)error="resource_limit";}
    }
    char *reply=NULL;size_t reply_bytes=0;
    if(!error){
        SamosaMmControl control={.deadline_ms=(int64_t)(scoped.deadline_monotonic*1000),.check=interrupted,.context=&scoped};
        char transport[SAMOSA_MM_ERROR_MAX];
        if(!samosa_mm_session_request_controlled(&h->session,&control,command,bytes,&reply,&reply_bytes,transport,sizeof(transport))){
            if(!strcmp(transport,"cancelled"))error="cancelled";
            else if(!strcmp(transport,"deadline_exceeded")||!strcmp(transport,"multimodal_deadline_exceeded"))error="deadline_exceeded";
            else error="provider_process_failed";
        }
        if(!error&&(reply_bytes!=strlen(reply)||reply_bytes>ENGINE_VISUAL_RESULT_MAX))error="invalid_provider_response";
        if(!error&&h->temporal)error=temporal_result(reply,input,text,prompt_tokens,generated_tokens);
        else if(!error){char digest[65];hash_bytes(rgba,width*height*4,digest);error=vision_result(reply,digest,tokens,text,prompt_tokens,generated_tokens);}
    }
    if(!error)error=inference_interrupted(&scoped);
    if(!error)error=assets(h,&scoped,1);
    if(!error)error=owned(h);
    free(command);free(reply);if(lease>=0)close(lease);
    if(error){
        free(*text);*text=NULL;
        /* The reused transport leaves a preflight-cancelled or fully completed
           frame usable. Interrupted exchanges invalidate it and must be reaped. */
        int reusable=(!strcmp(error,"cancelled")||!strcmp(error,"deadline_exceeded"))&&h->session.active&&!owned(h);
        if(reusable)remove_inputs(h);
        else{const char *cleanup=stop_locked(h);if(cleanup&&!strcmp(cleanup,"engine_shutdown_pending"))error=cleanup;}
    }
    else remove_inputs(h);
    pthread_mutex_unlock(&h->mutex);return error;
}
const char *visual_host_image(void *context,const InferenceConfig *cfg,const unsigned char *rgba,
        size_t width,size_t height,const char *prompt,size_t tokens,char **text,size_t *in,size_t *out,char prepared[65]) {
    return request(context,cfg,rgba,width,height,prompt,tokens,NULL,text,in,out,prepared);
}
const char *visual_host_temporal(void *context,const InferenceConfig *cfg,const TemporalInput *input,
                                char **text,size_t *in,size_t *out) {
    return request(context,cfg,NULL,0,0,NULL,0,input,text,in,out,NULL);
}
static const char *qualify(void *context) {
    VisualHost *h=context;
    if(fstat(h->activation.engine_fd,&h->engine_before)||fstat(h->activation.model_fd,&h->anchor_before))return "artifact_changed";
    SamosaMmProvider provider={.provider=h->temporal?"molmo2-native-v1":"visionpsy-native-v1",
        .executable=engine_path(h),.model_dir=model_path(h),
        .protocol=h->temporal?SAMOSA_MM_PROTOCOL_JSON_FRAME_V1:SAMOSA_MM_PROTOCOL_JSON_LINE,
        .ready_timeout_ms=h->temporal?90000:20000,.request_timeout_ms=h->temporal?120000:60000,
        .max_frame_bytes=ENGINE_VISUAL_RESULT_MAX};
    SamosaMmLifecycle lifecycle={transport_open,transport_close,h};
    SamosaMmControl control={.deadline_ms=(int64_t)(h->inference.deadline_monotonic*1000),.check=interrupted,.context=&h->inference};
    char transport[SAMOSA_MM_ERROR_MAX];
    if(!samosa_mm_session_start_owned(&h->supervisor,&provider,&lifecycle,&control,&h->session,transport,sizeof(transport))){
        const char *error=inference_interrupted(&h->inference);
        return error?error:"provider_process_failed";
    }
    unsigned char rgba[]={255,255,255,255};char *text=NULL,prepared[65];size_t in=0,out=0;
    const char *error;
    if(h->temporal){
        TemporalInput input={.count=1,.end_us=1000000,.tokens=32,.prompt="Describe the main color briefly."};
        input.frames[0]=(TemporalFrame){.timestamp_us=0,.width=1,.height=1,.rgba=rgba};
        hash_bytes(rgba,sizeof(rgba),input.input_sha256);
        error=request(h,&h->inference,NULL,0,0,NULL,0,&input,&text,&in,&out,NULL);
    }else error=request(h,&h->inference,rgba,1,1,"Describe the main color briefly.",32,NULL,&text,&in,&out,prepared);
    free(text);return error;
}
const char *visual_host_start(VisualHost *h,const InferenceConfig *cfg) {
    if(!h||!h->initialized)return "invalid_visual_host_configuration";
    if(atomic_load(&h->attempted))return visual_host_status(h);
    h->inference=*cfg;h->terminal=NULL;double limit=inference_now()+(h->temporal?120:60);
    if(h->inference.deadline_monotonic<=0||h->inference.deadline_monotonic>limit)h->inference.deadline_monotonic=limit;
    if(mkdir(cfg->admission_state,0700)&&errno!=EEXIST)return "unsafe_visual_state";
    struct stat st;if(lstat(cfg->admission_state,&st)||!S_ISDIR(st.st_mode)||st.st_uid!=geteuid()||(st.st_mode&077))return "unsafe_visual_state";
    ActivationConfig activation={.state=h->state,.self=h->temporal?cfg->temporal->self:cfg->vision->self,
        .engine=engine_path(h),.engine_sha=engine_pin(h),.engine_size=h->engine_size,
        .model=h->anchor,.model_sha=h->temporal?cfg->temporal->manifest_sha256:cfg->vision->config_sha256,
        .model_size=h->anchor_size,.byte_ceiling=67108864,.advance_current=1,.lifetime=86400,.log_limit=1048576,
        .verify_runtime=verify_bundle,.runtime_context=h,.argv_template="[\"@ENGINE@\",\"@MODEL@\"]",.model_argument=model_path(h),
        .bundle_identity=h->identity,.bundle_config=h->bundle,.launch=launch,.launch_context=h,.qualify=qualify,.qualification=h};
    atomic_store(&h->attempted,1);h->activating=1;
    const char *error=activation_start(&activation,&h->activation);h->activating=0;
    if(!error){atomic_store(&h->qualified,1);error=visual_host_status(h);}
    if(error){const char *cleanup=visual_host_stop(h);if(cleanup&&!strcmp(cleanup,"engine_shutdown_pending"))return cleanup;}
    return error;
}
