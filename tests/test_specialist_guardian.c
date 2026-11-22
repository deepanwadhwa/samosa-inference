#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/specialist_protocol.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static atomic_uint checks;
static pid_t peer,host;
static void failed(const char *expression,int line){fprintf(stderr,"FAIL %d: %s\n",line,expression);pid_t owned[]={host,peer};for(size_t i=0;i<2;i++)if(owned[i]>1){siginfo_t info={0};if(!waitid(P_PID,(id_t)owned[i],&info,WEXITED|WNOHANG|WNOWAIT)){(void)kill(owned[i],SIGKILL);while(waitpid(owned[i],NULL,0)<0&&errno==EINTR){}}}exit(1);}
#define CHECK(x) do{checks++;if(!(x))failed(#x,__LINE__);}while(0)
static void pause_ms(long ms){struct timespec t={ms/1000,(ms%1000)*1000000};while(nanosleep(&t,&t)&&errno==EINTR){}}
static int gone(pid_t pid){for(int i=0;i<300;i++){if(kill(pid,0)<0&&errno==ESRCH)return 1;pause_ms(10);}return 0;}
static pid_t pid_file(const char *path){for(int i=0;i<300;i++){FILE *f=fopen(path,"r");long value=0;if(f){(void)fscanf(f,"%ld",&value);fclose(f);if(value>1)return (pid_t)value;}pause_ms(10);}return 0;}
static int start(int framed,SamosaMmSupervisor *supervisor,SamosaMmSession *session,const char *engine,const char *model){
    return framed?samosa_mm_molmo2_start(supervisor,session,NULL,engine,model,1):samosa_mm_visionpsy_start(supervisor,session,NULL,engine,model,1);
}
static int helper_reproduction(const char *engine,const char *model,const char *helper_file){
    int all=1;
    for(int framed=0;framed<2;framed++){
        if(framed)CHECK(!setenv("SAMOSA_FAKE_MM_FRAMED","1",1));else CHECK(!unsetenv("SAMOSA_FAKE_MM_FRAMED"));
        CHECK(!setenv("SAMOSA_FAKE_MM_HELPER_PID_FILE",helper_file,1));(void)unlink(helper_file);
        SamosaMmSupervisor supervisor;CHECK(samosa_mm_supervisor_init(&supervisor));SamosaMmSession session={0};CHECK(start(framed,&supervisor,&session,engine,model));
        pid_t leader=session.pid,helper=pid_file(helper_file);CHECK(leader>1&&helper>1&&helper!=peer&&leader!=peer);
        CHECK(getpgid(leader)==leader&&getpgid(helper)==leader);
        samosa_mm_visual_close(&session,NULL);
        CHECK(gone(leader));int cleaned=gone(helper);
        if(!cleaned){CHECK(!kill(helper,SIGKILL));CHECK(gone(helper));all=0;}
        CHECK(!kill(peer,0));CHECK(!samosa_mm_supervisor_is_active(&supervisor,NULL,0));samosa_mm_supervisor_destroy(&supervisor);
        printf("%s original %s profile: leader gone, stubborn helper %s; unrelated peer survives; fixture survivors cleaned\n",cleaned?"PASS":"FAIL",framed?"Molmo":"VisionPsy",cleaned?"gone":"leaked");
    }
    fflush(stdout);return all;
}

