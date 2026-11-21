#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/backend_process.h>
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static unsigned checks;
static pid_t tracked[64];
static char temporary[PATH_MAX];
static void pause_ms(long ms){struct timespec t={ms/1000,(ms%1000)*1000000};while(nanosleep(&t,&t)&&errno==EINTR){}}
static void track(pid_t pid){for(size_t i=0;i<64;i++)if(!tracked[i]){tracked[i]=pid;return;}abort();}
static void forget(pid_t pid){for(size_t i=0;i<64;i++)if(tracked[i]==pid)tracked[i]=0;}
static void remove_tree(const char *path){DIR *d=opendir(path);if(!d){(void)unlink(path);return;}struct dirent *e;while((e=readdir(d))){if(!strcmp(e->d_name,".")||!strcmp(e->d_name,".."))continue;char p[PATH_MAX];if(snprintf(p,sizeof(p),"%s/%s",path,e->d_name)>=(int)sizeof(p))continue;struct stat st;if(!lstat(p,&st)&&S_ISDIR(st.st_mode))remove_tree(p);else(void)unlink(p);}closedir(d);(void)rmdir(path);}
static void cleanup(void){for(size_t i=0;i<64;i++)if(tracked[i]>1){if(getpgid(tracked[i])==tracked[i]&&tracked[i]!=getpgrp())(void)kill(-tracked[i],SIGKILL);(void)kill(tracked[i],SIGKILL);(void)waitpid(tracked[i],NULL,0);}if(temporary[0])remove_tree(temporary);}
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);cleanup();exit(1);}}while(0)
static double now(void){struct timespec t;CHECK(!clock_gettime(CLOCK_MONOTONIC,&t));return (double)t.tv_sec+t.tv_nsec/1e9;}
static void file_text(const char *path,const char *text){FILE *f=fopen(path,"w");CHECK(f&&fputs(text,f)>=0&&!fclose(f));}
static int read_text(const char *path,char *text,size_t size){FILE *f=fopen(path,"r");if(!f)return 0;size_t n=fread(text,1,size-1,f);text[n]=0;fclose(f);return n>0;}
static void path_for(char out[PATH_MAX],const char *name){CHECK(snprintf(out,PATH_MAX,"%s/%s",temporary,name)<PATH_MAX);}
static pid_t read_pid(const char *path){char text[80];for(int i=0;i<200;i++){if(read_text(path,text,sizeof(text))){pid_t p=(pid_t)strtol(text,NULL,10);if(p>1)return p;}pause_ms(10);}return 0;}
static int gone(pid_t pid){for(int i=0;i<200;i++){if(kill(pid,0)<0&&errno==ESRCH)return 1;pause_ms(10);}return 0;}
static int listener(int port,int *chosen){int fd=socket(AF_INET,SOCK_STREAM,0);CHECK(fd>=0);int yes=1;CHECK(!setsockopt(fd,SOL_SOCKET,SO_REUSEADDR,&yes,sizeof(yes)));struct sockaddr_in a={.sin_family=AF_INET,.sin_addr.s_addr=htonl(INADDR_LOOPBACK),.sin_port=htons((uint16_t)port)};CHECK(!bind(fd,(struct sockaddr*)&a,sizeof(a))&&!listen(fd,16));socklen_t n=sizeof(a);CHECK(!getsockname(fd,(struct sockaddr*)&a,&n));*chosen=ntohs(a.sin_port);return fd;}
static int free_port(void){int port;int fd=listener(0,&port);close(fd);return port;}

