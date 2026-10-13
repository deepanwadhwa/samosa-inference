#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "engine_process.h"
#include "json.h"
#include <spawn.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <sys/wait.h>
#include <time.h>
#include <errno.h>
#include <sys/stat.h>
#include <sys/resource.h>
extern char **environ;

static double monotonic(void) {
    struct timespec t;
    return clock_gettime(CLOCK_MONOTONIC,&t)?-1:(double)t.tv_sec+(double)t.tv_nsec/1e9;
}
static int cloexec(int fd){return fcntl(fd,F_SETFD,FD_CLOEXEC)==0;}
static int spawn_attributes(posix_spawnattr_t *a) {
    sigset_t empty,defaults;sigemptyset(&empty);sigemptyset(&defaults);
    sigaddset(&defaults,SIGPIPE);sigaddset(&defaults,SIGTERM);sigaddset(&defaults,SIGINT);
    sigaddset(&defaults,SIGHUP);sigaddset(&defaults,SIGCHLD);sigaddset(&defaults,SIGXFSZ);
    return !posix_spawnattr_setflags(a,POSIX_SPAWN_SETPGROUP|POSIX_SPAWN_SETSIGDEF|POSIX_SPAWN_SETSIGMASK) &&
        !posix_spawnattr_setpgroup(a,0) && !posix_spawnattr_setsigmask(a,&empty) &&
        !posix_spawnattr_setsigdefault(a,&defaults);
}
static void finish_child(pid_t child) {
    /* Child has not been reaped, so its process-group ID cannot be reused. */
    (void)kill(-child,SIGKILL);
    int status;while(waitpid(child,&status,0)<0 && errno==EINTR){}
}
static void clean_workspace(const char *path){
    struct stat retained,current;
    if(fstat(6,&retained)||lstat(path,&current)||!S_ISDIR(retained.st_mode)||retained.st_uid!=geteuid()||(retained.st_mode&077)||retained.st_dev!=current.st_dev||retained.st_ino!=current.st_ino)return;
    (void)unlinkat(6,"input.wav",0);(void)unlinkat(6,"result.srt",0);
    (void)unlinkat(6,"request.frame",0);(void)unlinkat(6,"result.frame",0);
    (void)unlinkat(6,"input.ppm",0);(void)unlinkat(6,"request.json",0);(void)unlinkat(6,"result.json",0);
    /* Temporal adapters create only these sixteen owned frame names. */
    for(size_t k=0;k<16;k++){char frame[32];snprintf(frame,sizeof(frame),"frame-%02zu.ppm",k);(void)unlinkat(6,frame,0);}
    (void)rmdir(path);
}
int engine_guardian_dispatch(int argc,char **argv) {
    int sized=argc>=2&&!strcmp(argv[1],"--inference-owned-engine-protocol-sized");
    int protocol=sized||(argc>=2&&!strcmp(argv[1],"--inference-owned-engine-protocol"));
    int workspace=protocol||(argc>=2&&!strcmp(argv[1],"--inference-owned-engine-workspace"));
    int bound=workspace||(argc>=2&&!strcmp(argv[1],"--inference-owned-engine-bound"));
    int leased=bound||(argc>=2&&!strcmp(argv[1],"--inference-owned-engine-leased"));
    if(argc<2 || (!leased&&strcmp(argv[1],"--inference-owned-engine")))return -1;
    if(argc<(sized?7:workspace?6:5) || argc>(sized?38:workspace?37:36) || fcntl(3,F_GETFD)<0)return 2;
    char *end;errno=0;double lifetime=strtod(argv[2],&end);
    if(errno||*end||end==argv[2]||!(lifetime>0)||lifetime>86400)return 2;
    errno=0;unsigned long limit=strtoul(argv[3],&end,10);
    if(errno||*end||end==argv[3]||!limit||limit>1048576)return 2;
    unsigned long result_limit=8196;
    if(sized){errno=0;result_limit=strtoul(argv[5],&end,10);
        if(errno||*end||end==argv[5]||!result_limit||result_limit>16777216)return 2;}
    if(!cloexec(3))return 2;
    if(leased&&!cloexec(4))return 2;
    if(bound&&(fcntl(5,F_GETFL)<0||(fcntl(5,F_GETFL)&O_ACCMODE)!=O_RDONLY||fcntl(5,F_SETFD,0)))return 2;
    if(workspace&&!cloexec(6))return 2;
    if(protocol&&(!cloexec(7)||!cloexec(8)||(fcntl(7,F_GETFL)&O_ACCMODE)!=O_RDONLY||(fcntl(8,F_GETFL)&O_ACCMODE)!=O_RDWR))return 2;
    /* Metal initialization also writes a temporary compiled library. Keep that
     * existing engine behavior while bounding individual child-created files. */
    /* Sized protocols additionally get an exact kernel-enforced file cap.
     * Legacy workspace/summary limits remain unchanged. */
    if(workspace){rlim_t cap=sized?(rlim_t)result_limit:67108864;
        struct rlimit size={cap,cap};if(setrlimit(RLIMIT_FSIZE,&size))return 2;}
    if(sized){struct rlimit core={0,0};if(setrlimit(RLIMIT_CORE,&core))return 2;}
    signal(SIGPIPE,SIG_IGN);
    int output[2];if(pipe(output))return 2;
    if(!cloexec(output[0])||!cloexec(output[1])){close(output[0]);close(output[1]);return 2;}
    posix_spawn_file_actions_t actions;posix_spawnattr_t attributes;
    int ar=posix_spawn_file_actions_init(&actions)==0,at=posix_spawnattr_init(&attributes)==0;
    int ok=ar&&at;pid_t child=-1;
    if(ok)ok=spawn_attributes(&attributes) &&
        !(protocol?posix_spawn_file_actions_adddup2(&actions,7,STDIN_FILENO):posix_spawn_file_actions_addopen(&actions,STDIN_FILENO,"/dev/null",O_RDONLY,0)) &&
        !posix_spawn_file_actions_adddup2(&actions,protocol?8:output[1],STDOUT_FILENO) &&
        !posix_spawn_file_actions_adddup2(&actions,output[1],STDERR_FILENO) &&
        !posix_spawn_file_actions_addclose(&actions,3);
    if(ok && output[0]>3)ok=!posix_spawn_file_actions_addclose(&actions,output[0]);
    if(ok && output[1]>3)ok=!posix_spawn_file_actions_addclose(&actions,output[1]);
    int command=sized?6:workspace?5:4;
    if(ok)ok=posix_spawn(&child,argv[command],&actions,&attributes,argv+command,environ)==0;
    if(ar)posix_spawn_file_actions_destroy(&actions);
    if(at)posix_spawnattr_destroy(&attributes);
    close(output[1]);
    if(!ok){close(output[0]);if(workspace)clean_workspace(argv[4]);fputs("exec_failed\n",stdout);fflush(stdout);return 2;}
    if(fputs("started\n",stdout)<0 || fflush(stdout)){finish_child(child);close(output[0]);if(workspace)clean_workspace(argv[4]);return 2;}
    double start=monotonic(),deadline=start+lifetime;const char *reason="stopped";
    if(start<0)reason="clock_failed";
    size_t logged=0;int exited=0;
    while(start>=0) {
        if(protocol){struct stat result;if(fstat(8,&result)||(result.st_size<0||(uint64_t)result.st_size>result_limit)){reason="protocol_limit";break;}}
        siginfo_t info;memset(&info,0,sizeof(info));
        if(waitid(P_PID,(id_t)child,&info,WEXITED|WNOHANG|WNOWAIT)<0){
            if(errno==EINTR)continue;
            reason="wait_failed";break;}
        if(info.si_pid==child){reason=info.si_code==CLD_EXITED && !info.si_status?"completed":
            sized&&info.si_status==SIGXFSZ&&(info.si_code==CLD_KILLED||info.si_code==CLD_DUMPED)?"protocol_limit":"engine_failed";
            exited=1;break;}
        double now=monotonic();if(now<0){reason="clock_failed";break;}
        if(now>=deadline){reason="lifetime_exceeded";break;}
        struct pollfd fds[2]={{3,POLLIN|POLLHUP,0},{output[0],POLLIN|POLLHUP,0}};
        int rc=poll(fds,2,10);
        if(rc<0){if(errno==EINTR)continue;reason="poll_failed";break;}
        if(fds[0].revents&(POLLIN|POLLHUP|POLLERR|POLLNVAL))break;
        if(fds[1].revents&POLLIN){char bytes[4096];ssize_t n=read(output[0],bytes,sizeof(bytes));
            if(n<0 && errno==EINTR)continue;
            if(n<0){reason="log_failed";break;}
            if((size_t)n>limit-logged){reason="log_limit";break;}logged+=(size_t)n;}
        if((fds[1].revents&POLLHUP) && !(fds[1].revents&POLLIN)){
            close(output[0]);output[0]=-1;
        }
    }
    finish_child(child);close(output[0]);close(3);
    if(workspace){clean_workspace(argv[4]);close(6);}
    if(leased)close(4);
    if(bound)close(5);
    fprintf(stdout,"%s\n",reason);fflush(stdout);
    return !strcmp(reason,"stopped") || (exited&&!strcmp(reason,"completed"))?0:1;
}

