#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "whisper_fixture.h"
/* Existing native T5 framing, reused without a new provider protocol. */
static int engine(int argc,char **argv) {
    if(argc!=11||strcmp(argv[1],"--model"))return -1;
    char *mode=file_read(argv[2],128);unsigned char prefix[4];CHECK(read(0,prefix,4)==4);
    size_t bytes=(size_t)prefix[0]<<24|(size_t)prefix[1]<<16|(size_t)prefix[2]<<8|prefix[3];CHECK(bytes>11&&bytes<=60011);
    char *text=malloc(bytes);CHECK(text);size_t used=0;
    while(used<bytes){ssize_t n=read(0,text+used,bytes-used);CHECK(n>0);used+=(size_t)n;}CHECK(!memcmp(text,"summarize: ",11));free(text);
    if(!strcmp(mode,"sleep")){
        const char *marker=getenv("SAMOSA_REGISTRY_QA_MARKER");CHECK(marker);file_write(marker,"engine started");for(;;)pause();
    }
    const unsigned char reply[]={0,0,0,7,'s','u','m','m','a','r','y'};write_all(1,reply,sizeof(reply));free(mode);return 0;
}
static char *call(const char *cli,const char *config,const char *request,int status,const char *error) {
    char *args[]={(char*)cli,"--config",(char*)config,NULL};char *body=finish(spawn_request(args,request),status);
    Json j;parse(body,&j);if(error)error_check(body,error);return body;
}
static void config_write(const char *path,const char *root,const char *engine,const char *model,const char *engine_sha,const char *model_sha,const char *owner) {
    char *body=NULL;size_t bytes;FILE *out=open_memstream(&body,&bytes);CHECK(out);
    fprintf(out,"{\"contract_version\":1,\"owner\":\"%s\",\"providers\":[{\"kind\":\"fake_text\",\"provider_id\":\"text\",\"context_tokens\":128,\"state\":\"%s/text\"},{\"kind\":\"summarization\",\"provider_id\":\"summary\",\"state\":\"%s/summary\",\"engine\":\"%s\",\"model\":\"%s\",\"engine_sha256\":\"%s\",\"model_sha256\":\"%s\"}]}",owner,root,root,engine,model,engine_sha,model_sha);
    CHECK(!fclose(out));file_write(path,body);free(body);
}
int main(int argc,char **argv) {
    int fixture=engine(argc,argv);if(fixture>=0)return fixture;
    CHECK(argc==2);char self[4096];CHECK(realpath(argv[0],self));
    char root[]="/tmp/samosa-registry-cli-XXXXXX";CHECK(mkdtemp(root));
    char config[256],model[256],worker[256],marker[256],alias[256];
    snprintf(config,sizeof(config),"%s/config.json",root);snprintf(model,sizeof(model),"%s/model",root);
    snprintf(worker,sizeof(worker),"%s/engine",root);snprintf(marker,sizeof(marker),"%s/marker",root);snprintf(alias,sizeof(alias),"%s/alias",root);
    file_copy(self,worker);file_write(model,"ok");char engine_sha[65],model_sha[65];file_sha(worker,engine_sha);file_sha(model,model_sha);
    config_write(config,root,worker,model,engine_sha,model_sha,"alice");
    char *body=call(argv[1],config,"{\"contract_version\":1,\"op\":\"describe\"}",0,NULL);
    CHECK(strstr(body,"text")&&strstr(body,"summarize"));CHECK(!strstr(body,root));CHECK(!strstr(body,engine_sha));free(body);
    body=call(argv[1],config,"{\"contract_version\":1,\"op\":\"provider_describe\",\"provider_id\":\"summary\"}",0,NULL);CHECK(strstr(body,"native_t5"));free(body);
    const char *request="{\"contract_version\":1,\"op\":\"summarize\",\"provider_id\":\"summary\",\"text\":\"Existing native engine transport\",\"max_output_tokens\":16,\"operation_id\":\"same-id\"}";
    char *saved=call(argv[1],config,request,0,NULL);CHECK(strstr(saved,"\"text\":\"summary\""));CHECK(strstr(saved,engine_sha));
    body=call(argv[1],config,"{\"contract_version\":1,\"op\":\"generate\",\"provider_id\":\"text\",\"prompt\":\"text prompt\",\"max_output_tokens\":8,\"operation_id\":\"same-id\"}",0,NULL);CHECK(strstr(body,"\"answer\":\"Fake ans\""));free(body);
    CHECK(!unlink(model));body=call(argv[1],config,request,0,NULL);CHECK(!strcmp(saved,body));free(body);free(saved);
    config_write(config,root,worker,model,engine_sha,model_sha,"bob");
    body=call(argv[1],config,"{\"contract_version\":1,\"op\":\"operation_get\",\"provider_id\":\"summary\",\"operation_id\":\"same-id\"}",1,"unknown_operation");free(body);
    config_write(config,root,worker,model,engine_sha,model_sha,"alice");
    body=call(argv[1],config,"{\"contract_version\":1,\"op\":\"operation_get\",\"provider_id\":\"summary\",\"operation_id\":\"same-id\"}",0,NULL);CHECK(strstr(body,"completed"));free(body);
    /* Scoped controls still reach the exact existing guardian-owned engine. */
    file_write(model,"sleep");file_sha(model,model_sha);config_write(config,root,worker,model,engine_sha,model_sha,"alice");CHECK(!setenv("SAMOSA_REGISTRY_QA_MARKER",marker,1));
    char *args[]={argv[1],"--config",config,NULL};
    Invocation active=spawn_request(args,"{\"contract_version\":1,\"op\":\"summarize\",\"provider_id\":\"summary\",\"text\":\"Cancel this engine\",\"max_output_tokens\":16,\"operation_id\":\"active\",\"timeout_ms\":10000}");
    size_t waits=0;while(access(marker,F_OK)&&waits++<500)delay();CHECK(!access(marker,F_OK));
    body=call(argv[1],config,"{\"contract_version\":1,\"op\":\"operation_cancel\",\"provider_id\":\"text\",\"operation_id\":\"active\"}",1,"unknown_operation");free(body);
    CHECK(kill(active.pid,0)==0);
    body=call(argv[1],config,"{\"contract_version\":1,\"op\":\"operation_cancel\",\"provider_id\":\"summary\",\"operation_id\":\"active\"}",0,NULL);free(body);
    body=finish(active,1);error_check(body,"cancelled");free(body);
    file_write(model,"ok");file_sha(model,model_sha);config_write(config,root,worker,model,engine_sha,model_sha,"alice");
    body=call(argv[1],config,"{\"contract_version\":1,\"op\":\"summarize\",\"provider_id\":\"summary\",\"text\":\"Surviving follow-up\",\"max_output_tokens\":16,\"operation_id\":\"survivor\"}",0,NULL);CHECK(strstr(body,"\"text\":\"summary\""));free(body);
    CHECK(!chmod(config,0644));body=call(argv[1],config,"{}",2,"invalid_registry_configuration");free(body);CHECK(!chmod(config,0600));
    CHECK(!symlink(config,alias));body=call(argv[1],alias,"{}",2,"invalid_registry_configuration");free(body);
    char *valid=file_read(config,65536);file_write(config,"{\"contract_version\":1,\"owner\":\"alice\",\"providers\":[{\"kind\":\"fake_text\",\"provider_id\":\"a\",\"state\":\"/tmp/a\",\"command\":\"evil\"}]}");
    body=call(argv[1],config,"{}",2,"unknown_registry_field");free(body);file_write(config,valid);free(valid);
    /* Credentials stay private even in registry discovery, and discovery must
     * neither create state nor contact the configured endpoint. */
    char key[256],untouched[256];snprintf(key,sizeof(key),"%s/key",root);snprintf(untouched,sizeof(untouched),"%s/untouched",root);
    const char *secret="registry_fixture_credential";file_write(key,secret);
    char credential_config[2048];snprintf(credential_config,sizeof(credential_config),"{\"contract_version\":1,\"owner\":\"alice\",\"providers\":[{\"kind\":\"text\",\"provider_id\":\"external\",\"endpoint\":\"http://127.0.0.1:18645\",\"context_tokens\":4096,\"model_alias\":\"configured.gguf\",\"state\":\"%s\",\"api_key_file\":\"%s\"}]}",untouched,key);
    file_write(config,credential_config);
    body=call(argv[1],config,"{\"contract_version\":1,\"op\":\"describe\"}",0,NULL);CHECK(!strstr(body,secret));CHECK(!strstr(body,key));free(body);
    body=call(argv[1],config,"{\"contract_version\":1,\"op\":\"provider_describe\",\"provider_id\":\"external\"}",0,NULL);CHECK(strstr(body,"llama-native-template-v1"));CHECK(!strstr(body,secret));CHECK(!strstr(body,key));free(body);CHECK(access(untouched,F_OK)<0&&errno==ENOENT);
    CHECK(!chmod(key,0644));body=call(argv[1],config,"{}",2,"invalid_registry_credentials");CHECK(!strstr(body,secret));free(body);CHECK(!chmod(key,0600));
    char key_alias[256];snprintf(key_alias,sizeof(key_alias),"%s/key-alias",root);CHECK(!link(key,key_alias));body=call(argv[1],config,"{}",2,"invalid_registry_credentials");free(body);CHECK(!unlink(key_alias));
    file_write(config,"{\"contract_version\":1,\"owner\":\"alice\",\"providers\":[]}");body=call(argv[1],config,"{}",2,"invalid_registry_configuration");free(body);
    file_write(config,"{\"contract_version\":1,\"owner\":\"alice\",\"providers\":[null]}");body=call(argv[1],config,"{}",2,"invalid_registry_configuration");free(body);
    file_write(config,"{\"contract_version\":1,\"owner\":\"alice\",\"providers\":[{\"kind\":\"fake_text\",\"provider_id\":\"a\",\"state\":\"/tmp/a\"},{\"kind\":\"fake_text\",\"provider_id\":\"a\",\"state\":\"/tmp/b\"}]}");body=call(argv[1],config,"{}",2,"duplicate_provider");free(body);
    file_write(config,"{\"contract_version\":1,\"owner\":\"alice\",\"providers\":[{\"kind\":\"fake_text\",\"provider_id\":\"a\",\"state\":\"/tmp/same-state\"},{\"kind\":\"fake_text\",\"provider_id\":\"b\",\"state\":\"/tmp/same-state\"}]}");body=call(argv[1],config,"{}",2,"shared_provider_state");free(body);
    cleanup(root);printf("PASS registry native CLI: %u checks; native engine, restart replay, scoped cancellation and private configuration\n",checks);return 0;
}
