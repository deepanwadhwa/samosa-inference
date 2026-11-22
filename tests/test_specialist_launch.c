#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/specialist_protocol.h>
#include <assert.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct { size_t count; char names[16][64],fields[16][2048]; } Trace;
static void event(void *context,const char *name,const char *fields) {
    Trace *t=context;assert(t->count<16);size_t i=t->count++;
    assert(strlen(name)<sizeof(t->names[i]));strcpy(t->names[i],name);
    assert(!fields||strlen(fields)<sizeof(t->fields[i]));strcpy(t->fields[i],fields?fields:"");
}
static int start(int framed,SamosaMmSupervisor *supervisor,SamosaMmSession *session,
                 const SamosaMmTrace *trace,const char *engine,const char *model,int available) {
    return framed?samosa_mm_molmo2_start(supervisor,session,trace,engine,model,available)
                 :samosa_mm_visionpsy_start(supervisor,session,trace,engine,model,available);
}
static void reaped(pid_t pid) {
    errno=0;assert(waitpid(pid,NULL,WNOHANG)==-1&&errno==ECHILD);
    errno=0;assert(kill(pid,0)<0&&errno==ESRCH);
}
int main(int argc,char **argv) {
    int dispatch=samosa_mm_process_dispatch(argc,argv);if(dispatch>=0)return dispatch;
    assert(argc==2&&argv[1][0]=='/');alarm(60);
    char model[]="/tmp/samosa-launch-quote\"-XXXXXX";assert(mkdtemp(model));
    SamosaMmSupervisor supervisor;assert(samosa_mm_supervisor_init(&supervisor));
    for(int framed=0;framed<2;framed++) {
        Trace captured={0};SamosaMmTrace trace={.event=event,.context=&captured};
        if(framed)assert(!setenv("SAMOSA_FAKE_MM_FRAMED","1",1));else assert(!unsetenv("SAMOSA_FAKE_MM_FRAMED"));
        SamosaMmSession session={0};
        assert(!start(framed,&supervisor,NULL,&trace,NULL,NULL,0)&&!captured.count);
        assert(!start(framed,&supervisor,&session,&trace,NULL,NULL,0));
        assert(captured.count==(framed?0u:1u));
        if(!framed)assert(!strcmp(captured.names[0],"visionpsy_start_failed")&&!strcmp(captured.fields[0],"\"code\":\"model_unavailable\""));
        assert(!samosa_mm_supervisor_is_active(&supervisor,NULL,0));
        captured=(Trace){0};
        assert(!start(framed,&supervisor,&session,&trace,"/nonexistent/samosa-visual-engine",model,1));
        assert(captured.count==2&&!strcmp(captured.names[1],framed?"molmo2_start_failed":"visionpsy_start_failed"));
        assert(!strcmp(captured.fields[1],"\"code\":\"multimodal_engine_unavailable\""));
        assert(!samosa_mm_supervisor_is_active(&supervisor,NULL,0));
        captured=(Trace){0};assert(start(framed,&supervisor,&session,&trace,argv[1],model,1));
        pid_t pid=session.pid;assert(pid>1&&session.active&&session.supervisor==&supervisor);
        assert(session.protocol==(framed?SAMOSA_MM_PROTOCOL_JSON_FRAME_V1:SAMOSA_MM_PROTOCOL_JSON_LINE));
        assert(!strcmp(session.provider,framed?"molmo2_4b":"visionpsy_nano_460m"));
        assert(session.request_timeout_ms==(framed?600000:120000));
        assert(session.shutdown_grace_ms==(framed?3000:2000)&&session.max_frame_bytes==(framed?1048576u:65536u));
        assert(captured.count==(framed?2u:3u));
        assert(!strcmp(captured.names[0],framed?"molmo2_starting":"visionpsy_starting"));
        if(framed)assert(!strcmp(captured.fields[0],"\"provider\":\"molmo2_4b\",\"protocol\":\"framed_v1\""));
        else {assert(strstr(captured.fields[0],argv[1])&&strstr(captured.fields[0],"samosa-launch-quote\\\"-"));
            assert(!strcmp(captured.names[1],"visionpsy_process_started")&&!captured.fields[2][0]);}
        assert(!strcmp(captured.names[captured.count-1],framed?"molmo2_ready":"visionpsy_ready"));
        Trace busy={0};SamosaMmTrace refused_trace={.event=event,.context=&busy};SamosaMmSession refused={0};
        assert(!start(framed,&supervisor,&refused,&refused_trace,argv[1],model,1));
        assert(busy.count==2&&!strcmp(busy.fields[1],"\"code\":\"multimodal_specialist_busy\""));
        assert(session.pid==pid&&samosa_mm_supervisor_active_pgid(&supervisor)==session.pgid);
        char *reply=NULL,error[96];size_t bytes=0;const char *inspect="{\"command\":\"inspect\"}";
        assert(samosa_mm_session_request(&session,inspect,strlen(inspect),&reply,&bytes,error,sizeof(error)));
        assert(bytes==strlen(reply)&&strstr(reply,"fixture"));free(reply);
        samosa_mm_visual_close(&session,&trace);
        assert(!session.active&&!session.supervisor&&!session.pid&&!samosa_mm_supervisor_is_active(&supervisor,NULL,0));
        assert(!strcmp(captured.names[captured.count-1],framed?"molmo2_process_stopped":"visionpsy_process_stopped"));
        reaped(pid);size_t events=captured.count;samosa_mm_visual_close(&session,&trace);assert(captured.count==events);

        /* A failed exchange invalidates framing, but retains cleanup ownership. */
        captured=(Trace){0};assert(start(framed,&supervisor,&session,&trace,argv[1],model,1));pid=session.pid;
        const char *crash="{\"command\":\"crash\"}";
        assert(!samosa_mm_session_request(&session,crash,strlen(crash),&reply,&bytes,error,sizeof(error)));
        assert(!session.active&&session.supervisor&&samosa_mm_supervisor_is_active(&supervisor,NULL,0));
        samosa_mm_visual_close(&session,&trace);
        int released=!session.supervisor&&!session.pid&&!samosa_mm_supervisor_is_active(&supervisor,NULL,0);
        if(!released) {
            fputs("FAIL: original inactive-session close guard retained worker/lease after failed exchange\n",stderr);
            samosa_mm_session_close(&session);samosa_mm_supervisor_destroy(&supervisor);assert(!rmdir(model));return 1;
        }
        reaped(pid);assert(!strcmp(captured.names[captured.count-1],framed?"molmo2_process_stopped":"visionpsy_process_stopped"));
        captured=(Trace){0};assert(start(framed,&supervisor,&session,&trace,argv[1],model,1));
        samosa_mm_visual_close(&session,&trace);assert(!samosa_mm_supervisor_is_active(&supervisor,NULL,0));
        printf("PASS original %s launch profile: availability/failure, traces, configured limits, busy peer, reuse, close/reap and failed-exchange restart\n",framed?"Molmo2":"VisionPsy");
    }
    samosa_mm_visual_close(NULL,NULL);samosa_mm_supervisor_destroy(&supervisor);assert(!rmdir(model));return 0;
}