static int fixture(int argc,char **argv){
    const char *mode=getenv("SAMOSA_PRIMARY_FIXTURE_MODE"),*record=getenv("SAMOSA_PRIMARY_RECORD"),*helper=getenv("SAMOSA_PRIMARY_HELPER");
    if(mode&&!strcmp(mode,"crash"))return 4;
    if(mode&&strstr(mode,"helper")){pid_t p=fork();if(p<0)return 5;if(!p){signal(SIGTERM,SIG_IGN);for(;;)pause();}FILE *f=fopen(helper,"w");if(!f)return 6;fprintf(f,"%ld\n",(long)p);fclose(f);if(!strcmp(mode,"crash-helper"))return 4;}
    if(mode&&!strcmp(mode,"stubborn"))signal(SIGTERM,SIG_IGN);
    int port=0;for(int i=1;i+1<argc;i++)if(!strcmp(argv[i],"--port"))port=atoi(argv[i+1]);
    int fd=listener(port,&port);
    FILE *f=fopen(record,"w");if(!f)return 7;fprintf(f,"PID:%ld\nPGID:%ld\nARGC:%d\n",(long)getpid(),(long)getpgrp(),argc);
    for(int i=1;i<argc;i++)fprintf(f,"ARG:%s\n",argv[i]);
    const char *keys[]={"SAMOSA_BIND","SAMOSA_LAN","SAMOSA_LAN_PASSWORD","SNAP","TOKENIZER","SAMOSA_CHATS_DIR","OMP_NUM_THREADS","SAMOSA_CONTEXT_TOKENS","SAMOSA_AUTO_COMPACT","SAMOSA_COMPACT_THRESHOLD"};
    for(size_t i=0;i<sizeof(keys)/sizeof(keys[0]);i++)fprintf(f,"ENV.%s:%s\n",keys[i],getenv(keys[i])?getenv(keys[i]):"<absent>");fclose(f);
    const char *route=!strcmp(argv[1],"-m")?"GET /health ":"GET /healthz ";
    for(;;){int client=accept(fd,NULL,NULL);if(client<0){if(errno==EINTR)continue;break;}char request[512]={0};ssize_t n=recv(client,request,sizeof(request)-1,0);if(n>0){if(mode&&!strcmp(mode,"stalled-health"))pause_ms(2000);const char *reply=strstr(request,route)?"HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\n{}":"HTTP/1.1 404 Not Found\r\nContent-Length: 0\r\n\r\n";(void)send(client,reply,strlen(reply),0);}close(client);}close(fd);return 0;
}

typedef struct{SamosaBackendProcess state;pthread_mutex_t mutex;SamosaBackendOwner owner;SamosaBackendLaunch launch;char marker[PATH_MAX],log[PATH_MAX],record[PATH_MAX],helper[PATH_MAX],model[PATH_MAX],sentinel[PATH_MAX];} Case;
static void setup(Case *c,const char *self,const char *backend,const char *mode){
    memset(c,0,sizeof(*c));CHECK(!pthread_mutex_init(&c->mutex,NULL));path_for(c->marker,"backend.pid");path_for(c->log,"backend.log");path_for(c->record,"record");path_for(c->helper,"helper");path_for(c->sentinel,"must-not-exist");
    CHECK(snprintf(c->model,sizeof(c->model),"%s/model with spaces; $(touch %s/must-not-exist)",temporary,temporary)<(int)sizeof(c->model));
    (void)unlink(c->record);(void)unlink(c->helper);(void)unlink(c->marker);
    CHECK(!setenv("SAMOSA_PRIMARY_RECORD",c->record,1)&&!setenv("SAMOSA_PRIMARY_HELPER",c->helper,1)&&!setenv("SAMOSA_PRIMARY_FIXTURE_MODE",mode,1));
    c->owner=(SamosaBackendOwner){&c->state,&c->mutex,c->marker};
    c->launch=(SamosaBackendLaunch){.backend=backend,.engine=self,.model=c->model,.tokenizer="/owned/tokenizer with spaces.json",.chats="/owned/chats with spaces",.log=c->log,.port=free_port(),.runtime={.cpu_effective=2,.context_effective=4096,.cpu_source="saved",.context_source="saved",.compact_threshold_percent=75}};
    c->launch.llama=(SamosaLlamaLaunch){.engine=self,.model=c->model,.alias="bonsai-27b-1bit",.projector="/owned/projector with spaces.gguf",.port=c->launch.port,.context_tokens=4096,.threads=2,.context_override="6144",.gpu_override="0",.threads_override="3",.ram_bytes=17179869184ULL,.perf_cores=4};
}
static pid_t start(Case *c){CHECK(samosa_backend_process_start(&c->owner,&c->launch));pid_t p=samosa_backend_process_pid(&c->owner);CHECK(p>1);track(p);pid_t group=getpgid(p);CHECK((group==p&&p!=getpgrp())||(group<0&&errno==ESRCH));return p;}
static void ready(Case *c){int ok=0;for(int i=0;i<200;i++){if(samosa_backend_process_probe(&c->owner,c->launch.backend,c->launch.port)){ok=1;break;}pause_ms(10);}CHECK(ok);pid_t p=samosa_backend_process_pid(&c->owner);CHECK(getpgid(p)==p&&p!=getpgrp());}
static void stop(Case *c,pid_t p){samosa_backend_process_stop(&c->owner,NULL);forget(p);CHECK(!samosa_backend_process_pid(&c->owner)&&!samosa_backend_process_running(&c->owner));CHECK(access(c->marker,F_OK)&&errno==ENOENT);CHECK(waitpid(p,NULL,WNOHANG)<0&&errno==ECHILD);}
static void finish(Case *c){CHECK(!pthread_mutex_destroy(&c->mutex));}
static void child_gone(Case *c,pid_t p){int dead=0;for(int i=0;i<200;i++){if(!samosa_backend_process_running(&c->owner)){dead=1;break;}pause_ms(10);}CHECK(dead);forget(p);CHECK(!samosa_backend_process_pid(&c->owner)&&access(c->marker,F_OK)&&errno==ENOENT);}
static void verify_record(Case *c,const char *text){char record[8192]={0};CHECK(read_text(c->record,record,sizeof(record)));if(!strstr(record,text))fprintf(stderr,"record missing %s\n%s",text,record);CHECK(strstr(record,text));}
static void descendants(const char *self,const char *mode){Case c;setup(&c,self,"qwen",mode);pid_t p=start(&c);pid_t helper=read_pid(c.helper);CHECK(helper>1);track(helper);if(!strcmp(mode,"crash-helper"))child_gone(&c,p);else{ready(&c);stop(&c,p);}int cleaned=gone(helper);if(!cleaned){kill(helper,SIGKILL);(void)gone(helper);}forget(helper);CHECK(cleaned);finish(&c);}

