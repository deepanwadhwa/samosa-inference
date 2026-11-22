#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/specialist_protocol.h>
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
static SamosaMmSession session;
static void failed(const char *expression,int line){fprintf(stderr,"FAIL actual %d: %s\n",line,expression);samosa_mm_session_close(&session);exit(1);}
#define CHECK(x) do{if(!(x))failed(#x,__LINE__);}while(0)
static int gone(pid_t pid){for(int i=0;i<300;i++){if(kill(pid,0)<0&&errno==ESRCH)return 1;struct timespec t={0,10000000};nanosleep(&t,NULL);}return 0;}
static void closed(SamosaMmSupervisor *supervisor){pid_t pid=session.pid;samosa_mm_session_close(&session);CHECK(gone(pid)&&!samosa_mm_supervisor_is_active(supervisor,NULL,0));}
static void copy_image(const char *source,const char *destination){int in=open(source,O_RDONLY|O_NOFOLLOW),out=open(destination,O_CREAT|O_EXCL|O_WRONLY,0600);CHECK(in>=0&&out>=0);char bytes[4096];for(;;){ssize_t n=read(in,bytes,sizeof(bytes));CHECK(n>=0);if(!n)break;size_t done=0;while(done<(size_t)n){ssize_t wrote=write(out,bytes+done,(size_t)n-done);CHECK(wrote>0);done+=(size_t)wrote;}}CHECK(!close(in)&&!close(out));}
int main(int argc,char **argv){
#ifndef SAMOSA_SPECIALIST_LEGACY
    int dispatch=samosa_mm_process_dispatch(argc,argv);if(dispatch>=0)return dispatch;
#endif
    CHECK(argc==7);alarm(240);
    char heading[2048],red[2048],blue[2048];CHECK(snprintf(heading,sizeof(heading),"%s/hello.ppm",argv[5])<(int)sizeof(heading)&&snprintf(red,sizeof(red),"%s/red.ppm",argv[5])<(int)sizeof(red)&&snprintf(blue,sizeof(blue),"%s/blue.ppm",argv[5])<(int)sizeof(blue));
    SamosaMmSupervisor supervisor;CHECK(samosa_mm_supervisor_init(&supervisor));CHECK(samosa_mm_visionpsy_start(&supervisor,&session,NULL,argv[1],argv[2],1));
    const char *input=heading;
#ifndef SAMOSA_SPECIALIST_LEGACY
    char temporary[]="/tmp/samosa-real-visual-XXXXXX",prepared[2048],owned[2048];CHECK(mkdtemp(temporary));CHECK(snprintf(prepared,sizeof(prepared),"%s/page.ppm",temporary)<(int)sizeof(prepared));copy_image(heading,prepared);CHECK(samosa_mm_session_take_temporary_image(&session,prepared,owned,sizeof(owned)));input=owned;
#else
    (void)copy_image;
#endif
    char observation[65536],error[96];int prompt_tokens=0,generated_tokens=0;
    const char *receipt=getenv("SAMOSA_VISUAL_ACTUAL_RECEIPT");FILE *record=receipt?fopen(receipt,"w"):NULL;CHECK(record);fputs("{",record);
    for(int tier=0;tier<2;tier++){int side=tier?1536:2048;
    CHECK(samosa_mm_visionpsy_inspect(&session,NULL,input,"Read the large heading on this page. Answer briefly.",side,32,observation,sizeof(observation),&prompt_tokens,&generated_tokens,error,sizeof(error)));
    printf("Actual default VisionPsy side=%d: %s; prompt=%d generated=%d\n",side,observation,prompt_tokens,generated_tokens);
    fprintf(record,"%s\"vision_%d\":",tier?",":"",side);json_quote(record,observation,strlen(observation));fflush(record);
    char lower[65536];snprintf(lower,sizeof(lower),"%s",observation);for(char *p=lower;*p;p++)*p=(char)tolower((unsigned char)*p);if(tier)CHECK(strstr(lower,"hello")&&generated_tokens>0);else CHECK(observation[0]&&generated_tokens>0);
    }
    closed(&supervisor);
#ifndef SAMOSA_SPECIALIST_LEGACY
    CHECK(access(owned,F_OK)<0&&errno==ENOENT);CHECK(access(prepared,F_OK)<0&&errno==ENOENT);CHECK(!rmdir(temporary));
#endif
    CHECK(samosa_mm_molmo2_start(&supervisor,&session,NULL,argv[3],argv[4],1));
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
    closed(&supervisor);samosa_mm_supervisor_destroy(&supervisor);CHECK(fputs("}\n",record)>=0&&!fclose(record));puts("PASS actual default visual profiles: native1536 heading,2048 execution, changed image/joint/native video and actual PID/group close");return 0;
}
