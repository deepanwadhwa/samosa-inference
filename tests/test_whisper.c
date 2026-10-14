#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "whisper_fixture.h"
static const char *argument(int argc,char **argv,const char *key){for(int i=1;i+1<argc;i++)if(!strcmp(argv[i],key))return argv[i+1];return NULL;}
static int fixture(int argc,char **argv){
    const char *model=argument(argc,argv,"-m"),*input=argument(argc,argv,"-f"),*output=argument(argc,argv,"-of"),*language=argument(argc,argv,"-l");CHECK(model&&!strcmp(model,"/dev/fd/5")&&input&&output&&language&&!strcmp(language,"en"));
    char *body=file_read(model,4096);Json j;parse(body,&j);char *mode=json_string(&j,json_get(&j,0,"mode")),*marker=json_string(&j,json_get(&j,0,"marker"));CHECK(mode&&marker);
    char *workspace=strdup(input),*slash=strrchr(workspace,'/');CHECK(slash);*slash=0;
    /* The parent polls this marker during cancellation/mutation tests. Publish
     * the complete closed JSON so it cannot observe a partially written file. */
    char temporary[4096];int named=snprintf(temporary,sizeof(temporary),"%s.pending",marker);CHECK(named>0&&(size_t)named<sizeof(temporary));
    FILE *ready=fopen(temporary,"w");CHECK(ready);fprintf(ready,"{\"pid\":%ld,\"workspace\":",(long)getpid());json_quote(ready,workspace,strlen(workspace));fputc('}',ready);CHECK(!fclose(ready));CHECK(!rename(temporary,marker));free(workspace);free(marker);free(body);
    if(!strcmp(mode,"slow")){signal(SIGTERM,SIG_IGN);for(;;)pause();}
    if(!strcmp(mode,"fail")){free(mode);return 9;}
    if(!strcmp(mode,"flood")){char bytes[4096];memset(bytes,'x',sizeof(bytes));for(;;)write_all(STDOUT_FILENO,bytes,sizeof(bytes));}
    if(!strcmp(mode,"mutate")){struct timespec t={0,500000000};nanosleep(&t,NULL);}
    char path[512];snprintf(path,sizeof(path),"%s.srt",output);const char *srt=!strcmp(mode,"bad")?"1\n00:00:00,000 --> 00:00:31,000\nForged late cue.\n\n":"1\n00:00:00,000 --> 00:00:00,500\nCoriander 7319.\n\n";
    if(!strcmp(mode,"grid"))srt="1\n00:00:00,000 --> 00:00:01,020\nCoriander 7319.\n\n";
    if(!strcmp(mode,"offgrid"))srt="1\n00:00:00,000 --> 00:00:01,019\nCoriander 7319.\n\n";
    if(!strcmp(mode,"beyondgrid"))srt="1\n00:00:00,000 --> 00:00:01,040\nCoriander 7319.\n\n";
    if(!strcmp(mode,"big")){char bytes[32769];memset(bytes,'x',32768);bytes[32768]=0;file_write(path,bytes);}else file_write(path,srt);free(mode);return 0;
}
static void model_mode(FixtureConfig *cfg,const char *mode,const char *marker){char body[1024];snprintf(body,sizeof(body),"{\"mode\":\"%s\",\"marker\":\"%s\"}",mode,marker);file_write(cfg->model,body);file_sha(cfg->model,cfg->model_sha);}
static char *observation(const char *marker,pid_t *pid){for(int i=0;i<500;i++){if(!access(marker,F_OK))break;delay();}char *body=file_read(marker,2048);Json j;parse(body,&j);size_t p=0;CHECK(json_uint(&j,json_get(&j,0,"pid"),&p)&&p>0);*pid=(pid_t)p;char *workspace=json_string(&j,json_get(&j,0,"workspace"));CHECK(workspace);free(body);return workspace;}
static void absent(const char *path){for(int i=0;i<500;i++){if(access(path,F_OK)&&errno==ENOENT)return;delay();}CHECK(0);}
int main(int argc,char **argv){
    if(argc>1&&!strcmp(argv[1],"-m"))return fixture(argc,argv);
    CHECK(argc==2&&argv[0][0]=='/'&&argv[1][0]=='/');
    char root[]="/tmp/samosa-whisper-native-XXXXXX";CHECK(mkdtemp(root));char engine[512],model[512],marker[512],state[512],saved_engine[512],saved_model[512];
    snprintf(engine,sizeof(engine),"%s/engine",root);snprintf(model,sizeof(model),"%s/model",root);snprintf(marker,sizeof(marker),"%s/ready",root);snprintf(state,sizeof(state),"%s/state",root);snprintf(saved_engine,sizeof(saved_engine),"%s/saved-engine",root);snprintf(saved_model,sizeof(saved_model),"%s/saved-model",root);
    FixtureConfig cfg={.cli=argv[1],.engine=engine,.model=model,.state=state,.owner="whisper-test-owner"};file_copy(argv[0],engine);file_sha(engine,cfg.engine_sha);model_mode(&cfg,"normal",marker);
    char *body=invoke(&cfg,"{\"contract_version\":1,\"op\":\"describe\"}",NULL);CHECK(strstr(body,"transcription")&&strstr(body,"30000"));free(body);
    char *full=silence(30000),*short_audio=silence(1000),*request=transcribe(full,"full-window",10000),*saved=invoke(&cfg,request,NULL);
    CHECK(strstr(saved,"Coriander 7319.")&&strstr(saved,"input_audio_milliseconds")&&strstr(saved,"speech_recognition_uncertain")&&strstr(saved,"\"duration_ms\":30000"));
    pid_t child;char *workspace=observation(marker,&child);absent(workspace);free(workspace);
    CHECK(!rename(engine,saved_engine)&&!rename(model,saved_model));body=invoke(&cfg,request,NULL);CHECK(!strcmp(body,saved));free(body);CHECK(!rename(saved_engine,engine)&&!rename(saved_model,model));
    char *different=transcribe(short_audio,"full-window",10000);body=invoke(&cfg,different,"idempotency_conflict");free(body);free(different);free(saved);free(request);
    cfg.owner="foreign-owner";request=control("operation_get","full-window");body=invoke(&cfg,request,"unknown_operation");free(body);free(request);cfg.owner="whisper-test-owner";
    request=transcribe("!!!!",NULL,0);body=invoke(&cfg,request,"invalid_audio");free(body);free(request);
    char *oversized=silence(30001);request=transcribe(oversized,NULL,0);body=invoke(&cfg,request,"invalid_audio");free(body);free(request);free(oversized);
    size_t n=strlen(short_audio);CHECK(!strcmp(short_audio+n-2,"=="));short_audio[n-3]='B';request=transcribe(short_audio,NULL,0);body=invoke(&cfg,request,"invalid_audio");free(body);free(request);free(short_audio);short_audio=silence(1000);
    request=transcribe(short_audio,NULL,0);char original[65];memcpy(original,cfg.model_sha,65);memset(cfg.model_sha,'0',64);cfg.model_sha[64]=0;body=invoke(&cfg,request,"artifact_checksum_mismatch");free(body);memcpy(cfg.model_sha,original,65);
    const char *modes[]={"bad","big","fail","flood"},*errors[]={"invalid_provider_response","invalid_provider_response","provider_process_failed","provider_process_failed"};
    for(size_t i=0;i<4;i++){model_mode(&cfg,modes[i],marker);body=invoke(&cfg,request,errors[i]);free(body);workspace=observation(marker,&child);absent(workspace);free(workspace);}free(request);
    char *grid_audio=silence(1001);request=transcribe(grid_audio,NULL,0);free(grid_audio);
    model_mode(&cfg,"grid",marker);body=invoke(&cfg,request,NULL);CHECK(strstr(body,"\"end_ms\":1001")&&strstr(body,"whisper_20ms_end_clipped_to_input"));free(body);
    const char *invalid_grid[]={"offgrid","beyondgrid"};for(size_t i=0;i<2;i++){model_mode(&cfg,invalid_grid[i],marker);body=invoke(&cfg,request,"invalid_provider_response");free(body);}free(request);
    model_mode(&cfg,"slow",marker);request=transcribe(short_audio,NULL,500);body=invoke(&cfg,request,"deadline_exceeded");free(body);free(request);workspace=observation(marker,&child);absent(workspace);free(workspace);
    model_mode(&cfg,"mutate",marker);CHECK(!unlink(marker));request=transcribe(short_audio,"mutated-model",10000);Invocation pending=launch(&cfg,request);workspace=observation(marker,&child);file_write(model,"altered model");body=finish(pending,1);error_check(body,"artifact_checksum_mismatch");free(body);free(request);absent(workspace);free(workspace);
    model_mode(&cfg,"slow",marker);CHECK(!unlink(marker));request=transcribe(short_audio,"cancel-transcript",10000);pending=launch(&cfg,request);free(request);workspace=observation(marker,&child);request=control("operation_cancel","cancel-transcript");body=invoke(&cfg,request,NULL);free(body);free(request);body=finish(pending,1);error_check(body,"cancelled");free(body);absent(workspace);free(workspace);
    CHECK(!unlink(marker));request=transcribe(short_audio,"kill-transcript",10000);pending=launch(&cfg,request);free(request);workspace=observation(marker,&child);CHECK(!kill(pending.pid,SIGKILL));int status;CHECK(waitpid(pending.pid,&status,0)==pending.pid&&WIFSIGNALED(status));CHECK(!close(pending.output));absent(workspace);free(workspace);CHECK(kill(child,0)<0&&errno==ESRCH);
    request=control("operation_get","kill-transcript");body=invoke(&cfg,request,NULL);CHECK(strstr(body,"interrupted"));free(body);free(request);free(short_audio);free(full);cleanup(root);
    printf("PASS native public Whisper adapter: %u checks; full30s, replay/owner/input/identity refusals, timed derived text, model mutation, failure/log/deadline/cancel/SIGKILL workspace cleanup; fixture engine only\n",checks);return 0;
}
