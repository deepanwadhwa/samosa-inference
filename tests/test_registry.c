#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "registry.h"
#include "platform.h"
#include "embedding.h"
#include "summary.h"
#include "whisper.h"
#include "vision.h"
#include "temporal.h"
#include "temporal_package.h"
#include "speech.h"
#ifdef SAMOSA_WITH_DECISION
#include "decision.h"
#endif
#include "whisper_fixture.h"

static char *call(const InferenceRegistry *r,const char *request,const char *error) {
    char *body=NULL;size_t bytes=0;FILE *out=open_memstream(&body,&bytes);CHECK(out);
    int rc=inference_registry_dispatch(r,request,strlen(request),out);CHECK(!fclose(out));
    CHECK(rc==!!error);Json j;parse(body,&j);if(error)error_check(body,error);return body;
}
static void refused(const InferenceRegistry *r,const char *request,const char *error){free(call(r,request,error));}
static char *generate(const InferenceRegistry *r,const char *provider,const char *id,const char *session,const char *prompt,const char *error){
    char *request=NULL;size_t bytes;FILE *out=open_memstream(&request,&bytes);CHECK(out);
    fprintf(out,"{\"contract_version\":1,\"op\":\"generate\",\"provider_id\":\"%s\",\"operation_id\":\"%s\",\"prompt\":\"%s\",\"max_output_tokens\":8",provider,id,prompt);
    if(session){fputs(",\"session_id\":",out);json_quote(out,session,strlen(session));fputs(",\"expected_session_revision\":0",out);}
    fputc('}',out);CHECK(!fclose(out));char *result=call(r,request,error);free(request);return result;
}
int main(void){
    char root[]="/tmp/samosa-inference-registry-XXXXXX";CHECK(mkdtemp(root));
    char state_a[256],state_b[256];snprintf(state_a,sizeof(state_a),"%s/a",root);snprintf(state_b,sizeof(state_b),"%s/b",root);
    InferenceConfig providers[2]={
        {.provider_id="first",.owner="alice",.context_tokens=4096,.admission_state=state_a},
        {.provider_id="second",.owner="alice",.context_tokens=128,.admission_state=state_b}
    };
    InferenceRegistry registry={providers,2};CHECK(!inference_registry_validate(&registry));
    char *body=call(&registry,"{\"contract_version\":1,\"op\":\"describe\"}",NULL);
    CHECK(strstr(body,"first")&&strstr(body,"second"));CHECK(strstr(body,"configured_unqualified"));CHECK(!strstr(body,state_a));CHECK(!strstr(body,"alice"));free(body);
    body=call(&registry,"{\"contract_version\":1,\"op\":\"provider_describe\",\"provider_id\":\"second\"}",NULL);
    CHECK(strstr(body,"\"context_tokens\":128"));CHECK(strstr(body,"fake_contract_only"));free(body);
    refused(&registry,"{\"contract_version\":2,\"op\":\"describe\"}","unsupported_contract");
    refused(&registry,"{\"contract_version\":1,\"op\":\"describe\",\"owner\":\"eve\"}","unknown_field");
    refused(&registry,"{\"contract_version\":1,\"op\":\"generate\"}","invalid_request");
    refused(&registry,"{\"contract_version\":1,\"op\":\"generate\",\"provider_id\":\"foreign\"}","provider_unavailable");
    refused(&registry,"{\"contract_version\":1,\"op\":\"generate\",\"provider_id\":\"first\",\"provider_id\":\"second\"}","invalid_json");
    refused(&registry,"{\"contract_version\":1,\"op\":\"transcribe\",\"provider_id\":\"first\"}","unsupported_operation");
    refused(&registry,"{\"contract_version\":1,\"op\":\"provider_describe\",\"provider_id\":\"first\",\"model\":\"private\"}","unknown_field");
    body=call(&registry,"{\"contract_version\":1,\"op\":\"session_create\",\"provider_id\":\"first\"}",NULL);
    char *session=string_field(body,"session_id");free(body);
    char *saved=generate(&registry,"first","same-id",session,"first prompt",NULL);
    body=generate(&registry,"second","same-id",NULL,"different prompt",NULL);CHECK(strstr(body,"second"));free(body);
    body=generate(&registry,"first","same-id",session,"first prompt",NULL);CHECK(!strcmp(saved,body));free(body);
    const char *host_platform=inference_host_platform();CHECK(host_platform);
    const char *foreign_platform=!strncmp(host_platform,"linux",5)?"macos-arm64":"linux-arm64";
    providers[0].platform=foreign_platform;
    body=call(&registry,"{\"contract_version\":1,\"op\":\"describe\"}",NULL);CHECK(strstr(body,"\"platform_policy\":\"incompatible\""));CHECK(strstr(body,host_platform));free(body);
    body=call(&registry,"{\"contract_version\":1,\"op\":\"provider_describe\",\"provider_id\":\"first\"}",NULL);CHECK(strstr(body,"\"platform_policy\":\"incompatible\""));CHECK(strstr(body,"fake_contract_only"));free(body);
    /* Saved results survive a deployment-target change without executing the
     * provider. Fresh work refuses and retains that exact failed operation. */
    body=generate(&registry,"first","same-id",session,"first prompt",NULL);CHECK(!strcmp(saved,body));free(body);free(saved);
    body=generate(&registry,"first","foreign-platform",NULL,"new work","unsupported_provider_platform");free(body);
    providers[0].platform=host_platform;
    body=generate(&registry,"first","foreign-platform",NULL,"new work","unsupported_provider_platform");free(body);
    body=generate(&registry,"first","compatible-platform",NULL,"new work",NULL);free(body);
    providers[0].platform="unknown-host";CHECK(!strcmp(inference_registry_validate(&registry),"invalid_registry_configuration"));providers[0].platform=NULL;
    body=generate(&registry,"second","foreign-session",session,"prompt","unknown_session");free(body);
    char request[512];snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"session_get\",\"provider_id\":\"first\",\"session_id\":\"%s\"}",session);
    body=call(&registry,request,NULL);CHECK(strstr(body,"\"revision\":1"));free(body);
    snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"session_get\",\"provider_id\":\"second\",\"session_id\":\"%s\"}",session);
    refused(&registry,request,"unknown_session");free(session);
    body=call(&registry,"{\"contract_version\":1,\"op\":\"operation_get\",\"provider_id\":\"second\",\"operation_id\":\"same-id\"}",NULL);CHECK(strstr(body,"completed"));free(body);
    refused(&registry,"{\"contract_version\":1,\"op\":\"operation_cancel\",\"provider_id\":\"second\",\"operation_id\":\"absent\"}","unknown_operation");
    refused(&registry,"{\"contract_version\":1,\"op\":\"operation_get\",\"provider_id\":\"second\",\"operation_id\":\"same-id\",\"owner\":\"alice\"}","invalid_request");
    providers[0].owner=providers[1].owner="bob";
    refused(&registry,"{\"contract_version\":1,\"op\":\"operation_get\",\"provider_id\":\"second\",\"operation_id\":\"same-id\"}","unknown_operation");
    providers[0].owner=providers[1].owner="alice";
    const char *original=providers[1].provider_id;providers[1].provider_id="first";CHECK(!strcmp(inference_registry_validate(&registry),"duplicate_provider"));providers[1].provider_id=original;
    providers[1].owner="bob";CHECK(!strcmp(inference_registry_validate(&registry),"registry_owner_mismatch"));providers[1].owner="alice";
    providers[1].admission_state=state_a;CHECK(!strcmp(inference_registry_validate(&registry),"shared_provider_state"));providers[1].admission_state=state_b;
    char alias[256];snprintf(alias,sizeof(alias),"%s/alias",root);CHECK(!symlink(state_a,alias));providers[1].admission_state=alias;CHECK(!strcmp(inference_registry_validate(&registry),"shared_provider_state"));providers[1].admission_state=state_b;
    providers[1].admission_state="/tmp/../ambiguous";CHECK(!strcmp(inference_registry_validate(&registry),"invalid_registry_configuration"));providers[1].admission_state=state_b;
    registry.count=17;CHECK(!strcmp(inference_registry_validate(&registry),"invalid_registry_configuration"));registry.count=2;
