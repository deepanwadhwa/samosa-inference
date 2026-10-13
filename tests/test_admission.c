#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "admission.h"
#include "sessions.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <spawn.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
extern char **environ;
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void ready(const char *path){int found=0;for(int i=0;i<500;i++){if(!access(path,F_OK)){found=1;break;}struct timespec delay={0,10000000};nanosleep(&delay,NULL);}CHECK(found);}
static void wait_success(pid_t pid){int status;CHECK(waitpid(pid,&status,0)==pid);CHECK(WIFEXITED(status)&&!WEXITSTATUS(status));}
static pid_t worker(const char *exe,const char *state,const char *marker,const char *log,const char *tag,const char *hold){
    char *args[]={(char*)exe,"--worker",(char*)state,(char*)marker,(char*)log,(char*)tag,(char*)hold,NULL};pid_t pid;CHECK(!posix_spawnp(&pid,exe,NULL,NULL,args,environ));return pid;
}
static void order(const char *path,const char *expected){FILE *in=fopen(path,"r");CHECK(in);char bytes[64]={0};size_t n=fread(bytes,1,sizeof(bytes)-1,in);CHECK(!ferror(in));CHECK(!fclose(in));CHECK(n==strlen(expected)&&!strcmp(bytes,expected));}
int main(int argc,char **argv){
    if(argc==7&&!strcmp(argv[1],"--worker")){
        CHECK(!setenv("SAMOSA_INFERENCE_QUEUE_READY",argv[3],1));
        InferenceConfig cfg={.admission_state=argv[2],.queue_capacity=3,.admission_wait_ms=10000};int lock=-1;
        const char *error=inference_schedule(&cfg,&lock);if(error){fprintf(stderr,"worker %s: %s\n",argv[5],error);return 1;}
        int fd=open(argv[4],O_CREAT|O_WRONLY|O_APPEND|O_CLOEXEC,0600);CHECK(fd>=0);CHECK(write(fd,argv[5],strlen(argv[5]))==(ssize_t)strlen(argv[5]));CHECK(!close(fd));
        if(!strcmp(argv[6],"hold")){char acquired[2048];snprintf(acquired,sizeof(acquired),"%s-acquired",argv[3]);fd=open(acquired,O_CREAT|O_WRONLY|O_CLOEXEC,0600);CHECK(fd>=0);CHECK(!close(fd));for(;;)pause();}
        struct timespec delay={0,80000000};nanosleep(&delay,NULL);CHECK(!close(lock));return 0;
    }
    char fixture[]="/tmp/samosa-native-admission-XXXXXX";CHECK(mkdtemp(fixture));
    char state[1024],log[1024],marker[5][1024];snprintf(state,sizeof(state),"%s/state",fixture);snprintf(log,sizeof(log),"%s/order",fixture);
    for(int i=0;i<5;i++)snprintf(marker[i],sizeof(marker[i]),"%s/ready-%d",fixture,i);
    int active=-1;CHECK(!inference_admit(state,&active));
    pid_t a=worker(argv[0],state,marker[0],log,"A","brief");ready(marker[0]);
    pid_t b=worker(argv[0],state,marker[1],log,"B","brief");ready(marker[1]);
    pid_t c=worker(argv[0],state,marker[2],log,"C","brief");ready(marker[2]);
    InferenceConfig cfg={.admission_state=state,.queue_capacity=3,.admission_wait_ms=1000};int lock=-1;
    const char *error=inference_schedule(&cfg,&lock);CHECK(error&&!strcmp(error,"queue_full"));CHECK(lock<0);
    cfg.queue_capacity=0;error=inference_schedule(&cfg,&lock);CHECK(error&&!strcmp(error,"busy"));CHECK(lock<0);
    CHECK(!close(active));wait_success(a);wait_success(b);wait_success(c);order(log,"ABC");
    /* Dead head is reaped through its released slot lease; later tickets keep order. */
    CHECK(!inference_admit(state,&active));CHECK(!unlink(marker[0]));CHECK(!unlink(marker[1]));CHECK(!unlink(marker[2]));
    a=worker(argv[0],state,marker[0],log,"X","brief");ready(marker[0]);
    b=worker(argv[0],state,marker[1],log,"D","brief");ready(marker[1]);
    CHECK(!kill(a,SIGKILL));int status;CHECK(waitpid(a,&status,0)==a);CHECK(WIFSIGNALED(status));
    c=worker(argv[0],state,marker[2],log,"E","brief");ready(marker[2]);
    CHECK(!close(active));wait_success(b);wait_success(c);order(log,"ABCDE");
    /* Own deadline removes its ticket, then live work can enter the same slot. */
    CHECK(!inference_admit(state,&active));cfg.queue_capacity=1;cfg.admission_wait_ms=60;
    struct timespec before,after;CHECK(!clock_gettime(CLOCK_MONOTONIC,&before));error=inference_schedule(&cfg,&lock);CHECK(!clock_gettime(CLOCK_MONOTONIC,&after));
    CHECK(error&&!strcmp(error,"admission_deadline_exceeded"));CHECK(lock<0);
    double elapsed=(double)(after.tv_sec-before.tv_sec)+(double)(after.tv_nsec-before.tv_nsec)/1e9;CHECK(elapsed>=0.05&&elapsed<2);
    a=worker(argv[0],state,marker[3],log,"F","brief");ready(marker[3]);CHECK(!close(active));wait_success(a);order(log,"ABCDEF");
    /* Active-worker death releases only its provider lease and admits the head. */
    a=worker(argv[0],state,marker[4],log,"G","hold");char acquired[2048];snprintf(acquired,sizeof(acquired),"%s-acquired",marker[4]);ready(acquired);
    CHECK(!unlink(marker[3]));b=worker(argv[0],state,marker[3],log,"H","brief");ready(marker[3]);
    CHECK(!kill(a,SIGKILL));CHECK(waitpid(a,&status,0)==a);CHECK(WIFSIGNALED(status));wait_success(b);order(log,"ABCDEFGH");
    cfg.queue_capacity=9;CHECK(!strcmp(inference_schedule(&cfg,&lock),"invalid_configuration"));cfg.queue_capacity=1;cfg.admission_wait_ms=60001;CHECK(!strcmp(inference_schedule(&cfg,&lock),"invalid_configuration"));
    char *cleanup[]={"find",fixture,"-depth","-delete",NULL};pid_t child;CHECK(!posix_spawnp(&child,"find",NULL,NULL,cleanup,environ));wait_success(child);
    printf("PASS FIFO admission: %u checks; actual accepted process order, capacity, dead head/active, deadline and slot reuse\n",checks);return 0;
}