static int64_t now_ms(void){struct timespec t;CHECK(!clock_gettime(CLOCK_MONOTONIC,&t));return (int64_t)t.tv_sec*1000+t.tv_nsec/1000000;}
static void frame_mode(int framed){if(framed)CHECK(!setenv("SAMOSA_FAKE_MM_FRAMED","1",1));else CHECK(!unsetenv("SAMOSA_FAKE_MM_FRAMED"));}
static void identity(const char *file,pid_t *engine,pid_t *guardian){FILE *f=fopen(file,"r");long a=0,b=0,c=0;CHECK(f&&fscanf(f,"%ld %ld %ld",&a,&b,&c)==3&&!fclose(f));CHECK(a>1&&b>1&&a==c);*engine=(pid_t)a;*guardian=(pid_t)b;}
static void healthy(SamosaMmSession *session){const char *request="{\"command\":\"inspect\"}";char *reply=NULL,error[96];size_t bytes=0;CHECK(samosa_mm_session_request(session,request,strlen(request),&reply,&bytes,error,sizeof(error)));CHECK(bytes&&strstr(reply,"fixture"));free(reply);}
static void cleaned(SamosaMmSupervisor *supervisor,SamosaMmSession *session,pid_t engine,pid_t helper,pid_t guardian){CHECK(!session->supervisor&&!session->pid&&!session->active);CHECK(!samosa_mm_supervisor_is_active(supervisor,NULL,0));CHECK(gone(engine)&&gone(helper)&&gone(guardian));CHECK(!kill(peer,0));}
static void boundary_cases(const char *engine,const char *model,const char *identity_file,const char *helper_file){
    for(int framed=0;framed<2;framed++){
        frame_mode(framed);SamosaMmSupervisor supervisor;CHECK(samosa_mm_supervisor_init(&supervisor));SamosaMmSession session={0};CHECK(start(framed,&supervisor,&session,engine,model));
        pid_t worker,guardian;identity(identity_file,&worker,&guardian);pid_t helper=pid_file(helper_file);CHECK(session.pid==worker&&guardian!=getpid()&&helper>1);
        size_t limit=framed?1048576:65536,payload=limit-(framed?0:1);char *request=malloc(payload+2);CHECK(request);
        int header=snprintf(request,payload+2,"{\"command\":\"boundary\",\"response_bytes\":%zu,\"padding\":\"",payload);CHECK(header>0&&(size_t)header+2<payload);
        memset(request+header,'x',payload-(size_t)header-2);memcpy(request+payload-2,"\"}",2);request[payload]=0;
        char *reply=NULL,error[96];size_t bytes=0;CHECK(samosa_mm_session_request(&session,request,payload,&reply,&bytes,error,sizeof(error)));CHECK(bytes==payload&&strlen(reply)==payload);free(reply);
        CHECK(!samosa_mm_session_request(&session,request,limit+1,&reply,&bytes,error,sizeof(error)));CHECK(!strcmp(error,"multimodal_request_too_large")&&session.active);free(request);
        char oversized[128];int n=snprintf(oversized,sizeof(oversized),"{\"command\":\"boundary\",\"response_bytes\":%zu}",payload+1);CHECK(n>0);
        CHECK(!samosa_mm_session_request(&session,oversized,(size_t)n,&reply,&bytes,error,sizeof(error)));CHECK(!session.active&&!reply&&samosa_mm_supervisor_is_active(&supervisor,NULL,0));
        samosa_mm_session_close(&session);cleaned(&supervisor,&session,worker,helper,guardian);samosa_mm_supervisor_destroy(&supervisor);
    }
}
static void grace_cases(const char *engine,const char *model,const char *identity_file,const char *helper_file){
    for(int framed=0;framed<2;framed++)for(int stubborn=0;stubborn<2;stubborn++){
        frame_mode(framed);CHECK(!setenv("SAMOSA_FAKE_MM_LIFECYCLE",stubborn?"stubborn":"ignore-quit",1));SamosaMmSupervisor supervisor;CHECK(samosa_mm_supervisor_init(&supervisor));SamosaMmSession session={0};CHECK(start(framed,&supervisor,&session,engine,model));
        pid_t worker,guardian;identity(identity_file,&worker,&guardian);pid_t helper=pid_file(helper_file);int64_t begin=now_ms();samosa_mm_session_close(&session);int64_t elapsed=now_ms()-begin;long grace=framed?3000:2000;
        CHECK(elapsed>=grace+(stubborn?650:0)&&elapsed<grace+1800);cleaned(&supervisor,&session,worker,helper,guardian);samosa_mm_supervisor_destroy(&supervisor);
    }CHECK(!unsetenv("SAMOSA_FAKE_MM_LIFECYCLE"));
}
static void pending_case(const char *engine,const char *model,const char *identity_file,const char *helper_file){
    frame_mode(0);SamosaMmSupervisor supervisor;CHECK(samosa_mm_supervisor_init(&supervisor));SamosaMmSession session={0};CHECK(start(0,&supervisor,&session,engine,model));pid_t worker,guardian;identity(identity_file,&worker,&guardian);pid_t helper=pid_file(helper_file);
    CHECK(!kill(guardian,SIGSTOP));pause_ms(50);const char *error=samosa_mm_session_close_owned(&session);CHECK(error&&!strcmp(error,"engine_shutdown_pending"));CHECK(session.supervisor==&supervisor&&session.pid==worker&&samosa_mm_supervisor_is_active(&supervisor,NULL,0));CHECK(!kill(worker,0));CHECK(!samosa_mm_supervisor_active_pgid(&supervisor)&&!samosa_mm_supervisor_cancel(&supervisor));
    SamosaMmSession refused={0};CHECK(!start(0,&supervisor,&refused,engine,model));CHECK(!kill(guardian,SIGCONT));CHECK(!samosa_mm_session_close_owned(&session));cleaned(&supervisor,&session,worker,helper,guardian);samosa_mm_supervisor_destroy(&supervisor);
}
static void failure_cases(const char *engine,const char *model,const char *identity_file,const char *helper_file,const char *root){
    char bad[1024];CHECK(snprintf(bad,sizeof(bad),"%s/not-an-executable",root)<(int)sizeof(bad));FILE *file=fopen(bad,"w");CHECK(file&&fputs("invalid executable\n",file)>=0&&!fclose(file)&&!chmod(bad,0700));
    SamosaMmSupervisor supervisor;CHECK(samosa_mm_supervisor_init(&supervisor));SamosaMmSession session={0};frame_mode(0);CHECK(!start(0,&supervisor,&session,bad,model));CHECK(!session.supervisor&&!samosa_mm_supervisor_is_active(&supervisor,NULL,0));CHECK(!unlink(bad));
    CHECK(!setenv("SAMOSA_FAKE_MM_LIFECYCLE","crash",1));CHECK(!start(0,&supervisor,&session,engine,model));pid_t worker,guardian;identity(identity_file,&worker,&guardian);pid_t helper=pid_file(helper_file);cleaned(&supervisor,&session,worker,helper,guardian);CHECK(!unsetenv("SAMOSA_FAKE_MM_LIFECYCLE"));
    CHECK(start(0,&supervisor,&session,engine,model));identity(identity_file,&worker,&guardian);helper=pid_file(helper_file);char *reply=NULL,error[96];size_t bytes=0;const char *crash="{\"command\":\"crash\"}";CHECK(!samosa_mm_session_request(&session,crash,strlen(crash),&reply,&bytes,error,sizeof(error)));CHECK(!session.active&&session.supervisor);samosa_mm_session_close(&session);cleaned(&supervisor,&session,worker,helper,guardian);samosa_mm_supervisor_destroy(&supervisor);
}
static void inherited_diagnostics(const char *engine,const char *model,const char *identity_file,const char *helper_file,const char *root){
    char log[1024],cache[1024];CHECK(snprintf(log,sizeof(log),"%s/stderr",root)<(int)sizeof(log)&&snprintf(cache,sizeof(cache),"%s/cache",root)<(int)sizeof(cache));CHECK(!setenv("SAMOSA_FAKE_MM_CACHE_PROBE",cache,1)&&!setenv("SAMOSA_FAKE_MM_LIFECYCLE","noisy",1));
    int saved=dup(STDERR_FILENO),out=open(log,O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC,0600);CHECK(saved>=0&&out>=0&&fcntl(saved,F_SETFD,FD_CLOEXEC)==0&&dup2(out,STDERR_FILENO)>=0);close(out);
    SamosaMmSupervisor supervisor;CHECK(samosa_mm_supervisor_init(&supervisor));SamosaMmSession session={0};frame_mode(0);CHECK(start(0,&supervisor,&session,engine,model));CHECK(dup2(saved,STDERR_FILENO)>=0);close(saved);
    pid_t worker,guardian;identity(identity_file,&worker,&guardian);pid_t helper=pid_file(helper_file);healthy(&session);samosa_mm_session_close(&session);cleaned(&supervisor,&session,worker,helper,guardian);samosa_mm_supervisor_destroy(&supervisor);struct stat st;CHECK(!stat(log,&st)&&st.st_size>1048576);CHECK(!stat(cache,&st)&&st.st_size==67108865);CHECK(!unlink(log)&&!unlink(cache));CHECK(!unsetenv("SAMOSA_FAKE_MM_LIFECYCLE")&&!unsetenv("SAMOSA_FAKE_MM_CACHE_PROBE"));
}