#ifdef SAMOSA_WITH_SUMMARY
    const char *sha="0123456789abcdef0123456789abcdef0123456789abcdef0123456789abcdef";
    SummaryConfig summary={.self="/unused/self",.model="/unused/model",.executable="/unused/engine",.model_sha256=sha,.executable_sha256=sha};
    WhisperConfig whisper={.self="/unused/self",.model="/unused/model",.executable="/unused/engine",.model_sha256=sha,.executable_sha256=sha};
    VisionConfig vision={.self="/unused/self",.model="/unused/model",.executable="/unused/engine",.model_sha256=sha,.executable_sha256=sha,.config_sha256=sha,.tokenizer_sha256=sha,.weights_sha256=sha};
    char vision_sha[65];vision_model_identity(&vision,vision_sha);vision.model_sha256=vision_sha;
    TemporalConfig temporal={.self="/unused/self",.model="/unused/model",.executable="/unused/engine",.model_sha256=sha,.executable_sha256=sha,.manifest_sha256=sha};
    char temporal_sha[65];temporal_package_identity(sha,temporal_sha);temporal.model_sha256=temporal_sha;
    EmbeddingConfig embedding={.model_id="fixture",.model_revision="revision",.model_sha256=sha,.engine_sha256=sha,.tokenizer_hash=sha,.query_prefix="query: ",.document_prefix="document: ",.recipe="fixture",.dimensions=768};
    InferenceConfig mixed[7];char states[7][256];const char *ids[]={"text","summary","speech-input","image","video","embedding","speech-output"};size_t count=6;