typedef struct{Case *c;int upstream,captured,detach,interrupt;} Hooks;
static void detach(void *context){Hooks *h=context;CHECK(pthread_mutex_trylock(&h->c->mutex)==EBUSY);h->captured=h->upstream;h->upstream=-1;h->detach++;}
static void interrupted(void *context){Hooks *h=context;CHECK(!pthread_mutex_trylock(&h->c->mutex));CHECK(!pthread_mutex_unlock(&h->c->mutex));if(h->captured>=0)CHECK(!shutdown(h->captured,SHUT_RDWR));h->interrupt++;}

static int gateway_repro(const char *self,const char *gateway){
    char home[PATH_MAX],model[PATH_MAX],experts[PATH_MAX],marker[PATH_MAX],helperfile[PATH_MAX],record[PATH_MAX],log[PATH_MAX];
    path_for(home,"home");CHECK(!mkdir(home,0700));char run[PATH_MAX];CHECK(snprintf(run,sizeof(run),"%s/run",home)<(int)sizeof(run));CHECK(!mkdir(run,0700));CHECK(snprintf(model,sizeof(model),"%s/model",home)<(int)sizeof(model));CHECK(!mkdir(model,0700));CHECK(snprintf(experts,sizeof(experts),"%s/experts.bin",model)<(int)sizeof(experts));file_text(experts,"native fixture\n");CHECK(snprintf(marker,sizeof(marker),"%s/run/backend.pid",home)<(int)sizeof(marker));path_for(helperfile,"helper");path_for(record,"record");path_for(log,"gateway.log");
    int port=free_port(),backend_port=free_port();char a[16],b[16];snprintf(a,sizeof(a),"%d",port);snprintf(b,sizeof(b),"%d",backend_port);
    pid_t g=fork();CHECK(g>=0);if(!g){setenv("SAMOSA_BIND","127.0.0.1",1);unsetenv("SAMOSA_LAN");unsetenv("SAMOSA_LAN_PASSWORD");setenv("SAMOSA_HOME",home,1);setenv("SAMOSA_PORT",a,1);setenv("SAMOSA_BACKEND_PORT",b,1);setenv("SAMOSA_QWEN_ENGINE",self,1);setenv("SAMOSA_QWEN_MODEL",model,1);setenv("SAMOSA_TOKENIZER","/owned/tokenizer",1);setenv("SAMOSA_BONSAI_MODEL","/missing-bonsai",1);setenv("SAMOSA_ORNITH_MODEL","/missing-ornith",1);setenv("SAMOSA_PRIMARY_FIXTURE_MODE","helper",1);setenv("SAMOSA_PRIMARY_HELPER",helperfile,1);setenv("SAMOSA_PRIMARY_RECORD",record,1);int out=open(log,O_CREAT|O_WRONLY,0600);if(out>=0){dup2(out,1);dup2(out,2);close(out);}execl(gateway,gateway,(char*)NULL);_exit(127);}track(g);
    pid_t primary=read_pid(marker),helper=read_pid(helperfile);if(primary<=1||helper<=1){char diagnostic[16384]={0};if(read_text(log,diagnostic,sizeof(diagnostic)))fprintf(stderr,"Gateway fixture startup failed: %s\n",diagnostic);}CHECK(primary>1&&helper>1);track(primary);track(helper);
    int serving=0;for(int i=0;i<200;i++){int fd=socket(AF_INET,SOCK_STREAM,0);CHECK(fd>=0);struct sockaddr_in address={.sin_family=AF_INET,.sin_port=htons((uint16_t)port),.sin_addr.s_addr=htonl(INADDR_LOOPBACK)};if(!connect(fd,(struct sockaddr*)&address,sizeof(address))){const char *request="GET /healthz HTTP/1.1\r\nHost: localhost\r\nConnection: close\r\n\r\n";if(send(fd,request,strlen(request),0)>0){char response[128]={0};if(recv(fd,response,sizeof(response)-1,0)>0&&strstr(response," 200 "))serving=1;}}close(fd);if(serving)break;pause_ms(10);}CHECK(serving);
    CHECK(!kill(g,SIGTERM));int status=0,done=0;for(int i=0;i<500;i++){if(waitpid(g,&status,WNOHANG)==g){done=1;break;}pause_ms(10);}CHECK(done&&WIFEXITED(status));forget(g);forget(primary);int cleaned=gone(helper);if(!cleaned){kill(helper,SIGKILL);(void)gone(helper);}forget(helper);CHECK(cleaned);puts("PASS actual Gateway stop reaps primary and removes stubborn helper");return 0;
}