static void closed_diagnostics(const char *engine,const char *model,const char *identity_file,const char *helper_file){
    frame_mode(1);CHECK(!setenv("SAMOSA_FAKE_MM_EXIT_AFTER_READY","1",1));int saved=fcntl(STDERR_FILENO,F_DUPFD_CLOEXEC,30);CHECK(saved>=0&&!close(STDERR_FILENO));
    SamosaMmSupervisor supervisor;CHECK(samosa_mm_supervisor_init(&supervisor));SamosaMmSession session={0};CHECK(start(1,&supervisor,&session,engine,model));CHECK(dup2(saved,STDERR_FILENO)>=0);close(saved);CHECK(!unsetenv("SAMOSA_FAKE_MM_EXIT_AFTER_READY"));
    pid_t worker,guardian;identity(identity_file,&worker,&guardian);pid_t helper=pid_file(helper_file);pause_ms(100);
    session.request_timeout_ms=1000;
    size_t n=512*1024;char *request=malloc(n);CHECK(request);memset(request,'x',n);char *reply=NULL,error[96];size_t bytes=0;int64_t begin=now_ms();CHECK(!samosa_mm_session_request(&session,request,n,&reply,&bytes,error,sizeof(error)));int64_t elapsed=now_ms()-begin;free(request);
    samosa_mm_session_close(&session);
#ifndef SAMOSA_SPECIALIST_LEGACY
    cleaned(&supervisor,&session,worker,helper,guardian);
#else
    CHECK(gone(worker));if(!gone(helper)){CHECK(!kill(helper,SIGKILL));CHECK(gone(helper));}CHECK(!samosa_mm_supervisor_is_active(&supervisor,NULL,0));
#endif
    samosa_mm_supervisor_destroy(&supervisor);printf("Closed writer after helper exit: elapsed=%lldms\n",(long long)elapsed);fflush(stdout);CHECK(elapsed<500);
}

