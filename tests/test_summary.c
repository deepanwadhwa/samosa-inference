#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "summary_fixture.h"
static int engine(int argc, char **argv) {
    if (argc != 11 || strcmp(argv[1], "--model")) return -1;
    char *mode = file_read(argv[2], 128);
    unsigned char prefix[4]; CHECK(read(0, prefix, 4) == 4);
    size_t n = (size_t)prefix[0] << 24 | (size_t)prefix[1] << 16 | (size_t)prefix[2] << 8 | prefix[3];
    int batch=n==0xc0000001u; if(batch){CHECK(read(0,prefix,4)==4);n=(size_t)prefix[0]<<24|(size_t)prefix[1]<<16|(size_t)prefix[2]<<8|prefix[3];}
    CHECK(n > 11 && n <= 60011);

    char *prompt = malloc(n); CHECK(prompt); size_t used = 0;
    while (used < n) { ssize_t count = read(0, prompt + used, n - used); CHECK(count > 0); used += (size_t)count; }
    CHECK(!memcmp(prompt, "summarize: ", 11)); free(prompt);
    fputs("diagnostic must never enter the summary\n", stderr);
    if (!strcmp(mode, "sleep")) {
        const char *root = getenv("SAMOSA_SUMMARY_QA_ROOT"); CHECK(root);
        char marker[1024]; snprintf(marker, sizeof(marker), "%s/ready", root); char result_path[1024];
#ifdef __APPLE__
        CHECK(fcntl(1,F_GETPATH,result_path)>=0);
#else
        ssize_t path_bytes=readlink("/proc/self/fd/1",result_path,sizeof(result_path)-1);CHECK(path_bytes>0);result_path[path_bytes]=0;
#endif
        file_write(marker, result_path);

        for (;;) pause();
    }
    if (!strcmp(mode, "fail")) return 7;
if(!strcmp(mode,"slow-reduce")){
    size_t size=batch?4000:(n-11)*9/10;unsigned char header[4]={(unsigned char)(size>>24),(unsigned char)(size>>16),(unsigned char)(size>>8),(unsigned char)size};char large[4000];memset(large,'x',sizeof(large));write_all(1,header,4);write_all(1,large,size);free(mode);return 0;
}
    if(!strcmp(mode,"reduce")||!strcmp(mode,"no-progress")){
        if(batch||!strcmp(mode,"no-progress")){const unsigned char head[]={0,0,9,196};write_all(1,head,4);char big[2500];memset(big,'x',sizeof(big));write_all(1,big,sizeof(big));free(mode);return 0;}
    }

    if (!strcmp(mode, "flood")) { unsigned char bytes[9000]; memset(bytes, 'x', sizeof(bytes)); write_all(1, bytes, sizeof(bytes)); }
    else if (!strcmp(mode, "empty")) { const unsigned char bytes[4] = {0}; write_all(1, bytes, sizeof(bytes)); }
    else if (!strcmp(mode, "truncated")) { const unsigned char bytes[5] = {0,0,0,3,'a'}; write_all(1, bytes, sizeof(bytes)); }
    else if (!strcmp(mode, "invalid-utf8")) { const unsigned char bytes[5] = {0,0,0,1,255}; write_all(1, bytes, sizeof(bytes)); }
    else { const unsigned char bytes[] = {0,0,0,7,'s','u','m','m','a','r','y'}; write_all(1, bytes, sizeof(bytes)); }
    free(mode); return 0;
}
int main(int argc, char **argv) {
    int fixture = engine(argc, argv); if (fixture >= 0) return fixture;
    CHECK(argc == 2 && argv[0][0] == '/' && argv[1][0] == '/');
    char root[] = "/tmp/samosa-summary-qa-XXXXXX"; CHECK(mkdtemp(root));
    CHECK(!setenv("SAMOSA_SUMMARY_QA_ROOT", root, 1));
    char model[512], state[512], ready[512];
    snprintf(model, sizeof(model), "%s/model", root); snprintf(state, sizeof(state), "%s/state", root);
    snprintf(ready, sizeof(ready), "%s/ready", root);
    file_write(model, "okay");
    FixtureConfig c = {.cli=argv[1], .engine=argv[0], .model=model, .state=state, .owner="alice"};
    file_sha(c.engine, c.engine_sha); file_sha(c.model, c.model_sha);
    char *req = request("Library construction begins in March.", "summary-1", 60000);
    char *body = invoke(&c, req, NULL), *text = string_field(body, "text"); CHECK(!strcmp(text, "summary")); free(text);
    Json j; parse(body, &j); CHECK(json_get(&j, 0, "provenance") >= 0);
    CHECK(strstr(body, "bounded_encoder_and_generated_summary_uncertain"));
char *opening_request=request_opening("Library construction begins in March.","summary-1",60000);
char *conflict=invoke(&c,opening_request,"idempotency_conflict");free(conflict);free(opening_request);
opening_request=strdup("{\"contract_version\":1,\"op\":\"summarize_opening\",\"provider_id\":\"native-t5-summary-v1\",\"text\":\"Opening text.\",\"max_output_tokens\":128,\"operation_id\":\"opening-1\",\"timeout_ms\":60000}");CHECK(opening_request);
char *mapped=invoke(&c,opening_request,NULL);CHECK(strstr(mapped,"samosa-native-summary-opening-map-reduce-v1")&&strstr(mapped,"chunk_max_output_tokens\":64")&&strstr(mapped,"native_token_chunks"));char *mapped_again=invoke(&c,opening_request,NULL);CHECK(!strcmp(mapped,mapped_again));free(mapped_again);free(mapped);free(opening_request);
    char *saved = strdup(body); CHECK(saved); free(body);
    CHECK(!unlink(model)); body = invoke(&c, req, NULL); CHECK(!strcmp(body, saved)); free(body);
    FixtureConfig other = c; other.owner = "bob";
    body = invoke(&other, req, "artifact_unavailable"); free(body);
    char *changed = request("Changed content.", "summary-1", 60000);
    body = invoke(&c, changed, "idempotency_conflict"); free(body); free(changed); free(saved); free(req);
    file_write(model, "okay"); file_sha(model, c.model_sha);
    body = invoke(&c, "{\"contract_version\":1,\"op\":\"describe\"}", NULL); CHECK(strstr(body,"summarization")); free(body);
    body = invoke(&c, "{\"contract_version\":1,\"op\":\"summarize\",\"provider_id\":\"native-t5-summary-v1\",\"text\":\"x\",\"max_output_tokens\":15}", "invalid_request"); free(body);
    body = invoke(&c, "{\"contract_version\":1,\"op\":\"summarize\",\"provider_id\":\"native-t5-summary-v1\",\"text\":\"x\",\"max_output_tokens\":64,\"model\":\"client override\"}", "unknown_field"); free(body);
    req = request("text", NULL, 60000);
    FixtureConfig bad = c; memset(bad.model_sha, '0', 64); bad.model_sha[64]=0;
    body = invoke(&bad, req, "artifact_checksum_mismatch"); free(body);
    CHECK(!chmod(model, 0622)); body = invoke(&c, req, "unsupported_artifact"); free(body); CHECK(!chmod(model, 0600));
    const char *modes[] = {"empty", "truncated", "invalid-utf8", "flood", "fail"};
    for (size_t i=0;i<sizeof(modes)/sizeof(modes[0]);i++) {
        file_write(model, modes[i]); file_sha(model, c.model_sha);
        body = invoke(&c, req, i == 4 ? "provider_process_failed" : "invalid_provider_response"); free(body);
    }
for(int pass=0;pass<3;pass++){
    file_write(model,pass==2?"slow-reduce":pass?"no-progress":"reduce");file_sha(model,c.model_sha);char *opening_case=request_opening("text",NULL,60000);
        char *reduced=invoke(&c,opening_case,NULL);CHECK(strstr(reduced,pass==2?"reduction_rounds\":4":"map_output_bytes\":2500")&&strstr(reduced,pass?"output_truncated\":true":"reduction_rounds\":1"));
    char *retained=string_field(reduced,"text");CHECK(strlen(retained)==(pass?1800u:15u));free(retained);free(reduced);free(opening_case);
}
    free(req); file_write(model, "sleep"); file_sha(model, c.model_sha);
    req = request("text", "cancel-summary", 60000); Invocation active = launch(&c, req);
    int seen=0; for(int i=0;i<3000;i++){if(!access(ready,F_OK)){seen=1;break;}delay();} CHECK(seen);
    char *cancel = control("operation_cancel", "cancel-summary"); body = invoke(&c, cancel, NULL); free(body); free(cancel);
    body = finish(active, 1); error_check(body, "cancelled"); free(body); free(req); CHECK(!unlink(ready));
    req = request("text", "deadline-summary", 150); body = invoke(&c, req, "deadline_exceeded"); free(body); free(req);
    if(!access(ready,F_OK))CHECK(!unlink(ready));
    req=request("text","killed-summary",60000);active=launch(&c,req);
    seen=0;for(int i=0;i<3000;i++){if(!access(ready,F_OK)){seen=1;break;}delay();}CHECK(seen);
    char *owned_result=file_read(ready,1024);CHECK(strstr(owned_result,"/samosa-inference-summary-")&&strstr(owned_result,"/result.frame"));
    CHECK(!kill(active.pid,SIGKILL));int status;CHECK(waitpid(active.pid,&status,0)==active.pid&&WIFSIGNALED(status));close(active.output);
    seen=0;for(int i=0;i<1200;i++){if(access(owned_result,F_OK)<0&&errno==ENOENT){seen=1;break;}delay();}CHECK(seen);
    char *slash=strrchr(owned_result,'/');CHECK(slash);*slash=0;CHECK(access(owned_result,F_OK)<0&&errno==ENOENT);free(owned_result);free(req);
    file_write(model,"okay");file_sha(model,c.model_sha);req=request("text","after-killed-summary",60000);body=invoke(&c,req,NULL);free(body);free(req);
    cleanup(root);

    printf("PASS public summary: %u checks; framing, pins, replay/owner, controls and invalid responses\n", checks);
    return 0;
}
