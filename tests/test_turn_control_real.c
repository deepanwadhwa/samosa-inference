#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/turn_control.h>
#include "../src/json.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
static SamosaVisualTurn turn;
#define session turn.session
static SamosaTurnGate gate;
static SamosaVisualTurnControl control;
static const char *vision_engine,*vision_model,*molmo_engine,*molmo_model;
static int selected;
static SamosaMmSupervisor *host_supervisor;
static void failed(const char *expression,int line){fprintf(stderr,"FAIL actual %d: %s\n",line,expression);if(gate.state)samosa_visual_turn_release(&control,&turn);exit(1);}
#define CHECK(x) do{if(!(x))failed(#x,__LINE__);}while(0)
static int available(void *context,int provider){(void)context;return selected==provider;}
static double required(void *context){(void)context;return 6.75;}
static long long total(void *context){(void)context;return 0;} /* Explicit unknown in this compatibility driver; policy has its separate native tests. */
static int primary(void *context){(void)context;return 0;}
static void stop(void *context){(void)context;CHECK(0);}
static int stopping(void *context){(void)context;return 0;}
static long long now(void *context){(void)context;struct timespec t;CHECK(!clock_gettime(CLOCK_MONOTONIC,&t));return (long long)t.tv_sec*1000+t.tv_nsec/1000000;}
static void sleep_ms(void *context,long ms){(void)context;struct timespec t={ms/1000,ms%1000*1000000};nanosleep(&t,NULL);}
static SamosaVisionResourceBudget budget(void *context,int detail){(void)context;(void)detail;return turn.budget;}
static SamosaVisionResourceBudget wait_memory(void *context,int detail,double gb,SamosaVisionResourceBudget initial){(void)context;(void)detail;(void)gb;return initial;}
static int start_session(void *context,int provider,SamosaMmSession *s){(void)context;return provider==2?samosa_mm_molmo2_start(host_supervisor,s,NULL,molmo_engine,molmo_model,1):samosa_mm_visionpsy_start(host_supervisor,s,NULL,vision_engine,vision_model,1);}
static void close_session(void *context,SamosaMmSession *s){(void)context;samosa_mm_visual_close(s,NULL);}
static int inspect(void *context,SamosaMmSession *s,const char *image,const char *prompt,int side,int tokens,char *out,size_t capacity,int *pt,int *gt,char *error,size_t ec){(void)context;return samosa_mm_visionpsy_inspect(s,NULL,image,prompt,side,tokens,out,capacity,pt,gt,error,ec);}
static int analyze(void *context,SamosaMmSession *s,const char *image,const char *prompt,int tokens,char *out,size_t capacity,int *pt,int *gt,char *error,size_t ec){(void)context;return samosa_mm_molmo2_analyze_request(s,NULL,image,NULL,0,"image",prompt,tokens,0,0,1,out,capacity,pt,gt,NULL,NULL,NULL,error,ec);}
static int start_visual(int provider){selected=provider;turn=(SamosaVisualTurn){.provider=provider,.budget={.admitted=1,.max_side_len=2048}};char error[96];return samosa_visual_turn_start(&control,&turn,error,sizeof(error));}
static int gone(pid_t pid){for(int i=0;i<300;i++){if(kill(pid,0)<0&&errno==ESRCH)return 1;struct timespec t={0,10000000};nanosleep(&t,NULL);}return 0;}
static void closed(SamosaMmSupervisor *supervisor){pid_t pid=session.pid;samosa_visual_turn_release(&control,&turn);SamosaTurnStatus status=samosa_turn_status(&gate);CHECK(gone(pid)&&!samosa_mm_supervisor_is_active(supervisor,NULL,0)&&!status.specialist_active&&!status.generating);}
static void copy_image(const char *source,const char *destination){int in=open(source,O_RDONLY|O_NOFOLLOW),out=open(destination,O_CREAT|O_EXCL|O_WRONLY,0600);CHECK(in>=0&&out>=0);char bytes[4096];for(;;){ssize_t n=read(in,bytes,sizeof(bytes));CHECK(n>=0);if(!n)break;size_t done=0;while(done<(size_t)n){ssize_t wrote=write(out,bytes+done,(size_t)n-done);CHECK(wrote>0);done+=(size_t)wrote;}}CHECK(!close(in)&&!close(out));}
int main(int argc,char **argv){
#ifndef SAMOSA_SPECIALIST_LEGACY
    int dispatch=samosa_mm_process_dispatch(argc,argv);if(dispatch>=0)return dispatch;
#endif
    CHECK(argc==7);alarm(240);
    char heading[2048],red[2048],blue[2048];CHECK(snprintf(heading,sizeof(heading),"%s/hello.ppm",argv[5])<(int)sizeof(heading)&&snprintf(red,sizeof(red),"%s/red.ppm",argv[5])<(int)sizeof(red)&&snprintf(blue,sizeof(blue),"%s/blue.ppm",argv[5])<(int)sizeof(blue));
    SamosaMmSupervisor supervisor;CHECK(samosa_mm_supervisor_init(&supervisor));CHECK(samosa_turn_gate_init(&gate));host_supervisor=&supervisor;
    vision_engine=argv[1];vision_model=argv[2];molmo_engine=argv[3];molmo_model=argv[4];
    control=(SamosaVisualTurnControl){&gate,available,required,total,primary,stop,primary,primary,stopping,now,sleep_ms,budget,wait_memory,start_session,close_session,inspect,analyze,NULL};
    CHECK(start_visual(1));
    const char *input=heading;
#ifndef SAMOSA_SPECIALIST_LEGACY
    char temporary[]="/tmp/samosa-real-visual-XXXXXX",prepared[2048],owned[2048];CHECK(mkdtemp(temporary));CHECK(snprintf(prepared,sizeof(prepared),"%s/page.ppm",temporary)<(int)sizeof(prepared));copy_image(heading,prepared);CHECK(samosa_mm_session_take_temporary_image(&session,prepared,owned,sizeof(owned)));input=owned;
#else
    (void)copy_image;
#endif
    char observation[65536],error[96];int prompt_tokens=0,generated_tokens=0;
    const char *receipt=getenv("SAMOSA_VISUAL_ACTUAL_RECEIPT");FILE *record=receipt?fopen(receipt,"w"):NULL;CHECK(record);fputs("{",record);
    for(int tier=0;tier<2;tier++){int side=tier?1536:2048;
    turn.budget.max_side_len=side;CHECK(samosa_visual_turn_inspect(&control,&turn,input,0,"Read the large heading on this page. Answer briefly.",32,observation,sizeof(observation),&prompt_tokens,&generated_tokens,error,sizeof(error)));
    printf("Actual default VisionPsy side=%d: %s; prompt=%d generated=%d\n",side,observation,prompt_tokens,generated_tokens);
    fprintf(record,"%s\"vision_%d\":",tier?",":"",side);json_quote(record,observation,strlen(observation));fflush(record);
    char lower[65536];snprintf(lower,sizeof(lower),"%s",observation);for(char *p=lower;*p;p++)*p=(char)tolower((unsigned char)*p);if(tier)CHECK(strstr(lower,"hello")&&generated_tokens>0);else CHECK(observation[0]&&generated_tokens>0);
    }
    closed(&supervisor);
#ifndef SAMOSA_SPECIALIST_LEGACY
    CHECK(access(owned,F_OK)<0&&errno==ENOENT);CHECK(access(prepared,F_OK)<0&&errno==ENOENT);CHECK(!rmdir(temporary));
#endif
    CHECK(start_visual(2));
    char previous[65536]={0};
    for(int i=0;i<4;i++){
        const char *images[]={red,blue};const char *kind=i==2?"images":i==3?"video":"image";const char *media=i==3?argv[6]:i==1?blue:red;
        int frames=0,count=0;double duration=0;
        CHECK(samosa_mm_molmo2_analyze_request(&session,NULL,media,i==2?images:NULL,i==2?2:0,kind,"Describe the color.",16,0,i==3?1:0,2,observation,sizeof(observation),&prompt_tokens,&generated_tokens,&duration,&frames,&count,error,sizeof(error)));
        printf("Actual default Molmo %s%s: %s; prompt=%d generated=%d images=%d frames=%d duration=%.3f\n",kind,i==1?" blue":"",observation,prompt_tokens,generated_tokens,count,frames,duration);fflush(stdout);
        fprintf(record,",\"molmo_%d\":",i);json_quote(record,observation,strlen(observation));fflush(record);
        CHECK(observation[0]&&generated_tokens>0);
        if(i==0)snprintf(previous,sizeof(previous),"%s",observation);
        if(i==1)CHECK(strcmp(previous,observation));
        if(i==2)CHECK(count==2);
        if(i==3)CHECK(frames>0&&frames<=2&&duration>0);
    }
    closed(&supervisor);samosa_mm_supervisor_destroy(&supervisor);samosa_turn_gate_destroy(&gate);CHECK(fputs("}\n",record)>=0&&!fclose(record));puts("PASS actual extracted turn coordinator: native1536 heading,2048 execution, changed image/joint/native video and actual PID/group close");return 0;
}
