#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#define SAMOSA_WHISPER_FIXTURE_ATOMIC
#include "registry_host.h"
#include "whisper_fixture.h"
#include "engine_process.h"
#include <poll.h>
#include <sys/file.h>
#ifdef __APPLE__
#include <libproc.h>
#endif
static char registry_path[1024],audio_path[1024],audio_sha[65],marker[1024];
static char *read_fixture(const char *path,size_t limit){if(access(path,R_OK))fprintf(stderr,"missing owned fixture: %s (%s)\n",path,strerror(errno));return file_read(path,limit);}
static size_t audio_size;
static const char *expected_text;
static int actual;
static pid_t peer,stopped_guardian;
static void cleanup_fixture(void){if(stopped_guardian>1){siginfo_t info={0};if(!waitid(P_PID,(id_t)stopped_guardian,&info,WEXITED|WNOHANG|WNOWAIT))kill(stopped_guardian,SIGCONT);}if(peer>1){kill(peer,SIGKILL);waitpid(peer,NULL,0);}}
static void model_mode(FixtureConfig *c,const char *mode){char body[2048];snprintf(body,sizeof(body),"{\"mode\":\"%s\",\"marker\":\"%s\"}",mode,marker);file_write(c->model,body);file_sha(c->model,c->model_sha);}
static void config(const FixtureConfig *c){
    struct stat engine,model;CHECK(!stat(c->engine,&engine)&&!stat(c->model,&model));
    char *body=NULL;size_t n=0;FILE *out=open_memstream(&body,&n);CHECK(out);
    fputs("{\"contract_version\":1,\"owner\":\"whisper-lifecycle-owner\",\"providers\":[{\"kind\":\"transcription\",\"provider_id\":\"whisper-base-en-v1\",\"state\":",out);json_quote(out,c->state,strlen(c->state));
    fputs(",\"engine\":",out);json_quote(out,c->engine,strlen(c->engine));fputs(",\"model\":",out);json_quote(out,c->model,strlen(c->model));
    fprintf(out,",\"engine_sha256\":\"%s\",\"model_sha256\":\"%s\",\"lifecycle\":\"on-demand-whisper-v1\",\"engine_size\":%llu,\"model_size\":%llu,\"qualification_wav\":",c->engine_sha,c->model_sha,(unsigned long long)engine.st_size,(unsigned long long)model.st_size);json_quote(out,audio_path,strlen(audio_path));
    fprintf(out,",\"qualification_sha256\":\"%s\",\"qualification_size\":%zu,\"qualification_text\":",audio_sha,audio_size);json_quote(out,expected_text,strlen(expected_text));
    fprintf(out,"},{\"kind\":\"fake_text\",\"provider_id\":\"other-provider\",\"state\":\"%s/other\"}]}",c->state);
    CHECK(!fclose(out));file_write(registry_path,body);free(body);
}
static char *request(InferenceRegistryHost *h,const char *input,int rc){char *body=NULL;size_t n=0;FILE *out=open_memstream(&body,&n);CHECK(out);int result=inference_registry_host_dispatch(h,input,strlen(input),out);CHECK(!fclose(out));if(result!=rc)fprintf(stderr,"request returned%d: %s\n",result,body);CHECK(result==rc);return body;}
static char *life(InferenceRegistryHost *h,const char *op,int rc,const char *contains){char json[512];snprintf(json,sizeof(json),"{\"contract_version\":1,\"op\":\"%s\",\"provider_id\":\"whisper-base-en-v1\"}",op);char *body=request(h,json,rc);if(contains&&!strstr(body,contains))fprintf(stderr,"%s expected%s: %s\n",op,contains,body);CHECK(!contains||strstr(body,contains));return body;}
static void expect_life(InferenceRegistryHost *h,const char *op,int rc,const char *contains){free(life(h,op,rc,contains));}
static char *saved(const char *state){char location[1024],*body=NULL;size_t n=0;CHECK(snprintf(location,sizeof(location),"%s/activation",state)<(int)sizeof(location));FILE *out=open_memstream(&body,&n);CHECK(out&&!activation_get(location,out)&&!fclose(out));return body;}
static void reload(InferenceRegistryHost *h,const FixtureConfig *c,const char *self){CHECK(!inference_registry_host_close(h));config(c);CHECK(!inference_registry_host_load(registry_path,self,h));}
static void gone(pid_t pid){for(int i=0;i<500;i++){if(kill(pid,0)<0&&errno==ESRCH)return;delay();}CHECK(0);}
static char *observed(pid_t *pid){for(int i=0;i<500;i++){if(!access(marker,F_OK))break;delay();}char *raw=read_fixture(marker,4096);Json j;parse(raw,&j);size_t value;CHECK(json_uint(&j,json_get(&j,0,"pid"),&value)&&value>1);*pid=(pid_t)value;char *workspace=json_string(&j,json_get(&j,0,"workspace"));CHECK(workspace);free(raw);return workspace;}
static void absent(const char *path){for(int i=0;i<500;i++){if(access(path,F_OK)<0&&errno==ENOENT)return;delay();}CHECK(0);}
typedef struct {InferenceRegistryHost *host;const char *input;char *body;int rc,start;atomic_int done;} Task;
static void *work(void *context){Task *t=context;if(t->start)t->body=life(t->host,"provider_start",t->rc,NULL);else t->body=request(t->host,t->input,t->rc);atomic_store(&t->done,1);return NULL;}
static void launch_work(pthread_t *thread,Task *task){pthread_attr_t attributes;CHECK(!pthread_attr_init(&attributes));CHECK(!pthread_attr_setstacksize(&attributes,4u*1048576u));CHECK(!pthread_create(thread,&attributes,work,task));CHECK(!pthread_attr_destroy(&attributes));}
static pid_t session(const char *binary,FILE **input,FILE **output){int in[2],out[2];CHECK(!pipe(in)&&!pipe(out));pid_t pid=fork();CHECK(pid>=0);if(!pid){dup2(in[0],STDIN_FILENO);dup2(out[1],STDOUT_FILENO);close(in[0]);close(in[1]);close(out[0]);close(out[1]);execl(binary,binary,"--config",registry_path,"--session",(char *)NULL);_exit(126);}close(in[0]);close(out[1]);*input=fdopen(in[1],"w");*output=fdopen(out[0],"r");CHECK(*input&&*output&&!setvbuf(*output,NULL,_IONBF,0));return pid;}
static void send_request(FILE *input,const char *body){CHECK(fprintf(input,"{\"contract_version\":1,\"request_id\":\"test\",\"request\":%s}\n",body)>0&&!fflush(input));}
static void response(FILE *output,const char *contains){struct pollfd watch={fileno(output),POLLIN,0};CHECK(poll(&watch,1,10000)>0);char *body=NULL;size_t n=0;CHECK(getline(&body,&n,output)>0);if(!strstr(body,contains))fprintf(stderr,"session reply: %s\n",body);CHECK(strstr(body,contains));free(body);}
static void session_life(FILE *input,FILE *output,const char *op,const char *contains){char body[512];snprintf(body,sizeof(body),"{\"contract_version\":1,\"op\":\"%s\",\"provider_id\":\"whisper-base-en-v1\"}",op);send_request(input,body);response(output,contains);}
static pid_t parent(pid_t child){
#ifdef __APPLE__
    struct proc_bsdinfo info;CHECK(proc_pidinfo(child,PROC_PIDTBSDINFO,0,&info,sizeof(info))==(int)sizeof(info));return (pid_t)info.pbi_ppid;
#else
    (void)child;return 0;
#endif
}
static void crash_recovery(InferenceRegistryHost *h,const FixtureConfig *c,const char *self,const char *binary,const char *input){
    /* Idle owner death leaves metadata but no process/model residency. */
    FILE *in,*out;pid_t owner=session(binary,&in,&out);session_life(in,out,"provider_start","\"resident\":false");
    CHECK(!kill(owner,SIGKILL));int status;CHECK(waitpid(owner,&status,0)==owner&&WIFSIGNALED(status));CHECK(!fclose(in)&&!fclose(out));
    CHECK(!inference_registry_host_load(registry_path,self,h));expect_life(h,"provider_lifecycle_status",0,"\"ready\":false");expect_life(h,"provider_start",0,"\"ready\":true");expect_life(h,"provider_stop",0,"\"resident\":false");CHECK(!inference_registry_host_close(h));
    owner=session(binary,&in,&out);session_life(in,out,"provider_start","\"ready\":true");CHECK(!unlink(marker));send_request(in,input);
    pid_t worker;char *workspace=observed(&worker);pid_t guardian=parent(worker);CHECK(guardian>1&&guardian!=owner&&guardian!=peer&&worker!=peer);
    /* Otherwise Darwin resumes an orphaned stopped guardian with SIGCONT.
       Keep its group non-orphaned using our unrelated direct child, outside
       the separately owned worker group. This makes the stalled lease test
       deterministic; ordinary owner death still exercises real cleanup. */
    CHECK(getpgid(guardian)==guardian&&getpgid(worker)==worker);
    CHECK(!setpgid(peer,guardian)&&getpgid(peer)!=getpgid(worker));
    CHECK(!kill(guardian,SIGSTOP));
    CHECK(!kill(owner,SIGKILL));CHECK(waitpid(owner,&status,0)==owner&&WIFSIGNALED(status));CHECK(!fclose(in)&&!fclose(out));
    char lease_path[1024];CHECK(snprintf(lease_path,sizeof(lease_path),"%s/provider.lock",c->state)<(int)sizeof(lease_path));
    int lease=open(lease_path,O_RDWR);CHECK(lease>=0);CHECK(flock(lease,LOCK_EX|LOCK_NB)<0&&(errno==EWOULDBLOCK||errno==EAGAIN));CHECK(!close(lease));
    CHECK(!inference_registry_host_load(registry_path,self,h));Task recovery={.host=h,.start=1,.rc=0};pthread_t thread;launch_work(&thread,&recovery);
    struct timespec wait={0,150000000};nanosleep(&wait,NULL);CHECK(!kill(worker,0));char *same=read_fixture(marker,4096);char pid[64];snprintf(pid,sizeof(pid),"\"pid\":%ld",(long)worker);CHECK(strstr(same,pid));free(same);CHECK(!atomic_load(&h->on_demand_whispers[0]->qualified));
    CHECK(!kill(guardian,SIGCONT));CHECK(!pthread_join(thread,NULL));CHECK(strstr(recovery.body,"\"ready\":true"));free(recovery.body);gone(worker);gone(guardian);absent(workspace);free(workspace);CHECK(!kill(peer,0));
    expect_life(h,"provider_stop",0,"\"state\":\"stopped\"");CHECK(!inference_registry_host_close(h));
    /* Killing a guardian is an unknown cleanup outcome. This host must keep
       its lifecycle fence and refuse false stop/restart success. */
    owner=session(binary,&in,&out);session_life(in,out,"provider_start","\"ready\":true");CHECK(!unlink(marker));send_request(in,input);workspace=observed(&worker);guardian=parent(worker);CHECK(guardian>1&&!kill(guardian,SIGKILL));
    response(out,"engine_guardian_failed");session_life(in,out,"provider_stop","engine_guardian_failed");session_life(in,out,"provider_start","engine_guardian_failed");
    CHECK(!inference_registry_host_load(registry_path,self,h));expect_life(h,"provider_start",1,"activation_busy");CHECK(!inference_registry_host_close(h));
    gone(worker);CHECK(!kill(owner,SIGKILL));CHECK(waitpid(owner,&status,0)==owner&&WIFSIGNALED(status));CHECK(!fclose(in)&&!fclose(out));
    if(!access(workspace,F_OK)){char name[2048];CHECK(snprintf(name,sizeof(name),"%s/input.wav",workspace)<(int)sizeof(name));unlink(name);CHECK(snprintf(name,sizeof(name),"%s/result.srt",workspace)<(int)sizeof(name));unlink(name);CHECK(!rmdir(workspace));}free(workspace);
    CHECK(!kill(peer,0));
}
int main(int argc,char **argv){
    int dispatch=engine_guardian_dispatch(argc,argv);if(dispatch>=0)return dispatch;
    CHECK((argc==3||argc==5)&&argv[0][0]=='/');actual=argc==5;alarm(180);peer=fork();CHECK(peer>=0);if(!peer){for(;;)pause();}CHECK(!atexit(cleanup_fixture));
    char root[]="/tmp/samosa-whisper-lifecycle-XXXXXX";CHECK(mkdtemp(root));char engine[1024],model[1024],state[1024];snprintf(engine,sizeof(engine),"%s/engine",root);snprintf(model,sizeof(model),"%s/model",root);snprintf(state,sizeof(state),"%s/state",root);snprintf(audio_path,sizeof(audio_path),"%s/probe.wav",root);snprintf(registry_path,sizeof(registry_path),"%s/registry.json",root);snprintf(marker,sizeof(marker),"%s/marker",root);
    FixtureConfig c={.engine=engine,.model=model,.state=state,.owner="whisper-lifecycle-owner"};CHECK(!mkdir(state,0700));file_copy(argv[1],engine);file_sha(engine,c.engine_sha);
    if(actual){file_copy(argv[2],model);file_sha(model,c.model_sha);file_copy(argv[3],audio_path);expected_text="fox";}
    else{model_mode(&c,"mutate");size_t n=32044;unsigned char *wav=calloc(n,1);CHECK(wav);memcpy(wav,"RIFF",4);put32(wav+4,(uint32_t)n-8);memcpy(wav+8,"WAVEfmt ",8);put32(wav+16,16);wav[20]=1;wav[22]=1;put32(wav+24,16000);put32(wav+28,32000);wav[32]=2;wav[34]=16;memcpy(wav+36,"data",4);put32(wav+40,(uint32_t)n-44);int fd=open(audio_path,O_CREAT|O_EXCL|O_WRONLY,0600);CHECK(fd>=0);write_all(fd,wav,n);close(fd);free(wav);expected_text="Coriander 7319.";}
    struct stat st;CHECK(!stat(audio_path,&st));audio_size=(size_t)st.st_size;file_sha(audio_path,audio_sha);CHECK(!chmod(engine,0755)&&!chmod(model,0644)&&!chmod(audio_path,0644));config(&c);
    InferenceRegistryHost h={0};CHECK(!inference_registry_host_load(registry_path,argv[0],&h));
    char *discovery=request(&h,"{\"contract_version\":1,\"op\":\"describe\"}",0);CHECK(strstr(discovery,"managed_lifecycle")&&strstr(discovery,"whisper-base-en-v1"));free(discovery);
    CHECK(access(marker,F_OK)<0&&errno==ENOENT);
    char *other=request(&h,"{\"contract_version\":1,\"op\":\"count_tokens\",\"provider_id\":\"other-provider\",\"prompt\":\"hello\"}",0);CHECK(strstr(other,"fake-utf8-byte-v1"));free(other);
    other=request(&h,"{\"contract_version\":1,\"op\":\"provider_start\",\"provider_id\":\"whisper-base-en-v1\",\"qualification_wav\":\"/public/path\"}",1);error_check(other,"unknown_field");free(other);
    expect_life(&h,"provider_lifecycle_status",0,"\"state\":\"stopped\"");
    int fd=open(audio_path,O_RDONLY);CHECK(fd>=0);unsigned char *wav=malloc(audio_size);CHECK(wav&&read(fd,wav,audio_size)==(ssize_t)audio_size&&!close(fd));char *encoded=encode(wav,audio_size);free(wav);char *input=transcribe(encoded,NULL,10000),*body=request(&h,input,1);error_check(body,"provider_unavailable");free(body);
    expect_life(&h,"provider_start",0,"\"ready\":true");WhisperHost *host=h.on_demand_whispers[0];CHECK(host&&host->activation.on_demand&&host->activation.process.guardian<0&&host->activation.process.response<0);unsigned revision=host->activation.revision;
    expect_life(&h,"provider_start",0,"\"resident\":false");CHECK(host->activation.revision==revision);body=request(&h,input,0);CHECK(strstr(body,expected_text)&&strstr(body,"speech_recognition_uncertain"));if(actual)printf("Actual common Whisper: %s",body);free(body);
    char *snapshot=saved(state);CHECK(strstr(snapshot,"\"execution_mode\":\"on_demand\"")&&strstr(snapshot,audio_sha)&&strstr(snapshot,"\"live_readiness\":\"unobserved\""));
    if(!actual){
        char *invalid=transcribe("!!!!",NULL,0);body=request(&h,invalid,1);error_check(body,"invalid_audio");free(body);free(invalid);expect_life(&h,"provider_lifecycle_status",0,"\"ready\":true");
        char *tracked=transcribe(encoded,"lifecycle-retained",10000),*retained=request(&h,tracked,0);
        expect_life(&h,"provider_stop",0,"\"state\":\"stopped\"");body=request(&h,tracked,0);CHECK(!strcmp(body,retained));free(body);free(retained);free(tracked);expect_life(&h,"provider_start",0,"\"ready\":true");
        free(snapshot);snapshot=saved(state);
        char *timed=transcribe(encoded,NULL,30);body=request(&h,timed,1);error_check(body,"deadline_exceeded");free(body);free(timed);expect_life(&h,"provider_lifecycle_status",0,"\"ready\":true");
        CHECK(!unlink(marker));char *blocked=transcribe(encoded,NULL,1000);Task stalled={.host=&h,.input=blocked,.rc=1};pthread_t stalled_thread;launch_work(&stalled_thread,&stalled);
        pid_t stalled_worker;char *stalled_workspace=observed(&stalled_worker);stopped_guardian=parent(stalled_worker);CHECK(stopped_guardian>1&&!kill(stopped_guardian,SIGSTOP));
        struct timespec suspended={6,0};nanosleep(&suspended,NULL);CHECK(!atomic_load(&stalled.done));expect_life(&h,"provider_stop",1,"provider_busy");
        CHECK(!kill(stopped_guardian,SIGCONT));stopped_guardian=0;CHECK(!pthread_join(stalled_thread,NULL));error_check(stalled.body,"deadline_exceeded");free(stalled.body);free(blocked);gone(stalled_worker);absent(stalled_workspace);free(stalled_workspace);expect_life(&h,"provider_lifecycle_status",0,"\"ready\":true");
        CHECK(!unlink(marker));Task pending={.host=&h,.input=input,.rc=0};pthread_t thread;launch_work(&thread,&pending);pid_t worker;char *workspace=observed(&worker);expect_life(&h,"provider_stop",1,"provider_busy");CHECK(!pthread_join(thread,NULL));free(pending.body);gone(worker);absent(workspace);free(workspace);
        CHECK(!unlink(marker));pending=(Task){.host=&h,.input=input,.rc=1};launch_work(&thread,&pending);workspace=observed(&worker);CHECK(!kill(worker,SIGKILL));CHECK(!pthread_join(thread,NULL));error_check(pending.body,"provider_process_failed");free(pending.body);absent(workspace);free(workspace);expect_life(&h,"provider_lifecycle_status",0,"\"ready\":false");expect_life(&h,"provider_start",1,"provider_unavailable");
        expect_life(&h,"provider_stop",0,"\"state\":\"stopped\"");reload(&h,&c,argv[0]);
        const char *modes[]={"bad","fail","normal"};
        for(size_t i=0;i<3;i++){model_mode(&c,modes[i]);expected_text=i==2?"not in the transcript":"Coriander 7319.";reload(&h,&c,argv[0]);expect_life(&h,"provider_start",1,i==0?"invalid_provider_response":i==1?"provider_process_failed":"provider_qualification_failed");char *current=saved(state);CHECK(!strcmp(current,snapshot));free(current);}
        model_mode(&c,"mutate");expected_text="Coriander 7319.";reload(&h,&c,argv[0]);
        int calibration=open(audio_path,O_RDWR);CHECK(calibration>=0&&pwrite(calibration,"X",1,0)==1);
        expect_life(&h,"provider_start",1,"artifact_checksum_mismatch");char *current=saved(state);CHECK(!strcmp(current,snapshot));free(current);
        CHECK(pwrite(calibration,"R",1,0)==1&&!close(calibration));
        expect_life(&h,"provider_start",0,"\"ready\":true");
        CHECK(!chmod(audio_path,0600));expect_life(&h,"provider_lifecycle_status",0,"provider_qualification_input_changed");body=request(&h,input,1);error_check(body,"provider_qualification_input_changed");free(body);expect_life(&h,"provider_stop",0,"\"resident\":false");
        CHECK(!chmod(audio_path,0644));reload(&h,&c,argv[0]);expect_life(&h,"provider_start",0,"\"ready\":true");CHECK(!inference_registry_host_close(&h));crash_recovery(&h,&c,argv[0],argv[2],input);
    }else{expect_life(&h,"provider_stop",0,"\"state\":\"stopped\"");expect_life(&h,"provider_start",0,"\"ready\":true");CHECK(!inference_registry_host_close(&h));}
    free(snapshot);free(encoded);free(input);CHECK(!kill(peer,0));CHECK(!kill(peer,SIGKILL)&&waitpid(peer,NULL,0)==peer);peer=0;CHECK(!access(engine,F_OK)&&!access(model,F_OK)&&!access(audio_path,F_OK));cleanup(root);
    printf("PASS common on-demand Whisper lifecycle: %u checks; observed qualified idle assets, no resident engine, actual requests, last-good/restart and scoped cleanup%s\n",atomic_load(&checks),actual?"; retained native engine/model":"; failure/mutation/busy/owner death/stalled-guardian recovery");return 0;
}