#ifdef SAMOSA_WITH_SPEECH
    count=7;SpeechConfig speech={.self="/unused/self",.package="/unused/package",.manifest_sha256=sha};
#endif
    for(size_t i=0;i<count;i++){snprintf(states[i],sizeof(states[i]),"%s/modality%zu",root,i);mixed[i]=(InferenceConfig){.provider_id=ids[i],.owner="alice",.context_tokens=4096,.admission_state=states[i]};}
    mixed[0].endpoint="http://127.0.0.1:1";mixed[0].model_alias="fixture";
    mixed[1].summary=&summary;mixed[2].whisper=&whisper;mixed[3].vision=&vision;mixed[4].temporal=&temporal;mixed[5].embedding=&embedding;mixed[5].endpoint="http://127.0.0.1:18645";
#ifdef SAMOSA_WITH_SPEECH
    mixed[6].speech=&speech;
#endif
    InferenceRegistry modalities={mixed,count};CHECK(!inference_registry_validate(&modalities));
    body=call(&modalities,"{\"contract_version\":1,\"op\":\"describe\"}",NULL);CHECK(strstr(body,"summarize_opening")&&strstr(body,"transcribe")&&strstr(body,"analyze_video_frames")&&strstr(body,"embed_text"));free(body);
    for(size_t i=0;i<count;i++){
        snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"provider_describe\",\"provider_id\":\"%s\"}",ids[i]);
        body=call(&modalities,request,NULL);CHECK(strstr(body,ids[i]));if(i==5){CHECK(strstr(body,"text_embeddings"));CHECK(strstr(body,"\"dimensions\":768"));CHECK(!strstr(body,"fake_contract_only"));}free(body);
    }
    const char *work_ops[]={"count_tokens","summarize","transcribe","analyze_image","analyze_video_frames","embed_text","speech_generate"};
    for(size_t i=0;i<count;i++){
        mixed[i].platform=foreign_platform;
        snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"%s\",\"provider_id\":\"%s\"}",work_ops[i],ids[i]);
        refused(&modalities,request,"unsupported_provider_platform");
        mixed[i].platform=NULL;
    }
    refused(&modalities,"{\"contract_version\":1,\"op\":\"generate\",\"provider_id\":\"embedding\",\"prompt\":\"hidden text\",\"max_output_tokens\":8}","unsupported_operation");
    /* An expired trusted deadline proves embedding request validation reached
     * the adapter without requiring a model or touching a network endpoint. */
    mixed[5].deadline_monotonic=1;
    refused(&modalities,"{\"contract_version\":1,\"op\":\"embed_text\",\"provider_id\":\"embedding\",\"text\":\"hello\",\"role\":\"query\",\"operation_id\":\"embedding-route\"}","deadline_exceeded");
    refused(&modalities,"{\"contract_version\":1,\"op\":\"embed_text\",\"provider_id\":\"embedding\",\"text\":\"hello\",\"role\":\"query\",\"endpoint\":\"http://untrusted\"}","invalid_request");
    body=call(&modalities,"{\"contract_version\":1,\"op\":\"operation_get\",\"provider_id\":\"embedding\",\"operation_id\":\"embedding-route\"}",NULL);CHECK(strstr(body,"failed"));free(body);
    refused(&modalities,"{\"contract_version\":1,\"op\":\"summarize\",\"provider_id\":\"image\"}","unsupported_operation");
    body=call(&modalities,"{\"contract_version\":1,\"op\":\"summarize\",\"provider_id\":\"summary\",\"text\":\"abc\",\"max_output_tokens\":16}","artifact_unavailable");free(body);
    char *wav=silence(100),*audio=transcribe(wav,NULL,0);char *provider=strstr(audio,"whisper-base-en-v1");CHECK(provider); /* route using its existing identity */
    mixed[2].provider_id="whisper-base-en-v1";body=call(&modalities,audio,"artifact_unavailable");free(body);free(audio);free(wav);
