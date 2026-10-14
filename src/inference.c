#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "inference.h"
#include "embedding.h"
#include "sessions.h"
#include "admission.h"
#include "operations.h"
#include "platform.h"
#include "json.h"
#include "sha256.h"
#include "budget.h"
#ifdef SAMOSA_WITH_SPEECH
#include "speech.h"
#endif
#ifdef SAMOSA_WITH_VISION
#include "vision.h"
#endif
#ifdef SAMOSA_WITH_TEMPORAL
#include "temporal.h"
#endif
#ifdef SAMOSA_WITH_WHISPER
#include "whisper.h"
#endif
#ifdef SAMOSA_WITH_SUMMARY
#include "summary.h"
#endif
#include <unistd.h>
#define PROVIDER "fake-text-v1"
#define CONTEXT 131072u
static int fail(FILE *out,const char *code) {
    fprintf(out,"{\"contract_version\":1,\"error\":{\"code\":\"%s\"}}\n",code);return 1;
}
static int reference_valid(const char *value){
    if(!value||!*value||strlen(value)>128)return 0;
    for(const char *p=value;*p;p++)if(!isalnum((unsigned char)*p)&&!strchr("-_:.",*p))return 0;
    return 1;
}
static int fail_tracked(FILE *out,const char *code,const char *id,const char *parent){
    if(!reference_valid(id)||(parent&&!reference_valid(parent)))return fail(out,code);
    fputs("{\"contract_version\":1,\"operation_id\":",out);json_quote(out,id,strlen(id));
    if(parent){fputs(",\"parent_operation_id\":",out);json_quote(out,parent,strlen(parent));}
    fputs(",\"error\":{\"code\":",out);json_quote(out,code,strlen(code));fputs("}}\n",out);return 1;
}
static int dispatch_core(const InferenceConfig *cfg,const char *input,size_t length,FILE *out) {
    const char *identity=cfg?cfg->provider_id:PROVIDER;
    size_t context=cfg?cfg->context_tokens:CONTEXT;
    int native=cfg&&cfg->endpoint;
    if(cfg&&(!inference_platform_valid(cfg->platform)||cfg->queue_capacity>8||cfg->admission_wait_ms>60000||
             (cfg->queue_capacity&&!cfg->admission_state)))return fail(out,"invalid_configuration");
    if(!identity||!*identity||strlen(identity)>128||context<128||context>131072)return fail(out,"invalid_configuration");
    for(const char *p=identity;*p;p++)if(!isalnum((unsigned char)*p)&&!strchr("-_:.",*p))return fail(out,"invalid_configuration");
#ifndef SAMOSA_WITH_LLAMA
    if(native)return fail(out,"provider_unavailable");
#endif
    if(length>INFERENCE_REQUEST_MAX)return fail(out,"request_too_large");
    Json request;size_t contract;
    if(!json_parse(&request,input,length)||request.tokens[0].type!=JT_OBJECT)return fail(out,"invalid_json");
    if(!json_uint(&request,json_get(&request,0,"contract_version"),&contract)||contract!=1)return fail(out,"unsupported_contract");
    if(cfg&&!inference_platform_allowed(cfg->platform)){
        char *work=json_string(&request,json_get(&request,0,"op"));
        int executes=work&&(!strcmp(work,"generate")||!strcmp(work,"count_tokens")||!strcmp(work,"provider_status")||
            !strcmp(work,"embed_text")||!strcmp(work,"summarize")||!strcmp(work,"summarize_opening")||
            !strcmp(work,"transcribe")||!strcmp(work,"analyze_image")||!strcmp(work,"analyze_video_frames")||!strcmp(work,"speech_generate"));
        free(work);
        /* Tracked replay happens before dispatch_core. Foreign deployment
         * targets refuse new model work while preserving retained data access. */
        if(executes)return fail(out,"unsupported_provider_platform");
    }
#ifdef SAMOSA_WITH_WHISPER
    if(cfg&&cfg->whisper)return whisper_dispatch(cfg,&request,out);
#endif
#ifdef SAMOSA_WITH_VISION
    if(cfg&&cfg->vision)return vision_dispatch(cfg,&request,out);
#endif
#ifdef SAMOSA_WITH_TEMPORAL
    if(cfg&&cfg->temporal)return temporal_dispatch(cfg,&request,out);
#endif
    #ifdef SAMOSA_WITH_SUMMARY
    if(cfg&&cfg->summary)return summary_dispatch(cfg,&request,out);
#endif
    #ifdef SAMOSA_WITH_SPEECH
    if(cfg&&cfg->speech)return speech_dispatch(cfg,&request,out);
#endif
    if(cfg&&cfg->embedding){char *embedding_op=json_string(&request,json_get(&request,0,"op"));int selected=embedding_op&&!strcmp(embedding_op,"embed_text"),descriptor=embedding_op&&!strcmp(embedding_op,"describe");free(embedding_op);if(descriptor)return embedding_describe(cfg,&request,out);if(selected)return embedding_dispatch(cfg,&request,out);}
    char *op=json_string(&request,json_get(&request,0,"op"));if(!op)return fail(out,"invalid_request");
    const char *fields=!strcmp(op,"describe")?"|op|contract_version|":!strcmp(op,"provider_status")?"|op|contract_version|provider_id|":!strcmp(op,"count_tokens")?"|op|contract_version|provider_id|prompt|operation_id|parent_operation_id|timeout_ms|":!strcmp(op,"generate")?"|op|contract_version|provider_id|prompt|max_output_tokens|session_id|expected_session_revision|operation_id|parent_operation_id|timeout_ms|":!strcmp(op,"session_create")?"|op|contract_version|provider_id|":!strcmp(op,"session_get")?"|op|contract_version|session_id|":NULL;
    const char *error=fields?NULL:"unsupported_operation";
    for(int t=1;!error&&t<request.tokens[0].next;t=request.tokens[request.tokens[t].next].next) {
        char *key=json_string(&request,t),match[128];int n=snprintf(match,sizeof(match),"|%s|",key?key:"");free(key);
        if(n<0||(size_t)n>=sizeof(match)||!strstr(fields,match))error="unknown_field";
    }
    if(error){free(op);return fail(out,error);}
    int session_capable=cfg&&cfg->admission_state&&cfg->owner;
    if(!strcmp(op,"describe")) {
        fprintf(out,"{\"contract_version\":1,\"component\":\"samosa-inference\",\"version\":\"0.1.0\",\"provider_id\":\"%s\",\"capabilities\":[\"text\",\"count_tokens\",\"request_budgets\",\"provider_status\"%s],\"context_tokens\":%zu,\"tokenizer\":\"%s\",\"quality\":\"%s\",\"admission\":{\"configured\":%s,\"active_capacity\":1,\"queue_capacity\":%zu,\"wait_deadline_ms\":%zu,\"order\":\"fifo\"}}\n",identity,session_capable?",\"sessions\",\"operations\",\"cancellation\",\"operation_parents\"":"",context,native?"llama-native-template-v1":"fake-utf8-byte-v1",native?"native":"fake_contract_only",cfg&&cfg->admission_state?"true":"false",cfg?cfg->queue_capacity:0,cfg&&cfg->admission_wait_ms?cfg->admission_wait_ms:10000);
        free(op);return 0;
    }
    char *session=json_string(&request,json_get(&request,0,"session_id"));
    if(!strcmp(op,"session_get")) {
        int rc=session?session_public(cfg,op,session,out):fail(out,"invalid_request");free(session);free(op);return rc;
    }
    char *provider=json_string(&request,json_get(&request,0,"provider_id"));
    if(!provider)error="invalid_request";else if(strcmp(provider,identity))error="provider_unavailable";
    if(!strcmp(op,"provider_status")) {
        int lease=-1;
        if(!error && native) {
            error=inference_schedule(cfg,&lease);
#ifdef SAMOSA_WITH_LLAMA
            if(!error)error=llama_properties(cfg);
#endif
        }
        if(lease>=0)close(lease);
        int owned=native&&cfg&&cfg->owned_status;
        if(!error)fprintf(out,"{\"contract_version\":1,\"provider_id\":\"%s\",\"status\":\"%s\",\"ready\":%s,\"artifact_identity\":\"%s\"}\n",
            identity,owned?"ready":native?"metadata_compatible":"fake_contract_only",owned?"true":"false",owned?"owned_system_runtime_v1":native?"unqualified_external_engine":"not_applicable");
        free(provider);free(session);free(op);return error?fail(out,error):0;
    }
    if(!strcmp(op,"session_create")) {
        int rc=error?fail(out,error):session_public(cfg,op,NULL,out);free(provider);free(session);free(op);return rc;
    }
    char *prompt=json_string(&request,json_get(&request,0,"prompt"));size_t reserve=0,expected=0;
    int generation=!strcmp(op,"generate"),session_begin_ok=0,lock=-1;
    if(!prompt)error="invalid_request";
    else if(generation&&(!json_uint(&request,json_get(&request,0,"max_output_tokens"),&reserve)||!reserve||reserve>4096))error="invalid_budget";
    int has_session=json_get(&request,0,"session_id")>=0,has_revision=json_get(&request,0,"expected_session_revision")>=0;
    if(!error && generation&&(has_session!=has_revision||(has_session&&(!session||!json_uint(&request,json_get(&request,0,"expected_session_revision"),&expected)||expected>=2147483647u))))error="invalid_request";
    if(!error&&session&&!session_capable)error="state_unavailable";
    size_t count=prompt?strlen(prompt):0,output=0,processed=0;char *answer=NULL;
    if(!error&&!native&&generation&&(count>context||reserve>context-count))error="context_limit";
    if(!error)error=inference_interrupted(cfg);
    if(!error&&(native||(cfg&&cfg->admission_state)))error=inference_schedule(cfg,&lock);
    if(!error&&session){error=session_begin(cfg,session,expected,prompt);session_begin_ok=!error;}
    if(!error&&native) {
#ifdef SAMOSA_WITH_LLAMA
        error=generation?llama_generate(cfg,prompt,reserve,&answer,&count,&output,&processed):llama_count(cfg,prompt,&count);
#endif
    } else if(!error&&generation) {
        const char *fake="Fake answer; native contract test only.";output=strlen(fake);if(output>reserve)output=reserve;
        answer=malloc(output+1);if(!answer)error="resource_limit";else {memcpy(answer,fake,output);answer[output]=0;processed=count;}
    }
    if(!error&&answer&&strlen(answer)>65536)error="response_too_large";
    if(!error)error=inference_interrupted(cfg);
    if(session_begin_ok) {
        const char *commit=session_finish(cfg,session,expected,prompt,error?NULL:answer);
        if(!error&&commit)error=commit;
    }
    if(lock>=0)close(lock);
    if(!error&&!generation) {
        fprintf(out,"{\"contract_version\":1,\"provider_id\":\"%s\",\"input_tokens\":%zu,\"context_tokens\":%zu,\"tokenizer\":\"%s\"}\n",identity,count,context,native?"llama-native-template-v1":"fake-utf8-byte-v1");
    } else if(!error) {
        fprintf(out,"{\"contract_version\":1,\"provider_id\":\"%s\",\"answer\":",identity);json_quote(out,answer,strlen(answer));
        fprintf(out,",\"usage\":{\"input_tokens\":%zu,\"output_tokens\":%zu,\"processed_input_tokens\":%zu},\"quality\":\"%s\"",count,output,processed,native?"native":"fake_contract_only");
        if(session){fputs(",\"session_id\":",out);json_quote(out,session,strlen(session));fprintf(out,",\"session_revision\":%zu",expected+1);}fputs("}\n",out);
    }
    free(provider);free(prompt);free(op);free(session);free(answer);return error?fail(out,error):0;
}
static char *normalized(const InferenceConfig *cfg,const Json *j,const char *op){
#ifdef SAMOSA_WITH_SPEECH
    if(!strcmp(op,"speech_generate")&&cfg&&cfg->speech)return speech_normalized(cfg,j);
#endif
    if(!strcmp(op,"embed_text")&&cfg&&cfg->embedding)return embedding_normalized(cfg,j);
#ifdef SAMOSA_WITH_SUMMARY
    if((!strcmp(op,"summarize")||!strcmp(op,"summarize_opening"))&&cfg&&cfg->summary)return summary_normalized(cfg,j);
#endif
#ifdef SAMOSA_WITH_VISION
    if(!strcmp(op,"analyze_image")&&cfg&&cfg->vision)return vision_normalized(cfg,j);
#endif
#ifdef SAMOSA_WITH_TEMPORAL
    if(!strcmp(op,"analyze_video_frames")&&cfg&&cfg->temporal)return temporal_normalized(cfg,j);
#endif
#ifdef SAMOSA_WITH_WHISPER
    if(!strcmp(op,"transcribe")&&cfg&&cfg->whisper)return whisper_normalized(cfg,j);
#endif
    const char *allowed=!strcmp(op,"generate")?"|op|contract_version|operation_id|parent_operation_id|timeout_ms|provider_id|prompt|max_output_tokens|session_id|expected_session_revision|":"|op|contract_version|operation_id|parent_operation_id|timeout_ms|provider_id|prompt|";
    for(int t=1;t<j->tokens[0].next;t=j->tokens[j->tokens[t].next].next){char *key=json_string(j,t),field[128];int n=snprintf(field,sizeof(field),"|%s|",key?key:"");free(key);if(n<0||(size_t)n>=sizeof(field)||!strstr(allowed,field))return NULL;}
    char *provider=json_string(j,json_get(j,0,"provider_id")),*prompt=json_string(j,json_get(j,0,"prompt"));
    char *session=json_string(j,json_get(j,0,"session_id")),*parent=json_string(j,json_get(j,0,"parent_operation_id"));size_t reserve=0,revision=0,budget=0;int budget_token=json_get(j,0,"timeout_ms");
    int has_parent=json_get(j,0,"parent_operation_id")>=0,parent_valid=!has_parent||(parent&&*parent&&strlen(parent)<=128);
    if(parent)for(const char *p=parent;*p;p++)if(!isalnum((unsigned char)*p)&&!strchr("-_:.",*p))parent_valid=0;
    int generation=!strcmp(op,"generate"),has_session=json_get(j,0,"session_id")>=0,has_revision=json_get(j,0,"expected_session_revision")>=0;
    int valid=(budget_token<0||(json_uint(j,budget_token,&budget)&&budget&&budget<=60000))&&parent_valid&&provider&&prompt&&(!generation||(json_uint(j,json_get(j,0,"max_output_tokens"),&reserve)&&reserve&&reserve<=4096))&&
              has_session==has_revision&&(!has_session||(session&&json_uint(j,json_get(j,0,"expected_session_revision"),&revision)&&revision<2147483647u));
    char *out=NULL;size_t length=0;FILE *stream=valid?open_memstream(&out,&length):NULL;
    if(stream){fputs("{\"op\":",stream);json_quote(stream,op,strlen(op));fputs(",\"provider_id\":",stream);json_quote(stream,provider,strlen(provider));
        fputs(",\"configured_provider\":",stream);if(cfg&&cfg->provider_id)json_quote(stream,cfg->provider_id,strlen(cfg->provider_id));else fputs("null",stream);
        fprintf(stream,",\"context\":%zu,\"model_alias\":",cfg?cfg->context_tokens:0);if(cfg&&cfg->model_alias)json_quote(stream,cfg->model_alias,strlen(cfg->model_alias));else fputs("null",stream);
        fputs(",\"prompt\":",stream);json_quote(stream,prompt,strlen(prompt));fprintf(stream,",\"reserve\":%zu,\"revision\":%zu,\"session\":",reserve,revision);
        if(session)json_quote(stream,session,strlen(session));else fputs("null",stream);
        if(parent){fputs(",\"parent_operation_id\":",stream);json_quote(stream,parent,strlen(parent));}if(budget_token>=0)fprintf(stream,",\"timeout_ms\":%zu",budget);fputc('}',stream);
        if(fclose(stream)){free(out);out=NULL;}
    }
    free(provider);free(prompt);free(session);free(parent);return out;
}
int inference_dispatch_config(const InferenceConfig *cfg,const char *input,size_t length,FILE *out){
    double received=inference_now();
    if(length>INFERENCE_REQUEST_MAX)return fail(out,"request_too_large");
    Json j;size_t version;
    if(!json_parse(&j,input,length)||j.tokens[0].type!=JT_OBJECT||!json_uint(&j,json_get(&j,0,"contract_version"),&version)||version!=1)return dispatch_core(cfg,input,length,out);
    char *op=json_string(&j,json_get(&j,0,"op")),*id=json_string(&j,json_get(&j,0,"operation_id"));
    size_t budget=0;int budget_token=json_get(&j,0,"timeout_ms");InferenceConfig scoped={.provider_id=PROVIDER,.context_tokens=CONTEXT,.owner="local"};if(cfg)scoped=*cfg;
    int transcription=0,visual=0,summarization=0,temporal=0,speech=0;
#ifdef SAMOSA_WITH_SPEECH
    speech=cfg&&cfg->speech&&op&&!strcmp(op,"speech_generate");
    if(speech&&budget_token<0){if(received<0){free(op);free(id);return fail(out,"state_unavailable");}double deadline=received+60;if(!scoped.deadline_monotonic||deadline<scoped.deadline_monotonic)scoped.deadline_monotonic=deadline;cfg=&scoped;}
#endif
    int embedding=cfg&&cfg->embedding&&op&&!strcmp(op,"embed_text");
    if(embedding&&budget_token<0){if(received<0){free(op);free(id);return fail(out,"state_unavailable");}double deadline=received+60;if(!scoped.deadline_monotonic||deadline<scoped.deadline_monotonic)scoped.deadline_monotonic=deadline;cfg=&scoped;}
#ifdef SAMOSA_WITH_SUMMARY
    summarization=cfg&&cfg->summary&&op&&(!strcmp(op,"summarize")||!strcmp(op,"summarize_opening"));
    if(summarization&&budget_token<0){if(received<0){free(op);free(id);return fail(out,"state_unavailable");}double deadline=received+60;if(!scoped.deadline_monotonic||deadline<scoped.deadline_monotonic)scoped.deadline_monotonic=deadline;cfg=&scoped;}
#endif
#ifdef SAMOSA_WITH_VISION
    visual=cfg&&cfg->vision&&op&&!strcmp(op,"analyze_image");
    if(visual&&budget_token<0){if(received<0){free(op);free(id);return fail(out,"state_unavailable");}double deadline=received+60;if(!scoped.deadline_monotonic||deadline<scoped.deadline_monotonic)scoped.deadline_monotonic=deadline;cfg=&scoped;}
#endif
#ifdef SAMOSA_WITH_WHISPER
    transcription=cfg&&cfg->whisper&&op&&!strcmp(op,"transcribe");
    if(cfg&&cfg->whisper&&op&&!strcmp(op,"transcribe")&&budget_token<0){if(received<0){free(op);free(id);return fail(out,"state_unavailable");}double deadline=received+60;if(!scoped.deadline_monotonic||deadline<scoped.deadline_monotonic)scoped.deadline_monotonic=deadline;cfg=&scoped;}
#endif
#ifdef SAMOSA_WITH_TEMPORAL
    temporal=cfg&&cfg->temporal&&op&&!strcmp(op,"analyze_video_frames");
    if(temporal&&budget_token<0){if(received<0){free(op);free(id);return fail(out,"state_unavailable");}double deadline=received+120;if(!scoped.deadline_monotonic||deadline<scoped.deadline_monotonic)scoped.deadline_monotonic=deadline;cfg=&scoped;}
#endif
    if(budget_token>=0){if(!op||(strcmp(op,"generate")&&strcmp(op,"count_tokens")&&!transcription&&!visual&&!summarization&&!embedding&&!temporal&&!speech)||!json_uint(&j,budget_token,&budget)||!budget||budget>(temporal?120000u:60000u)){free(op);free(id);return fail(out,"invalid_request");}if(received<0){free(op);free(id);return fail(out,"state_unavailable");}double deadline=received+(double)budget/1000;if(!scoped.deadline_monotonic||deadline<scoped.deadline_monotonic)scoped.deadline_monotonic=deadline;cfg=&scoped;}
    if(op&&(!strcmp(op,"operation_get")||!strcmp(op,"operation_cancel"))){
        int valid=j.tokens[0].count==3&&id;
        int rc=valid?operation_public(cfg,op,id,out):fail(out,"invalid_request");free(op);free(id);return rc;
    }
    if(json_get(&j,0,"parent_operation_id")>=0&&json_get(&j,0,"operation_id")<0){free(op);free(id);return fail(out,"invalid_request");}
    if(!op||json_get(&j,0,"operation_id")<0){free(op);free(id);return dispatch_core(cfg,input,length,out);}
    if(!id||(strcmp(op,"generate")&&strcmp(op,"count_tokens")&&!transcription&&!visual&&!summarization&&!embedding&&!temporal&&!speech)){free(op);free(id);return fail(out,"invalid_request");}
    char *request=normalized(cfg,&j,op);free(op);
    if(!request){free(id);return fail(out,"invalid_request");}
    char digest[65];hash_bytes(request,strlen(request),digest);free(request);
    InferenceOperation operation;char *saved=NULL;
    char *parent=json_string(&j,json_get(&j,0,"parent_operation_id"));
    if(json_get(&j,0,"parent_operation_id")>=0&&!parent){free(id);return fail(out,"resource_limit");}
    const char *error=operation_begin_parent(cfg,id,digest,parent,&operation,&saved);
    if(error){operation_close(&operation);int rc=fail_tracked(out,error,id,parent);free(id);free(parent);return rc;}
    free(id);free(parent);
    if(saved){
        Json response;size_t contract;int valid=json_parse(&response,saved,strlen(saved))&&
            json_uint(&response,json_get(&response,0,"contract_version"),&contract)&&contract==1;
        char *saved_id=valid?json_string(&response,json_get(&response,0,"operation_id")):NULL;
        /* A cached operation has no active lease/id allocation; compare request ID. */
        char *requested_id=json_string(&j,json_get(&j,0,"operation_id"));
        valid=valid&&saved_id&&requested_id&&!strcmp(saved_id,requested_id);
        char *requested_parent=json_string(&j,json_get(&j,0,"parent_operation_id")),*saved_parent=valid?json_string(&response,json_get(&response,0,"parent_operation_id")):NULL;
        valid=valid&&((!requested_parent&&!saved_parent)||(requested_parent&&saved_parent&&!strcmp(requested_parent,saved_parent)));
        free(requested_parent);free(saved_parent);
        int failed=valid&&json_get(&response,0,"error")>=0;
        free(saved_id);free(requested_id);
        if(valid)fputs(saved,out);
        free(saved);operation_close(&operation);
        return valid?failed:fail(out,"corrupt_operation");
    }
    InferenceConfig tracked=*cfg;tracked.cancelled=operation_cancelled;tracked.cancellation=&operation;
    operation.parent_cancelled=cfg->cancelled;operation.parent_cancellation=cfg->cancellation;
    char *response=NULL;size_t response_length=0;FILE *stream=open_memstream(&response,&response_length);
    if(!stream){operation_close(&operation);return fail(out,"resource_limit");}
    int rc=dispatch_core(&tracked,input,length,stream);int closed=fclose(stream);
    if(closed)error="resource_limit";
    else{
        while(response_length&&isspace((unsigned char)response[response_length-1]))response_length--;
        char *qualified=NULL;size_t qualified_length=0;FILE *qualified_stream=open_memstream(&qualified,&qualified_length);
        if(!qualified_stream)error="resource_limit";
        else{
            if(response_length&&response[response_length-1]=='}'){
                fwrite(response,1,response_length-1,qualified_stream);fputs(",\"operation_id\":",qualified_stream);
                json_quote(qualified_stream,operation.id,strlen(operation.id));
                int parent_token=json_get(&j,0,"parent_operation_id");if(parent_token>=0){fputs(",\"parent_operation_id\":",qualified_stream);fwrite(input+j.tokens[parent_token].begin,1,j.tokens[parent_token].end-j.tokens[parent_token].begin,qualified_stream);}fputs("}\n",qualified_stream);
            }
            if(fclose(qualified_stream)||!qualified_length)error="invalid_result";
            else error=operation_finish(&operation,qualified,rc);
        }
        free(response);response=qualified;
    }
    operation_close(&operation);
    if(error){free(response);return fail(out,error);}
    fputs(response,out);free(response);return rc;
}
int inference_dispatch(const char *input,size_t length,FILE *out) {
    return inference_dispatch_config(NULL,input,length,out);
}
