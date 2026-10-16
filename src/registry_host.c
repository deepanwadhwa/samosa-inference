#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "registry_host.h"
#include "private_key.h"

void inference_registry_host_free(InferenceRegistryHost *h) {
    if(!h)return;
    for(size_t i=0;i<INFERENCE_PROVIDERS_MAX;i++){embedding_config_free(&h->embeddings[i]);private_key_clear(h->keys[i]);}
    for(size_t i=0;i<h->string_count;i++)free(h->strings[i]);
    memset(h,0,sizeof(*h));
}
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
    int decision=!strcmp(kind,"text_decisions"),speech=!strcmp(kind,"speech_output"),fake=!strcmp(kind,"fake_text"),text=!strcmp(kind,"text"),embedding=!strcmp(kind,"embeddings"),
        summary=!strcmp(kind,"summarization"),whisper=!strcmp(kind,"transcription"),vision=!strcmp(kind,"image_analysis"),temporal=!strcmp(kind,"video_analysis");
    if(!fake&&!text&&!embedding&&!summary&&!whisper&&!vision&&!temporal&&!speech&&!decision)return "unsupported_provider_kind";
    char allowed[512];snprintf(allowed,sizeof(allowed),"%s%s",common,
        decision?"assets|engine_sha256|runtime_sha256|model_sha256|tokenizer_sha256|":speech?"package|manifest_sha256|":text?"endpoint|model_alias|api_key_file|":embedding?"endpoint|model_alias|api_key_file|embedding_config_file|":
        vision?"engine|model|engine_sha256|model_sha256|config_sha256|tokenizer_sha256|weights_sha256|":
        temporal?"engine|model|engine_sha256|model_sha256|manifest_sha256|":
        fake?"":"engine|model|engine_sha256|model_sha256|");
    if(!fields(j,object,allowed))return "unknown_registry_field";
    InferenceConfig *c=&h->providers[i];c->owner=owner;
    c->provider_id=string(h,j,object,"provider_id");c->admission_state=string(h,j,object,"state");
    if(json_get(j,object,"platform")>=0){c->platform=string(h,j,object,"platform");if(!c->platform)return "invalid_registry_configuration";}
    if(!number(j,object,"context_tokens",4096,128,131072,&c->context_tokens)||
       !number(j,object,"queue_capacity",4,0,8,&c->queue_capacity)||
       !number(j,object,"admission_wait_ms",10000,1,60000,&c->admission_wait_ms))return "invalid_registry_configuration";
    if(decision){DecisionConfig *d=&h->decisions[i];d->self=self;d->assets=string(h,j,object,"assets");
        const char *keys[]={"engine_sha256","runtime_sha256","model_sha256","tokenizer_sha256"};
        if(!absolute(self)||!absolute(d->assets))return "invalid_registry_configuration";
        for(unsigned k=0;k<DECISION_ASSET_COUNT;k++){d->pins[k]=string(h,j,object,keys[k]);if(!digest(d->pins[k]))return "invalid_registry_configuration";}
        c->decision=d;c->context_tokens=512;
        size_t declared=512;if(!number(j,object,"context_tokens",512,512,512,&declared))return "invalid_registry_configuration";
    }
    if(speech){const char *package=string(h,j,object,"package"),*pin=string(h,j,object,"manifest_sha256");
        if(!absolute(self)||!absolute(package)||!digest(pin))return "invalid_registry_configuration";
        h->speeches[i]=(SpeechConfig){.self=self,.package=package,.manifest_sha256=pin};c->speech=&h->speeches[i];}
    if(text||embedding){
        c->endpoint=string(h,j,object,"endpoint");c->model_alias=string(h,j,object,"model_alias");
        if(!c->endpoint||!c->model_alias||!*c->model_alias||strlen(c->model_alias)>128||json_get(j,object,"context_tokens")<0)return "invalid_registry_configuration";
        if(json_get(j,object,"api_key_file")>=0){const char *key=string(h,j,object,"api_key_file");if(!private_key_read(key,h->keys[i]))return "invalid_registry_credentials";c->api_key=h->keys[i];}
        if(embedding){const char *path=string(h,j,object,"embedding_config_file");if(!embedding_config_load(path,&h->embeddings[i]))return "invalid_embedding_configuration";c->embedding=&h->embeddings[i];}
    }
    if(summary||whisper||vision||temporal){
        const char *engine=string(h,j,object,"engine"),*model=string(h,j,object,"model"),
            *engine_sha=string(h,j,object,"engine_sha256"),*model_sha=string(h,j,object,"model_sha256");
        if(!absolute(self)||!absolute(engine)||!absolute(model)||!digest(engine_sha)||!digest(model_sha))return "invalid_registry_configuration";
        if(summary){h->summaries[i]=(SummaryConfig){self,engine,model,engine_sha,model_sha};c->summary=&h->summaries[i];}
        if(whisper){h->transcriptions[i]=(WhisperConfig){self,engine,model,engine_sha,model_sha};c->whisper=&h->transcriptions[i];}
        if(vision){h->images[i]=(VisionConfig){.self=self,.executable=engine,.model=model,.executable_sha256=engine_sha,.model_sha256=model_sha,
            .config_sha256=string(h,j,object,"config_sha256"),.tokenizer_sha256=string(h,j,object,"tokenizer_sha256"),.weights_sha256=string(h,j,object,"weights_sha256")};
            if(!digest(h->images[i].config_sha256)||!digest(h->images[i].tokenizer_sha256)||!digest(h->images[i].weights_sha256))return "invalid_registry_configuration";
            char identity[65];vision_model_identity(&h->images[i],identity);
            if(strcmp(identity,model_sha))return "invalid_registry_configuration";
            c->vision=&h->images[i];}
        if(temporal){h->videos[i]=(TemporalConfig){.self=self,.executable=engine,.model=model,.executable_sha256=engine_sha,.model_sha256=model_sha,.manifest_sha256=string(h,j,object,"manifest_sha256")};
            if(!digest(h->videos[i].manifest_sha256))return "invalid_registry_configuration";
            c->temporal=&h->videos[i];if(!temporal_configured(c))return "invalid_registry_configuration";}
    }
    return NULL;
}
const char *inference_registry_host_load(const char *path,const char *self,InferenceRegistryHost *h) {
    memset(h,0,sizeof(*h));size_t bytes=0;char *body=configuration(path,&bytes);Json j;size_t version;
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
