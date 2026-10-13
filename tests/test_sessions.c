#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "inference.h"
#include "sessions.h"
#include "json.h"
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
extern char **environ;
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static char *run(const InferenceConfig *cfg,const char *req,int expected) {
    char *out=NULL;size_t n;FILE *stream=open_memstream(&out,&n);CHECK(stream);CHECK(inference_dispatch_config(cfg,req,strlen(req),stream)==expected);CHECK(!fclose(stream));Json j;CHECK(json_parse(&j,out,n));return out;
}
static void error(const InferenceConfig *cfg,const char *req,const char *code){char *out=run(cfg,req,1);if(!strstr(out,code))fprintf(stderr,"expected %s received %s",code,out);CHECK(strstr(out,code));free(out);}
static char *create(const InferenceConfig *cfg) {
    char req[256];snprintf(req,sizeof(req),"{\"contract_version\":1,\"op\":\"session_create\",\"provider_id\":\"%s\"}",cfg->provider_id);char *out=run(cfg,req,0);Json j;CHECK(json_parse(&j,out,strlen(out)));char *id=json_string(&j,json_get(&j,0,"session_id"));CHECK(id&&strlen(id)==48);free(out);return id;
}
static void generate(char *out,size_t n,const char *id,size_t revision,size_t reserve) {
    snprintf(out,n,"{\"contract_version\":1,\"op\":\"generate\",\"provider_id\":\"fake-session-v7\",\"prompt\":\"Some literal text\",\"max_output_tokens\":%zu,\"session_id\":\"%s\",\"expected_session_revision\":%zu}",reserve,id,revision);
}
int main(int argc,char **argv) {
    if(argc==5&&!strcmp(argv[1],"--held-session")) {
        InferenceConfig cfg={.provider_id="fake-session-v7",.admission_state=argv[2],.context_tokens=131072,.owner="alice"};
        int lock=-1;if(inference_admit(cfg.admission_state,&lock)||session_begin(&cfg,argv[3],1,"uncertain prompt"))return 2;
        FILE *marker=fopen(argv[4],"w");if(!marker)return 2;fputs("busy",marker);fclose(marker);for(;;)pause();
    }
    char fixture[]="/tmp/samosa-native-sessions-XXXXXX";CHECK(mkdtemp(fixture));char state[512],req[512];snprintf(state,sizeof(state),"%s/state",fixture);
    InferenceConfig cfg={.provider_id="fake-session-v7",.admission_state=state,.context_tokens=131072,.owner="alice"};
    char *id=create(&cfg);generate(req,sizeof(req),id,0,64);char *out=run(&cfg,req,0);CHECK(strstr(out,"\"session_revision\":1"));CHECK(strstr(out,"Fake answer"));free(out);
    error(&cfg,req,"stale_session");generate(req,sizeof(req),id,1,0);error(&cfg,req,"invalid_budget");
    snprintf(req,sizeof(req),"{\"contract_version\":1,\"op\":\"session_get\",\"session_id\":\"%s\"}",id);out=run(&cfg,req,0);CHECK(strstr(out,"\"revision\":1"));CHECK(strstr(out,"\"busy\":false"));free(out);
    cfg.owner="bob";error(&cfg,req,"unknown_session");cfg.owner="alice";cfg.provider_id="foreign";error(&cfg,req,"provider_changed");cfg.provider_id="fake-session-v7";
    int lock=-1;CHECK(inference_admit(state,&lock)==NULL);generate(req,sizeof(req),id,1,16);error(&cfg,req,"busy");close(lock);
    /* Root process dies with a reserved cache marker; last-good revision stays1. */
    char marker[512];snprintf(marker,sizeof(marker),"%s/held",fixture);char *args[]={argv[0],"--held-session",state,id,marker,NULL};pid_t child;CHECK(posix_spawnp(&child,argv[0],NULL,NULL,args,environ)==0);
    int found=0;for(int i=0;i<500;i++){if(access(marker,F_OK)==0){found=1;break;}struct timespec delay={0,10000000};nanosleep(&delay,NULL);}CHECK(found);
    snprintf(req,sizeof(req),"{\"contract_version\":1,\"op\":\"session_get\",\"session_id\":\"%s\"}",id);out=run(&cfg,req,0);CHECK(strstr(out,"\"busy\":true"));CHECK(strstr(out,"\"revision\":1"));free(out);
    CHECK(kill(child,SIGKILL)==0);int status;CHECK(waitpid(child,&status,0)==child);out=run(&cfg,req,0);CHECK(strstr(out,"\"busy\":false"));CHECK(strstr(out,"\"revision\":1"));free(out);
    generate(req,sizeof(req),id,1,64);out=run(&cfg,req,0);CHECK(strstr(out,"\"session_revision\":2"));free(out);
    /* Native cache retention is explicit, no silently evicted live handles. */
    char *handles[128]={0};handles[0]=id;
    for(int i=1;i<128;i++)handles[i]=create(&cfg);
    error(&cfg,"{\"contract_version\":1,\"op\":\"session_create\",\"provider_id\":\"fake-session-v7\"}","storage_limit");
    snprintf(req,sizeof(req),"{\"contract_version\":1,\"op\":\"session_get\",\"session_id\":\"%s\"}",id);out=run(&cfg,req,0);CHECK(strstr(out,"\"revision\":2"));free(out);
    /* Fill bytes through actual successful generations, retaining old handles. */
    char *large_request=malloc(132000);CHECK(large_request);int refused=0;
    for(int i=1;i<128;i++) {
        int prefix=snprintf(large_request,132000,"{\"contract_version\":1,\"op\":\"generate\",\"provider_id\":\"fake-session-v7\",\"prompt\":\"");
        memset(large_request+prefix,'A',131000);
        snprintf(large_request+prefix+131000,132000-(size_t)prefix-131000,"\",\"max_output_tokens\":1,\"session_id\":\"%s\",\"expected_session_revision\":0}",handles[i]);
        char *reply=NULL;size_t n;FILE *stream=open_memstream(&reply,&n);CHECK(stream);int rc=inference_dispatch_config(&cfg,large_request,strlen(large_request),stream);CHECK(!fclose(stream));Json parsed;CHECK(json_parse(&parsed,reply,n));
        if(rc){CHECK(strstr(reply,"storage_limit"));refused=1;free(reply);snprintf(req,sizeof(req),"{\"contract_version\":1,\"op\":\"session_get\",\"session_id\":\"%s\"}",handles[i]);out=run(&cfg,req,0);CHECK(strstr(out,"\"revision\":0"));free(out);break;}
        CHECK(strstr(reply,"\"session_revision\":1"));free(reply);
    }
    CHECK(refused);free(large_request);
    snprintf(req,sizeof(req),"{\"contract_version\":1,\"op\":\"session_get\",\"session_id\":\"%s\"}",id);out=run(&cfg,req,0);CHECK(strstr(out,"\"revision\":2"));free(out);
    printf("PASS native opaque sessions: %u checks; owner/provider/revision, actual crash, last-good recovery,128-handle quota\n",checks);
    for(int i=0;i<128;i++){free(handles[i]);}char *cleanup[]={"find",fixture,"-depth","-delete",NULL};CHECK(posix_spawnp(&child,cleanup[0],NULL,NULL,cleanup,environ)==0);CHECK(waitpid(child,&status,0)==child);CHECK(WIFEXITED(status)&&WEXITSTATUS(status)==0);return 0;
}
