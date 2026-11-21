#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "summary_owned_fixture.h"
#include <signal.h>
#include <spawn.h>
#include <sys/wait.h>
#include <time.h>
static atomic_uint checks;
#define CHECK(x) do{atomic_fetch_add(&checks,1);if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static long long now_ms(void){struct timespec t;CHECK(!clock_gettime(CLOCK_MONOTONIC,&t));return (long long)t.tv_sec*1000+t.tv_nsec/1000000;}
static void tick(void){struct timespec t={0,1000000};nanosleep(&t,NULL);}
typedef struct {SummaryOwnedFixture *host;atomic_int cancel,marked,full;int on_marker,on_full,observe_marker,observe_full;} Probe;
static const char *check(void *context){
    Probe *p=context;if(atomic_load(&p->cancel))return "cancelled";
    if(p->observe_marker&&access(p->host->marker,F_OK)==0){atomic_store(&p->marked,1);if(p->on_marker)return "cancelled";}
    if(p->observe_full&&p->host->write_fd>=0){struct pollfd w={p->host->write_fd,POLLOUT,0};
        if(poll(&w,1,0)==0){atomic_store(&p->full,1);if(p->on_full&&atomic_load(&p->marked))return "cancelled";}}
    return NULL;
}
static void probe_init(Probe *p,SummaryOwnedFixture *host){
    memset(p,0,sizeof(*p));p->host=host;atomic_init(&p->cancel,0);atomic_init(&p->marked,0);atomic_init(&p->full,0);
}
typedef struct {SamosaSummaryProcess *process;SamosaSummaryProcessConfig config;const SamosaSummaryControl *control;
    const char *text,*error;char *reply;atomic_int queued,running;int coalesce;} Job;
static void activity(void *context,const char *event){Job *j=context;
    if(!strcmp(event,"summary_queued"))atomic_store(&j->queued,1);else if(!strcmp(event,"summarizing"))atomic_store(&j->running,1);}
