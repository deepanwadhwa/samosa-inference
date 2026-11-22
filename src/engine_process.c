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
#include <limits.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
extern char **environ;

int engine_process_executable(char self[PATH_MAX]) {
#ifdef __APPLE__
    uint32_t size=PATH_MAX;char path[PATH_MAX];
    return !_NSGetExecutablePath(path,&size)&&realpath(path,self)!=NULL;
#else
    ssize_t n=readlink("/proc/self/exe",self,PATH_MAX-1);if(n<=0)return 0;self[n]=0;return 1;
#endif
}
static double monotonic(void) {
    struct timespec t;
    return clock_gettime(CLOCK_MONOTONIC,&t)?-1:(double)t.tv_sec+(double)t.tv_nsec/1e9;
}
static int cloexec(int fd){return fcntl(fd,F_SETFD,FD_CLOEXEC)==0;}
static int fifo_mode(int fd,int mode){
    struct stat s;int flags=fcntl(fd,F_GETFL);
    return flags>=0&&(flags&O_ACCMODE)==mode&&!fstat(fd,&s)&&S_ISFIFO(s.st_mode);
}
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
    (void)unlinkat(6,"input.wav",0);(void)unlinkat(6,"result.srt",0);(void)unlinkat(6,"result.txt",0);
    (void)unlinkat(6,"request.frame",0);(void)unlinkat(6,"result.frame",0);
    (void)unlinkat(6,"input.rgba",0);(void)unlinkat(6,"input.ppm",0);(void)unlinkat(6,"request.json",0);(void)unlinkat(6,"result.json",0);
    /* Temporal adapters create only these sixteen owned frame names. */
    for(size_t k=0;k<16;k++){char frame[32];snprintf(frame,sizeof(frame),"frame-%02zu.ppm",k);(void)unlinkat(6,frame,0);}
    (void)rmdir(path);
}
/* Called while the retained owner lock and unreaped group leader still reserve
 * this marker/identity. Never remove another PID's marker. */
