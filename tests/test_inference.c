#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "inference.h"
#include "json.h"
#include <assert.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static char *run(const char *req,int expected) {
    char *out=NULL;size_t n;FILE *stream=open_memstream(&out,&n);CHECK(stream);
    CHECK(inference_dispatch(req,strlen(req),stream)==expected);CHECK(!fclose(stream));
    Json j;CHECK(json_parse(&j,out,n));return out;
}
static char *describe(const InferenceConfig *cfg) {
    const char *req="{\"contract_version\":1,\"op\":\"describe\"}";
    char *out=NULL;size_t n;FILE *stream=open_memstream(&out,&n);CHECK(stream);
    CHECK(!inference_dispatch_config(cfg,req,strlen(req),stream));CHECK(!fclose(stream));
    Json j;CHECK(json_parse(&j,out,n));return out;
}
static void error(const char *req,const char *code){char *out=run(req,1);CHECK(strstr(out,code));free(out);}
int main(void) {
    char *status=run("{\"contract_version\":1,\"op\":\"provider_status\",\"provider_id\":\"fake-text-v1\"}",0);
    CHECK(strstr(status,"\"ready\":false"));CHECK(strstr(status,"\"status\":\"fake_contract_only\""));free(status);
    error("{\"contract_version\":1,\"op\":\"provider_status\",\"provider_id\":\"foreign\"}","provider_unavailable");
    char *out=run("{\"contract_version\":1,\"op\":\"describe\"}",0);CHECK(strstr(out,"fake_contract_only"));CHECK(strstr(out,"fake-utf8-byte-v1"));free(out);
    out=describe(NULL);CHECK(strstr(out,"\"configured\":false"));CHECK(strstr(out,"\"active_capacity\":1"));CHECK(strstr(out,"\"queue_capacity\":0"));CHECK(strstr(out,"\"wait_deadline_ms\":10000"));CHECK(strstr(out,"\"order\":\"fifo\""));CHECK(!strstr(out,"\"operations\""));CHECK(!strstr(out,"\"cancellation\""));free(out);
    InferenceConfig cfg={.provider_id="fake-text-v1",.context_tokens=4096,.admission_state="/unused-describe-state",.owner="alice",.queue_capacity=4,.admission_wait_ms=1234};
    out=describe(&cfg);CHECK(strstr(out,"\"configured\":true"));CHECK(strstr(out,"\"queue_capacity\":4"));CHECK(strstr(out,"\"wait_deadline_ms\":1234"));CHECK(strstr(out,"\"sessions\""));CHECK(strstr(out,"\"operations\""));CHECK(strstr(out,"\"cancellation\""));free(out);
    cfg.owner=NULL;out=describe(&cfg);CHECK(!strstr(out,"\"sessions\""));CHECK(!strstr(out,"\"operations\""));CHECK(!strstr(out,"\"cancellation\""));free(out);
    out=run("{\"contract_version\":1,\"op\":\"count_tokens\",\"provider_id\":\"fake-text-v1\",\"prompt\":\"\\u00e9\"}",0);CHECK(strstr(out,"\"input_tokens\":2"));free(out);
    out=run("{\"contract_version\":1,\"op\":\"generate\",\"provider_id\":\"fake-text-v1\",\"prompt\":\"abc\",\"max_output_tokens\":4}",0);CHECK(strstr(out,"\"answer\":\"Fake\""));CHECK(strstr(out,"\"input_tokens\":3"));CHECK(strstr(out,"\"output_tokens\":4"));free(out);
    error("{\"contract_version\":1,\"op\":\"generate\",\"provider_id\":\"unknown\",\"prompt\":\"abc\",\"max_output_tokens\":4}","provider_unavailable");
    error("{\"contract_version\":1,\"op\":\"generate\",\"provider_id\":\"fake-text-v1\",\"prompt\":\"abc\",\"max_output_tokens\":0}","invalid_budget");
    error("{\"contract_version\":1,\"op\":\"generate\",\"provider_id\":\"fake-text-v1\",\"prompt\":\"abc\",\"max_output_tokens\":1.5}","invalid_budget");
    error("{\"contract_version\":1,\"op\":\"generate\",\"provider_id\":\"fake-text-v1\",\"prompt\":\"abc\",\"max_output_tokens\":4097}","invalid_budget");
    error("{\"contract_version\":1,\"op\":\"describe\",\"model\":\"private\"}","unknown_field");
    error("{\"contract_version\":1,\"op\":\"vision\"}","unsupported_operation");
    error("{\"contract_version\":1,\"op\":\"describe\",\"op\":\"generate\"}","invalid_json");
    size_t n=131072;char *large=malloc(n+256);CHECK(large);
    const char *prefix="{\"contract_version\":1,\"op\":\"generate\",\"provider_id\":\"fake-text-v1\",\"prompt\":\"";
    size_t off=strlen(prefix);memcpy(large,prefix,off);memset(large+off,'x',n);strcpy(large+off+n,"\",\"max_output_tokens\":1}");
    error(large,"context_limit");free(large);
    printf("PASS native fake Inference: %u checks; contract only\n",checks);return 0;
}