static void job_init(Job *j,SamosaSummaryProcess *process,const SamosaSummaryProcessConfig *config,const SamosaSummaryControl *control,int coalesce){
    memset(j,0,sizeof(*j));j->process=process;j->config=*config;j->control=control;j->coalesce=coalesce;
    j->text="A resident summary keeps the correct source subject.";atomic_init(&j->queued,0);atomic_init(&j->running,0);
}
static void *run(void *context){Job *j=context;
    if(j->coalesce){char *parts[]={(char *)j->text},*replies[]={NULL};
        (void)samosa_summary_process_coalesce_controlled(j->process,&j->config,parts,1,replies,0,activity,j,j->control,&j->error);j->reply=replies[0];}
    else j->reply=samosa_summary_process_once_controlled(j->process,&j->config,j->text,strlen(j->text),j->control,&j->error);
    return NULL;
}
static void wait_marker(const char *path){long long deadline=now_ms()+3000;while(access(path,F_OK)&&now_ms()<deadline)tick();CHECK(!access(path,F_OK));}
static void healthy(SamosaSummaryProcess *process,const SamosaSummaryProcessConfig *config){
    const char *error=NULL,*source="A healthy resident summary preserves its source subject.";
    SamosaSummaryControl c={.deadline_ms=now_ms()+5000};
    char *reply=samosa_summary_process_once_controlled(process,config,source,strlen(source),&c,&error);
    if(error)fprintf(stderr,"healthy: %s\n",error);CHECK(reply&&!error&&!strcmp(reply,source));free(reply);
}
int main(int argc,char **argv){
    int dispatch=engine_guardian_dispatch(argc,argv);if(dispatch>=0)return dispatch;
    if(argc==4&&!strcmp(argv[1],"--parent-owner")){
        char model[1024],ready[1024];snprintf(model,sizeof(model),"%s/model",argv[3]);snprintf(ready,sizeof(ready),"%s/owner-ready",argv[3]);
        SummaryOwnedFixture owner;owned_init(&owner,argv[0],argv[3]);owner.mode="echo";
        SamosaSummaryProcess transport;CHECK(samosa_summary_process_init(&transport));
        SamosaSummaryLifecycle hooks={owned_open,owned_close,&owner};CHECK(samosa_summary_process_bind(&transport,&hooks));
        SamosaSummaryProcessConfig config={argv[2],model,"unused","0"};healthy(&transport,&config);
        FILE *file=fopen(ready,"w");CHECK(file);CHECK(fputs("ready",file)>=0&&!fclose(file));for(;;)pause();
    }
    CHECK(argc==2&&argv[0][0]=='/'&&argv[1][0]=='/');
    char root[]="/tmp/samosa-summary-owned-XXXXXX";CHECK(mkdtemp(root));char model[512];snprintf(model,sizeof(model),"%s/model",root);
    int fd=open(model,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);CHECK(fd>=0&&write(fd,"abc",3)==3&&!close(fd));
    SummaryOwnedFixture host;owned_init(&host,argv[0],root);host.mode="echo";
    SamosaSummaryProcess process;CHECK(samosa_summary_process_init(&process));
    SamosaSummaryLifecycle lifecycle={owned_open,owned_close,&host};CHECK(samosa_summary_process_bind(&process,&lifecycle));
    CHECK(!samosa_summary_process_bind(&process,&lifecycle));
    SamosaSummaryProcessConfig config={argv[1],model,"unused-guardian-log","0"};
    healthy(&process,&config);int guardian=atomic_load(&host.guardian);CHECK(guardian>0&&process.warmed);
    char *parts[]={"The first distinct independently framed summary.","The second distinct independently framed summary."},*replies[2]={NULL,NULL};
    const char *error=NULL;CHECK(samosa_summary_process_batch_controlled(&process,&config,parts,2,replies,1,NULL,&error)&&!error);
    for(int i=0;i<2;i++){CHECK(!strcmp(replies[i],parts[i]));free(replies[i]);replies[i]=NULL;}
    Job jobs[8];pthread_t threads[8];
    for(int i=0;i<8;i++){job_init(&jobs[i],&process,&config,NULL,1);CHECK(!pthread_create(&threads[i],NULL,run,&jobs[i]));}
    for(int i=0;i<8;i++){CHECK(!pthread_join(threads[i],NULL));CHECK(jobs[i].reply&&!jobs[i].error&&!strcmp(jobs[i].reply,jobs[i].text));free(jobs[i].reply);}
    CHECK(atomic_load(&host.opens)==1&&atomic_load(&host.guardian)==guardian);
    CHECK(!samosa_summary_process_close(&process)&&owned_released(&host));
    host.mode="invalid-utf8";
    char *reply=samosa_summary_process_once_controlled(&process,&config,parts[0],strlen(parts[0]),NULL,&error);
    CHECK(!reply&&error&&!strcmp(error,"invalid_summary_reply")&&owned_released(&host));host.mode="echo";healthy(&process,&config);
    CHECK(!samosa_summary_process_close(&process));
    /* Both read and full-pipe write interruption must invalidate the frame. */
    for(int writing=0;writing<2;writing++)for(int cancel=0;cancel<2;cancel++){
        host.mode=writing?"stall-input":"stall-output";if(!access(host.marker,F_OK))CHECK(!unlink(host.marker));
        Probe probe;probe_init(&probe,&host);probe.observe_marker=1;probe.observe_full=writing;probe.on_marker=cancel&&!writing;probe.on_full=cancel&&writing;
        SamosaSummaryControl control={.deadline_ms=now_ms()+750,.check=check,.context=&probe};
        long long start=now_ms();
        if(writing){
            char *large=malloc(60001);CHECK(large);memset(large,'a',60000);large[60000]=0;
            char *inputs[64],*outputs[64]={0};for(int i=0;i<64;i++)inputs[i]=large;
            CHECK(!samosa_summary_process_batch_controlled(&process,&config,inputs,64,outputs,0,&control,&error));
            for(int i=0;i<64;i++)CHECK(!outputs[i]);free(large);CHECK(atomic_load(&probe.full));
        }else{reply=samosa_summary_process_once_controlled(&process,&config,parts[0],strlen(parts[0]),&control,&error);CHECK(!reply);}
        CHECK(error&&!strcmp(error,cancel?"cancelled":"deadline_exceeded"));CHECK(atomic_load(&probe.marked));
        CHECK(now_ms()-start<3000&&owned_released(&host));host.mode="echo";healthy(&process,&config);CHECK(!samosa_summary_process_close(&process));
    }
    /* A queued caller can cancel while another control scope owns the frame. */
    host.mode="stall-output";CHECK(!unlink(host.marker));Probe active,queued;probe_init(&active,&host);probe_init(&queued,&host);
    SamosaSummaryControl active_control={.deadline_ms=now_ms()+5000,.check=check,.context=&active};
    SamosaSummaryControl queued_control={.deadline_ms=now_ms()+5000,.check=check,.context=&queued};
    Job first,second;job_init(&first,&process,&config,&active_control,1);job_init(&second,&process,&config,&queued_control,1);
    CHECK(!pthread_create(&threads[0],NULL,run,&first));wait_marker(host.marker);guardian=atomic_load(&host.guardian);
    CHECK(!pthread_create(&threads[1],NULL,run,&second));long long wait=now_ms()+1000;
    while(!atomic_load(&second.queued)&&now_ms()<wait)tick();CHECK(atomic_load(&second.queued));atomic_store(&queued.cancel,1);
    CHECK(!pthread_join(threads[1],NULL));CHECK(!second.reply&&second.error&&!strcmp(second.error,"cancelled")&&!atomic_load(&second.running));
    CHECK(atomic_load(&host.guardian)==guardian&&!kill((pid_t)guardian,0));
    SamosaSummaryControl queued_deadline={.deadline_ms=now_ms()+100};Job expired;
    job_init(&expired,&process,&config,&queued_deadline,1);(void)run(&expired);
    CHECK(!expired.reply&&expired.error&&!strcmp(expired.error,"deadline_exceeded")&&!atomic_load(&expired.running));
    CHECK(atomic_load(&host.guardian)==guardian&&!kill((pid_t)guardian,0));
    SamosaSummaryControl mutex_wait={.deadline_ms=now_ms()+100};
    reply=samosa_summary_process_once_controlled(&process,&config,parts[0],strlen(parts[0]),&mutex_wait,&error);
    CHECK(!reply&&error&&!strcmp(error,"deadline_exceeded")&&atomic_load(&host.guardian)==guardian);
    atomic_store(&active.cancel,1);CHECK(!pthread_join(threads[0],NULL));CHECK(!first.reply&&first.error&&!strcmp(first.error,"cancelled")&&owned_released(&host));
    host.mode="echo";healthy(&process,&config);guardian=atomic_load(&host.guardian);
    CHECK(!kill((pid_t)guardian,SIGSTOP));int stopped;CHECK(waitpid((pid_t)guardian,&stopped,WUNTRACED)==guardian&&WIFSTOPPED(stopped));
    error=samosa_summary_process_close(&process);CHECK(error&&!strcmp(error,"engine_shutdown_pending")&&!owned_released(&host));
    unsigned opens=atomic_load(&host.opens),closes=atomic_load(&host.closes);
    reply=samosa_summary_process_once_controlled(&process,&config,parts[0],strlen(parts[0]),NULL,&error);
    CHECK(!reply&&error&&!strcmp(error,"engine_shutdown_pending")&&atomic_load(&host.opens)==opens&&atomic_load(&host.closes)==closes);
    CHECK(!kill((pid_t)guardian,SIGCONT));CHECK(!samosa_summary_process_close(&process)&&owned_released(&host));
    host.mode="closed-input";reply=samosa_summary_process_once_controlled(&process,&config,parts[0],strlen(parts[0]),NULL,&error);
    CHECK(!reply&&error&&owned_released(&host));host.mode="echo";healthy(&process,&config);
    CHECK(!samosa_summary_process_close(&process));samosa_summary_process_destroy(&process);
    pid_t peer=fork();CHECK(peer>=0);if(!peer){for(;;)pause();}
    pid_t parent;char *arguments[]={argv[0],"--parent-owner",argv[1],root,NULL};extern char **environ;
    CHECK(!posix_spawn(&parent,argv[0],NULL,NULL,arguments,environ));
    char ready[1024];snprintf(ready,sizeof(ready),"%s/owner-ready",root);wait_marker(ready);CHECK(!owned_released(&host));
    CHECK(!kill(parent,SIGKILL));int result;CHECK(waitpid(parent,&result,0)==parent);
    long long until=now_ms()+3000;while(!owned_released(&host)&&now_ms()<until)tick();CHECK(owned_released(&host));
    CHECK(!kill(peer,0)&&!kill(peer,SIGKILL)&&waitpid(peer,&result,0)==peer);
    char command[1200];snprintf(command,sizeof(command),"rm -rf '%s'",root);CHECK(!system(command));
    printf("PASS owned summary transport: %u checks; guardian residency, scalar/batch/coalescing, bounded I/O, scoped cancellation and pending cleanup\n",atomic_load(&checks));return 0;
}
