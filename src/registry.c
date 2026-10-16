#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "registry.h"
#include "json.h"
#ifdef SAMOSA_WITH_DECISION
#define parse_request(j,s,n) json_parse_opaque(j,s,n,"request")
#else
#define parse_request(j,s,n) json_parse(j,s,n)
#endif
#include "platform.h"
#include <sys/stat.h>

static int fail(FILE *out,const char *code) {
    fprintf(out,"{\"contract_version\":1,\"error\":{\"code\":\"%s\"}}\n",code);
    return 1;
}
static int identity(const char *s,size_t limit) {
    if(!s||!*s||strlen(s)>limit)return 0;
    for(const char *p=s;*p;p++)if(!isalnum((unsigned char)*p)&&!strchr("-_:.",*p))return 0;
    return 1;
}
static int path(const char *s) {
    if(!s||s[0]!='/'||!s[1]||strlen(s)>4095)return 0;
    for(const char *p=s;*p;){
        if(*p++!='/')return 0;
        const char *start=p;while(*p&&*p!='/')p++;
        size_t n=(size_t)(p-start);
        if(!n||(n==1&&start[0]=='.')||(n==2&&start[0]=='.'&&start[1]=='.'))return 0;
    }
    return s[strlen(s)-1]!='/';
}
static const char *kind(const InferenceConfig *c) {
    if(c->decision)return "text_decisions";
    if(c->speech)return "speech_output";
    if(c->whisper)return "transcription";
    if(c->vision)return "image_analysis";
    if(c->temporal)return "video_analysis";
    if(c->summary)return "summarization";
    if(c->embedding)return "embeddings";
    return c->endpoint?"text":"fake_text";
}
static int compiled(const InferenceConfig *c) {
    (void)c;
#ifndef SAMOSA_WITH_DECISION
    if(c->decision)return 0;
#endif
#ifndef SAMOSA_WITH_SPEECH
    if(c->speech)return 0;
#endif
#ifndef SAMOSA_WITH_WHISPER
    if(c->whisper)return 0;
#endif
#ifndef SAMOSA_WITH_VISION
    if(c->vision)return 0;
#endif
#ifndef SAMOSA_WITH_TEMPORAL
    if(c->temporal)return 0;
#endif
#ifndef SAMOSA_WITH_SUMMARY
    if(c->summary)return 0;
#endif
#ifndef SAMOSA_WITH_LLAMA
    if(c->endpoint||c->embedding)return 0;
#endif
    return 1;
}
const char *inference_registry_validate(const InferenceRegistry *r) {
    if(!r||!r->providers||!r->count||r->count>INFERENCE_PROVIDERS_MAX)return "invalid_registry_configuration";
    for(size_t i=0;i<r->count;i++){
        const InferenceConfig *c=&r->providers[i];
        unsigned modalities=!!c->whisper+!!c->vision+!!c->temporal+!!c->summary+!!c->embedding+!!c->speech+!!c->decision;
        if(!identity(c->provider_id,128)||!identity(c->owner,64)||!path(c->admission_state)||
           c->context_tokens<128||c->context_tokens>131072||c->queue_capacity>8||
           c->admission_wait_ms>60000||!inference_platform_valid(c->platform)||modalities>1||
           (c->endpoint&&modalities&&!c->embedding)||(c->embedding&&!c->endpoint))return "invalid_registry_configuration";
        if(!compiled(c))return "provider_not_compiled";
        for(size_t j=0;j<i;j++){
            const InferenceConfig *prior=&r->providers[j];
            if(!strcmp(c->provider_id,prior->provider_id))return "duplicate_provider";
            if(strcmp(c->owner,prior->owner))return "registry_owner_mismatch";
            if(!strcmp(c->admission_state,prior->admission_state))return "shared_provider_state";
            struct stat a,b;
            if(!stat(c->admission_state,&a)&&!stat(prior->admission_state,&b)&&
               a.st_dev==b.st_dev&&a.st_ino==b.st_ino)return "shared_provider_state";
        }
    }
    return NULL;
}
static int work(const InferenceConfig *c,const char *op) {
    if(c->decision)return !strcmp(op,"decide_text");
    if(c->speech)return !strcmp(op,"speech_generate")||!strcmp(op,"audio_read")||!strcmp(op,"audio_delete");
    if(c->whisper)return !strcmp(op,"transcribe");
    if(c->vision)return !strcmp(op,"analyze_image");
    if(c->temporal)return !strcmp(op,"analyze_video_frames");
    if(c->summary)return !strcmp(op,"summarize")||!strcmp(op,"summarize_opening");
    if(c->embedding)return !strcmp(op,"embed_text");
    return !strcmp(op,"count_tokens")||!strcmp(op,"generate")||
           !strcmp(op,"provider_status")||!strcmp(op,"session_create")||!strcmp(op,"session_get");
}
static const char *operations(const InferenceConfig *c) {
    if(c->decision)return "\"decide_text\"";
    if(c->speech)return "\"speech_generate\",\"audio_read\",\"audio_delete\"";
    if(c->whisper)return "\"transcribe\"";
    if(c->vision)return "\"analyze_image\"";
    if(c->temporal)return "\"analyze_video_frames\"";
    if(c->summary)return "\"summarize\",\"summarize_opening\"";
    if(c->embedding)return "\"embed_text\"";
    return "\"count_tokens\",\"generate\",\"provider_status\",\"session_create\",\"session_get\"";
}
static void platform_metadata(const InferenceConfig *c,FILE *out) {
    const char *host=inference_host_platform();
    fputs(",\"host_platform\":",out);if(host)json_quote(out,host,strlen(host));else fputs("null",out);
    fputs(",\"declared_platform\":",out);if(c->platform)json_quote(out,c->platform,strlen(c->platform));else fputs("null",out);
    fprintf(out,",\"platform_policy\":\"%s\"",!c->platform?"unspecified":inference_platform_allowed(c->platform)?"compatible":"incompatible");
}
static int describe(const InferenceRegistry *r,FILE *out) {
    fputs("{\"contract_version\":1,\"component\":\"samosa-inference\",\"version\":\"0.1.0\",\"interface\":\"provider-registry-v1\",\"max_providers\":16,\"providers\":[",out);
    for(size_t i=0;i<r->count;i++){
        const InferenceConfig *c=&r->providers[i];
        if(i)fputc(',',out);
        fputs("{\"provider_id\":",out);json_quote(out,c->provider_id,strlen(c->provider_id));
        fprintf(out,",\"kind\":\"%s\",\"availability\":\"configured_unqualified\",\"operations\":[\"provider_describe\",\"operation_get\",\"operation_cancel\",%s],\"context_tokens\":%zu",kind(c),operations(c),c->context_tokens);
        platform_metadata(c,out);fputc('}',out);
    }
    fputs("]}\n",out);return ferror(out)?1:0;
}
/* Strip routing metadata for legacy controls/descriptors and embedding work.
 * Retain other fields byte-for-byte so strict validators and retry identity
 * remain authoritative. This never forwards a private configuration field. */