#ifndef SAMOSA_SPECIALIST_LEGACY
static void prepared_file(const char *path){int fd=open(path,O_CREAT|O_EXCL|O_WRONLY,0644);CHECK(fd>=0&&write(fd,"prepared pixels\n",16)==16&&!close(fd));}
static void image_handoff(const char *engine,const char *model,const char *identity_file,const char *helper_file,const char *root){
    char source[1024],owned[1024],link_path[1024];CHECK(snprintf(source,sizeof(source),"%s/prepared-page",root)<(int)sizeof(source)&&snprintf(link_path,sizeof(link_path),"%s/prepared-link",root)<(int)sizeof(link_path));
    frame_mode(0);SamosaMmSupervisor supervisor;CHECK(samosa_mm_supervisor_init(&supervisor));SamosaMmSession session={0};CHECK(start(0,&supervisor,&session,engine,model));pid_t worker,guardian;identity(identity_file,&worker,&guardian);pid_t helper=pid_file(helper_file);
    prepared_file(source);CHECK(!linkat(AT_FDCWD,source,AT_FDCWD,link_path,0));CHECK(!samosa_mm_session_take_temporary_image(&session,source,owned,sizeof(owned)));CHECK(!unlink(link_path));
    CHECK(!symlink(source,link_path));CHECK(!samosa_mm_session_take_temporary_image(&session,link_path,owned,sizeof(owned))&&!access(source,F_OK));CHECK(!unlink(link_path));
    CHECK(!samosa_mm_session_take_temporary_image(&session,source,owned,1)&&!access(source,F_OK));
    CHECK(samosa_mm_session_take_temporary_image(&session,source,owned,sizeof(owned)));CHECK(access(source,F_OK)<0&&errno==ENOENT);struct stat st;CHECK(!stat(owned,&st)&&st.st_size==16&&!(st.st_mode&077));
    prepared_file(source);CHECK(!samosa_mm_session_return_temporary_image(&session,source));CHECK(!stat(source,&st)&&st.st_size==16&&!access(owned,F_OK));CHECK(!unlink(source));
    CHECK(samosa_mm_session_return_temporary_image(&session,source));CHECK(access(owned,F_OK)<0&&errno==ENOENT);CHECK(!stat(source,&st)&&st.st_nlink==1&&st.st_size==16);
    // Keep the existing unload/lower-side/restart path usable after give-back.
    samosa_mm_session_close(&session);cleaned(&supervisor,&session,worker,helper,guardian);CHECK(!access(source,F_OK));
    CHECK(start(0,&supervisor,&session,engine,model));identity(identity_file,&worker,&guardian);helper=pid_file(helper_file);CHECK(samosa_mm_session_take_temporary_image(&session,source,owned,sizeof(owned)));healthy(&session);
    char workspace[1024];CHECK(strlen(owned)<sizeof(workspace));strcpy(workspace,owned);char *slash=strrchr(workspace,'/');CHECK(slash);*slash=0;
    samosa_mm_session_close(&session);cleaned(&supervisor,&session,worker,helper,guardian);CHECK(access(owned,F_OK)<0&&errno==ENOENT);CHECK(access(workspace,F_OK)<0&&errno==ENOENT);samosa_mm_supervisor_destroy(&supervisor);
}
#endif

