#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "engine_process.h"
#include "json.h"
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <signal.h>
#include <spawn.h>
#include <unistd.h>
#include <time.h>
#include <poll.h>
extern char **environ;
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void pause_ms(void){struct timespec d={0,10000000};nanosleep(&d,NULL);}
static void terminal(EngineProcess *p){struct pollfd watch={p->response,POLLIN|POLLHUP,0};int rc;
    do{rc=poll(&watch,1,5000);}while(rc<0&&errno==EINTR);CHECK(rc>0);}
static void expect_stop(EngineProcess *p,const char *reason){const char *e=engine_process_stop(p);CHECK(e&&!strcmp(e,reason));}
static int lease(const char *path){int fd=open(path,O_RDWR|O_CREAT,0600);if(fd<0||flock(fd,LOCK_EX|LOCK_NB))return -1;return fd;}
static int released(const char *path){int fd=open(path,O_RDWR);if(fd<0)return 0;int ok=!flock(fd,LOCK_EX|LOCK_NB);close(fd);return ok;}
static void observe(const char *path,int wanted){int seen=0;for(int i=0;i<300;i++){if(!access(path,F_OK)&&released(path)==wanted){seen=1;break;}pause_ms();}CHECK(seen);}
static char *command(const char *self,const char *mode,const char *path){char *out=NULL;size_t n;FILE *f=open_memstream(&out,&n);CHECK(f);
    fputc('[',f);json_quote(f,self,strlen(self));fputs(",\"--fixture\",",f);json_quote(f,mode,strlen(mode));fputc(',',f);json_quote(f,path,strlen(path));fputc(']',f);CHECK(!fclose(f));return out;}