static char *forward(const Json *j,int descriptor,size_t *length) {
    char *body=NULL;FILE *out=open_memstream(&body,length);if(!out)return NULL;
    fputc('{',out);int comma=0;
    for(int t=1;t<j->tokens[0].next;t=j->tokens[j->tokens[t].next].next){
        char *key=json_string(j,t);int skip=key&&!strcmp(key,"provider_id");int op=key&&!strcmp(key,"op");free(key);
        if(skip)continue;
        if(comma++)fputc(',',out);
        fwrite(j->text+j->tokens[t].begin,1,j->tokens[t].end-j->tokens[t].begin,out);fputc(':',out);
        int value=j->tokens[t].next;
        if(descriptor&&op)fputs("\"describe\"",out);
        else fwrite(j->text+j->tokens[value].begin,1,j->tokens[value].end-j->tokens[value].begin,out);
    }
    fputc('}',out);
    if(fclose(out)){free(body);return NULL;}return body;
}
int inference_registry_dispatch(const InferenceRegistry *r,const char *input,size_t length,FILE *out) {
    const char *error=inference_registry_validate(r);if(error)return fail(out,error);
    if(length>INFERENCE_REQUEST_MAX)return fail(out,"request_too_large");
    Json j;size_t version;
    if(!input||!parse_request(&j,input,length)||j.tokens[0].type!=JT_OBJECT)return fail(out,"invalid_json");
    if(!json_uint(&j,json_get(&j,0,"contract_version"),&version)||version!=1)return fail(out,"unsupported_contract");
    char *op=json_string(&j,json_get(&j,0,"op"));if(!op)return fail(out,"invalid_request");
    if(!strcmp(op,"describe")){free(op);return j.tokens[0].count==2?describe(r,out):fail(out,"unknown_field");}
    char *id=json_string(&j,json_get(&j,0,"provider_id"));
    const InferenceConfig *selected=NULL;
    if(identity(id,128))for(size_t i=0;i<r->count;i++)if(!strcmp(id,r->providers[i].provider_id)){selected=&r->providers[i];break;}
    if(!selected){error=identity(id,128)?"provider_unavailable":"invalid_request";free(id);free(op);return fail(out,error);}
    int descriptor=!strcmp(op,"provider_describe");
    int control=!strcmp(op,"operation_get")||!strcmp(op,"operation_cancel")||!strcmp(op,"session_get");
    if(!descriptor&&!work(selected,op)&&strcmp(op,"operation_get")&&strcmp(op,"operation_cancel"))error="unsupported_operation";
    if(descriptor&&j.tokens[0].count!=3)error="unknown_field";
    if(error){free(id);free(op);return fail(out,error);}
    char *body=NULL;size_t bytes=length;
    if(descriptor||control||(selected->embedding&&!strcmp(op,"embed_text"))){body=forward(&j,descriptor,&bytes);if(!body){free(id);free(op);return fail(out,"resource_limit");}}
    int rc;
    if(descriptor){
        char *result=NULL;size_t result_length=0;FILE *capture=open_memstream(&result,&result_length);
        if(!capture)rc=fail(out,"resource_limit");
        else{rc=inference_dispatch_config(selected,body,bytes,capture);
            if(fclose(capture))rc=fail(out,"resource_limit");
            else if(rc)fwrite(result,1,result_length,out);
            else{while(result_length&&isspace((unsigned char)result[result_length-1]))result_length--;
                if(!result_length||result[result_length-1]!='}')rc=fail(out,"invalid_provider_response");
                else{fwrite(result,1,result_length-1,out);platform_metadata(selected,out);fputs("}\n",out);}}
            free(result);}
    }else rc=inference_dispatch_config(selected,body?body:input,bytes,out);
    free(body);free(id);free(op);return rc;
}