static void clean_server_marker(const char *path,pid_t child) {
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_NONBLOCK);if(fd<0)return;
    char text[64];ssize_t n=read(fd,text,sizeof(text)-1);struct stat held,named;
    if(n>0&&n<(ssize_t)sizeof(text)-1&&!fstat(fd,&held)&&S_ISREG(held.st_mode)&&
       held.st_uid==geteuid()&&!lstat(path,&named)&&held.st_dev==named.st_dev&&held.st_ino==named.st_ino){
        text[n]=0;char expected[64];snprintf(expected,sizeof(expected),"%ld\n",(long)child);
        if(!strcmp(text,expected))(void)unlink(path);
    }
    close(fd);
}
static void server_term(pid_t child) {
    (void)kill(-child,SIGTERM);double deadline=monotonic()+8;
    for(;;){siginfo_t info={0};int rc=waitid(P_PID,(id_t)child,&info,WEXITED|WNOHANG|WNOWAIT);
        if(!rc&&info.si_pid==child)break;
        if(rc<0&&errno!=EINTR)break;
        double now=monotonic();if(now<0||now>=deadline)break;
        struct timespec delay={0,100000000};nanosleep(&delay,NULL);
    }
}
int engine_guardian_dispatch(int argc,char **argv) {
    int specialist=argc>=2&&!strcmp(argv[1],"--inference-owned-engine-specialist");
    int command_profile=argc>=2&&!strcmp(argv[1],"--inference-owned-engine-command");
    int server=argc>=2&&!strcmp(argv[1],"--inference-owned-engine-server");
    int workspace_duplex=argc>=2&&!strcmp(argv[1],"--inference-owned-engine-workspace-duplex");
    int uncapped=argc>=2&&!strcmp(argv[1],"--inference-owned-engine-workspace-uncapped");
    int duplex=specialist||workspace_duplex||(argc>=2&&!strcmp(argv[1],"--inference-owned-engine-duplex"));
    int captured=argc>=2&&!strcmp(argv[1],"--inference-owned-engine-workspace-captured");
    int sized=argc>=2&&!strcmp(argv[1],"--inference-owned-engine-protocol-sized");
    int protocol=sized||(argc>=2&&!strcmp(argv[1],"--inference-owned-engine-protocol"));
    int workspace=uncapped||specialist||command_profile||workspace_duplex||captured||protocol||(argc>=2&&!strcmp(argv[1],"--inference-owned-engine-workspace"));
    int bound=(duplex&&!specialist)||(!command_profile&&!specialist&&workspace)||(argc>=2&&!strcmp(argv[1],"--inference-owned-engine-bound"));
    int leased=specialist||server||bound||(argc>=2&&!strcmp(argv[1],"--inference-owned-engine-leased"));
    if(argc<2 || (!leased&&!command_profile&&strcmp(argv[1],"--inference-owned-engine")))return -1;
    if(argc<(sized?7:(workspace||server)?6:5) || argc>(sized?38:(workspace||server)?37:36) || fcntl(3,F_GETFD)<0)return 2;
    /* Captured output is reserved for our existing bounded bridge. */
    if(captured&&(argc!=11||strcmp(argv[5],argv[0])||strcmp(argv[6],"--vision-engine-bounded-bridge")))return 2;
    char *end;errno=0;double lifetime=strtod(argv[2],&end);
    if(errno||*end||end==argv[2]||((server||command_profile||specialist)?lifetime!=0:(!(lifetime>0)||lifetime>86400)))return 2;
    errno=0;unsigned long limit=strtoul(argv[3],&end,10);
    if(errno||*end||end==argv[3]||!limit||limit>1048576)return 2;
    unsigned long result_limit=8196;
    if(sized){errno=0;result_limit=strtoul(argv[5],&end,10);
        if(errno||*end||end==argv[5]||!result_limit||result_limit>16777216)return 2;}
    if(!cloexec(3))return 2;
    if(leased&&!cloexec(4))return 2;
    if(server&&(argc!=7||strcmp(argv[5],argv[0])||strcmp(argv[6],"--inference-primary-backend")||
       fcntl(9,F_GETFD)<0||fcntl(9,F_SETFD,0)||!cloexec(10)||!cloexec(11)))return 2;
    if(bound&&(fcntl(5,F_GETFL)<0||(fcntl(5,F_GETFL)&O_ACCMODE)!=O_RDONLY||fcntl(5,F_SETFD,0)))return 2;
    if(workspace&&!cloexec(6))return 2;
    if(captured){struct stat result;
        if(!cloexec(8)||fstat(8,&result)||!S_ISREG(result.st_mode)||result.st_uid!=geteuid()||
           result.st_nlink!=1||(result.st_mode&077)||result.st_size||
           (fcntl(8,F_GETFL)&O_ACCMODE)!=O_RDWR)return 2;}
    if(protocol&&(!cloexec(7)||!cloexec(8)||(fcntl(7,F_GETFL)&O_ACCMODE)!=O_RDONLY||(fcntl(8,F_GETFL)&O_ACCMODE)!=O_RDWR))return 2;
    if(duplex&&(!fifo_mode(7,O_RDONLY)||!fifo_mode(8,O_WRONLY)||!cloexec(7)||!cloexec(8)))return 2;
    /* The guardian captures GPU stdout into a bounded result. A process-wide file
     * cap would also reject growth of Metal's existing shared shader cache. */
    /* Duplex protocols retain their pipe/caller bounds, including with an
     * owned workspace: a file cap would also constrain the Metal cache.
     * Sized protocols additionally get an exact kernel-enforced file cap.
     * Legacy workspace/summary limits remain unchanged. */
    if(workspace&&!command_profile&&!captured&&!duplex&&!uncapped){rlim_t cap=sized?(rlim_t)result_limit:67108864;
        struct rlimit size={cap,cap};if(setrlimit(RLIMIT_FSIZE,&size))return 2;}
    if(sized||captured){struct rlimit core={0,0};if(setrlimit(RLIMIT_CORE,&core))return 2;}
    /* An orphaned stopped guardian group receives SIGHUP/SIGCONT when its
     * caller dies. Survive HUP so the lifeline can trigger owned-group reap;
     * spawned workers still get their explicit default signal dispositions. */
    signal(SIGHUP,SIG_IGN);
    signal(SIGPIPE,SIG_IGN);
    int output[2],capture[2]={-1,-1};if(pipe(output))return 2;
    if(captured&&(pipe(capture)||!cloexec(capture[0])||!cloexec(capture[1]))){
        close(output[0]);close(output[1]);if(capture[0]>=0)close(capture[0]);if(capture[1]>=0)close(capture[1]);return 2;}
    if(!cloexec(output[0])||!cloexec(output[1])){close(output[0]);close(output[1]);return 2;}
    posix_spawn_file_actions_t actions;posix_spawnattr_t attributes;
    int ar=posix_spawn_file_actions_init(&actions)==0,at=posix_spawnattr_init(&attributes)==0;
    int ok=ar&&at;pid_t child=-1;
    if(ok)ok=spawn_attributes(&attributes) &&
        !((protocol||duplex)?posix_spawn_file_actions_adddup2(&actions,7,STDIN_FILENO):posix_spawn_file_actions_addopen(&actions,STDIN_FILENO,"/dev/null",O_RDONLY,0)) &&
        !(command_profile?posix_spawn_file_actions_addopen(&actions,STDOUT_FILENO,"/dev/null",O_WRONLY,0):posix_spawn_file_actions_adddup2(&actions,server?10:captured?capture[1]:(protocol||duplex)?8:output[1],STDOUT_FILENO)) &&
        !(command_profile?posix_spawn_file_actions_addopen(&actions,STDERR_FILENO,"/dev/null",O_WRONLY,0):posix_spawn_file_actions_adddup2(&actions,specialist?4:server?11:output[1],STDERR_FILENO)) &&
        !posix_spawn_file_actions_addclose(&actions,3);
    if(ok && output[0]>3)ok=!posix_spawn_file_actions_addclose(&actions,output[0]);
    if(ok && output[1]>3)ok=!posix_spawn_file_actions_addclose(&actions,output[1]);
    if(ok&&captured)ok=!posix_spawn_file_actions_addclose(&actions,capture[0])&&
        !posix_spawn_file_actions_addclose(&actions,capture[1]);
    int command=sized?6:(workspace||server)?5:4;
    if(ok)ok=posix_spawn(&child,argv[command],&actions,&attributes,argv+command,environ)==0;
    if(ar)posix_spawn_file_actions_destroy(&actions);
    if(at)posix_spawnattr_destroy(&attributes);
    close(output[1]);if(capture[1]>=0)close(capture[1]);
    /* Never keep a spare protocol reader/writer in the guardian. EOF and a
     * lost caller must reach the resident worker and host independently. */
    if(duplex){close(7);close(8);}
    if(!ok){close(output[0]);if(capture[0]>=0)close(capture[0]);if(workspace)clean_workspace(argv[4]);fputs("exec_failed\n",stdout);fflush(stdout);return 2;}
    if(((server||command_profile||specialist)?fprintf(stdout,"started:%ld\n",(long)child):fputs("started\n",stdout))<0 || fflush(stdout)){finish_child(child);close(output[0]);if(capture[0]>=0)close(capture[0]);if(workspace)clean_workspace(argv[4]);return 2;}
    if(server){close(9);close(10);close(11);}
    if(server||command_profile||specialist){close(output[0]);output[0]=-1;}
    double start=monotonic(),deadline=start+lifetime;const char *reason="stopped";
    if(start<0)reason="clock_failed";
    size_t logged=0,captured_bytes=0;int exited=0;
    while(start>=0) {
        if(protocol){struct stat result;if(fstat(8,&result)||(result.st_size<0||(uint64_t)result.st_size>result_limit)){reason="protocol_limit";break;}}
        siginfo_t info;memset(&info,0,sizeof(info));
        if(waitid(P_PID,(id_t)child,&info,WEXITED|WNOHANG|WNOWAIT)<0){
            if(errno==EINTR)continue;
            reason="wait_failed";break;}
        if(info.si_pid==child&&!exited){reason=info.si_code==CLD_EXITED && !info.si_status?"completed":
            sized&&info.si_status==SIGXFSZ&&(info.si_code==CLD_KILLED||info.si_code==CLD_DUMPED)?"protocol_limit":"engine_failed";
            exited=1;
            if(!captured)break;
            /* Stop descendants holding a writer before draining the remaining
             * result. Keep the direct child unreaped until group cleanup. */
            (void)kill(-child,SIGKILL);}
        if(exited&&captured&&capture[0]<0)break;
        double now=monotonic();if(now<0){reason="clock_failed";break;}
        if(!server&&!command_profile&&!specialist&&now>=deadline){reason="lifetime_exceeded";break;}
        struct pollfd fds[3]={{3,POLLIN|POLLHUP,0},{output[0],POLLIN|POLLHUP,0},{capture[0],POLLIN|POLLHUP,0}};
        int rc=poll(fds,captured?3:2,10);
        if(rc<0){if(errno==EINTR)continue;reason="poll_failed";break;}
        if(fds[0].revents&(POLLIN|POLLHUP|POLLERR|POLLNVAL))break;
        if(fds[1].revents&POLLIN){char bytes[4096];ssize_t n=read(output[0],bytes,sizeof(bytes));
            if(n<0 && errno==EINTR)continue;
            if(n<0){reason="log_failed";break;}
            if((size_t)n>limit-logged){reason="log_limit";break;}logged+=(size_t)n;}
        if(captured&&(fds[2].revents&(POLLIN|POLLHUP|POLLERR|POLLNVAL))){
            unsigned char bytes[4096];ssize_t n=read(capture[0],bytes,sizeof(bytes));
            if(n<0&&errno==EINTR)continue;
            if(n<0){reason="protocol_limit";break;}
            if(!n){close(capture[0]);capture[0]=-1;}
            else{
                if((size_t)n>ENGINE_VISUAL_RESULT_MAX-captured_bytes){reason="protocol_limit";break;}
                size_t done=0;
                while(done<(size_t)n){ssize_t wrote=pwrite(8,bytes+done,(size_t)n-done,(off_t)(captured_bytes+done));
                    if(wrote<0&&errno==EINTR)continue;
                    if(wrote<=0){reason="protocol_limit";break;}done+=(size_t)wrote;}
                if(done!=(size_t)n)break;
                captured_bytes+=(size_t)n;
            }
        }
        if((fds[1].revents&POLLHUP) && !(fds[1].revents&POLLIN)){
            close(output[0]);output[0]=-1;
        }
    }
    if(server||command_profile||specialist){
        if(server&&!exited)server_term(child);
        /* Keep the PID reserved through marker cleanup and host acknowledgement.
         * Closing the lifeline after owner death also releases this wait. */
        (void)kill(-child,SIGKILL);
        fprintf(stdout,"%s\n",reason);fflush(stdout);
        for(;;){struct pollfd owner={3,POLLIN|POLLHUP,0};int rc=poll(&owner,1,10);
            if(rc<0&&errno==EINTR)continue;
            if(rc<0||owner.revents)break;
        }
        if(server)clean_server_marker(argv[4],child);
    }
    finish_child(child);close(output[0]);if(capture[0]>=0)close(capture[0]);if(captured)close(8);close(3);
    if(workspace){clean_workspace(argv[4]);close(6);}
    if(leased)close(4);
    if(bound)close(5);
    if(!server&&!command_profile&&!specialist){fprintf(stdout,"%s\n",reason);fflush(stdout);}
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
                        size_t limit,int residency,int model,const char *workspace,int directory,int input,int result,size_t result_limit,int transport,const EngineServer *server,EngineProcess *p) {
    int captured=transport==1,duplex=transport==2||transport==5,command_profile=transport==4,specialist=transport==5;
    *p=(EngineProcess){.guardian=-1,.control=-1,.response=-1};
    if(!self || self[0]!='/' || !argv_json || strlen(argv_json)>(command_profile?65536:16384) ||
       ((server||command_profile||specialist)?lifetime!=0:(!(lifetime>0)||lifetime>86400))||!limit||limit>1048576)return "invalid_engine_configuration";
    Json j;if(!json_parse(&j,argv_json,strlen(argv_json))||j.tokens[0].type!=JT_ARRAY||
        j.tokens[0].count<1||j.tokens[0].count>32)return "invalid_engine_configuration";
    int sized=result_limit!=0;
    if((captured&&(result<0||input>=0))||(sized&&(input<0||result_limit>16777216)))return "invalid_engine_configuration";
    char *args[39]={0};int first=sized?6:(workspace||server)?5:4,count=first;char seconds[32],bytes[32],result_bytes[32];
    snprintf(result_bytes,sizeof(result_bytes),"%zu",result_limit);
    snprintf(seconds,sizeof(seconds),"%.6f",lifetime);snprintf(bytes,sizeof(bytes),"%zu",limit);
    args[0]=(char*)self;args[1]=transport==6?"--inference-owned-engine-workspace-uncapped":specialist?"--inference-owned-engine-specialist":command_profile?"--inference-owned-engine-command":server?"--inference-owned-engine-server":duplex?(workspace?"--inference-owned-engine-workspace-duplex":"--inference-owned-engine-duplex"):captured?"--inference-owned-engine-workspace-captured":sized?"--inference-owned-engine-protocol-sized":input>=0?"--inference-owned-engine-protocol":workspace?"--inference-owned-engine-workspace":model>=0?"--inference-owned-engine-bound":residency>=0?"--inference-owned-engine-leased":"--inference-owned-engine";args[2]=seconds;args[3]=bytes;if(workspace)args[4]=(char*)workspace;if(server)args[4]=(char*)server->marker;if(sized)args[5]=result_bytes;
    const char *error=NULL;
    for(int t=1;t<j.tokens[0].next;t=j.tokens[t].next){args[count]=json_string(&j,t);
        if(!args[count]||strlen(args[count])>4096){error="invalid_engine_configuration";break;}count++;}
    if(!error && !command_profile && args[first][0]!='/')error="invalid_engine_configuration";
    int server_copy[3]={-1,-1,-1};
    if(server){int source[3]={server->configuration,server->output,server->error};
        for(size_t i=0;i<3;i++){server_copy[i]=fcntl(source[i],F_DUPFD_CLOEXEC,20);if(server_copy[i]<0)error="engine_io_failed";}}
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
        for(size_t i=0;ok&&i<3;i++)if(server_copy[i]>=0)ok=!posix_spawn_file_actions_adddup2(&actions,server_copy[i],(int)i+9);
        int all[4]={control[0],control[1],response[0],response[1]};
        for(size_t i=0;ok&&i<4;i++)if(all[i]>3 && !(server&&all[i]>=9&&all[i]<=11) && !(lease_copy>=0&&all[i]==4) && !(model_copy>=0&&all[i]==5) && !(directory_copy>=0&&all[i]==6) && !(input_copy>=0&&all[i]==7) && !(result_copy>=0&&all[i]==8))ok=!posix_spawn_file_actions_addclose(&actions,all[i]);
        if(ok&&lease_copy>=0)ok=!posix_spawn_file_actions_addclose(&actions,lease_copy);
        if(ok&&model_copy>=0)ok=!posix_spawn_file_actions_addclose(&actions,model_copy);
        if(ok&&directory_copy>=0)ok=!posix_spawn_file_actions_addclose(&actions,directory_copy);
        if(ok&&input_copy>=0)ok=!posix_spawn_file_actions_addclose(&actions,input_copy);
        if(ok&&result_copy>=0)ok=!posix_spawn_file_actions_addclose(&actions,result_copy);
        for(size_t i=0;ok&&i<3;i++)if(server_copy[i]>=0)ok=!posix_spawn_file_actions_addclose(&actions,server_copy[i]);
        if(!ok || posix_spawn(&p->guardian,self,&actions,&attr,args,environ))error="engine_guardian_unavailable";
        if(ar)posix_spawn_file_actions_destroy(&actions);
        if(at)posix_spawnattr_destroy(&attr);
    }
    if(control[0]>=0)close(control[0]);
    if(response[1]>=0)close(response[1]);
    if(lease_copy>=0)close(lease_copy);
    if(model_copy>=0)close(model_copy);
    p->server=server!=NULL;
    if(p->guardian>0){p->control=control[1];p->response=response[0];char reply[64];
        double now=monotonic();
        if(now<0 || !line(p->response,reply,now+2))error="engine_exec_failed";
        else if(server||command_profile||specialist){char *end=NULL;long pid=!strncmp(reply,"started:",8)?strtol(reply+8,&end,10):0;
            if(pid<=1||pid>INT32_MAX||!end||*end)error="engine_exec_failed";else p->engine=(pid_t)pid;
        }else if(strcmp(reply,"started"))error="engine_exec_failed";
        if(error){const char *stopped=engine_process_stop(p);
            if(stopped && !strcmp(stopped,"engine_shutdown_pending"))error="engine_shutdown_pending";}
    }else{if(control[1]>=0)close(control[1]);if(response[0]>=0)close(response[0]);}
    if(directory_copy>=0)close(directory_copy);
    if(input_copy>=0)close(input_copy);
    if(result_copy>=0)close(result_copy);
    for(size_t i=0;i<3;i++)if(server_copy[i]>=0)close(server_copy[i]);
    for(size_t i=(size_t)first;i<39;i++)free(args[i]);
    return error;
}
const char *engine_process_start(const char *self,const char *argv,double lifetime,size_t limit,EngineProcess *p){
    return start(self,argv,lifetime,limit,-1,-1,NULL,-1,-1,-1,0,0,NULL,p);
}
const char *engine_process_start_leased(const char *self,const char *argv,double lifetime,size_t limit,int residency,EngineProcess *p){
    if(residency<0){*p=(EngineProcess){.guardian=-1,.control=-1,.response=-1};return "invalid_engine_configuration";}
    return start(self,argv,lifetime,limit,residency,-1,NULL,-1,-1,-1,0,0,NULL,p);
}
const char *engine_process_start_bound(const char *self,const char *argv,double lifetime,size_t limit,int residency,int model,EngineProcess *p){
    if(residency<0||model<0){*p=(EngineProcess){.guardian=-1,.control=-1,.response=-1};return "invalid_engine_configuration";}
    return start(self,argv,lifetime,limit,residency,model,NULL,-1,-1,-1,0,0,NULL,p);
}
const char *engine_process_start_bound_duplex(const char *self,const char *argv,double lifetime,size_t limit,int residency,int model,int input,int output,EngineProcess *p){
    if(!p)return "invalid_engine_configuration";
    *p=(EngineProcess){.guardian=-1,.control=-1,.response=-1};
    if(residency<0||model<0||!fifo_mode(input,O_RDONLY)||!fifo_mode(output,O_WRONLY))return "invalid_engine_configuration";
    return start(self,argv,lifetime,limit,residency,model,NULL,-1,input,output,0,2,NULL,p);
}
const char *engine_process_start_bound_workspace_duplex(const char *self,const char *argv,double lifetime,size_t limit,int residency,int model,const char *workspace,int directory,int input,int output,EngineProcess *p){
    if(!p)return "invalid_engine_configuration";
    *p=(EngineProcess){.guardian=-1,.control=-1,.response=-1};
    struct stat retained,named;
    if(!workspace||workspace[0]!='/'||directory<0||residency<0||model<0||
       !fifo_mode(input,O_RDONLY)||!fifo_mode(output,O_WRONLY)||
       fstat(directory,&retained)||lstat(workspace,&named)||!S_ISDIR(retained.st_mode)||
       retained.st_uid!=geteuid()||(retained.st_mode&077)||
       retained.st_dev!=named.st_dev||retained.st_ino!=named.st_ino)
        return "invalid_engine_configuration";
    return start(self,argv,lifetime,limit,residency,model,workspace,directory,input,output,0,2,NULL,p);
}
const char *engine_process_start_bound_workspace(const char *self,const char *argv,double lifetime,size_t limit,int residency,int model,const char *workspace,int directory,EngineProcess *p){
    if(!workspace||workspace[0]!='/'||directory<0||residency<0||model<0){*p=(EngineProcess){.guardian=-1,.control=-1,.response=-1};return "invalid_engine_configuration";}
    return start(self,argv,lifetime,limit,residency,model,workspace,directory,-1,-1,0,0,NULL,p);
}
const char *engine_process_start_bound_workspace_uncapped(const char *self,const char *argv,double lifetime,size_t limit,int residency,int model,const char *workspace,int directory,EngineProcess *p){
    if(!workspace||workspace[0]!='/'||directory<0||residency<0||model<0){*p=(EngineProcess){.guardian=-1,.control=-1,.response=-1};return "invalid_engine_configuration";}
    return start(self,argv,lifetime,limit,residency,model,workspace,directory,-1,-1,0,6,NULL,p);
}
const char *engine_process_start_bound_workspace_captured(const char *self,const char *argv,double lifetime,size_t limit,int residency,int model,const char *workspace,int directory,int result,EngineProcess *p){
    if(!workspace||workspace[0]!='/'||directory<0||residency<0||model<0){*p=(EngineProcess){.guardian=-1,.control=-1,.response=-1};return "invalid_engine_configuration";}
    return start(self,argv,lifetime,limit,residency,model,workspace,directory,-1,result,0,1,NULL,p);
}
const char *engine_process_start_bound_protocol(const char *self,const char *argv,double lifetime,size_t limit,int residency,int model,const char *workspace,int directory,int input,int result,EngineProcess *p){
    if(!workspace||workspace[0]!='/'||directory<0||residency<0||model<0||input<0||result<0){*p=(EngineProcess){.guardian=-1,.control=-1,.response=-1};return "invalid_engine_configuration";}
    return start(self,argv,lifetime,limit,residency,model,workspace,directory,input,result,0,0,NULL,p);
}
const char *engine_process_start_bound_protocol_sized(const char *self,const char *argv,double lifetime,size_t limit,int residency,int model,const char *workspace,int directory,int input,int result,size_t result_limit,EngineProcess *p){
    if(!p)return "invalid_engine_configuration";
    *p=(EngineProcess){.guardian=-1,.control=-1,.response=-1};
    if(!workspace||workspace[0]!='/'||directory<0||residency<0||model<0||input<0||result<0||!result_limit||result_limit>16777216)return "invalid_engine_configuration";
    return start(self,argv,lifetime,limit,residency,model,workspace,directory,input,result,result_limit,0,NULL,p);
}
/* Returned terminal errors outlive the caller-owned process record. The
 * guardian protocol is closed; unknown terminal words refuse generically. */