static int fixture(const char *mode,const char *path){
    if(!strcmp(mode,"fail"))return 9;
    if(!strcmp(mode,"protocol")||!strcmp(mode,"protocol-flood")){
        fputs("diagnostic belongs only in stderr\n",stderr);
        if(!strcmp(mode,"protocol-flood")){char b[9000];memset(b,'x',sizeof(b));return write(1,b,sizeof(b))==(ssize_t)sizeof(b)?0:4;}
        char b[128];ssize_t n;while((n=read(0,b,sizeof(b)))>0)if(write(1,b,(size_t)n)!=n)return 4;
        return n<0?4:0;
    }

    int fd=lease(path);if(fd<0)return 4;
    if(!strcmp(mode,"grandchild")||!strcmp(mode,"crash")){
        char descendant[512];snprintf(descendant,sizeof(descendant),"%s.descendant",path);
        pid_t pid=fork();if(pid<0)return 4;if(!pid){int hold=lease(descendant);if(hold<0)_exit(4);for(;;)pause();}
    }
    if(!strcmp(mode,"crash")){for(int i=0;i<30;i++){pause_ms();}kill(getpid(),SIGKILL);return 4;}
    if(!strcmp(mode,"flood")){char bytes[4096];memset(bytes,'x',sizeof(bytes));for(;;)if(write(STDOUT_FILENO,bytes,sizeof(bytes))<1)return 4;}
    if(!strcmp(mode,"closed-log")){close(STDOUT_FILENO);close(STDERR_FILENO);}
    for(;;)pause();
}
int main(int argc,char **argv){
    int guardian=engine_guardian_dispatch(argc,argv);if(guardian>=0)return guardian;
    if(argc==4&&!strcmp(argv[1],"--fixture"))return fixture(argv[2],argv[3]);
    if((argc==4||argc==5)&&!strcmp(argv[1],"--parent-worker")){
        char *cmd=command(argv[0],"grandchild",argv[2]);EngineProcess p;
        int residency=argc==5?lease(argv[4]):-1;
        if(argc==5&&residency<0)return 3;
        const char *error=argc==5?engine_process_start_leased(argv[0],cmd,30,65536,residency,&p):engine_process_start(argv[0],cmd,30,65536,&p);
        free(cmd);if(error)return 3;
        int fd=open(argv[3],O_WRONLY|O_CREAT|O_EXCL,0600);if(fd<0)return 3;close(fd);for(;;)pause();
    }
    CHECK(argv[0][0]=='/');char root[]="/tmp/samosa-engine-process-XXXXXX";CHECK(mkdtemp(root));
    char path[512],descendant[512],ready[512];snprintf(path,sizeof(path),"%s/engine",root);
    snprintf(descendant,sizeof(descendant),"%s/engine.descendant",root);snprintf(ready,sizeof(ready),"%s/ready",root);
    EngineProcess p;char *cmd=command(argv[0],"grandchild",path);
    CHECK(!engine_process_start(argv[0],cmd,30,65536,&p));observe(path,0);observe(descendant,0);
    CHECK(!engine_process_stop(&p));observe(path,1);observe(descendant,1);CHECK(!engine_process_stop(&p));free(cmd);
    CHECK(!unlink(path));CHECK(!unlink(descendant));
    cmd=command(argv[0],"crash",path);CHECK(!engine_process_start(argv[0],cmd,30,65536,&p));
    observe(path,0);observe(descendant,0);terminal(&p);
    expect_stop(&p,"engine_failed");observe(path,1);observe(descendant,1);
    free(cmd);CHECK(!unlink(path));CHECK(!unlink(descendant));
    cmd=command(argv[0],"sleep",path);CHECK(!engine_process_start(argv[0],cmd,0.1,65536,&p));
    observe(path,0);terminal(&p);expect_stop(&p,"lifetime_exceeded");observe(path,1);free(cmd);CHECK(!unlink(path));
    cmd=command(argv[0],"closed-log",path);CHECK(!engine_process_start(argv[0],cmd,30,65536,&p));observe(path,0);
    CHECK(!engine_process_stop(&p));observe(path,1);free(cmd);CHECK(!unlink(path));
    char residency_path[512];snprintf(residency_path,sizeof(residency_path),"%s/residency",root);
    int residency=lease(residency_path);CHECK(residency>=0);
    cmd=command(argv[0],"sleep",path);CHECK(!engine_process_start_leased(argv[0],cmd,30,65536,residency,&p));observe(path,0);
    CHECK(!close(residency));CHECK(!released(residency_path));
    CHECK(!kill(p.guardian,SIGSTOP));
    int stopped_status;CHECK(waitpid(p.guardian,&stopped_status,WUNTRACED)==p.guardian);CHECK(WIFSTOPPED(stopped_status));
    expect_stop(&p,"engine_shutdown_pending");CHECK(p.guardian>0&&p.response>=0&&!p.terminal);
    CHECK(!released(path));CHECK(!kill(p.guardian,SIGCONT));CHECK(!engine_process_stop(&p));observe(path,1);
    observe(residency_path,1);free(cmd);CHECK(!unlink(path));CHECK(!unlink(residency_path));
    cmd=command(argv[0],"flood",path);CHECK(!engine_process_start(argv[0],cmd,30,1024,&p));
    terminal(&p);expect_stop(&p,"log_limit");observe(path,1);free(cmd);CHECK(!unlink(path));
    cmd=command(argv[0],"fail",path);CHECK(!engine_process_start(argv[0],cmd,30,65536,&p));
    terminal(&p);expect_stop(&p,"engine_failed");free(cmd);
    CHECK(!strcmp(engine_process_start(argv[0],"[\"/missing/engine\"]",30,65536,&p),"engine_exec_failed"));
    CHECK(!strcmp(engine_process_start(argv[0],"[\"relative\"]",30,65536,&p),"invalid_engine_configuration"));
    CHECK(!strcmp(engine_process_start(argv[0],"[]",30,65536,&p),"invalid_engine_configuration"));
    CHECK(!strcmp(engine_process_start(argv[0],"[\"/missing\"]",0,65536,&p),"invalid_engine_configuration"));
    CHECK(!strcmp(engine_process_start(argv[0],"[\"/missing\"]",30,1048577,&p),"invalid_engine_configuration"));
    /* Killing the owning parent releases only this lifeline. The independent
     * unrelated fixture retains its lease while the owned group is stopped. */
    char foreign[512];snprintf(foreign,sizeof(foreign),"%s/foreign",root);pid_t external;
    char *foreign_args[]={argv[0],"--fixture","sleep",foreign,NULL};CHECK(!posix_spawn(&external,argv[0],NULL,NULL,foreign_args,environ));observe(foreign,0);
    pid_t parent;char *worker_args[]={argv[0],"--parent-worker",path,ready,residency_path,NULL};CHECK(!posix_spawn(&parent,argv[0],NULL,NULL,worker_args,environ));
    int seen=0;for(int i=0;i<300;i++){if(!access(ready,F_OK)){seen=1;break;}pause_ms();}CHECK(seen);
    observe(path,0);observe(descendant,0);observe(residency_path,0);CHECK(!kill(parent,SIGKILL));int status;CHECK(waitpid(parent,&status,0)==parent);
    observe(path,1);observe(descendant,1);observe(residency_path,1);CHECK(!released(foreign));CHECK(!kill(external,SIGKILL));CHECK(waitpid(external,&status,0)==external);observe(foreign,1);
    CHECK(!unlink(path));CHECK(!unlink(descendant));CHECK(!unlink(ready));CHECK(!unlink(foreign));CHECK(!unlink(residency_path));/* The actual monolith protocol needs stdin and stdout separate from logs. */
    for(int flood=0;flood<2;flood++){
        char workspace[512],input_path[600],result_path[600];
        snprintf(workspace,sizeof(workspace),"%s/protocol",root);CHECK(!mkdir(workspace,0700));
        snprintf(input_path,sizeof(input_path),"%s/request.frame",workspace);
        snprintf(result_path,sizeof(result_path),"%s/result.frame",workspace);
        int in=open(input_path,O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(in>=0);
        const char frame[]={0,0,0,3,'a','b','c'};CHECK(write(in,frame,sizeof(frame))==(ssize_t)sizeof(frame));CHECK(!close(in));
        in=open(input_path,O_RDONLY);int result=open(result_path,O_RDWR|O_CREAT|O_EXCL,0600);
        int directory=open(workspace,O_RDONLY|O_DIRECTORY);int model=open(input_path,O_RDONLY);
        residency=lease(residency_path);CHECK(in>=0&&result>=0&&directory>=0&&model>=0&&residency>=0);
        cmd=command(argv[0],flood?"protocol-flood":"protocol",path);
        CHECK(!engine_process_start_bound_protocol(argv[0],cmd,30,65536,residency,model,workspace,directory,in,result,&p));
        terminal(&p);
        if(flood)expect_stop(&p,"protocol_limit");else{CHECK(!engine_process_stop(&p));CHECK(!strcmp(p.reason,"completed"));char actual[128];CHECK(pread(result,actual,sizeof(actual),0)==(ssize_t)sizeof(frame));CHECK(!memcmp(actual,frame,sizeof(frame)));}
        CHECK(access(workspace,F_OK)<0&&errno==ENOENT);
        free(cmd);close(in);close(result);close(directory);close(model);close(residency);CHECK(!unlink(residency_path));
    }
    CHECK(!rmdir(root));

    printf("PASS engine process ownership: %u checks; bounded logs/lifetime, owned stop/group/parent-death, foreign survivor\n",checks);return 0;
}
