#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "speech.h"
#include "speech_worker.h"
#include "speech_package.h"
#include "audio_store.h"
#include "admission.h"
#include "budget.h"
#include "sha256.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
static int fail(FILE *out,const char *e){fprintf(out,"{\"contract_version\":1,\"error\":{\"code\":\"%s\"}}\n",e);return 1;}
static int fields(const Json *j,const char *allowed){
    for(int t=1;t<j->tokens[0].next;t=j->tokens[j->tokens[t].next].next){char *key=json_string(j,t),match[128];
        int n=snprintf(match,sizeof(match),"|%s|",key?key:"");free(key);if(n<0||(size_t)n>=sizeof(match)||!strstr(allowed,match))return 0;}return 1;}
static const char *generate_fields="|contract_version|op|provider_id|text|voice|operation_id|parent_operation_id|timeout_ms|";
static int identity(const char *s){if(!s||!*s||strlen(s)>128)return 0;for(;*s;s++)if(!isalnum((unsigned char)*s)&&!strchr("-_:.",*s))return 0;return 1;}
char *speech_normalized(const InferenceConfig *c,const Json *j) {
    if(!c||!c->speech||!c->provider_id||!c->speech->manifest_sha256||strlen(c->speech->manifest_sha256)!=64||strspn(c->speech->manifest_sha256,"0123456789abcdef")!=64||!fields(j,generate_fields))return NULL;
    char *provider=json_string(j,json_get(j,0,"provider_id")),*text=json_string(j,json_get(j,0,"text")),
        *voice=json_string(j,json_get(j,0,"voice")),*id=json_string(j,json_get(j,0,"operation_id")),*parent=json_string(j,json_get(j,0,"parent_operation_id"));
    size_t timeout=0;int t=json_get(j,0,"timeout_ms");
    int valid=provider&&!strcmp(provider,c->provider_id)&&text&&*text&&strlen(text)<=2400&&identity(id)&&
        (json_get(j,0,"voice")<0||(voice&&*voice&&strlen(voice)<=16))&&
        (json_get(j,0,"parent_operation_id")<0||identity(parent))&&(t<0||(json_uint(j,t,&timeout)&&timeout&&timeout<=60000));
    char *body=NULL;size_t n=0;FILE *f=valid?open_memstream(&body,&n):NULL;
    if(f){fputs("{\"provider\":",f);json_quote(f,provider,strlen(provider));fputs(",\"text\":",f);json_quote(f,text,strlen(text));
        fputs(",\"voice\":",f);json_quote(f,voice?voice:"",voice?strlen(voice):0);fputs(",\"manifest_sha256\":",f);
        json_quote(f,c->speech->manifest_sha256,strlen(c->speech->manifest_sha256));
        if(parent){fputs(",\"parent_operation_id\":",f);json_quote(f,parent,strlen(parent));}if(t>=0)fprintf(f,",\"timeout_ms\":%zu",timeout);
        fputc('}',f);if(fclose(f)){free(body);body=NULL;}}
    free(provider);free(text);free(voice);free(id);free(parent);return body;
}
static const char *control(void *p){return inference_interrupted(p);}
static const char *public_pcm(void *context,const unsigned char *pcm,size_t bytes,int rate) {
    const InferenceConfig *c=context;
    return c->speech->pcm_sink(c->speech->pcm_context,pcm,bytes,rate,control,context);
}
static const char *run(const InferenceConfig *c,const char *text,const char *voice,const char *id,const char *pin,AudioReceipt *receipt) {
    int lease=-1,package=-1,dir=-1,input=-1,output=-1,writer=-1;char workspace[]="/tmp/samosa-inference-speech-XXXXXX";int created=0;
    const char *e=inference_schedule(c,&lease);
    if(!e){package=open(c->speech->package,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(package<0)e="speech_package_unavailable";}
    if(!e){created=mkdtemp(workspace)!=NULL;if(!created)e="engine_io_failed";}
    if(!e){dir=open(workspace,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);if(dir<0)e="engine_io_failed";}
    if(!e){writer=openat(dir,"request.frame",O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);output=openat(dir,"result.frame",O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC,0600);
        if(writer<0||output<0)e="engine_io_failed";}
    if(!e){FILE *f=fdopen(writer,"w");if(!f)e="engine_io_failed";else{writer=-1;fputs("{\"text\":",f);json_quote(f,text,strlen(text));
        if(voice){fputs(",\"voice\":",f);json_quote(f,voice,strlen(voice));}fputc('}',f);if(fclose(f))e="engine_io_failed";}}
    if(!e){input=openat(dir,"request.frame",O_RDONLY|O_NOFOLLOW|O_CLOEXEC);if(input<0)e="engine_io_failed";}
    EngineProcess p={.guardian=-1,.control=-1,.response=-1};
    if(!e){double left=c->deadline_monotonic?c->deadline_monotonic-inference_now():60;
        SpeechProcessConfig worker={.self=c->speech->self,.package_path=c->speech->package,.manifest_sha256=c->speech->manifest_sha256,
            .workspace_path=workspace,.residency_fd=lease,.package_fd=package,.workspace_fd=dir,.input_fd=input,.output_fd=output,
            .lifetime_seconds=left,.log_limit=262144,.process=&p,
            .pcm_sink=c->speech->pcm_sink?public_pcm:NULL,.pcm_context=(void *)c};
        SpeechRuntimeResult native;e=left<=0?"deadline_exceeded":speech_process_run(&worker,control,(void *)c,&native);
        if(e&&!strcmp(e,"lifetime_exceeded"))e="deadline_exceeded";}
    /* Retain ownership until the guardian is reaped; never discard a pending
     * record or release its workspace/admission lease while descendants live. */
    while(p.guardian>0){const char *stop=engine_process_stop(&p);if(stop&&strcmp(stop,"engine_shutdown_pending")){e=stop;break;}
        if(p.guardian>0){struct timespec delay={0,10000000};nanosleep(&delay,NULL);}}
    if(!e){AudioStoreConfig store={c->admission_state,c->owner,c->provider_id};e=audio_store_publish(&store,id,pin,output,control,(void *)c,receipt);}
    if(writer>=0)close(writer);if(input>=0)close(input);if(output>=0)close(output);
    if(created){if(dir>=0){unlinkat(dir,"request.frame",0);unlinkat(dir,"result.frame",0);}rmdir(workspace);}
    if(dir>=0)close(dir);if(package>=0)close(package);if(lease>=0)close(lease);return e;
}
static void metadata(FILE *f,const AudioReceipt *r){fprintf(f,"\"audio_id\":\"%s\",\"format\":\"pcm_s16le\",\"channels\":1,\"sample_rate\":%d,\"samples\":%llu,\"bytes\":%llu,\"pcm_sha256\":\"%s\"",r->id,r->sample_rate,(unsigned long long)r->samples,(unsigned long long)r->bytes,r->pcm_sha256);}
static void package_metadata(FILE *out,const SpeechConfig *c) {
    char identity[65];speech_package_identity(c->manifest_sha256,identity);
    fprintf(out,"\"manifest_sha256\":\"%s\",\"package_identity\":\"%s\"",c->manifest_sha256,identity);
}
static void base64(FILE *f,const unsigned char *b,size_t n){static const char *abc="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";fputc('"',f);
    for(size_t i=0;i<n;i+=3){uint32_t v=(uint32_t)b[i]<<16;if(i+1<n)v|=(uint32_t)b[i+1]<<8;if(i+2<n)v|=b[i+2];
        fputc(abc[v>>18],f);fputc(abc[(v>>12)&63],f);fputc(i+1<n?abc[(v>>6)&63]:'=',f);fputc(i+2<n?abc[v&63]:'=',f);}fputc('"',f);}
int speech_dispatch(const InferenceConfig *c,const Json *j,FILE *out) {
    if(!c||!c->speech||!c->speech->self||c->speech->self[0]!='/'||!c->speech->package||c->speech->package[0]!='/'||
        !c->speech->manifest_sha256||strlen(c->speech->manifest_sha256)!=64||strspn(c->speech->manifest_sha256,"0123456789abcdef")!=64||!c->admission_state)return fail(out,"invalid_configuration");
    char *op=json_string(j,json_get(j,0,"op"));if(!op)return fail(out,"invalid_request");
    if(!strcmp(op,"describe")){free(op);if(!fields(j,"|contract_version|op|"))return fail(out,"unknown_field");
        fprintf(out,"{\"contract_version\":1,\"provider_id\":\"%s\",\"capabilities\":[\"speech_output\",\"speech_pcm_stream\",\"audio_ranges\",\"operations\",\"cancellation\",\"request_budgets\"],\"max_input_bytes\":2400,\"max_audio_range_bytes\":32768,\"availability\":\"configured_unqualified\",\"declared_package\":{",c->provider_id);
        package_metadata(out,c->speech);fputs("}}\n",out);return 0;}
    char *provider=json_string(j,json_get(j,0,"provider_id"));const char *e=!provider?"invalid_request":strcmp(provider,c->provider_id)?"provider_unavailable":NULL;
    AudioStoreConfig store={c->admission_state,c->owner,c->provider_id};AudioReceipt r;
    if(!e&&!strcmp(op,"speech_generate")) {
        char *normalized=speech_normalized(c,j),*text=json_string(j,json_get(j,0,"text")),*voice=json_string(j,json_get(j,0,"voice")),*id=json_string(j,json_get(j,0,"operation_id"));
        if(!normalized)e="invalid_request";
        else{char pin[65];hash_bytes(normalized,strlen(normalized),pin);e=run(c,text,voice,id,pin,&r);}
        if(!e){char input_sha[65];hash_bytes(text,strlen(text),input_sha);
            fprintf(out,"{\"contract_version\":1,\"provider_id\":\"%s\",",c->provider_id);metadata(out,&r);
            /* Persist observed package provenance only on successful generation.
             * Range reads never attribute old audio to the current configuration. */
            fputs(",\"provenance\":{\"validation\":\"verified_before_and_after_generation\",",out);package_metadata(out,c->speech);
            fprintf(out,",\"input_sha256\":\"%s\",\"requested_voice\":",input_sha);
            if(voice)json_quote(out,voice,strlen(voice));else fputs("null",out);fputs("}}\n",out);}
        free(normalized);free(text);free(voice);free(id);
    }else if(!e&&(!strcmp(op,"audio_read")||!strcmp(op,"audio_delete"))) {
        int reading=!strcmp(op,"audio_read");char *id=json_string(j,json_get(j,0,"audio_id"));size_t offset=0,length=0;
        if(!fields(j,reading?"|contract_version|op|provider_id|audio_id|offset|length|":"|contract_version|op|provider_id|audio_id|"))e="unknown_field";
        else if(!id||(reading&&(!json_uint(j,json_get(j,0,"offset"),&offset)||!json_uint(j,json_get(j,0,"length"),&length)||!length||length>AUDIO_RANGE_MAX)))e="invalid_request";
        unsigned char bytes[AUDIO_RANGE_MAX];
        if(!e)e=reading?audio_store_read(&store,id,offset,length,bytes,control,(void *)c,&r):audio_store_delete(&store,id);
        if(!e){fprintf(out,"{\"contract_version\":1,\"provider_id\":\"%s\",",c->provider_id);
            if(reading){metadata(out,&r);fprintf(out,",\"offset\":%zu,\"length\":%zu,\"pcm_base64\":",offset,length);base64(out,bytes,length);}
            else{fputs("\"audio_id\":",out);json_quote(out,id,strlen(id));fputs(",\"deleted\":true",out);}fputs("}\n",out);}
        free(id);
    }else if(!e)e="unsupported_operation";
    free(provider);free(op);return e?fail(out,e):0;
}