#endif

#ifdef SAMOSA_WITH_DECISION
    const char *decision_pin="0000000000000000000000000000000000000000000000000000000000000000";
    DecisionConfig dc={.self="/unused/self",.assets="/unused/assets",.pins={decision_pin,decision_pin,decision_pin,decision_pin}};
    char decision_state[256];snprintf(decision_state,sizeof(decision_state),"%s/decision",root);
    InferenceConfig decision_cfg={.provider_id="decision",.owner="alice",.context_tokens=512,.admission_state=decision_state,.decision=&dc};
    InferenceRegistry decision_registry={&decision_cfg,1};CHECK(!inference_registry_validate(&decision_registry));
    body=call(&decision_registry,"{\"contract_version\":1,\"op\":\"provider_describe\",\"provider_id\":\"decision\"}",NULL);
    CHECK(strstr(body,"text_decisions")&&strstr(body,"relations")&&!strstr(body,"/unused"));free(body);
    refused(&decision_registry,"{\"contract_version\":1,\"op\":\"generate\",\"provider_id\":\"decision\"}","unsupported_operation");
    const char *decision_request="{\"contract_version\":1,\"op\":\"decide_text\",\"provider_id\":\"decision\",\"request\":{\"method\":\"relation\",\"state\":{\"z\":true,\"a\":2},\"proposition\":\"p\",\"contradiction\":\"c\"}}";
    Json dj;CHECK(json_parse(&dj,decision_request,strlen(decision_request)));
    char *dn=decision_normalized(&decision_cfg,&dj);CHECK(dn);
    const char *equivalent="{\"contract_version\":1,\"op\":\"decide_text\",\"provider_id\":\"decision\",\"request\":{\"method\":\"relation\",\"state\":{\"a\":2,\"z\":true},\"proposition\":\"p\",\"contradiction\":\"c\",\"threshold\":0.5}}";
    CHECK(json_parse(&dj,equivalent,strlen(equivalent)));char *de=decision_normalized(&decision_cfg,&dj);CHECK(de&&!strcmp(dn,de));free(de);
    dc.pins[DECISION_RUNTIME]="1111111111111111111111111111111111111111111111111111111111111111";
    de=decision_normalized(&decision_cfg,&dj);CHECK(de&&strcmp(dn,de));free(de);free(dn);
    const char *invalid_decisions[]={
        "{\"method\":\"choice\",\"state\":0,\"instructions\":\"q\",\"criteria\":{}}",
        "{\"method\":\"relation\",\"state\":0,\"proposition\":\"p\",\"contradiction\":\"c\",\"threshold\":2}",
        "{\"method\":\"noul\",\"state\":0,\"instructions\":\"q\",\"command\":\"private\"}"};
    for(size_t i=0;i<sizeof(invalid_decisions)/sizeof(invalid_decisions[0]);i++){
        char bad[1024];snprintf(bad,sizeof(bad),"{\"contract_version\":1,\"op\":\"decide_text\",\"provider_id\":\"decision\",\"request\":%s}",invalid_decisions[i]);
        refused(&decision_registry,bad,"invalid_request");
    }
    /* Only the owned decision payload is opaque to the envelope parser. It
     * still passes through the existing strict Rust decoder before admission. */
    const char *nul_decision="{\"contract_version\":1,\"op\":\"decide_text\",\"provider_id\":\"decision\",\"request\":{\"method\":\"choice\",\"state\":\"hi\\u0000x\",\"instructions\":\"q\",\"criteria\":{\"z\\u0000x\":\"last\",\"a\":\"first\"}}}";
    CHECK(!json_parse(&dj,nul_decision,strlen(nul_decision)));
    CHECK(json_parse_opaque(&dj,nul_decision,strlen(nul_decision),"request"));
    dn=decision_normalized(&decision_cfg,&dj);CHECK(dn&&strstr(dn,"\\u0000"));free(dn);
    const char *invalid_payloads[]={
        "{\"method\":\"noul\",\"state\":\"\\q\",\"instructions\":\"q\"}",
        "{\"method\":\"noul\",\"state\":0,\"instructions\":\"q\",}",
        "{\"method\":\"noul\",\"state\":0,\"instructions\":\"q\",\"instructions\":\"duplicate\"}",
        "{\"method\":\"noul\",\"state\":0,\"instructions\":\"q\",\"\\u0000private\":1}"};
    for(size_t i=0;i<sizeof(invalid_payloads)/sizeof(invalid_payloads[0]);i++){
        char bad[1024];snprintf(bad,sizeof(bad),"{\"contract_version\":1,\"op\":\"decide_text\",\"provider_id\":\"decision\",\"request\":%s}",invalid_payloads[i]);
        refused(&decision_registry,bad,"invalid_request");
    }
    refused(&decision_registry,"{\"contract_version\":1,\"op\":\"decide_text\",\"provider_id\":\"decision\",\"request\":{},\"request\":{}}","invalid_json");
    refused(&decision_registry,"{\"contract_version\":1,\"op\":\"decide_text\",\"provider_id\":\"decision\",\"request\":{]}","invalid_json");
    refused(&decision_registry,"{\"contract_version\":1,\"op\":\"decide_text\",\"provider_id\":\"decision\\u0000foreign\",\"request\":{}}","invalid_json");
    decision_cfg.deadline_monotonic=1;
    refused(&decision_registry,decision_request,"deadline_exceeded");
    decision_cfg.deadline_monotonic=0;decision_cfg.platform=foreign_platform;
    refused(&decision_registry,decision_request,"unsupported_provider_platform");decision_cfg.platform=NULL;
#endif
    cleanup(root);printf("PASS provider registry: %u checks; routed namespaces, owner controls, strict discovery and adapter reuse\n",checks);return 0;
}
