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
static void observe(const char *path,int wanted){int seen=0;for(int i=0;i<300;i++){if(!access(path,F_OK)&&released(path)==wanted){seen=1;break;}pause_ms();}if(!seen)fprintf(stderr,"lease observation timed out: %s, released=%d\n",path,wanted);CHECK(seen);}
static char *command(const char *self,const char *mode,const char *path){char *out=NULL;size_t n;FILE *f=open_memstream(&out,&n);CHECK(f);
    fputc('[',f);json_quote(f,self,strlen(self));fputs(",\"--fixture\",",f);json_quote(f,mode,strlen(mode));fputc(',',f);json_quote(f,path,strlen(path));fputc(']',f);CHECK(!fclose(f));return out;}
static char *captured_command(const char *self,const char *mode,const char *path,const char *input,const char *result){
    const char *args[]={self,"--vision-engine-bounded-bridge",mode,path,input,result};
    char *body=NULL;size_t bytes=0;FILE *out=open_memstream(&body,&bytes);CHECK(out);
    fputc('[',out);for(size_t k=0;k<6;k++){if(k)fputc(',',out);json_quote(out,args[k],strlen(args[k]));}
    fputc(']',out);CHECK(!fclose(out));return body;
}
static void protocol_pipes(int input[2],int output[2]){
    CHECK(!pipe(input)&&!pipe(output));int all[]={input[0],input[1],output[0],output[1]};
    for(size_t i=0;i<4;i++)CHECK(!fcntl(all[i],F_SETFD,FD_CLOEXEC));
}
static void exact(int fd,const void *wanted,size_t length){
    unsigned char bytes[4096];CHECK(length<=sizeof(bytes));size_t used=0;
    while(used<length){struct pollfd watch={fd,POLLIN|POLLHUP,0};CHECK(poll(&watch,1,3000)>0);
        ssize_t n=read(fd,bytes+used,length-used);CHECK(n>0);used+=(size_t)n;}
    CHECK(!memcmp(bytes,wanted,length));
}
static void backpressure(int fd){
    for(unsigned i=0;i<300;i++){
        struct pollfd watch={fd,POLLOUT,0};int ready=poll(&watch,1,0);CHECK(ready>=0);
        if(!ready)return;
        CHECK(!(watch.revents&(POLLERR|POLLHUP|POLLNVAL)));pause_ms();
    }
    CHECK(0);
}
static int fixture(const char *mode,const char *path){
    if(!strcmp(mode,"fail"))return 9;
    if(!strcmp(mode,"complete"))return 0;
    if(!strncmp(mode,"duplex",6)){
        char model[3];if(pread(5,model,3,0)!=3||memcmp(model,"abc",3))return 4;
        int held=lease(path);if(held<0)return 4;
        char descendant[512];snprintf(descendant,sizeof(descendant),"%s.descendant",path);
        pid_t pid=fork();if(pid<0)return 4;
        if(!pid){close(0);close(1);close(2);int hold=lease(descendant);if(hold<0)_exit(4);for(;;)pause();}
        int ready=0;for(int i=0;i<300;i++){if(!access(descendant,F_OK)&&!released(descendant)){ready=1;break;}pause_ms();}
        if(!ready)return 4;
        fputs("resident diagnostic on stderr\n",stderr);fflush(stderr);
        if(write(STDOUT_FILENO,model,3)!=3)return 4;
        if(!strcmp(mode,"duplex-closed-output")){close(STDOUT_FILENO);for(;;)pause();}
        if(!strcmp(mode,"duplex-log")||!strcmp(mode,"duplex-flood")){
            char bytes[4096];memset(bytes,'x',sizeof(bytes));int output=!strcmp(mode,"duplex-log")?STDERR_FILENO:STDOUT_FILENO;
            for(;;)if(write(output,bytes,sizeof(bytes))<1)return 4;
        }
        char bytes[128];ssize_t n;
        while((n=read(STDIN_FILENO,bytes,sizeof(bytes)))>0){
            if(!strcmp(mode,"duplex-crash")){kill(getpid(),SIGKILL);return 4;}
            if(!strcmp(mode,"duplex-hup")){kill(getpid(),SIGHUP);return 0;}
            if(write(STDOUT_FILENO,bytes,(size_t)n)!=n)return 4;
        }
        return n<0?4:0;
    }
    if(!strcmp(mode,"protocol")||!strcmp(mode,"protocol-flood")||!strcmp(mode,"protocol-large")){
        fputs("diagnostic belongs only in stderr\n",stderr);
        if(!strcmp(mode,"protocol-flood")){char b[9000];memset(b,'x',sizeof(b));return write(1,b,sizeof(b))==(ssize_t)sizeof(b)?0:4;}
        if(!strcmp(mode,"protocol-large")){char b[8192];memset(b,'x',sizeof(b));for(unsigned i=0;i<16;i++)if(write(1,b,sizeof(b))!=(ssize_t)sizeof(b))return 4;return 0;}
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
static void leased_command_tests(const char *self,const char *root) {
    char workspace[512],residency_path[512],engine_path[512];
    snprintf(workspace,sizeof(workspace),"%s/command-workspace",root);
    snprintf(residency_path,sizeof(residency_path),"%s/command-residency",root);
    snprintf(engine_path,sizeof(engine_path),"%s/command-engine",root);
    CHECK(!mkdir(workspace,0700));
    int directory=open(workspace,O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    int residency=lease(residency_path);
    CHECK(directory>=0 && residency>=0);
    EngineProcess p;
    char *cmd=command(self,"sleep",engine_path);
    CHECK(!engine_process_start_command(self,cmd,workspace,directory,residency,1,&p));
    CHECK(!close(directory) && !close(residency));
    observe(engine_path,0);
    terminal(&p);
    CHECK(!engine_process_running(&p) && !strcmp(p.reason,"lifetime_exceeded"));
    CHECK(!released(residency_path) && !access(workspace,F_OK));
    expect_stop(&p,"lifetime_exceeded");
    CHECK(released(residency_path) && access(workspace,F_OK)<0 && errno==ENOENT);
    free(cmd);
    CHECK(!unlink(engine_path));

    CHECK(!mkdir(workspace,0700));
    directory=open(workspace,O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    residency=lease(residency_path);
    CHECK(directory>=0 && residency>=0);
    cmd=command(self,"complete",engine_path);
    CHECK(!engine_process_start_command(self,cmd,workspace,directory,residency,0,&p));
    terminal(&p);
    CHECK(!engine_process_running(&p) && !strcmp(p.reason,"completed"));
    CHECK(!kill(p.guardian,SIGKILL));
    expect_stop(&p,"engine_guardian_failed");
    CHECK(!released(residency_path));
    CHECK(!close(directory) && !close(residency));
    CHECK(!rmdir(workspace) && !unlink(residency_path));
    free(cmd);
}
static void duplex_tests(const char *self,const char *root){
    char path[512],descendant[600],residency_path[512],model_path[512],ready[512],foreign[512];
    snprintf(path,sizeof(path),"%s/duplex-engine",root);snprintf(descendant,sizeof(descendant),"%s.descendant",path);
    snprintf(residency_path,sizeof(residency_path),"%s/duplex-residency",root);snprintf(model_path,sizeof(model_path),"%s/duplex-model",root);
    snprintf(ready,sizeof(ready),"%s/duplex-ready",root);snprintf(foreign,sizeof(foreign),"%s/duplex-foreign",root);
    int fd=open(model_path,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);CHECK(fd>=0);
    CHECK(write(fd,"abc",3)==3&&!close(fd));int model=open(model_path,O_RDONLY|O_CLOEXEC);CHECK(model>=0);
    /* The worker uses bound fd5, even when no original model pathname exists. */
    CHECK(!unlink(model_path));
    pid_t external;char *foreign_args[]={(char *)self,"--fixture","sleep",foreign,NULL};
    CHECK(!posix_spawn(&external,self,NULL,NULL,foreign_args,environ));observe(foreign,0);
    const char *modes[]={"duplex","duplex-eof","duplex-crash","duplex-flood","duplex-log","duplex-closed-output","duplex-pending","duplex-lifetime","duplex-hup"};
    for(size_t mode=0;mode<sizeof(modes)/sizeof(*modes);mode++){
        int input[2],output[2];protocol_pipes(input,output);int residency=lease(residency_path);CHECK(residency>=0);
        char *cmd=command(self,modes[mode],path);EngineProcess p;
        if(!mode){
            CHECK(!strcmp(engine_process_start_bound_duplex(self,cmd,30,65536,residency,model,model,output[1],&p),"invalid_engine_configuration"));
            CHECK(p.guardian<0&&p.response<0);
            CHECK(!strcmp(engine_process_start_bound_duplex(self,cmd,30,65536,residency,model,input[1],output[1],&p),"invalid_engine_configuration"));
            CHECK(!strcmp(engine_process_start_bound_duplex(self,cmd,30,65536,residency,model,input[0],output[0],&p),"invalid_engine_configuration"));
            CHECK(!strcmp(engine_process_start_bound_duplex(self,cmd,30,65536,-1,model,input[0],output[1],&p),"invalid_engine_configuration"));
        }
        CHECK(!engine_process_start_bound_duplex(self,cmd,mode==7?1:30,mode==4?1024:65536,residency,model,input[0],output[1],&p));
        CHECK(!close(input[0])&&!close(residency));if(mode!=3)CHECK(!close(output[1]));
        exact(output[0],"abc",3);if(mode!=4&&mode!=7){observe(path,0);observe(descendant,0);}
        if(mode==3){backpressure(output[1]);CHECK(!close(output[1]));}
        if(mode!=4&&mode!=7)CHECK(!released(residency_path));
        if(!mode){
            const unsigned char frame[]={0,0,0,3,'a','b','c'};
            for(unsigned i=0;i<3;i++){CHECK(write(input[1],frame,sizeof(frame))==(ssize_t)sizeof(frame));exact(output[0],frame,sizeof(frame));}
            CHECK(!engine_process_stop(&p));
        }else if(mode==1){CHECK(!close(input[1]));input[1]=-1;terminal(&p);CHECK(!engine_process_stop(&p));CHECK(!strcmp(p.reason,"completed"));}
        else if(mode==2||mode==8){CHECK(write(input[1],"x",1)==1);terminal(&p);expect_stop(&p,"engine_failed");}
        else if(mode==3)CHECK(!engine_process_stop(&p));
        else if(mode==4){terminal(&p);expect_stop(&p,"log_limit");}
        else if(mode==5){
            struct pollfd watch={output[0],POLLIN|POLLHUP,0};CHECK(poll(&watch,1,3000)>0);char byte;CHECK(read(output[0],&byte,1)==0);
            CHECK(!engine_process_stop(&p));
        }else if(mode==6){
            CHECK(!kill(p.guardian,SIGSTOP));int stopped;CHECK(waitpid(p.guardian,&stopped,WUNTRACED)==p.guardian&&WIFSTOPPED(stopped));
            expect_stop(&p,"engine_shutdown_pending");CHECK(p.guardian>0&&p.response>=0);
            CHECK(!released(residency_path)&&!released(path)&&!released(descendant));
            CHECK(!kill(p.guardian,SIGCONT));CHECK(!engine_process_stop(&p));
        }else {terminal(&p);expect_stop(&p,"lifetime_exceeded");}
        observe(path,1);observe(descendant,1);observe(residency_path,1);CHECK(!released(foreign));
        if(input[1]>=0)CHECK(!close(input[1]));
        CHECK(!close(output[0]));free(cmd);CHECK(!unlink(path)&&!unlink(descendant)&&!unlink(residency_path));
    }
    /* An orphaned stopped group gets SIGHUP/SIGCONT from the OS. The guardian
     * must survive HUP and reap even with a worker blocked in stdout. */
    fd=open(model_path,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);CHECK(fd>=0);CHECK(write(fd,"abc",3)==3&&!close(fd));
    pid_t parent;char *args[]={(char *)self,"--duplex-parent",path,ready,residency_path,model_path,NULL};
    CHECK(!posix_spawn(&parent,self,NULL,NULL,args,environ));
    int seen=0;for(int i=0;i<300;i++){if(!access(ready,F_OK)){seen=1;break;}pause_ms();}CHECK(seen);
    FILE *file=fopen(ready,"r");CHECK(file);long guardian;CHECK(fscanf(file,"%ld",&guardian)==1&&guardian>1&&!fclose(file));
    /* The direct owner confirmed SIGSTOP before publishing this ready record. */
    observe(path,0);observe(descendant,0);observe(residency_path,0);
    CHECK(!kill(parent,SIGKILL));int status;CHECK(waitpid(parent,&status,0)==parent);
    observe(path,1);observe(descendant,1);observe(residency_path,1);CHECK(!released(foreign));
    CHECK(!kill(external,SIGKILL));CHECK(waitpid(external,&status,0)==external);observe(foreign,1);
    CHECK(!close(model));CHECK(!unlink(path)&&!unlink(descendant)&&!unlink(residency_path)&&!unlink(ready)&&!unlink(model_path)&&!unlink(foreign));
}
int main(int argc,char **argv){
    int guardian=engine_guardian_dispatch(argc,argv);if(guardian>=0)return guardian;
    if(argc==4&&!strcmp(argv[1],"--fixture"))return fixture(argv[2],argv[3]);
    if(argc==6&&!strcmp(argv[1],"--vision-engine-bounded-bridge")){
        int input=open(argv[4],O_RDONLY);if(input<0||dup2(input,0)<0)return 4;close(input);
        return fixture(argv[2],argv[3]);
    }
    if(argc==6&&!strcmp(argv[1],"--duplex-parent")){
        int residency=lease(argv[4]),model=open(argv[5],O_RDONLY|O_CLOEXEC),input[2],output[2];
        if(residency<0||model<0)return 3;
        protocol_pipes(input,output);char *cmd=command(argv[0],"duplex-flood",argv[2]);EngineProcess p;
        if(engine_process_start_bound_duplex(argv[0],cmd,30,65536,residency,model,input[0],output[1],&p))return 3;
        free(cmd);close(input[0]);close(model);close(residency);
        exact(output[0],"abc",3);
        backpressure(output[1]);close(output[1]);
        if(kill(p.guardian,SIGSTOP))return 3;
        int stopped;if(waitpid(p.guardian,&stopped,WUNTRACED)!=p.guardian||!WIFSTOPPED(stopped))return 3;
        FILE *ready=fopen(argv[3],"w");if(!ready)return 3;
        fprintf(ready,"%ld\n",(long)p.guardian);if(fclose(ready))return 3;
        /* Leave stdout unread: blocked engine output cannot mask parent death. */
        for(;;)pause();
    }
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
    /* Lifetime includes process/ASan startup on the hosted runner. Give the
     * fixture time to acquire its lease before asserting automatic expiry;
     * terminal() still bounds the entire observation to five seconds. */
    cmd=command(argv[0],"sleep",path);CHECK(!engine_process_start(argv[0],cmd,1,65536,&p));
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
    for(int flood=0;flood<6;flood++){
        char workspace[512],input_path[600],result_path[600];
        snprintf(workspace,sizeof(workspace),"%s/protocol",root);CHECK(!mkdir(workspace,0700));
        snprintf(input_path,sizeof(input_path),"%s/request.frame",workspace);
        snprintf(result_path,sizeof(result_path),"%s/result.frame",workspace);
        int in=open(input_path,O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(in>=0);
        const char frame[]={0,0,0,3,'a','b','c'};CHECK(write(in,frame,sizeof(frame))==(ssize_t)sizeof(frame));CHECK(!close(in));
        in=open(input_path,O_RDONLY);int result=open(result_path,O_RDWR|O_CREAT|O_EXCL,0600);
        int directory=open(workspace,O_RDONLY|O_DIRECTORY);int model=open(input_path,O_RDONLY);
        residency=lease(residency_path);CHECK(in>=0&&result>=0&&directory>=0&&model>=0&&residency>=0);
        cmd=command(argv[0],flood>=2?"protocol-large":flood?"protocol-flood":"protocol",path);
        if(!flood){
            /* Cache-compatible execution requires our capture bridge; arbitrary
             * host commands cannot accidentally bypass the legacy file cap. */
            const char *refused=engine_process_start_bound_workspace_captured(argv[0],cmd,30,65536,residency,model,workspace,directory,result,&p);
            CHECK(refused&&!strcmp(refused,"engine_exec_failed")&&p.guardian<0);
            struct stat untouched;CHECK(!fstat(result,&untouched)&&!untouched.st_size);
        }
        if(flood>=4){free(cmd);cmd=captured_command(argv[0],flood==4?"protocol-large":"protocol",path,input_path,result_path);
            CHECK(!engine_process_start_bound_workspace_captured(argv[0],cmd,30,65536,residency,model,workspace,directory,result,&p));}
        else if(flood<2)CHECK(!engine_process_start_bound_protocol(argv[0],cmd,30,65536,residency,model,workspace,directory,in,result,&p));
        else CHECK(!engine_process_start_bound_protocol_sized(argv[0],cmd,30,65536,residency,model,workspace,directory,in,result,flood==2?200000:65536,&p));
        terminal(&p);
        if(flood==1||flood==3||flood==4){expect_stop(&p,"protocol_limit");if(flood==3||flood==4){struct stat size;CHECK(!fstat(result,&size));CHECK(size.st_size<=(flood==4?ENGINE_VISUAL_RESULT_MAX:65536));}}
        else if(flood==2){CHECK(!engine_process_stop(&p));struct stat size;CHECK(!fstat(result,&size));CHECK(size.st_size==131072);}
        else{CHECK(!engine_process_stop(&p));CHECK(!strcmp(p.reason,"completed"));char actual[128];CHECK(pread(result,actual,sizeof(actual),0)==(ssize_t)sizeof(frame));CHECK(!memcmp(actual,frame,sizeof(frame)));}
        CHECK(access(workspace,F_OK)<0&&errno==ENOENT);
        free(cmd);close(in);close(result);close(directory);close(model);close(residency);CHECK(!unlink(residency_path));
    }
    /* A requester can die after the native worker starts but before receiving
     * the guardian's startup acknowledgment. A broken response reader must
     * still reap the worker and remove its owned protocol workspace. */
    char ack_workspace[512],ack_input[600],ack_result[600];
    snprintf(ack_workspace,sizeof(ack_workspace),"%s/ack-workspace",root);
    CHECK(!mkdir(ack_workspace,0700));
    snprintf(ack_input,sizeof(ack_input),"%s/request.frame",ack_workspace);
    snprintf(ack_result,sizeof(ack_result),"%s/result.frame",ack_workspace);
    int ack_write=open(ack_input,O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(ack_write>=0);CHECK(write(ack_write,"x",1)==1);CHECK(!close(ack_write));
    int ack_in=open(ack_input,O_RDONLY),ack_out=open(ack_result,O_RDWR|O_CREAT|O_EXCL,0600);
    int ack_dir=open(ack_workspace,O_RDONLY|O_DIRECTORY),ack_lease=lease(residency_path);
    CHECK(ack_in>=0&&ack_out>=0&&ack_dir>=0&&ack_lease>=0);
    int ack_control[2],ack_response[2];CHECK(!pipe(ack_control));CHECK(!pipe(ack_response));
    CHECK(!close(ack_response[0])); /* Deliberately no reader for "started". */
    const int sources[]={ack_control[0],ack_lease,ack_in,ack_dir,ack_in,ack_out,ack_response[1]};
    const int targets[]={3,4,5,6,7,8,STDOUT_FILENO};int copies[7];
    posix_spawn_file_actions_t actions;CHECK(!posix_spawn_file_actions_init(&actions));
    for(size_t i=0;i<7;i++){copies[i]=fcntl(sources[i],F_DUPFD_CLOEXEC,20);CHECK(copies[i]>=0);CHECK(!posix_spawn_file_actions_adddup2(&actions,copies[i],targets[i]));CHECK(!posix_spawn_file_actions_addclose(&actions,copies[i]));}
    CHECK(!posix_spawn_file_actions_addopen(&actions,STDERR_FILENO,"/dev/null",O_WRONLY,0));
    char *ack_args[]={argv[0],"--inference-owned-engine-protocol-sized","30","65536",ack_workspace,"8196",argv[0],"--fixture","sleep",path,NULL};
    pid_t ack_guardian;CHECK(!posix_spawn(&ack_guardian,argv[0],&actions,NULL,ack_args,environ));
    CHECK(!posix_spawn_file_actions_destroy(&actions));
    for(size_t i=0;i<7;i++)CHECK(!close(copies[i]));
    CHECK(!close(ack_control[0]));CHECK(!close(ack_control[1]));CHECK(!close(ack_response[1]));
    CHECK(waitpid(ack_guardian,&status,0)==ack_guardian);CHECK(WIFEXITED(status)&&WEXITSTATUS(status)==2);
    CHECK(access(ack_workspace,F_OK)<0&&errno==ENOENT);
    close(ack_in);close(ack_out);close(ack_dir);close(ack_lease);
    CHECK(!unlink(residency_path));if(!access(path,F_OK))CHECK(!unlink(path));
    duplex_tests(argv[0],root);leased_command_tests(argv[0],root);CHECK(!rmdir(root));

    printf("PASS engine process ownership: %u checks; bounded logs/lifetime, owned stop/group/parent-death, foreign survivor\n",checks);return 0;
}
