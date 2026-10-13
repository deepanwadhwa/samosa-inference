#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "inference.h"
#include "sessions.h"
#include "operations.h"
#include "budget.h"
#include "json.h"
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
extern char **environ;
static unsigned checks;
static int parent_cancelled(void *flag){return *(int*)flag;}
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static char *call(const InferenceConfig *cfg,const char *input,int expected){char *out=NULL;size_t length;FILE *stream=open_memstream(&out,&length);CHECK(stream);int rc=inference_dispatch_config(cfg,input,strlen(input),stream);CHECK(!fclose(stream));if(rc!=expected)fprintf(stderr,"response %s\n",out);CHECK(rc==expected);Json j;CHECK(json_parse(&j,out,length));return out;}
static char *public_request(const char *op,const char *id){char *out=NULL;size_t length;FILE *stream=open_memstream(&out,&length);CHECK(stream);fprintf(stream,"{\"contract_version\":1,\"op\":\"%s\",\"operation_id\":\"%s\"}",op,id);CHECK(!fclose(stream));return out;}
static void ready(const char *path){int found=0;for(int i=0;i<500;i++){if(!access(path,F_OK)){found=1;break;}struct timespec delay={0,10000000};nanosleep(&delay,NULL);}CHECK(found);}
static pid_t worker(const char *exe,const char *state,const char *marker,const char *output,const char *id){char *args[]={(char*)exe,"--worker",(char*)state,(char*)marker,(char*)output,(char*)id,NULL};pid_t pid;CHECK(!posix_spawnp(&pid,exe,NULL,NULL,args,environ));return pid;}
static void wait_success(pid_t pid){int status;CHECK(waitpid(pid,&status,0)==pid);CHECK(WIFEXITED(status)&&!WEXITSTATUS(status));}
static char *read_file(const char *path){FILE *in=fopen(path,"r");CHECK(in);char bytes[8192];size_t n=fread(bytes,1,sizeof(bytes)-1,in);CHECK(!ferror(in));CHECK(!fclose(in));bytes[n]=0;return strdup(bytes);}
int main(int argc,char **argv){
    if(argc==5&&!strcmp(argv[1],"--reserve")){
        InferenceConfig cfg={.provider_id="fixture-v1",.context_tokens=128,.owner="alice",.admission_state=argv[2]};
        InferenceOperation operation;char *saved=NULL;CHECK(!operation_begin(&cfg,argv[4],"reservation-fixture",&operation,&saved));CHECK(!saved);
        FILE *marker=fopen(argv[3],"w");CHECK(marker);CHECK(!fclose(marker));for(;;)pause();
    }
    if(argc==6&&!strcmp(argv[1],"--worker")){
        CHECK(!setenv("SAMOSA_INFERENCE_QUEUE_READY",argv[3],1));
        InferenceConfig cfg={.provider_id="fixture-v1",.context_tokens=128,.owner="alice",.admission_state=argv[2],.queue_capacity=4,.admission_wait_ms=10000};
        char input[1024];snprintf(input,sizeof(input),"{\"contract_version\":1,\"op\":\"generate\",\"operation_id\":\"%s\",\"provider_id\":\"fixture-v1\",\"prompt\":\"code7319\",\"max_output_tokens\":16}",argv[5]);
        FILE *out=fopen(argv[4],"w");CHECK(out);inference_dispatch_config(&cfg,input,strlen(input),out);CHECK(!fclose(out));return 0;
    }
    char fixture[]="/tmp/samosa-native-operations-XXXXXX";CHECK(mkdtemp(fixture));char state[1024],marker[2][1024],output[2][1024];snprintf(state,sizeof(state),"%s/state",fixture);
    for(int i=0;i<2;i++){snprintf(marker[i],sizeof(marker[i]),"%s/ready-%d",fixture,i);snprintf(output[i],sizeof(output[i]),"%s/result-%d",fixture,i);}
    InferenceConfig cfg={.provider_id="fixture-v1",.context_tokens=128,.owner="alice",.admission_state=state,.queue_capacity=4,.admission_wait_ms=10000};
    const char *first="{\"contract_version\":1,\"op\":\"generate\",\"operation_id\":\"first\",\"provider_id\":\"fixture-v1\",\"prompt\":\"code7319\",\"max_output_tokens\":16}";
    char *out=call(&cfg,first,0);CHECK(strstr(out,"Fake answer"));
    const char *reordered="{\"prompt\":\"code7319\",\"max_output_tokens\":16,\"provider_id\":\"fixture-v1\",\"operation_id\":\"first\",\"op\":\"generate\",\"contract_version\":1}";
    char *retry=call(&cfg,reordered,0);CHECK(!strcmp(out,retry));free(retry);free(out);
    out=call(&cfg,"{\"contract_version\":1,\"op\":\"generate\",\"operation_id\":\"first\",\"provider_id\":\"fixture-v1\",\"prompt\":\"changed\",\"max_output_tokens\":16}",1);CHECK(strstr(out,"idempotency_conflict"));free(out);
    char *request=public_request("operation_get","first");out=call(&cfg,request,0);CHECK(strstr(out,"\"status\":\"completed\""));free(out);
    cfg.owner="bob";out=call(&cfg,request,1);CHECK(strstr(out,"unknown_operation"));free(out);cfg.owner="alice";free(request);
    int active=-1;CHECK(!inference_admit(state,&active));
    pid_t cancelled=worker(argv[0],state,marker[0],output[0],"cancel-me");ready(marker[0]);
    pid_t survivor=worker(argv[0],state,marker[1],output[1],"survivor");ready(marker[1]);
    request=public_request("operation_cancel","cancel-me");cfg.owner="bob";out=call(&cfg,request,1);CHECK(strstr(out,"unknown_operation"));free(out);cfg.owner="alice";
    out=call(&cfg,request,0);CHECK(strstr(out,"\"cancel_requested\":true"));free(out);free(request);wait_success(cancelled);
    out=read_file(output[0]);CHECK(strstr(out,"\"code\":\"cancelled\""));free(out);
    request=public_request("operation_get","cancel-me");out=call(&cfg,request,0);CHECK(strstr(out,"\"status\":\"cancelled\""));free(out);free(request);
    CHECK(!close(active));wait_success(survivor);out=read_file(output[1]);CHECK(strstr(out,"Fake answer"));free(out);
    CHECK(!inference_admit(state,&active));CHECK(!unlink(marker[0]));
    pid_t interrupted=worker(argv[0],state,marker[0],output[0],"dead-worker");ready(marker[0]);CHECK(!kill(interrupted,SIGKILL));int status;CHECK(waitpid(interrupted,&status,0)==interrupted);CHECK(WIFSIGNALED(status));
    request=public_request("operation_get","dead-worker");out=call(&cfg,request,0);CHECK(strstr(out,"\"status\":\"interrupted\""));free(out);free(request);CHECK(!close(active));
    out=call(&cfg,"{\"contract_version\":1,\"op\":\"generate\",\"operation_id\":\"dead-worker\",\"provider_id\":\"fixture-v1\",\"prompt\":\"code7319\",\"max_output_tokens\":16}",1);CHECK(strstr(out,"operation_interrupted"));free(out);
    /* Actual128 history ceiling; no deletion or eviction of successful retries. */
    char input[1024];for(int i=0;i<124;i++){snprintf(input,sizeof(input),"{\"contract_version\":1,\"op\":\"count_tokens\",\"operation_id\":\"quota-%d\",\"provider_id\":\"fixture-v1\",\"prompt\":\"x\"}",i);out=call(&cfg,input,0);free(out);}
    out=call(&cfg,"{\"contract_version\":1,\"op\":\"count_tokens\",\"operation_id\":\"overflow\",\"provider_id\":\"fixture-v1\",\"prompt\":\"x\"}",1);CHECK(strstr(out,"operation_limit"));free(out);
    CHECK(!inference_admit(state,&active));out=call(&cfg,first,0);CHECK(strstr(out,"Fake answer"));free(out);CHECK(!close(active));
    /* Live worker reservations bound aggregate results before provider work.
     * A killed reservation is reaped automatically on new operation admission. */
    char reserved_state[1024];snprintf(reserved_state,sizeof(reserved_state),"%s/reservations",fixture);cfg.admission_state=reserved_state;
    pid_t reservers[16];
    for(int i=0;i<16;i++){
        char reserved_marker[1024],id[32];snprintf(reserved_marker,sizeof(reserved_marker),"%s/reserved-%d",fixture,i);snprintf(id,sizeof(id),"reserved-%d",i);
        char *args[]={argv[0],"--reserve",reserved_state,reserved_marker,id,NULL};CHECK(!posix_spawnp(&reservers[i],argv[0],NULL,NULL,args,environ));ready(reserved_marker);
    }
    out=call(&cfg,first,1);CHECK(strstr(out,"operation_bytes_limit"));free(out);
    CHECK(!kill(reservers[0],SIGKILL));CHECK(waitpid(reservers[0],&status,0)==reservers[0]);CHECK(WIFSIGNALED(status));
    out=call(&cfg,first,0);CHECK(strstr(out,"Fake answer"));free(out);
    char linkage_state[1024];snprintf(linkage_state,sizeof(linkage_state),"%s/linked",fixture);cfg.admission_state=linkage_state;
    const char *linked="{\"contract_version\":1,\"op\":\"count_tokens\",\"operation_id\":\"linked-child\",\"parent_operation_id\":\"agent-parent\",\"provider_id\":\"fixture-v1\",\"prompt\":\"x\"}";
    out=call(&cfg,linked,0);CHECK(strstr(out,"\"parent_operation_id\":\"agent-parent\""));char *linked_result=strdup(out);CHECK(linked_result);free(out);
    out=call(&cfg,linked,0);CHECK(!strcmp(out,linked_result));free(out);free(linked_result);
    request=public_request("operation_get","linked-child");out=call(&cfg,request,0);CHECK(strstr(out,"\"parent_operation_id\":\"agent-parent\""));free(out);
    cfg.owner="foreign";out=call(&cfg,request,1);CHECK(strstr(out,"unknown_operation"));free(out);cfg.owner="alice";free(request);
    out=call(&cfg,"{\"contract_version\":1,\"op\":\"count_tokens\",\"operation_id\":\"linked-child\",\"parent_operation_id\":\"different-parent\",\"provider_id\":\"fixture-v1\",\"prompt\":\"x\"}",1);CHECK(strstr(out,"idempotency_conflict"));free(out);
    const char *bad_parents[]={"", "a b", "a/b", "linked-invalid"};char parent_input[1024];
    for(size_t i=0;i<4;i++){snprintf(parent_input,sizeof(parent_input),"{\"contract_version\":1,\"op\":\"count_tokens\",\"operation_id\":\"linked-invalid\",\"parent_operation_id\":\"%s\",\"provider_id\":\"fixture-v1\",\"prompt\":\"x\"}",bad_parents[i]);out=call(&cfg,parent_input,1);CHECK(strstr(out,"invalid_request"));free(out);}
    out=call(&cfg,"{\"contract_version\":1,\"op\":\"count_tokens\",\"parent_operation_id\":\"agent-parent\",\"provider_id\":\"fixture-v1\",\"prompt\":\"x\"}",1);CHECK(strstr(out,"invalid_request"));free(out);
    cfg.admission_state=reserved_state;
    request=public_request("operation_get","reserved-0");out=call(&cfg,request,0);CHECK(strstr(out,"\"status\":\"interrupted\""));free(out);free(request);
    for(int i=1;i<16;i++){CHECK(!kill(reservers[i],SIGKILL));CHECK(waitpid(reservers[i],&status,0)==reservers[i]);CHECK(WIFSIGNALED(status));}
    char parent_state[1024];snprintf(parent_state,sizeof(parent_state),"%s/parent",fixture);cfg.admission_state=parent_state;
    int parent_stop=1;cfg.cancelled=parent_cancelled;cfg.cancellation=&parent_stop;
    const char *parent_request="{\"contract_version\":1,\"op\":\"generate\",\"operation_id\":\"parent-stop\",\"provider_id\":\"fixture-v1\",\"prompt\":\"x\",\"max_output_tokens\":16}";
    out=call(&cfg,parent_request,1);CHECK(strstr(out,"\"code\":\"cancelled\""));char *parent_result=strdup(out);CHECK(parent_result);free(out);
    parent_stop=0;out=call(&cfg,parent_request,1);CHECK(!strcmp(out,parent_result));free(out);free(parent_result);
    request=public_request("operation_get","parent-stop");out=call(&cfg,request,0);CHECK(strstr(out,"\"status\":\"cancelled\""));free(out);free(request);
    out=call(&cfg,first,0);CHECK(strstr(out,"Fake answer"));free(out);
    char budget_state[1024];snprintf(budget_state,sizeof(budget_state),"%s/budget",fixture);cfg.admission_state=budget_state;cfg.cancelled=NULL;
    const char *timed_request="{\"contract_version\":1,\"op\":\"count_tokens\",\"operation_id\":\"timed\",\"parent_operation_id\":\"budget-parent\",\"provider_id\":\"fixture-v1\",\"prompt\":\"x\",\"timeout_ms\":10000}";
    out=call(&cfg,timed_request,0);char *timed_result=strdup(out);CHECK(timed_result);free(out);
    cfg.deadline_monotonic=inference_now()-1;out=call(&cfg,timed_request,0);CHECK(!strcmp(out,timed_result));free(out);free(timed_result);cfg.deadline_monotonic=0;
    out=call(&cfg,"{\"contract_version\":1,\"op\":\"count_tokens\",\"operation_id\":\"timed\",\"parent_operation_id\":\"budget-parent\",\"provider_id\":\"fixture-v1\",\"prompt\":\"x\",\"timeout_ms\":10001}",1);CHECK(strstr(out,"idempotency_conflict"));free(out);
    const char *bad_budget[]={"0","60001","1.5","\"100\""};
    for(unsigned i=0;i<4;i++){snprintf(input,sizeof(input),"{\"contract_version\":1,\"op\":\"count_tokens\",\"provider_id\":\"fixture-v1\",\"prompt\":\"x\",\"timeout_ms\":%s}",bad_budget[i]);out=call(&cfg,input,1);CHECK(strstr(out,"invalid_request"));free(out);}
    CHECK(!inference_admit(budget_state,&active));
    const char *queued_budget="{\"contract_version\":1,\"op\":\"generate\",\"operation_id\":\"budget-queued\",\"parent_operation_id\":\"budget-parent\",\"provider_id\":\"fixture-v1\",\"prompt\":\"x\",\"max_output_tokens\":16,\"timeout_ms\":200}";
    double before=inference_now();out=call(&cfg,queued_budget,1);CHECK(inference_now()-before<3);CHECK(strstr(out,"deadline_exceeded"));CHECK(strstr(out,"\"parent_operation_id\":\"budget-parent\""));timed_result=strdup(out);CHECK(timed_result);free(out);CHECK(!close(active));
    out=call(&cfg,queued_budget,1);CHECK(!strcmp(out,timed_result));free(out);free(timed_result);
    request=public_request("operation_get","budget-queued");out=call(&cfg,request,0);CHECK(strstr(out,"\"status\":\"failed\""));CHECK(strstr(out,"\"cancel_requested\":false"));free(out);free(request);
    out=call(&cfg,first,0);CHECK(strstr(out,"Fake answer"));free(out);
    char *cleanup[]={"find",fixture,"-depth","-delete",NULL};pid_t child;CHECK(!posix_spawnp(&child,"find",NULL,NULL,cleanup,environ));wait_success(child);
    printf("PASS scoped operations: %u checks; owner/retry/conflict, actual queued cancel/death, survivor isolation,128 histories/no eviction\n",checks);return 0;
}