int main(int argc,char **argv){
    signal(SIGPIPE,SIG_IGN);
    if(argc>1&&(!strcmp(argv[1],"--serve")||!strcmp(argv[1],"--model-dir")||!strcmp(argv[1],"-m")))return fixture(argc,argv);
    CHECK(argc==2||argc==4);CHECK(snprintf(temporary,sizeof(temporary),"/tmp/samosa-primary-process-XXXXXX")<(int)sizeof(temporary));CHECK(mkdtemp(temporary));
    if(argc==4){CHECK(!strcmp(argv[2],"--gateway"));int rc=gateway_repro(argv[1],argv[3]);cleanup();return rc;}
    CHECK(!setenv("SAMOSA_BIND","0.0.0.0",1)&&!setenv("SAMOSA_LAN","1",1)&&!setenv("SAMOSA_LAN_PASSWORD","fixture-only",1));CHECK(!setenv("OMP_NUM_THREADS","7",1)&&!setenv("SAMOSA_CONTEXT_TOKENS","2048",1));
    for(int mode=0;mode<3;mode++){Case c;setup(&c,argv[1],"qwen","ready");c.launch.runtime.cpu_auto=mode==1;c.launch.runtime.context_auto=mode==1;c.launch.runtime.cpu_locked=mode==2;c.launch.runtime.context_locked=mode==2;pid_t p=start(&c);ready(&c);verify_record(&c,"ARGC:6\nARG:--serve\nARG:--port\n");verify_record(&c,"ARG:--tokenizer\nARG:/owned/tokenizer with spaces.json\n");verify_record(&c,"ENV.SAMOSA_BIND:127.0.0.1\nENV.SAMOSA_LAN:<absent>\nENV.SAMOSA_LAN_PASSWORD:<absent>\n");verify_record(&c,mode?"ENV.OMP_NUM_THREADS:7\n":"ENV.OMP_NUM_THREADS:2\n");verify_record(&c,mode==1?"ENV.SAMOSA_CONTEXT_TOKENS:auto\n":mode==2?"ENV.SAMOSA_CONTEXT_TOKENS:2048\n":"ENV.SAMOSA_CONTEXT_TOKENS:4096\n");verify_record(&c,"ENV.SAMOSA_AUTO_COMPACT:0\nENV.SAMOSA_COMPACT_THRESHOLD:75\n");verify_record(&c,c.model);CHECK(access(c.sentinel,F_OK)&&errno==ENOENT);CHECK(!strcmp(getenv("OMP_NUM_THREADS"),"7")&&!strcmp(getenv("SAMOSA_BIND"),"0.0.0.0"));struct stat st;CHECK(!stat(c.marker,&st)&&(st.st_mode&0777)==0600);CHECK(!stat(c.log,&st)&&(st.st_mode&0777)==0600);stop(&c,p);finish(&c);}
    const char *backends[]={"maple","bonsai","ornith"};for(size_t i=0;i<3;i++){Case c;setup(&c,argv[1],backends[i],"ready");pid_t p=start(&c);ready(&c);if(!i)verify_record(&c,"ARGC:5\nARG:--model-dir\n");else{verify_record(&c,"ARG:-ngl\nARG:0\nARG:-c\nARG:6144\nARG:-t\nARG:3\nARG:-np\nARG:1\nARG:--cache-ram\nARG:0\nARG:--host\nARG:127.0.0.1\n");verify_record(&c,"ARG:--mmproj\nARG:/owned/projector with spaces.gguf\n");}stop(&c,p);finish(&c);}
    {Case c;setup(&c,argv[1],"qwen","ready");int chosen=0,fd=listener(0,&chosen);c.launch.port=chosen;CHECK(!samosa_backend_process_start(&c.owner,&c.launch)&&!samosa_backend_process_pid(&c.owner));samosa_backend_process_stop(&c.owner,NULL);CHECK(!close(fd));finish(&c);}
    {Case c;setup(&c,argv[1],"qwen","ready");char colliding[PATH_MAX];CHECK(snprintf(colliding,sizeof(colliding),"%s.tmp.%ld",c.marker,(long)getpid())<(int)sizeof(colliding));file_text(colliding,"leave-me\n");pid_t p=start(&c);ready(&c);CHECK(access(c.marker,F_OK)&&errno==ENOENT);stop(&c,p);char text[64];CHECK(read_text(colliding,text,sizeof(text))&&!strcmp(text,"leave-me\n"));CHECK(!unlink(colliding));finish(&c);}
    {Case c;setup(&c,argv[1],"qwen","crash");pid_t p=start(&c);child_gone(&c,p);CHECK(!samosa_backend_process_probe(&c.owner,"qwen",c.launch.port));finish(&c);}
    {Case c;setup(&c,argv[1],"qwen","ready");c.launch.engine="/missing/native-engine";pid_t p=start(&c);child_gone(&c,p);finish(&c);}
    {Case c;setup(&c,argv[1],"qwen","stalled-health");pid_t p=start(&c);char text[8192];int wrote=0;for(int i=0;i<200;i++){if(read_text(c.record,text,sizeof(text))){wrote=1;break;}pause_ms(10);}CHECK(wrote);double t=now();CHECK(!samosa_backend_process_probe(&c.owner,"qwen",c.launch.port));CHECK(now()-t<1.8);stop(&c,p);finish(&c);}
    {Case c;setup(&c,argv[1],"qwen","ready");pid_t p=start(&c);ready(&c);int pair[2];CHECK(!socketpair(AF_UNIX,SOCK_STREAM,0,pair));Hooks h={.c=&c,.upstream=pair[0],.captured=-1};SamosaBackendStopControl control={detach,interrupted,&h};samosa_backend_process_stop(&c.owner,&control);forget(p);CHECK(h.detach==1&&h.interrupt==1&&h.upstream==-1&&!samosa_backend_process_pid(&c.owner));char byte;CHECK(read(pair[1],&byte,1)==0);samosa_backend_process_stop(&c.owner,&control);CHECK(h.detach==2&&h.interrupt==2);close(pair[0]);close(pair[1]);finish(&c);}
    descendants(argv[1],"helper");descendants(argv[1],"crash-helper");
    {Case c;setup(&c,argv[1],"qwen","stubborn");pid_t p=start(&c);ready(&c);double t=now();stop(&c,p);CHECK(now()-t>=7.5&&now()-t<10);finish(&c);}
    printf("PASS primary backend owner: %u checks; original launch/environment/health/marker/stop, literal paths, rollback inputs, descendant cleanup\n",checks);cleanup();return 0;
}
