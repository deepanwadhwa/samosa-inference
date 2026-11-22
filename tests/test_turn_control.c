#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/turn_control.h>
#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static atomic_uint checks;
#define CHECK(x) do { atomic_fetch_add(&checks, 1); if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); abort(); } } while (0)
typedef struct {
    SamosaTurnGate gate, *shared;
    SamosaMmSupervisor supervisor;
    const char *engine,*model;
    int vision,molmo,running,stop,starts,closes,primary_stops,primary_starts,probes,sleeps,ready_after;
    int start_failure,primary_failure,inspect_failures,inspects,sides[4],sample_count,wait_count,detail,real,framed;
    int stop_after_sleep;
    long long total,now;
    double required;
    SamosaVisionResourceBudget budget,waited;
    char events[256],first_owned[PATH_MAX];
    atomic_int *text,*visual;
} Fixture;
static SamosaTurnGate *gate(Fixture *f) { return f->shared?f->shared:&f->gate; }
static void event(Fixture *f,char e) { size_t n=strlen(f->events);CHECK(n+1<sizeof(f->events));f->events[n]=e;f->events[n+1]=0; }
static int available(void *c,int p) { Fixture *f=c;return p==2?f->molmo:f->vision; }
static double required(void *c) {return ((Fixture *)c)->required;}
static long long total(void *c) {return ((Fixture *)c)->total;}
static int running(void *c) {return ((Fixture *)c)->running;}
static void primary_stop(void *c) {Fixture *f=c;CHECK(!samosa_turn_primary_begin(gate(f)));f->primary_stops++;f->running=0;event(f,'p');}
static int primary_start(void *c) {Fixture *f=c;CHECK(!samosa_turn_primary_begin(gate(f)));f->primary_starts++;event(f,'r');return !f->primary_failure;}
static int probe(void *c) {Fixture *f=c;CHECK(!samosa_turn_primary_begin(gate(f)));f->probes++;return f->ready_after>=0&&f->probes>f->ready_after;}
static int stopping(void *c) {return ((Fixture *)c)->stop;}
static long long now(void *c) {return ((Fixture *)c)->now;}
static void sleep_ms(void *c,long ms) {Fixture *f=c;CHECK(ms==100);f->now+=ms;f->sleeps++;if(f->stop_after_sleep&&f->sleeps>=f->stop_after_sleep)f->stop=1;}
static SamosaVisionResourceBudget budget(void *c,int detail) {Fixture *f=c;f->sample_count++;f->detail=detail;event(f,'b');return f->budget;}
static SamosaVisionResourceBudget wait_memory(void *c,int detail,double gb,SamosaVisionResourceBudget initial) {
    Fixture *f=c;CHECK(detail==f->detail&&gb==f->required&&initial.max_side_len==f->budget.max_side_len);f->wait_count++;event(f,'w');return f->waited;
}
static int session_start(void *c,int provider,SamosaMmSession *s) {
    Fixture *f=c;CHECK(!samosa_turn_primary_begin(gate(f)));f->starts++;event(f,'s');
    if(f->start_failure)return 0;
    if(f->text) {CHECK(!atomic_load(f->text));CHECK(atomic_fetch_add(f->visual,1)==0);}
    if(f->real)return provider==2?samosa_mm_molmo2_start(&f->supervisor,s,NULL,f->engine,f->model,1):samosa_mm_visionpsy_start(&f->supervisor,s,NULL,f->engine,f->model,1);
    s->active=1;return 1;
}
static void session_close(void *c,SamosaMmSession *s) {
    Fixture *f=c;f->closes++;event(f,'c');
    if(s->active&&f->text)CHECK(atomic_fetch_sub(f->visual,1)==1);
    if(f->real)samosa_mm_visual_close(s,NULL);else s->active=0;
}
static int inspect(void *c,SamosaMmSession *s,const char *image,const char *prompt,int side,int tokens,char *out,size_t capacity,int *pt,int *gt,char *error,size_t ec) {
    Fixture *f=c;CHECK(tokens==192&&!strcmp(prompt,"read page")&&f->inspects<4);f->sides[f->inspects++]=side;
    if(f->real){CHECK(access(image,F_OK)==0);if(!f->first_owned[0])snprintf(f->first_owned,sizeof(f->first_owned),"%s",image);else CHECK(strcmp(image,f->first_owned)&&access(f->first_owned,F_OK)<0);}
    if(f->inspect_failures){f->inspect_failures--;snprintf(error,ec,"fixture_pressure");return 0;}
    if(f->real)return samosa_mm_visionpsy_inspect(s,NULL,image,prompt,side,tokens,out,capacity,pt,gt,error,ec);
    snprintf(out,capacity,"page observed");*pt=5;*gt=2;return 1;
}
static int analyze(void *c,SamosaMmSession *s,const char *image,const char *prompt,int tokens,char *out,size_t capacity,int *pt,int *gt,char *error,size_t ec) {
    Fixture *f=c;f->inspects++;if(f->real)return samosa_mm_molmo2_analyze_request(s,NULL,image,NULL,0,"image",prompt,tokens,0,0,1,out,capacity,pt,gt,NULL,NULL,NULL,error,ec);
    return 0;
}
static SamosaVisualTurnControl control(Fixture *f) {
    return (SamosaVisualTurnControl){gate(f),available,required,total,running,primary_stop,primary_start,probe,stopping,now,sleep_ms,budget,wait_memory,session_start,session_close,inspect,analyze,f};
}
static void init(Fixture *f) {
    memset(f,0,sizeof(*f));CHECK(samosa_turn_gate_init(&f->gate));f->vision=f->molmo=1;f->ready_after=2;f->now=1000;f->required=6.75;f->total=32LL*1024*1024*1024;
    f->budget=(SamosaVisionResourceBudget){.admitted=1,.max_side_len=2048,.available_gb=9};f->waited=f->budget;
}
static void released(Fixture *f,SamosaVisualTurn *t) {
    SamosaTurnStatus status=samosa_turn_status(gate(f));CHECK(!status.generating&&!status.specialist_active&&!status.specialist_provider&&!t->generation_gate_held&&!t->session.active);
    CHECK(samosa_turn_primary_begin(gate(f)));samosa_turn_primary_end(gate(f));
}
static void policy_cases(void) {
    Fixture f;init(&f);SamosaVisualTurnControl c=control(&f);SamosaVisualTurn t={.budget=f.budget};char e[96];
    CHECK(samosa_turn_primary_begin(&f.gate)&&samosa_turn_primary_begin(&f.gate));CHECK(samosa_turn_generating(&f.gate)==2);
    CHECK(!samosa_visual_turn_start(&c,&t,e,sizeof(e))&&!strcmp(e,"vision_specialist_busy"));samosa_turn_primary_end(&f.gate);samosa_turn_primary_end(&f.gate);
    CHECK(samosa_visual_turn_start(&c,&t,e,sizeof(e))&&t.provider==2);CHECK(!samosa_turn_primary_begin(&f.gate));
    SamosaVisualTurn peer={.budget=f.budget};CHECK(!samosa_visual_turn_start(&c,&peer,e,sizeof(e))&&!strcmp(e,"vision_specialist_busy"));
    CHECK(samosa_visual_turn_start(&c,&t,e,sizeof(e))&&f.starts==1);SamosaTurnStatus status=samosa_turn_status(&f.gate);CHECK(status.specialist_provider==2&&status.specialist_active&&!status.generating);
    samosa_turn_direct_generating(&f.gate,1);samosa_turn_direct_generating(&f.gate,1);
    CHECK(samosa_turn_generating(&f.gate)==1);samosa_visual_turn_release(&c,&t);
    /* Direct visual close releases exclusion before withdrawing its health
       flag. A primary request can be admitted in this exact original window. */
    CHECK(samosa_turn_primary_begin(&f.gate));
    samosa_turn_direct_generating(&f.gate,0);
    samosa_turn_direct_generating(&f.gate,0);
    CHECK(samosa_turn_generating(&f.gate)==1);
    samosa_turn_primary_end(&f.gate);released(&f,&t);
    f.molmo=0;t=(SamosaVisualTurn){.budget=f.budget};CHECK(samosa_visual_turn_start(&c,&t,e,sizeof(e))&&t.provider==1);samosa_visual_turn_release(&c,&t);released(&f,&t);
    t=(SamosaVisualTurn){.budget=f.budget,.extended_visual=1};CHECK(!samosa_visual_turn_start(&c,&t,e,sizeof(e))&&!strcmp(e,"molmo2_model_required"));
    t=(SamosaVisualTurn){.budget=f.budget,.provider=2};CHECK(!samosa_visual_turn_start(&c,&t,e,sizeof(e))&&!strcmp(e,"molmo2_model_required"));
    f.vision=0;t=(SamosaVisualTurn){.budget=f.budget};CHECK(!samosa_visual_turn_start(&c,&t,e,sizeof(e))&&!strcmp(e,"vision_model_required"));
    f.vision=f.molmo=1;f.start_failure=1;t=(SamosaVisualTurn){.budget=f.budget};CHECK(!samosa_visual_turn_start(&c,&t,e,sizeof(e))&&!strcmp(e,"vision_session_failed"));released(&f,&t);
    f.start_failure=0;t=(SamosaVisualTurn){.budget=f.budget};t.budget.admitted=0;CHECK(!samosa_visual_turn_start(&c,&t,e,sizeof(e))&&!strcmp(e,"molmo2_resource_pressure"));released(&f,&t);
    t=(SamosaVisualTurn){.budget=f.budget,.provider=1};t.budget.admitted=0;CHECK(!samosa_visual_turn_start(&c,&t,e,sizeof(e))&&!strcmp(e,"vision_resource_pressure"));released(&f,&t);samosa_turn_gate_destroy(&f.gate);
}
static void reclaim_cases(void) {
    const long long totals[]={0,18LL*1024*1024*1024,18LL*1024*1024*1024+1,16LL*1024*1024*1024};
    for(size_t i=0;i<4;i++)for(int running_value=0;running_value<2;running_value++)for(int failure=0;failure<5;failure++){
        Fixture f;init(&f);f.total=totals[i];f.running=running_value;f.start_failure=failure==1;f.primary_failure=failure==2;f.ready_after=failure==3?-1:2;f.stop_after_sleep=failure==4?1:0;
        if(failure==4)f.waited.available_gb=1;
        SamosaVisualTurnControl c=control(&f);SamosaVisualTurn t={.budget=f.budget,.detail_needed=1};char error[96];int small=f.total>0&&f.total<=18LL*1024*1024*1024;
        int success=samosa_visual_turn_start(&c,&t,error,sizeof(error));CHECK(success==(!f.start_failure&&!(failure==4&&small)));
        if(success)samosa_visual_turn_release(&c,&t);
        CHECK(f.sample_count==small&&f.wait_count==small&&f.primary_stops==small*running_value);
        CHECK(f.primary_starts==small*running_value);if(small&&running_value){CHECK(strchr(f.events,'p')<strchr(f.events,'b'));CHECK(strchr(f.events,'c')<strchr(f.events,'r'));}
        if(small&&running_value&&!f.primary_failure){CHECK(f.sleeps==(failure==3?600:failure==4?1:2));}
        released(&f,&t);samosa_turn_gate_destroy(&f.gate);
    }
    Fixture f;init(&f);f.total=16LL*1024*1024*1024;f.running=1;SamosaVisualTurnControl c=control(&f);SamosaVisualTurn t={.budget=f.budget};char e[96];CHECK(samosa_visual_turn_start(&c,&t,e,sizeof(e)));f.stop=1;samosa_visual_turn_release(&c,&t);CHECK(!f.primary_starts&&t.backend_paused);released(&f,&t);samosa_turn_gate_destroy(&f.gate);
}
static void retry_cases(void) {
    int sides[]={2048,1536,1024,512};int lower[]={1536,1024,512,0};
    for(int i=0;i<4;i++)for(int failures=1;failures<3;failures++){
        Fixture f;init(&f);f.molmo=0;f.inspect_failures=failures;SamosaVisualTurnControl c=control(&f);SamosaVisualTurn t={.budget=f.budget};t.budget.max_side_len=sides[i];
        char error[96],result[128];int pt=0,gt=0;int ok=samosa_visual_turn_inspect(&c,&t,"durable-image",0,"read page",192,result,sizeof(result),&pt,&gt,error,sizeof(error));CHECK(ok==(lower[i]&&failures==1));CHECK(f.inspects==(lower[i]?2:1)&&f.sides[0]==sides[i]);if(lower[i])CHECK(f.sides[1]==lower[i]&&f.starts==2);samosa_visual_turn_release(&c,&t);released(&f,&t);samosa_turn_gate_destroy(&f.gate);
    }
}
typedef struct {SamosaTurnGate *gate;atomic_int *text,*visual;int specialist;} Thread;
static void *mixed(void *arg) {
    Thread *thread=arg;Fixture f;init(&f);f.shared=thread->gate;f.text=thread->text;f.visual=thread->visual;SamosaVisualTurnControl c=control(&f);
    for(int n=0;n<300;n++){
        if(thread->specialist){SamosaVisualTurn t={.budget=f.budget};char error[96];if(samosa_visual_turn_start(&c,&t,error,sizeof(error))){CHECK(!atomic_load(thread->text));samosa_visual_turn_release(&c,&t);}else CHECK(!strcmp(error,"vision_specialist_busy"));}
        else if(samosa_turn_primary_begin(thread->gate)){atomic_fetch_add(thread->text,1);CHECK(!atomic_load(thread->visual));atomic_fetch_sub(thread->text,1);samosa_turn_primary_end(thread->gate);}
        f.events[0]=0;
    }samosa_turn_gate_destroy(&f.gate);return NULL;
}
static void concurrency(void) {
    SamosaTurnGate gate={0};CHECK(samosa_turn_gate_init(&gate));atomic_int text=0,visual=0;pthread_t threads[8];Thread args[8];
    for(int i=0;i<8;i++){args[i]=(Thread){&gate,&text,&visual,i%2};CHECK(!pthread_create(&threads[i],NULL,mixed,&args[i]));}
    for(int i=0;i<8;i++)CHECK(!pthread_join(threads[i],NULL));SamosaTurnStatus s=samosa_turn_status(&gate);CHECK(!s.generating&&!s.specialist_active&&!text&&!visual);samosa_turn_gate_destroy(&gate);
}
static void native_page(const char *engine,int molmo) {
    char directory[]="/tmp/samosa-turn-control-XXXXXX",source[PATH_MAX];CHECK(mkdtemp(directory));CHECK(snprintf(source,sizeof(source),"%s/page.ppm",directory)<(int)sizeof(source));
    int file=open(source,O_CREAT|O_EXCL|O_WRONLY,0644);CHECK(file>=0&&write(file,"prepared page\n",14)==14&&!close(file));
    Fixture f;init(&f);f.real=1;f.engine=engine;f.model=directory;f.molmo=molmo;f.inspect_failures=molmo?0:1;CHECK(samosa_mm_supervisor_init(&f.supervisor));
    if(molmo)CHECK(!setenv("SAMOSA_FAKE_MM_FRAMED","1",1));else CHECK(!unsetenv("SAMOSA_FAKE_MM_FRAMED"));
    SamosaVisualTurnControl c=control(&f);SamosaVisualTurn turn={.budget=f.budget};char result[256],error[96];int pt=0,gt=0;
    CHECK(samosa_visual_turn_inspect(&c,&turn,source,1,"read page",192,result,sizeof(result),&pt,&gt,error,sizeof(error)));CHECK(access(source,F_OK)<0&&errno==ENOENT);CHECK(strstr(result,"fixture"));
    samosa_visual_turn_release(&c,&turn);released(&f,&turn);CHECK(!samosa_mm_supervisor_is_active(&f.supervisor,NULL,0));CHECK(!rmdir(directory));samosa_mm_supervisor_destroy(&f.supervisor);samosa_turn_gate_destroy(&f.gate);
}
int main(int argc,char **argv) {
    int dispatch=samosa_mm_process_dispatch(argc,argv);if(dispatch>=0)return dispatch;
    CHECK(argc==2&&argv[1][0]=='/');alarm(120);policy_cases();reclaim_cases();retry_cases();concurrency();native_page(argv[1],0);native_page(argv[1],1);
    printf("PASS extracted turn coordination: %u checks; shared exclusion/counts, original availability/reclaim/resume deadlines, retry/give-back and native guardian pages; no new scheduler\n",atomic_load(&checks));return 0;
}