static int line(int fd,char buffer[64],double deadline) {
    size_t used=0;
    while(used<63) {
        double now=monotonic();if(now<0 || now>=deadline)return 0;
        struct pollfd p={fd,POLLIN|POLLHUP,0};int rc=poll(&p,1,10);
        if(rc<0){if(errno==EINTR)continue;return 0;}
        if(!rc)continue;
        char c;ssize_t n=read(fd,&c,1);if(n<0&&errno==EINTR)continue;if(n!=1)return 0;
        if(c=='\n'){buffer[used]=0;return 1;}buffer[used++]=c;
    }
    return 0;
}
static const char *start(const char *self,const char *argv_json,double lifetime,
                        size_t limit,int residency,int model,const char *workspace,int directory,int input,int result,size_t result_limit,EngineProcess *p) {
    *p=(EngineProcess){.guardian=-1,.control=-1,.response=-1};
    if(!self || self[0]!='/' || !argv_json || strlen(argv_json)>16384 ||
       !(lifetime>0)||lifetime>86400||!limit||limit>1048576)return "invalid_engine_configuration";
    Json j;if(!json_parse(&j,argv_json,strlen(argv_json))||j.tokens[0].type!=JT_ARRAY||
        j.tokens[0].count<1||j.tokens[0].count>32)return "invalid_engine_configuration";
    int sized=result_limit!=0;
    if(sized&&(input<0||result_limit>16777216))return "invalid_engine_configuration";
    char *args[39]={0};int first=sized?6:workspace?5:4,count=first;char seconds[32],bytes[32],result_bytes[32];
    snprintf(result_bytes,sizeof(result_bytes),"%zu",result_limit);
    snprintf(seconds,sizeof(seconds),"%.6f",lifetime);snprintf(bytes,sizeof(bytes),"%zu",limit);
    args[0]=(char*)self;args[1]=sized?"--inference-owned-engine-protocol-sized":input>=0?"--inference-owned-engine-protocol":workspace?"--inference-owned-engine-workspace":model>=0?"--inference-owned-engine-bound":residency>=0?"--inference-owned-engine-leased":"--inference-owned-engine";args[2]=seconds;args[3]=bytes;if(workspace)args[4]=(char*)workspace;if(sized)args[5]=result_bytes;
    const char *error=NULL;
    for(int t=1;t<j.tokens[0].next;t=j.tokens[t].next){args[count]=json_string(&j,t);
        if(!args[count]||strlen(args[count])>4096){error="invalid_engine_configuration";break;}count++;}
    if(!error && args[first][0]!='/')error="invalid_engine_configuration";
    int control[2]={-1,-1},response[2]={-1,-1};
    int lease_copy=residency>=0?fcntl(residency,F_DUPFD_CLOEXEC,20):-1;
    if(residency>=0&&lease_copy<0)error="engine_io_failed";
    int model_copy=model>=0?fcntl(model,F_DUPFD_CLOEXEC,20):-1;
    if(model>=0&&model_copy<0)error="engine_io_failed";
    int directory_copy=directory>=0?fcntl(directory,F_DUPFD_CLOEXEC,20):-1;
    if(directory>=0&&directory_copy<0)error="engine_io_failed";
    int input_copy=input>=0?fcntl(input,F_DUPFD_CLOEXEC,20):-1;
    int result_copy=result>=0?fcntl(result,F_DUPFD_CLOEXEC,20):-1;
    if((input>=0&&input_copy<0)||(result>=0&&result_copy<0))error="engine_io_failed";
    if(!error && (pipe(control)||pipe(response)))error="engine_io_failed";
    /* Avoid a same-fd dup2 action retaining CLOEXEC on macOS. */
    if(!error && control[0]==3){int moved=fcntl(control[0],F_DUPFD,10);
        if(moved<0)error="engine_io_failed";
        else{close(control[0]);control[0]=moved;}}
    if(!error) {
        int all[4]={control[0],control[1],response[0],response[1]};
        for(size_t i=0;i<4;i++)if(!cloexec(all[i]))error="engine_io_failed";
    }
    if(!error) {
        posix_spawn_file_actions_t actions;posix_spawnattr_t attr;
        int ar=posix_spawn_file_actions_init(&actions)==0,at=posix_spawnattr_init(&attr)==0;
        int ok=ar&&at;
        if(ok)ok=spawn_attributes(&attr) &&
            !posix_spawn_file_actions_adddup2(&actions,control[0],3) &&
            !posix_spawn_file_actions_adddup2(&actions,response[1],STDOUT_FILENO) &&
            !posix_spawn_file_actions_addopen(&actions,STDIN_FILENO,"/dev/null",O_RDONLY,0) &&
            !posix_spawn_file_actions_addopen(&actions,STDERR_FILENO,"/dev/null",O_WRONLY,0);
        if(ok&&lease_copy>=0)ok=!posix_spawn_file_actions_adddup2(&actions,lease_copy,4);
        if(ok&&model_copy>=0)ok=!posix_spawn_file_actions_adddup2(&actions,model_copy,5);
        if(ok&&directory_copy>=0)ok=!posix_spawn_file_actions_adddup2(&actions,directory_copy,6);
        if(ok&&input_copy>=0)ok=!posix_spawn_file_actions_adddup2(&actions,input_copy,7);
        if(ok&&result_copy>=0)ok=!posix_spawn_file_actions_adddup2(&actions,result_copy,8);
        int all[4]={control[0],control[1],response[0],response[1]};
        for(size_t i=0;ok&&i<4;i++)if(all[i]>3 && !(lease_copy>=0&&all[i]==4) && !(model_copy>=0&&all[i]==5) && !(directory_copy>=0&&all[i]==6) && !(input_copy>=0&&all[i]==7) && !(result_copy>=0&&all[i]==8))ok=!posix_spawn_file_actions_addclose(&actions,all[i]);
        if(ok&&lease_copy>=0)ok=!posix_spawn_file_actions_addclose(&actions,lease_copy);
        if(ok&&model_copy>=0)ok=!posix_spawn_file_actions_addclose(&actions,model_copy);
        if(ok&&directory_copy>=0)ok=!posix_spawn_file_actions_addclose(&actions,directory_copy);
        if(ok&&input_copy>=0)ok=!posix_spawn_file_actions_addclose(&actions,input_copy);
        if(ok&&result_copy>=0)ok=!posix_spawn_file_actions_addclose(&actions,result_copy);
        if(!ok || posix_spawn(&p->guardian,self,&actions,&attr,args,environ))error="engine_guardian_unavailable";
        if(ar)posix_spawn_file_actions_destroy(&actions);
        if(at)posix_spawnattr_destroy(&attr);
    }
    if(control[0]>=0)close(control[0]);
    if(response[1]>=0)close(response[1]);
    if(lease_copy>=0)close(lease_copy);
    if(model_copy>=0)close(model_copy);
    if(p->guardian>0){p->control=control[1];p->response=response[0];char reply[64];
        double now=monotonic();
        if(now<0 || !line(p->response,reply,now+2) || strcmp(reply,"started"))error="engine_exec_failed";
        if(error){const char *stopped=engine_process_stop(p);
            if(stopped && !strcmp(stopped,"engine_shutdown_pending"))error="engine_shutdown_pending";}
    }else{if(control[1]>=0)close(control[1]);if(response[0]>=0)close(response[0]);}
    if(directory_copy>=0)close(directory_copy);
    if(input_copy>=0)close(input_copy);
    if(result_copy>=0)close(result_copy);
    for(size_t i=(size_t)first;i<39;i++)free(args[i]);
    return error;
}
const char *engine_process_start(const char *self,const char *argv,double lifetime,size_t limit,EngineProcess *p){
    return start(self,argv,lifetime,limit,-1,-1,NULL,-1,-1,-1,0,p);
}
const char *engine_process_start_leased(const char *self,const char *argv,double lifetime,size_t limit,int residency,EngineProcess *p){
    if(residency<0){*p=(EngineProcess){.guardian=-1,.control=-1,.response=-1};return "invalid_engine_configuration";}
    return start(self,argv,lifetime,limit,residency,-1,NULL,-1,-1,-1,0,p);
}
const char *engine_process_start_bound(const char *self,const char *argv,double lifetime,size_t limit,int residency,int model,EngineProcess *p){
    if(residency<0||model<0){*p=(EngineProcess){.guardian=-1,.control=-1,.response=-1};return "invalid_engine_configuration";}
    return start(self,argv,lifetime,limit,residency,model,NULL,-1,-1,-1,0,p);
}
const char *engine_process_start_bound_workspace(const char *self,const char *argv,double lifetime,size_t limit,int residency,int model,const char *workspace,int directory,EngineProcess *p){
    if(!workspace||workspace[0]!='/'||directory<0||residency<0||model<0){*p=(EngineProcess){.guardian=-1,.control=-1,.response=-1};return "invalid_engine_configuration";}
    return start(self,argv,lifetime,limit,residency,model,workspace,directory,-1,-1,0,p);
}
const char *engine_process_start_bound_protocol(const char *self,const char *argv,double lifetime,size_t limit,int residency,int model,const char *workspace,int directory,int input,int result,EngineProcess *p){
    if(!workspace||workspace[0]!='/'||directory<0||residency<0||model<0||input<0||result<0){*p=(EngineProcess){.guardian=-1,.control=-1,.response=-1};return "invalid_engine_configuration";}
    return start(self,argv,lifetime,limit,residency,model,workspace,directory,input,result,0,p);
}
const char *engine_process_start_bound_protocol_sized(const char *self,const char *argv,double lifetime,size_t limit,int residency,int model,const char *workspace,int directory,int input,int result,size_t result_limit,EngineProcess *p){
    if(!p)return "invalid_engine_configuration";
    *p=(EngineProcess){.guardian=-1,.control=-1,.response=-1};
    if(!workspace||workspace[0]!='/'||directory<0||residency<0||model<0||input<0||result<0||!result_limit||result_limit>16777216)return "invalid_engine_configuration";
    return start(self,argv,lifetime,limit,residency,model,workspace,directory,input,result,result_limit,p);
}
/* Returned terminal errors outlive the caller-owned process record. The
 * guardian protocol is closed; unknown terminal words refuse generically. */