static void parent_death(const char *engine,const char *model,const char *identity_file,const char *helper_file,const char *ready,int framed){
    (void)unlink(ready);(void)unlink(identity_file);(void)unlink(helper_file);frame_mode(framed);CHECK(!setenv("SAMOSA_FAKE_MM_LIFECYCLE","stubborn",1)&&!setenv("SAMOSA_FAKE_MM_HANG_READY",ready,1));int notice[2];CHECK(!pipe(notice)&&!fcntl(notice[0],F_SETFD,FD_CLOEXEC)&&!fcntl(notice[1],F_SETFD,FD_CLOEXEC));
    host=fork();CHECK(host>=0);if(!host){close(notice[0]);SamosaMmSupervisor supervisor;CHECK(samosa_mm_supervisor_init(&supervisor));SamosaMmSession session={0};CHECK(start(framed,&supervisor,&session,engine,model));
#ifndef SAMOSA_SPECIALIST_LEGACY
        char prepared[1024],owned[1024];CHECK(snprintf(prepared,sizeof(prepared),"%s/caller-page",model)<(int)sizeof(prepared));prepared_file(prepared);CHECK(samosa_mm_session_take_temporary_image(&session,prepared,owned,sizeof(owned)));CHECK(write(notice[1],owned,strlen(owned)+1)==(ssize_t)strlen(owned)+1);
#else
        CHECK(write(notice[1],"R",2)==2);
#endif
        close(notice[1]);const char *hang="{\"command\":\"hang\"}";char *reply=NULL,error[96];size_t bytes=0;(void)samosa_mm_session_request(&session,hang,strlen(hang),&reply,&bytes,error,sizeof(error));_exit(6);}
    close(notice[1]);char owned[1024]={0};ssize_t count=read(notice[0],owned,sizeof(owned)-1);CHECK(count>1&&owned[count-1]==0);close(notice[0]);int seen=0;for(int i=0;i<300;i++){if(!access(ready,F_OK)){seen=1;break;}pause_ms(10);}CHECK(seen);
    pid_t worker,guardian;identity(identity_file,&worker,&guardian);pid_t helper=pid_file(helper_file);CHECK(guardian!=host&&guardian!=getpid()&&worker!=peer&&helper!=peer);CHECK(!kill(host,SIGKILL));int status;CHECK(waitpid(host,&status,0)==host&&WIFSIGNALED(status));host=0;CHECK(gone(worker)&&gone(helper)&&gone(guardian));
#ifndef SAMOSA_SPECIALIST_LEGACY
    CHECK(access(owned,F_OK)<0&&errno==ENOENT);char *slash=strrchr(owned,'/');CHECK(slash);*slash=0;CHECK(access(owned,F_OK)<0&&errno==ENOENT);
#endif
    CHECK(!kill(peer,0));CHECK(!unsetenv("SAMOSA_FAKE_MM_LIFECYCLE")&&!unsetenv("SAMOSA_FAKE_MM_HANG_READY"));
}

