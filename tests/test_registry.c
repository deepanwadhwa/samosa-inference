#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "registry.h"
#include "embedding.h"
#include "summary.h"
#include "whisper.h"
#include "vision.h"
#include "temporal.h"
#include "temporal_package.h"
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
    body=generate(&registry,"first","same-id",session,"first prompt",NULL);CHECK(!strcmp(saved,body));free(body);free(saved);
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
    InferenceConfig mixed[6];char states[6][256];const char *ids[]={"text","summary","speech-input","image","video","embedding"};
    for(size_t i=0;i<6;i++){snprintf(states[i],sizeof(states[i]),"%s/modality%zu",root,i);mixed[i]=(InferenceConfig){.provider_id=ids[i],.owner="alice",.context_tokens=4096,.admission_state=states[i]};}
    mixed[1].summary=&summary;mixed[2].whisper=&whisper;mixed[3].vision=&vision;mixed[4].temporal=&temporal;mixed[5].embedding=&embedding;mixed[5].endpoint="http://127.0.0.1:18645";
    InferenceRegistry modalities={mixed,6};CHECK(!inference_registry_validate(&modalities));
    body=call(&modalities,"{\"contract_version\":1,\"op\":\"describe\"}",NULL);CHECK(strstr(body,"summarize_opening")&&strstr(body,"transcribe")&&strstr(body,"analyze_video_frames")&&strstr(body,"embed_text"));free(body);
    for(size_t i=0;i<6;i++){
        snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"provider_describe\",\"provider_id\":\"%s\"}",ids[i]);
        body=call(&modalities,request,NULL);CHECK(strstr(body,ids[i]));if(i==5){CHECK(strstr(body,"text_embeddings"));CHECK(strstr(body,"\"dimensions\":768"));CHECK(!strstr(body,"fake_contract_only"));}free(body);
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
    cleanup(root);printf("PASS provider registry: %u checks; routed namespaces, owner controls, strict discovery and adapter reuse\n",checks);return 0;
}