static const char *terminal_error(const EngineProcess *p) {
    static const char *const errors[]={"engine_failed","clock_failed","lifetime_exceeded","log_failed","log_limit","poll_failed","protocol_limit","wait_failed","engine_guardian_failed"};
    if(!strcmp(p->reason,"stopped")||!strcmp(p->reason,"completed"))return NULL;
    for(size_t i=0;i<sizeof(errors)/sizeof(*errors);i++)if(!strcmp(p->reason,errors[i]))return errors[i];
    return "engine_guardian_failed";
}
const char *engine_process_start_server(const char *self,const char *argv,int lease,const EngineServer *server,EngineProcess *p) {
    if(!p)return "invalid_engine_configuration";
    *p=(EngineProcess){.guardian=-1,.control=-1,.response=-1};
    if(!server||!server->marker||server->marker[0]!='/'||lease<0||server->configuration<0||server->output<0||server->error<0)return "invalid_engine_configuration";
    return start(self,argv,0,1,lease,-1,NULL,-1,-1,-1,0,3,server,p);
}
const char *engine_process_start_command(const char *self,const char *argv,const char *workspace,int directory,EngineProcess *p) {
    if(!p)return "invalid_engine_configuration";
    *p=(EngineProcess){.guardian=-1,.control=-1,.response=-1};
    struct stat retained,named;
    if(!workspace||workspace[0]!='/'||directory<0||fstat(directory,&retained)||lstat(workspace,&named)||
       !S_ISDIR(retained.st_mode)||retained.st_uid!=geteuid()||(retained.st_mode&077)||
       retained.st_dev!=named.st_dev||retained.st_ino!=named.st_ino)return "invalid_engine_configuration";
    return start(self,argv,0,1,-1,-1,workspace,directory,-1,-1,0,4,NULL,p);
}
const char *engine_process_start_specialist(const char *self,const char *argv,int input,int output,int error_fd,const char *workspace,int directory,EngineProcess *p) {
    if(!p)return "invalid_engine_configuration";
    *p=(EngineProcess){.guardian=-1,.control=-1,.response=-1};
    struct stat retained,named;
    if(!workspace||workspace[0]!='/'||directory<0||fstat(directory,&retained)||lstat(workspace,&named)||!S_ISDIR(retained.st_mode)||retained.st_uid!=geteuid()||(retained.st_mode&077)||retained.st_dev!=named.st_dev||retained.st_ino!=named.st_ino)return "invalid_engine_configuration";
    if(error_fd<0||!fifo_mode(input,O_RDONLY)||!fifo_mode(output,O_WRONLY))return "invalid_engine_configuration";
    /* Reuse retained-descriptor plumbing: FD4 is inherited stderr in this
     * compatibility profile, not a claim of global residency admission. */
    return start(self,argv,0,1,error_fd,-1,workspace,directory,input,output,0,5,NULL,p);
}
int engine_process_running(EngineProcess *p) {
    if(p->guardian<=0||p->terminal||p->reason[0])return 0;
    struct pollfd watch={p->response,POLLIN|POLLHUP,0};int rc;
    do{rc=poll(&watch,1,0);}while(rc<0&&errno==EINTR);
    if(rc<0){snprintf(p->reason,sizeof(p->reason),"engine_guardian_failed");return 0;}
    if(!rc)return 1;
    char reply[64];double now=monotonic();
    if(now>=0&&line(p->response,reply,now+0.1))snprintf(p->reason,sizeof(p->reason),"%s",reply);
    else snprintf(p->reason,sizeof(p->reason),"engine_guardian_failed");
    return 0;
}
const char *engine_process_stop(EngineProcess *p) {
    if(p->control>=0){close(p->control);p->control=-1;}
    if(p->guardian<=0){if(p->response>=0)close(p->response);p->response=-1;
        return p->terminal?terminal_error(p):NULL;}
    char reply[64];double now=monotonic();int observed=p->reason[0]!=0;
    if(!observed && now>=0 && line(p->response,reply,now+(p->server?10:2))){
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
