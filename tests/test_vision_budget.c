#define _POSIX_C_SOURCE 200809L
#include <samosa_inference/vision_budget.h>
#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    SamosaVisionResourceSample values[32];
    int count,samples,sleeps,stop,stop_after,budget_events,wait_events;
    long long now;
    char fields[320],wait[256];
} Fixture;
static Fixture fixture(void) {Fixture f={0};f.count=1;f.now=1000;f.stop_after=-1;return f;}
static SamosaVisionResourceSample sample(void *context) {
    Fixture *f=context;int index=f->samples++;if(index>=f->count)index=f->count-1;return f->values[index];
}
static long long now(void *context) {return ((Fixture *)context)->now;}
static void sleep_ms(void *context,long ms) {Fixture *f=context;assert(ms==100);f->now+=ms;f->sleeps++;}
static int stopping(void *context) {Fixture *f=context;return f->stop||(f->stop_after>=0&&f->samples>=f->stop_after);}
static void event(void *context,const char *name,const char *fields) {
    Fixture *f=context;
    if(!strcmp(name,"vision_resource_budget")){f->budget_events++;assert(strlen(fields)<sizeof(f->fields));strcpy(f->fields,fields);}
    else {assert(!strcmp(name,"molmo2_memory_reclaim_wait"));f->wait_events++;assert(strlen(fields)<sizeof(f->wait));strcpy(f->wait,fields);}
}
static SamosaVisionBudgetControl control(Fixture *f) {return (SamosaVisionBudgetControl){sample,now,sleep_ms,stopping,event,f};}
static SamosaVisionResourceSample reading(double available,int pressure,int thermal) {
    return (SamosaVisionResourceSample){available,pressure,thermal,32000000000LL};
}
static int same(SamosaVisionResourceBudget a,SamosaVisionResourceBudget b) {
    return a.max_side_len==b.max_side_len&&a.admitted==b.admitted&&a.memory_pressure==b.memory_pressure&&
        a.thermal_pressure==b.thermal_pressure&&a.available_gb==b.available_gb&&!strcmp(a.reason,b.reason);
}
int main(void) {
    struct {double available;long long total;int pressure,thermal,detail,side,admitted;} cases[]={
        {9,32000000000LL,0,0,0,2048,1},{8,32000000000LL,0,0,0,2048,1},
        {7.999,32000000000LL,0,0,0,1536,1},{5.5,32000000000LL,0,0,0,1536,1},
        {5.499,32000000000LL,0,0,0,1024,1},{3.5,32000000000LL,0,0,0,1024,1},
        {3.499,32000000000LL,0,0,0,512,1},{2.5,32000000000LL,0,0,0,512,1},
        {2.499,32000000000LL,0,0,0,512,0},{0,0,0,0,0,2048,1},
        {-1,0,0,0,0,2048,1},{9,18000000000LL,0,0,0,1536,1},
        {9,18000000001LL,0,0,0,2048,1},{9,17179869184LL,0,0,0,1536,1},
        {9,32000000000LL,1,0,0,2048,1},{9,32000000000LL,2,0,0,1024,1},
        {9,32000000000LL,3,0,0,1024,1},{9,32000000000LL,4,0,1,512,0},
        {9,32000000000LL,0,1,0,2048,1},{9,32000000000LL,0,2,0,512,1},
        {9,32000000000LL,0,2,1,1024,1},{9,32000000000LL,0,3,0,512,0},
        {9,32000000000LL,0,3,1,1024,0},{3.999,32000000000LL,0,2,1,512,1},
        {4,32000000000LL,0,2,1,1024,1}
    };
    for(size_t i=0;i<sizeof(cases)/sizeof(*cases);i++) {
        Fixture f=fixture();f.values[0]=(SamosaVisionResourceSample){cases[i].available,cases[i].pressure,cases[i].thermal,cases[i].total};
        SamosaVisionBudgetControl c=control(&f);SamosaVisionResourceBudget b=samosa_vision_resource_budget(&c,cases[i].detail);
        if(b.max_side_len!=cases[i].side||b.admitted!=cases[i].admitted)fprintf(stderr,"budget case %zu: side=%d admitted=%d\n",i,b.max_side_len,b.admitted);
        assert(b.max_side_len==cases[i].side&&b.admitted==cases[i].admitted&&b.available_gb==cases[i].available);
        assert(b.memory_pressure==cases[i].pressure&&b.thermal_pressure==cases[i].thermal);
        char reason[96];snprintf(reason,sizeof(reason),"available=%.2fGB memory_pressure=%d thermal=%d",cases[i].available,cases[i].pressure,cases[i].thermal);
        assert(!strcmp(b.reason,reason)&&f.samples==1&&f.budget_events==1&&!f.wait_events);
        assert(strstr(f.fields,cases[i].detail?"\"detail_needed\":true":"\"detail_needed\":false"));
    }
    Fixture exact=fixture();exact.values[0]=reading(9,0,0);SamosaVisionBudgetControl ec=control(&exact);
    (void)samosa_vision_resource_budget(&ec,0);
    assert(!strcmp(exact.fields,"\"admitted\":true,\"max_side_len\":2048,\"detail_needed\":false,\"available_gb\":9.000,\"total_gb\":32.000,\"memory_pressure\":0,\"thermal_pressure\":0,\"reason\":\"available=9.00GB memory_pressure=0 thermal=0\""));
    assert(samosa_molmo2_required_available_gb(3750000000.0,17179869184LL)==6.75);
    assert(samosa_molmo2_required_available_gb(3750000000.0,18000000000LL)==6.75);
    assert(samosa_molmo2_required_available_gb(3750000000.0,18000000001LL)==7.75);
    assert(samosa_molmo2_required_available_gb(3750000000.0,0)==7.75);
    assert(samosa_molmo2_required_available_gb(0,17179869184LL)==6.75&&samosa_molmo2_required_available_gb(-1,32000000000LL)==6.75);
    assert(SAMOSA_MOLMO2_MANIFEST_UNAVAILABLE_GB==7.75);

    SamosaVisionResourceBudget initial={.admitted=1,.available_gb=4,.max_side_len=1024};
    for(int i=0;i<5;i++) {
        Fixture f=fixture();SamosaVisionBudgetControl c=control(&f);SamosaVisionResourceBudget in=initial;
        double required=6.75;if(i==0)required=0;if(i==1)in.available_gb=0;if(i==2)in.available_gb=6.75;if(i==3)in.memory_pressure=4;if(i==4)in.thermal_pressure=3;
        SamosaVisionResourceBudget b=samosa_vision_wait_for_molmo_memory(&c,0,required,in);
        assert(same(b,in)&&!f.samples&&!f.sleeps&&!f.wait_events);
    }
    Fixture f=fixture();f.count=2;f.values[0]=reading(5,0,0);f.values[1]=reading(6.75,0,0);SamosaVisionBudgetControl c=control(&f);
    SamosaVisionResourceBudget b=samosa_vision_wait_for_molmo_memory(&c,0,6.75,initial);
    assert(b.available_gb==6.75&&b.admitted&&f.samples==2&&f.sleeps==2&&f.budget_events==2&&f.wait_events==1);
    assert(!strcmp(f.wait,"\"required_gb\":6.750,\"available_gb\":6.750,\"peak_gb\":6.750,\"waited_ms\":200,\"samples\":2,\"admitted\":true"));
    f=fixture();f.count=3;f.values[0]=reading(6,0,0);f.values[1]=reading(4.5,0,0);f.values[2]=reading(6.8,0,0);c=control(&f);
    b=samosa_vision_wait_for_molmo_memory(&c,0,6.75,initial);
    assert(b.available_gb==6.8&&f.samples==3&&strstr(f.wait,"\"peak_gb\":6.800")&&strstr(f.wait,"\"waited_ms\":300"));
    for(int i=0;i<3;i++) {
        f=fixture();f.values[0]=reading(i==0?0:5,i==1?4:0,i==2?3:0);c=control(&f);
        b=samosa_vision_wait_for_molmo_memory(&c,1,6.75,initial);
        assert(f.samples==1&&f.sleeps==1&&f.wait_events==1);
        assert(i==0?b.available_gb==0&&b.admitted:!b.admitted);
        assert(strstr(f.wait,i==0?"\"admitted\":true":"\"admitted\":false"));
    }
    f=fixture();f.values[0]=reading(4,0,0);c=control(&f);
    b=samosa_vision_wait_for_molmo_memory(&c,0,6.75,initial);
    assert(b.admitted&&b.available_gb==4&&f.samples==30&&f.sleeps==30&&f.now==4000);
    assert(strstr(f.wait,"\"waited_ms\":3000,\"samples\":30,\"admitted\":false"));
    f=fixture();f.stop=1;c=control(&f);b=samosa_vision_wait_for_molmo_memory(&c,0,6.75,initial);
    assert(same(b,initial)&&!f.samples&&!f.sleeps&&f.wait_events==1&&strstr(f.wait,"\"waited_ms\":0"));
    f=fixture();f.stop_after=2;f.values[0]=reading(5,0,0);c=control(&f);
    b=samosa_vision_wait_for_molmo_memory(&c,0,6.75,initial);
    assert(b.available_gb==5&&f.samples==2&&f.now==1200&&strstr(f.wait,"\"admitted\":false"));
    SamosaVisionResourceSample native=samosa_vision_host_sample();assert(native.total_bytes>=0&&native.available_gb>=0);
    b=samosa_vision_resource_budget(NULL,0);assert(b.max_side_len==512||b.max_side_len==1024||b.max_side_len==1536||b.max_side_len==2048);
    assert(b.admitted==0||b.admitted==1);printf("Actual host sample: total_bytes=%lld available_gb=%.3f memory_pressure=%d thermal=%d\n",native.total_bytes,native.available_gb,native.memory_pressure,native.thermal_pressure);
    printf("PASS extracted visual policy: %zu threshold cases, original traces/reserve, recovery/unknown/pressure/stop and bounded30-poll reclaim\n",sizeof(cases)/sizeof(*cases));return 0;
}