int main(int argc,char **argv){
#ifndef SAMOSA_SPECIALIST_LEGACY
    int dispatch=samosa_mm_process_dispatch(argc,argv);if(dispatch>=0)return dispatch;
#endif
    CHECK((argc==2||argc==3)&&argv[1][0]=='/');alarm(120);
    char root[]="/tmp/samosa-visual-guardian-XXXXXX";CHECK(mkdtemp(root));char helper_file[1024],identity_file[1024],ready[1024],model[1024];CHECK(snprintf(helper_file,sizeof(helper_file),"%s/helper",root)<(int)sizeof(helper_file));CHECK(snprintf(identity_file,sizeof(identity_file),"%s/identity",root)<(int)sizeof(identity_file)&&snprintf(ready,sizeof(ready),"%s/hang-ready",root)<(int)sizeof(ready)&&snprintf(model,sizeof(model),"%s/model with \"quotes\" $(literal)",root)<(int)sizeof(model));CHECK(!mkdir(model,0700));
    CHECK(!setenv("SAMOSA_FAKE_MM_EXPECTED_MODEL",model,1)&&!setenv("SAMOSA_FAKE_MM_IDENTITY",identity_file,1)&&!setenv("SAMOSA_FAKE_MM_HELPER_PID_FILE",helper_file,1));peer=fork();CHECK(peer>=0);if(!peer){for(;;)pause();}
    int ok=getenv("SAMOSA_SPECIALIST_CLOSED_ONLY")?1:helper_reproduction(argv[1],model,helper_file);
    if(argc==2&&ok&&getenv("SAMOSA_SPECIALIST_CLOSED_ONLY")){puts("BEGIN closed diagnostics");fflush(stdout);closed_diagnostics(argv[1],model,identity_file,helper_file);puts("END closed diagnostics");}
    if(argc==2&&ok&&!getenv("SAMOSA_SPECIALIST_CLOSED_ONLY")){boundary_cases(argv[1],model,identity_file,helper_file);grace_cases(argv[1],model,identity_file,helper_file);pending_case(argv[1],model,identity_file,helper_file);failure_cases(argv[1],model,identity_file,helper_file,root);inherited_diagnostics(argv[1],model,identity_file,helper_file,root);closed_diagnostics(argv[1],model,identity_file,helper_file);
#ifndef SAMOSA_SPECIALIST_LEGACY
    image_handoff(argv[1],model,identity_file,helper_file,root);
#endif
    parent_death(argv[1],model,identity_file,helper_file,ready,0);parent_death(argv[1],model,identity_file,helper_file,ready,1);}
    CHECK(!kill(peer,SIGKILL)&&waitpid(peer,NULL,0)==peer);peer=0;CHECK(!unlink(helper_file)&&!unlink(identity_file));if(!access(ready,F_OK))CHECK(!unlink(ready));CHECK(!rmdir(model)&&!rmdir(root));
    printf("Visual guardian fixture: %u checks\n",atomic_load(&checks));return ok?0:1;
}
