#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "registry_host.h"
#include "private_key.h"
#include "platform.h"

const char *inference_registry_host_close(InferenceRegistryHost *h) {
    if(!h)return NULL;
    const char *terminal=NULL;
    for(size_t i=0;i<INFERENCE_PROVIDERS_MAX;i++)if(h->users[i])return "provider_busy";
    for(size_t i=0;i<INFERENCE_PROVIDERS_MAX;i++)if(h->managed[i]){
        const char *error=managed_host_close(h->managed[i]);
        if(error){if(h->managed[i]->attempted||h->managed[i]->key_exists)return error;terminal=error;}
    }
    for(size_t i=0;i<INFERENCE_PROVIDERS_MAX;i++)if(h->resident_summaries[i]){
        const char *error=summary_host_close(h->resident_summaries[i]);
        if(error){if(h->resident_summaries[i]->attempted)return error;terminal=error;}
    }
    for(size_t i=0;i<INFERENCE_PROVIDERS_MAX;i++)if(h->resident_speeches[i]){
        const char *error=speech_host_stop(h->resident_speeches[i]);
        if(error){if(h->resident_speeches[i]->attempted)return error;terminal=error;}
    }
    for(size_t i=0;i<INFERENCE_PROVIDERS_MAX;i++)if(h->resident_decisions[i]){
        const char *error=decision_host_stop(h->resident_decisions[i]);
        if(error){if(h->resident_decisions[i]->attempted)return error;terminal=error;}
    }
    for(size_t i=0;i<INFERENCE_PROVIDERS_MAX;i++)if(h->resident_visuals[i]){
        const char *error=visual_host_close(h->resident_visuals[i]);
        if(error){if(h->resident_visuals[i]->attempted)return error;terminal=error;}
    }
    for(size_t i=0;i<INFERENCE_PROVIDERS_MAX;i++)if(h->on_demand_whispers[i]){
        const char *error=whisper_host_stop(h->on_demand_whispers[i]);
        if(error){if(h->on_demand_whispers[i]->lifecycle.attempted)return error;terminal=error;}
    }
    for(size_t i=0;i<INFERENCE_PROVIDERS_MAX;i++)free(h->on_demand_whispers[i]);
    for(size_t i=0;i<INFERENCE_PROVIDERS_MAX;i++)free(h->resident_visuals[i]);
    for(size_t i=0;i<INFERENCE_PROVIDERS_MAX;i++)free(h->resident_decisions[i]);
    for(size_t i=0;i<INFERENCE_PROVIDERS_MAX;i++)free(h->resident_speeches[i]);
    for(size_t i=0;i<INFERENCE_PROVIDERS_MAX;i++)free(h->managed[i]);
    for(size_t i=0;i<INFERENCE_PROVIDERS_MAX;i++)free(h->resident_summaries[i]);
    for(size_t i=0;i<INFERENCE_PROVIDERS_MAX;i++){embedding_config_free(&h->embeddings[i]);private_key_clear(h->keys[i]);}
    for(size_t i=0;i<h->string_count;i++)free(h->strings[i]);
    if(h->gate_initialized)pthread_mutex_destroy(&h->gate);
    memset(h,0,sizeof(*h));
    return terminal;
}
void inference_registry_host_free(InferenceRegistryHost *h){(void)inference_registry_host_close(h);}
static const char *managed_status(void *context){return managed_provider_status(&((ManagedHost *)context)->provider);}
static char *string(InferenceRegistryHost *h,const Json *j,int object,const char *key) {
    if(h->string_count==sizeof(h->strings)/sizeof(h->strings[0]))return NULL;
    char *s=json_string(j,json_get(j,object,key));
    if(s)h->strings[h->string_count++]=s;
    return s;
}
static int fields(const Json *j,int object,const char *allowed) {
    if(object<0||j->tokens[object].type!=JT_OBJECT)return 0;
    for(int t=object+1;t<j->tokens[object].next;t=j->tokens[j->tokens[t].next].next){
        char *key=json_string(j,t),match[128];int n=snprintf(match,sizeof(match),"|%s|",key?key:"");free(key);
        if(n<0||(size_t)n>=sizeof(match)||!strstr(allowed,match))return 0;
    }
    return 1;
}
static int number(const Json *j,int object,const char *key,size_t fallback,size_t min,size_t max,size_t *out) {
    int t=json_get(j,object,key);*out=fallback;
    return (t<0||json_uint(j,t,out))&&*out>=min&&*out<=max;
}
static int digest(const char *s){return s&&strlen(s)==64&&strspn(s,"0123456789abcdef")==64;}
static int absolute(const char *s){return s&&s[0]=='/'&&s[1]&&strlen(s)<=4095;}
static int stable(const struct stat *a,const struct stat *b) {
    if(a->st_dev!=b->st_dev||a->st_ino!=b->st_ino||a->st_size!=b->st_size||a->st_uid!=b->st_uid||a->st_mode!=b->st_mode||a->st_nlink!=b->st_nlink)return 0;
#ifdef __APPLE__
    return a->st_mtimespec.tv_sec==b->st_mtimespec.tv_sec&&a->st_mtimespec.tv_nsec==b->st_mtimespec.tv_nsec&&a->st_ctimespec.tv_sec==b->st_ctimespec.tv_sec&&a->st_ctimespec.tv_nsec==b->st_ctimespec.tv_nsec;
#else
    return a->st_mtim.tv_sec==b->st_mtim.tv_sec&&a->st_mtim.tv_nsec==b->st_mtim.tv_nsec&&a->st_ctim.tv_sec==b->st_ctim.tv_sec&&a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
#endif
}
static char *configuration(const char *path,size_t *bytes) {
    if(!absolute(path))return NULL;
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC);if(fd<0)return NULL;
    struct stat before,after,named;char *body=NULL;size_t used=0;
    int valid=!fstat(fd,&before)&&S_ISREG(before.st_mode)&&before.st_uid==geteuid()&&!(before.st_mode&077)&&before.st_nlink==1&&before.st_size>0&&before.st_size<=65536;
    if(valid){body=malloc((size_t)before.st_size+1);valid=body!=NULL;}
    while(valid&&used<(size_t)before.st_size){ssize_t n=pread(fd,body+used,(size_t)before.st_size-used,(off_t)used);if(n<0&&errno==EINTR)continue;if(n<=0){valid=0;break;}used+=(size_t)n;}
    valid=valid&&!fstat(fd,&after)&&!lstat(path,&named)&&stable(&before,&after)&&stable(&after,&named);
    close(fd);if(!valid){free(body);return NULL;}body[used]=0;*bytes=used;return body;
}
static const char *provider(InferenceRegistryHost *h,const Json *j,int object,size_t i,const char *self,const char *owner) {
    const char *common="|provider_id|kind|state|platform|context_tokens|queue_capacity|admission_wait_ms|";
    char *kind=string(h,j,object,"kind");if(!kind)return "invalid_registry_configuration";
    int image_decision=!strcmp(kind,"image_decisions");
    int decision=image_decision||!strcmp(kind,"text_decisions"),speech=!strcmp(kind,"speech_output"),fake=!strcmp(kind,"fake_text"),text=!strcmp(kind,"text"),embedding=!strcmp(kind,"embeddings"),
        summary=!strcmp(kind,"summarization"),whisper=!strcmp(kind,"transcription"),vision=!strcmp(kind,"image_analysis"),temporal=!strcmp(kind,"video_analysis");
    if(!fake&&!text&&!embedding&&!summary&&!whisper&&!vision&&!temporal&&!speech&&!decision)return "unsupported_provider_kind";
    int managed=json_get(j,object,"managed_config_file")>=0;
    int resident=json_get(j,object,"lifecycle")>=0;
    if(resident&&!summary&&!speech&&!decision&&!vision&&!temporal&&!whisper)return "invalid_resident_provider_kind";
    if(managed&&!text&&!embedding)return "invalid_managed_provider_kind";
    char allowed[512];snprintf(allowed,sizeof(allowed),"%s%s",common,
        managed?"managed_config_file|":decision?(resident?"assets|engine_sha256|runtime_sha256|model_sha256|tokenizer_sha256|lifecycle|engine_size|model_size|":"assets|engine_sha256|runtime_sha256|model_sha256|tokenizer_sha256|"):speech?(resident?"package|manifest_sha256|lifecycle|worker_sha256|worker_size|manifest_size|":"package|manifest_sha256|"):text?"endpoint|model_alias|api_key_file|":embedding?"endpoint|model_alias|api_key_file|embedding_config_file|":
        whisper?(resident?"engine|model|engine_sha256|model_sha256|lifecycle|engine_size|model_size|qualification_wav|qualification_sha256|qualification_size|qualification_text|":"engine|model|engine_sha256|model_sha256|"):
        vision?(resident?"engine|model|engine_sha256|model_sha256|config_sha256|tokenizer_sha256|weights_sha256|lifecycle|engine_size|config_size|":"engine|model|engine_sha256|model_sha256|config_sha256|tokenizer_sha256|weights_sha256|"):
        temporal?(resident?"engine|model|engine_sha256|model_sha256|manifest_sha256|lifecycle|engine_size|manifest_size|":"engine|model|engine_sha256|model_sha256|manifest_sha256|"):
        fake?"":resident?"engine|model|engine_sha256|model_sha256|lifecycle|engine_size|model_size|":"engine|model|engine_sha256|model_sha256|");
    if(!fields(j,object,allowed))return "unknown_registry_field";
    InferenceConfig *c=&h->providers[i];c->owner=owner;
    c->provider_id=string(h,j,object,"provider_id");c->admission_state=string(h,j,object,"state");
    if(json_get(j,object,"platform")>=0){c->platform=string(h,j,object,"platform");if(!c->platform)return "invalid_registry_configuration";}
    if(!number(j,object,"context_tokens",4096,image_decision?64:128,131072,&c->context_tokens)||
       !number(j,object,"queue_capacity",4,0,8,&c->queue_capacity)||
       !number(j,object,"admission_wait_ms",10000,1,60000,&c->admission_wait_ms))return "invalid_registry_configuration";
    if(decision){DecisionConfig *d=&h->decisions[i];d->profile=image_decision?DECISION_ASSETS_IMAGE:DECISION_ASSETS_TEXT;d->self=self;d->assets=string(h,j,object,"assets");
        const char *keys[]={"engine_sha256","runtime_sha256","model_sha256","tokenizer_sha256"};
        if(!absolute(self)||!absolute(d->assets))return "invalid_registry_configuration";
        for(unsigned k=0;k<DECISION_ASSET_COUNT;k++){d->pins[k]=string(h,j,object,keys[k]);if(!digest(d->pins[k]))return "invalid_registry_configuration";}
        c->decision=d;c->context_tokens=image_decision?64:512;
        size_t declared=c->context_tokens;if(!number(j,object,"context_tokens",c->context_tokens,c->context_tokens,c->context_tokens,&declared))return "invalid_registry_configuration";
        if(resident){const char *profile=string(h,j,object,"lifecycle");size_t engine_size=0,model_size=0;
            if(!profile||strcmp(profile,image_decision?"resident-image-decision-v1":"resident-text-decision-v1")||json_get(j,object,"engine_size")<0||json_get(j,object,"model_size")<0||
                !number(j,object,"engine_size",0,64,67108864,&engine_size)||!number(j,object,"model_size",0,1,image_decision?2147483648ULL:1073741824,&model_size))return "invalid_decision_host_configuration";
            DecisionHost *host=calloc(1,sizeof(*host));if(!host)return "resource_limit";h->resident_decisions[i]=host;
            if(image_decision)d->run_image=decision_host_run_image;else d->run=decision_host_run;d->context=host;c->owned_status=decision_host_status;c->owned_context=host;
            const char *error=decision_host_init(host,c,engine_size,model_size);if(error)return error;
        }
    }
    if(speech){const char *package=string(h,j,object,"package"),*pin=string(h,j,object,"manifest_sha256");
        if(!absolute(self)||!absolute(package)||!digest(pin))return "invalid_registry_configuration";
        h->speeches[i]=(SpeechConfig){.self=self,.package=package,.manifest_sha256=pin};c->speech=&h->speeches[i];
        if(resident){const char *profile=string(h,j,object,"lifecycle"),*worker_pin=string(h,j,object,"worker_sha256");size_t worker_size=0,manifest_size=0;
            if(!profile||strcmp(profile,"resident-speech-v1")||!digest(worker_pin)||json_get(j,object,"worker_size")<0||json_get(j,object,"manifest_size")<0||
                !number(j,object,"worker_size",0,64,268435456,&worker_size)||!number(j,object,"manifest_size",0,1,1048576,&manifest_size))return "invalid_speech_host_configuration";
            SpeechHost *host=calloc(1,sizeof(*host));if(!host)return "resource_limit";h->resident_speeches[i]=host;
            h->speeches[i].run=speech_host_run;h->speeches[i].context=host;c->owned_status=speech_host_status;c->owned_context=host;
            const char *error=speech_host_init(host,c,worker_pin,worker_size,manifest_size);if(error)return error;
        }
    }
    if(managed){
        const char *path=string(h,j,object,"managed_config_file");
        if(!absolute(path)||!absolute(self))return "invalid_registry_configuration";
        ManagedHost *m=calloc(1,sizeof(*m));if(!m)return "resource_limit";
        m->key_root=m->key_fd=-1;h->managed[i]=m;
        const char *error=managed_host_load(path,m);if(error)return error;
        if(!c->provider_id||strcmp(c->provider_id,m->identity)||strcmp(owner,m->owner)||
           !c->admission_state||strcmp(c->admission_state,m->admission)||
           c->context_tokens!=m->context||embedding!=m->embedding_mode)return "managed_registry_binding_mismatch";
        c->endpoint=m->endpoint;c->model_alias=m->alias;c->api_key=m->key;
        c->embedding=embedding?&m->embedding:NULL;
        c->owned_status=managed_status;c->owned_context=m;
    }
    if((text||embedding)&&!managed){
        c->endpoint=string(h,j,object,"endpoint");c->model_alias=string(h,j,object,"model_alias");
        if(!c->endpoint||!c->model_alias||!*c->model_alias||strlen(c->model_alias)>128||json_get(j,object,"context_tokens")<0)return "invalid_registry_configuration";
        if(json_get(j,object,"api_key_file")>=0){const char *key=string(h,j,object,"api_key_file");if(!private_key_read(key,h->keys[i]))return "invalid_registry_credentials";c->api_key=h->keys[i];}
        if(embedding){const char *path=string(h,j,object,"embedding_config_file");if(!embedding_config_load(path,&h->embeddings[i]))return "invalid_embedding_configuration";c->embedding=&h->embeddings[i];}
    }
    if(summary||whisper||vision||temporal){
        const char *engine=string(h,j,object,"engine"),*model=string(h,j,object,"model"),
            *engine_sha=string(h,j,object,"engine_sha256"),*model_sha=string(h,j,object,"model_sha256");
        if(!absolute(self)||!absolute(engine)||!absolute(model)||!digest(engine_sha)||!digest(model_sha))return "invalid_registry_configuration";
        if(summary){
            h->summaries[i]=(SummaryConfig){.self=self,.executable=engine,.model=model,.executable_sha256=engine_sha,.model_sha256=model_sha};c->summary=&h->summaries[i];
            if(resident){
                const char *profile=string(h,j,object,"lifecycle");size_t engine_size=0,model_size=0;
                if(!profile||strcmp(profile,"resident-budget-v1")||json_get(j,object,"engine_size")<0||json_get(j,object,"model_size")<0||
                   !number(j,object,"engine_size",0,64,67108864,&engine_size)||!number(j,object,"model_size",0,1,536870912,&model_size))return "invalid_summary_host_configuration";
                SummaryHost *host=calloc(1,sizeof(*host));if(!host)return "resource_limit";h->resident_summaries[i]=host;
                h->summaries[i].run=summary_host_run;h->summaries[i].context=host;c->owned_status=summary_host_status;c->owned_context=host;
                const char *error=summary_host_init(host,c,engine_size,model_size);if(error)return error;
            }
        }
        if(whisper){
            h->transcriptions[i]=(WhisperConfig){.self=self,.executable=engine,.model=model,.executable_sha256=engine_sha,.model_sha256=model_sha};c->whisper=&h->transcriptions[i];
            if(resident){
                const char *profile=string(h,j,object,"lifecycle"),*audio=string(h,j,object,"qualification_wav"),
                    *audio_sha=string(h,j,object,"qualification_sha256"),*expected=string(h,j,object,"qualification_text");
                size_t engine_size=0,model_size=0,audio_size=0;
                if(!profile||strcmp(profile,"on-demand-whisper-v1")||!absolute(audio)||!digest(audio_sha)||!expected||!*expected||strlen(expected)>128||
                   json_get(j,object,"engine_size")<0||json_get(j,object,"model_size")<0||json_get(j,object,"qualification_size")<0||
                   !number(j,object,"engine_size",0,64,67108864,&engine_size)||!number(j,object,"model_size",0,1,536870912,&model_size)||
                   !number(j,object,"qualification_size",0,76,960044,&audio_size))return "invalid_whisper_host_configuration";
                WhisperHost *host=calloc(1,sizeof(*host));if(!host)return "resource_limit";h->on_demand_whispers[i]=host;
                h->transcriptions[i].run=whisper_host_run;h->transcriptions[i].context=host;h->transcriptions[i].retain_cleanup=1;c->owned_status=whisper_host_status;c->owned_context=host;
                const char *error=whisper_host_init(host,c,engine_size,model_size,audio,audio_sha,audio_size,expected);if(error)return error;
            }
        }
        if(vision){h->images[i]=(VisionConfig){.self=self,.executable=engine,.model=model,.executable_sha256=engine_sha,.model_sha256=model_sha,
            .config_sha256=string(h,j,object,"config_sha256"),.tokenizer_sha256=string(h,j,object,"tokenizer_sha256"),.weights_sha256=string(h,j,object,"weights_sha256")};
            if(!digest(h->images[i].config_sha256)||!digest(h->images[i].tokenizer_sha256)||!digest(h->images[i].weights_sha256))return "invalid_registry_configuration";
            char identity[65];vision_model_identity(&h->images[i],identity);
            if(strcmp(identity,model_sha))return "invalid_registry_configuration";
            c->vision=&h->images[i];}
        if(temporal){h->videos[i]=(TemporalConfig){.self=self,.executable=engine,.model=model,.executable_sha256=engine_sha,.model_sha256=model_sha,.manifest_sha256=string(h,j,object,"manifest_sha256")};
            if(!digest(h->videos[i].manifest_sha256))return "invalid_registry_configuration";
            c->temporal=&h->videos[i];if(!temporal_configured(c))return "invalid_registry_configuration";}
        if(resident&&(vision||temporal)){
            const char *profile=string(h,j,object,"lifecycle"),*anchor=temporal?"manifest_size":"config_size";
            size_t engine_size=0,anchor_size=0;
            if(!profile||strcmp(profile,temporal?"resident-temporal-v1":"resident-vision-v1")||
               json_get(j,object,"engine_size")<0||json_get(j,object,anchor)<0||
               !number(j,object,"engine_size",0,64,67108864,&engine_size)||
               !number(j,object,anchor,0,1,1048576,&anchor_size))return "invalid_visual_host_configuration";
            VisualHost *host=calloc(1,sizeof(*host));if(!host)return "resource_limit";h->resident_visuals[i]=host;
            if(vision){h->images[i].run=visual_host_image;h->images[i].context=host;}
            else{h->videos[i].run=visual_host_temporal;h->videos[i].context=host;}
            c->owned_status=visual_host_status;c->owned_context=host;
            const char *error=visual_host_init(host,c,engine_size,anchor_size);if(error)return error;
        }
    }
    return NULL;
}
const char *inference_registry_host_load(const char *path,const char *self,InferenceRegistryHost *h) {
    memset(h,0,sizeof(*h));
    if(pthread_mutex_init(&h->gate,NULL))return "resource_limit";
    h->gate_initialized=1;h->self=self;
    size_t bytes=0;char *body=configuration(path,&bytes);Json j;size_t version;
    const char *error="invalid_registry_configuration";
    if(!body||!json_parse(&j,body,bytes)||!fields(&j,0,"|contract_version|owner|providers|")||
       !json_uint(&j,json_get(&j,0,"contract_version"),&version)||version!=1)goto done;
    const char *owner=string(h,&j,0,"owner");int providers=json_get(&j,0,"providers");
    if(!owner||providers<0||j.tokens[providers].type!=JT_ARRAY||j.tokens[providers].count<1||j.tokens[providers].count>(int)INFERENCE_PROVIDERS_MAX)goto done;
    h->registry.providers=h->providers;
    for(int t=providers+1;t<j.tokens[providers].next;t=j.tokens[t].next){
        error=provider(h,&j,t,h->registry.count,self,owner);if(error)goto done;h->registry.count++;
    }
    error=inference_registry_validate(&h->registry);
done:
    free(body);if(error)inference_registry_host_free(h);return error;
}
static int lifecycle_error(FILE *out,const char *error){
    fputs("{\"contract_version\":1,\"error\":{\"code\":",out);
    json_quote(out,error,strlen(error));fputs("}}\n",out);return 1;
}
int inference_registry_host_dispatch(void *context,const char *input,size_t length,FILE *out){
    InferenceRegistryHost *h=context;Json j;
    if(!input||length>INFERENCE_REQUEST_MAX||!json_parse_opaque(&j,input,length,"request")||j.tokens[0].type!=JT_OBJECT)
        return inference_registry_dispatch(&h->registry,input,length,out);
    char *op=json_string(&j,json_get(&j,0,"op")),*id=json_string(&j,json_get(&j,0,"provider_id"));
    if(op&&!strcmp(op,"describe")&&j.tokens[0].count==2){
        free(op);free(id);char *body=NULL;size_t bytes=0;FILE *capture=open_memstream(&body,&bytes);
        if(!capture)return lifecycle_error(out,"resource_limit");
        int rc=inference_registry_dispatch(&h->registry,input,length,capture);
        if(fclose(capture)){free(body);return lifecycle_error(out,"resource_limit");}
        while(bytes&&isspace((unsigned char)body[bytes-1]))bytes--;
        if(rc||!bytes||body[bytes-1]!='}'){fwrite(body,1,bytes,out);fputc('\n',out);free(body);return rc?rc:1;}
        fwrite(body,1,bytes-1,out);free(body);
        fputs(",\"managed_lifecycle\":{\"operations\":[\"provider_start\",\"provider_lifecycle_status\",\"provider_stop\"],\"provider_ids\":[",out);
        int comma=0;for(size_t i=0;i<h->registry.count;i++)if(h->managed[i]||h->resident_summaries[i]||h->resident_speeches[i]||h->resident_decisions[i]||h->resident_visuals[i]||h->on_demand_whispers[i]){
            if(comma++)fputc(',',out);
            json_quote(out,h->providers[i].provider_id,strlen(h->providers[i].provider_id));
        }
        fputs("]}}\n",out);return ferror(out)?1:0;
    }
    int start=op&&!strcmp(op,"provider_start"),stop=op&&!strcmp(op,"provider_stop"),status=op&&!strcmp(op,"provider_lifecycle_status");
    int lifecycle=start||stop||status;size_t index=h->registry.count;
    for(size_t i=0;id&&i<h->registry.count;i++)if(!strcmp(id,h->providers[i].provider_id)){index=i;break;}
    free(op);free(id);
    if(index==h->registry.count)return inference_registry_dispatch(&h->registry,input,length,out);
    if(lifecycle){size_t version;
        if(!json_uint(&j,json_get(&j,0,"contract_version"),&version)||version!=1)return lifecycle_error(out,"unsupported_contract");
        if(j.tokens[0].count!=3||!fields(&j,0,"|contract_version|op|provider_id|"))return lifecycle_error(out,"unknown_field");
        if(!h->managed[index]&&!h->resident_summaries[index]&&!h->resident_speeches[index]&&!h->resident_decisions[index]&&!h->resident_visuals[index]&&!h->on_demand_whispers[index])return lifecycle_error(out,"provider_lifecycle_unmanaged");
    }
    pthread_mutex_lock(&h->gate);
    if(h->users[index]<0||(lifecycle&&h->users[index])){
        pthread_mutex_unlock(&h->gate);return lifecycle_error(out,"provider_busy");
    }
    h->users[index]=lifecycle?-1:h->users[index]+1;
    pthread_mutex_unlock(&h->gate);
    int rc=0;const char *error=NULL;ManagedHost *m=h->managed[index];SummaryHost *summary=h->resident_summaries[index];SpeechHost *speech=h->resident_speeches[index];DecisionHost *decision=h->resident_decisions[index];VisualHost *visual=h->resident_visuals[index];WhisperHost *whisper=h->on_demand_whispers[index];
    if(lifecycle){
        if(start&&!inference_platform_allowed(h->providers[index].platform))error="unsupported_provider_platform";
        else if(start){
            if(whisper)error=whisper->lifecycle.attempted?whisper_host_status(whisper):whisper_host_start(whisper,&h->providers[index]);
            else if(visual)error=visual->attempted?visual_host_status(visual):visual_host_start(visual,&h->providers[index]);
            else if(decision)error=decision->attempted?decision_host_status(decision):decision_host_start(decision,&h->providers[index]);
            else if(speech)error=speech->attempted?speech_host_status(speech):speech_host_start(speech,&h->providers[index]);
            else if(summary)error=summary->attempted?summary_host_status(summary):summary_host_start(summary,&h->providers[index]);
            else if(m->attempted)error=managed_status(m);
            else error=managed_host_activate(h->self,1,h->cancelled,h->cancellation,m);
        }else if(stop)error=whisper?whisper_host_stop(whisper):visual?visual_host_stop(visual):decision?decision_host_stop(decision):speech?speech_host_stop(speech):summary?summary_host_stop(summary):managed_host_deactivate(m);
        if(error)rc=lifecycle_error(out,error);
        else{
            const char *health=whisper?whisper_host_status(whisper):visual?visual_host_status(visual):decision?decision_host_status(decision):speech?speech_host_status(speech):summary?summary_host_status(summary):managed_status(m);
            fputs("{\"contract_version\":1,\"provider_id\":",out);json_quote(out,h->providers[index].provider_id,strlen(h->providers[index].provider_id));
            fprintf(out,",\"ready\":%s,\"state\":\"%s\",\"health_error\":",health?"false":"true",!(whisper?whisper->lifecycle.attempted:visual?visual->attempted:decision?decision->attempted:speech?speech->attempted:summary?summary->attempted:m->attempted)?"stopped":health?"unavailable":"ready");
            if(health)json_quote(out,health,strlen(health));else fputs("null",out);
            if(whisper)fputs(",\"execution_mode\":\"on_demand\",\"resident\":false",out);
            fputs("}\n",out);
        }
    }else rc=inference_registry_dispatch(&h->registry,input,length,out);
    pthread_mutex_lock(&h->gate);
    if(lifecycle)h->users[index]=0;else h->users[index]--;
    pthread_mutex_unlock(&h->gate);
    return rc;
}