static const char *terminal_error(const EngineProcess *p) {
    static const char *const errors[]={"engine_failed","clock_failed","lifetime_exceeded","log_failed","log_limit","poll_failed","protocol_limit","wait_failed","engine_guardian_failed"};
    if(!strcmp(p->reason,"stopped")||!strcmp(p->reason,"completed"))return NULL;
    for(size_t i=0;i<sizeof(errors)/sizeof(*errors);i++)if(!strcmp(p->reason,errors[i]))return errors[i];
    return "engine_guardian_failed";
}
const char *engine_process_stop(EngineProcess *p) {
    if(p->control>=0){close(p->control);p->control=-1;}
    if(p->guardian<=0){if(p->response>=0)close(p->response);p->response=-1;
        return p->terminal?terminal_error(p):NULL;}
    char reply[64];double now=monotonic();int observed=p->reason[0]!=0;
    if(!observed && now>=0 && line(p->response,reply,now+2)){
        snprintf(p->reason,sizeof(p->reason),"%s",reply);observed=1;
    }
    /* A guardian is a reserved direct child. Never signal an external PID. */
    int status;pid_t reaped;double deadline=monotonic()+2;
    for(;;){reaped=waitpid(p->guardian,&status,WNOHANG);
        if(reaped>0 || (reaped<0&&errno!=EINTR))break;
        double current=monotonic();
        if(current<0 || current>=deadline)return "engine_shutdown_pending";
        struct timespec delay={0,10000000};nanosleep(&delay,NULL);
    }
    close(p->response);p->response=-1;p->guardian=-1;p->terminal=1;
    if(reaped<0 || !observed)snprintf(p->reason,sizeof(p->reason),"engine_guardian_failed");
    return terminal_error(p);
}
