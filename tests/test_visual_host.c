#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "visual_host.h"
#include "budget.h"
#include "vision_fixture.h"
#include <arpa/inet.h>
#include <poll.h>
#include <sqlite3.h>

static void exact(int fd,void *bytes,size_t size){
    size_t at=0;while(at<size){ssize_t n=read(fd,(char *)bytes+at,size-at);if(n<0&&errno==EINTR)continue;CHECK(n>0);at+=(size_t)n;}
}
static char *message(int framed){
    char *body=NULL;size_t size=0;
    if(framed){uint32_t n;exact(0,&n,4);size=ntohl(n);CHECK(size&&size<32768);body=malloc(size+1);CHECK(body);exact(0,body,size);body[size]=0;}
    else CHECK(getline(&body,&size,stdin)>0);
    return body;
}
static void response(int framed,const char *body){
    if(framed){uint32_t size=htonl((uint32_t)strlen(body));write_all(1,&size,4);}
    write_all(1,body,strlen(body));if(!framed)write_all(1,"\n",1);
}
static int engine_fixture(const char *model){
    struct stat held,named;CHECK(!fstat(5,&held)&&!stat(model,&named)&&held.st_ino==named.st_ino&&held.st_dev==named.st_dev);
    int framed=getenv("SAMOSA_VISUAL_FRAME_PROTOCOL")!=NULL;
    for(;;){char *body=message(framed);Json j;parse(body,&j);
        char *command=json_string(&j,json_get(&j,0,"command"));CHECK(command);
        if(!strcmp(command,"ping")||!strcmp(command,"hello")){response(framed,"{\"status\":\"ok\"}");free(body);free(command);continue;}
        char *prompt=json_string(&j,json_get(&j,0,"prompt")),*id=json_string(&j,json_get(&j,0,"id"));CHECK(prompt&&id);
        int paths=json_get(&j,0,"media_paths"),times=json_get(&j,0,"timestamps_seconds");
        char *image=json_string(&j,framed?paths+1:json_get(&j,0,"image_path"));CHECK(image);
        FILE *ppm=fopen(image,"rb");CHECK(ppm);size_t width,height;CHECK(fscanf(ppm,"P6\n%zu %zu\n255",&width,&height)==2&&fgetc(ppm)=='\n');
        unsigned char rgb[3];CHECK(fread(rgb,1,3,ppm)==3&&!fclose(ppm));
        const char *color=rgb[0]==255&&rgb[1]==0?"red":rgb[2]==255&&rgb[0]==0?"blue":"white";
        const char *log=getenv("SAMOSA_VISUAL_FIXTURE_LOG");FILE *trace=fopen(log,"a");CHECK(trace);
        fprintf(trace,"%ld %s\n",(long)getpid(),image);CHECK(!fclose(trace));
        if(!strcmp(prompt,"hold")){char marker[1024],release[1024];snprintf(marker,sizeof(marker),"%s.held",log);snprintf(release,sizeof(release),"%s.release",log);
            file_write(marker,"held");while(access(release,F_OK))delay();}
        const char *cache=getenv("SAMOSA_VISUAL_CACHE_PROBE");if(cache){int fd=open(cache,O_WRONLY|O_CREAT,0600);CHECK(fd>=0&&pwrite(fd,"x",1,67108864)==1&&!close(fd));}
        if(!strcmp(prompt,"stall")){signal(SIGTERM,SIG_IGN);for(;;)pause();}
        if(!strcmp(prompt,"crash"))_exit(17);
        if(!strcmp(prompt,"malformed")){response(framed,"{}");free(image);free(id);free(prompt);free(command);free(body);continue;}
        char *reply=NULL;size_t bytes=0;FILE *out=open_memstream(&reply,&bytes);CHECK(out);
        fputs("{\"status\":\"ok\",\"id\":",out);json_quote(out,id,strlen(id));
        fputs(",\"observation\":",out);json_quote(out,getenv("SAMOSA_VISUAL_BAD_READY")?"":color,getenv("SAMOSA_VISUAL_BAD_READY")?0:strlen(color));
        fputs(",\"prompt_tokens\":4,\"generated_tokens\":2",out);
        if(framed){CHECK(paths>=0&&times>=0);fprintf(out,",\"frames\":%d,\"images\":0,\"sample_times_seconds\":",j.tokens[paths].count);
            fwrite(body+j.tokens[times].begin,1,j.tokens[times].end-j.tokens[times].begin,out);}
        fputc('}',out);CHECK(!fclose(out));response(framed,reply);
        free(reply);free(image);free(id);free(prompt);free(command);free(body);
    }
}
static void package(const char *model,const char *metadata){
    const char *names[]={"config.json","tokenizer.json","processor.json","weights.safetensors"};char path[4096],source[4096],manifest[4096];
    snprintf(manifest,sizeof(manifest),"%s/manifest.json",model);FILE *out=fopen(manifest,"w");CHECK(out&&fchmod(fileno(out),0600)==0);
    fputs("{\"format\":\"samosa.molmo2.mlx.v1\",\"package_id\":\"molmo2-4b-mlx-q4-v1\",\"model_id\":\"allenai/Molmo2-4B\",\"upstream_revision\":\"042abfa7a38879a376cec03d949eff0aefaa0600\",\"processor_fingerprint\":\"808de9add76144a557348c5f5180a8408b12ca83592c7a6a257ae69c968e51df\",\"quantization\":{\"mode\":\"affine\",\"bits\":4,\"group_size\":64},\"estimated_resident_bytes\":3932160000,\"files\":[",out);
    for(size_t k=0;k<4;k++){
        snprintf(path,sizeof(path),"%s/%s",model,names[k]);
        if(k<3){snprintf(source,sizeof(source),"%s/%s",metadata,names[k]);file_copy(source,path);}else file_write(path,"controlled fixture weights, not a model");
        char pin[65];struct stat st;file_sha(path,pin);CHECK(!stat(path,&st));
        fprintf(out,"%s{\"name\":\"%s\",\"role\":\"%s\",\"bytes\":%llu,\"sha256\":\"%s\"}",k?",":"",names[k],k==3?"weights":k==1?"tokenizer":"metadata",(unsigned long long)st.st_size,pin);
    }
    fputs("]}",out);CHECK(!fclose(out));
}
static unsigned revision(const char *state){
    char path[1024];snprintf(path,sizeof(path),"%s/activation/activation.sqlite3",state);
    sqlite3 *db=NULL;CHECK(sqlite3_open_v2(path,&db,SQLITE_OPEN_READONLY,NULL)==SQLITE_OK);sqlite3_stmt *q=NULL;
    CHECK(sqlite3_prepare_v2(db,"SELECT revision FROM current WHERE id=1",-1,&q,NULL)==SQLITE_OK&&sqlite3_step(q)==SQLITE_ROW);
    unsigned value=(unsigned)sqlite3_column_int(q,0);sqlite3_finalize(q);CHECK(sqlite3_close(db)==SQLITE_OK);return value;
}
typedef struct {
    VisualHost *host;InferenceConfig cfg;int temporal;
    atomic_int cancel,checked;const char *error,*prompt;char *text;
} ControlledCall;
static int cancelled(void *context){ControlledCall *call=context;atomic_store(&call->checked,1);return atomic_load(&call->cancel);}
static void *controlled_call(void *context){
    ControlledCall *call=context;unsigned char rgba[]={255,0,0,255};char prepared[65];size_t in=0,out=0;
    if(call->temporal){TemporalInput input={.count=1,.end_us=1000000,.tokens=32,.prompt=(char *)call->prompt};
        input.frames[0]=(TemporalFrame){.width=1,.height=1,.rgba=rgba};hash_bytes(rgba,4,input.input_sha256);
        call->error=visual_host_temporal(call->host,&call->cfg,&input,&call->text,&in,&out);}
    else call->error=visual_host_image(call->host,&call->cfg,rgba,1,1,call->prompt,32,&call->text,&in,&out,prepared);
    return NULL;
}
static void queued_cancel(VisualHost *host,const InferenceConfig *cfg,const char *log,int temporal){
    ControlledCall active={.host=host,.cfg=*cfg,.temporal=temporal,.prompt="hold"},queued={.host=host,.cfg=*cfg,.temporal=temporal,.prompt="color"};
    atomic_init(&active.cancel,0);atomic_init(&active.checked,0);atomic_init(&queued.cancel,0);atomic_init(&queued.checked,0);
    queued.cfg.cancelled=cancelled;queued.cfg.cancellation=&queued;
    char marker[1024],release[1024];snprintf(marker,sizeof(marker),"%s.held",log);snprintf(release,sizeof(release),"%s.release",log);
    pthread_t threads[2];pid_t guardian=host->activation.process.guardian;
    CHECK(!pthread_create(&threads[0],NULL,controlled_call,&active));
    for(int i=0;i<500&&access(marker,F_OK);i++)delay();CHECK(!access(marker,F_OK));
    CHECK(!pthread_create(&threads[1],NULL,controlled_call,&queued));
    for(int i=0;i<500&&!atomic_load(&queued.checked);i++)delay();CHECK(atomic_load(&queued.checked));
    atomic_store(&queued.cancel,1);CHECK(!pthread_join(threads[1],NULL));
    CHECK(queued.error&&!strcmp(queued.error,"cancelled")&&!queued.text);
    CHECK(!visual_host_status(host)&&!strcmp(visual_host_stop(host),"provider_busy")&&host->activation.process.guardian==guardian);
    file_write(release,"release");CHECK(!pthread_join(threads[0],NULL));
    CHECK(!active.error&&active.text&&!strcmp(active.text,"red"));free(active.text);
    CHECK(!visual_host_status(host)&&host->activation.process.guardian==guardian);
}
static void parent_death(const char *root,const InferenceConfig *source,size_t engine_size,size_t anchor_size,const char *log,int temporal,const char *metadata){
    char state[1024];snprintf(state,sizeof(state),"%s/death-%d",root,temporal);
    int notify[2];CHECK(!pipe(notify));pid_t peer=fork();CHECK(peer>=0);if(!peer){close(notify[0]);close(notify[1]);for(;;)pause();}
    char descriptor[32];snprintf(descriptor,sizeof(descriptor),"%d",notify[1]);
    const char *self=temporal?source->temporal->self:source->vision->self;
    char *args[]={(char *)self,(char *)metadata,(char *)(temporal?source->temporal->executable:source->vision->executable),
        (char *)(temporal?source->temporal->model:source->vision->model),temporal?"temporal":"vision","--death-owner",state,descriptor,NULL};
    /* SQLite/system logging is not safe in a post-thread fork child on macOS.
       Execute a fresh owner before opening its state, as the real host does. */
    pid_t owner=fork();CHECK(owner>=0);
    if(!owner){close(notify[0]);execv(self,args);_exit(126);}
    close(notify[1]);char workspace[256];exact(notify[0],workspace,sizeof(workspace));close(notify[0]);
    FILE *trace=fopen(log,"r");CHECK(trace);long engine=0,current;char path[512];
    while(fscanf(trace,"%ld %511s",&current,path)==2)engine=current;CHECK(!fclose(trace)&&engine>1);
    CHECK(!kill(owner,SIGKILL));int status;CHECK(waitpid(owner,&status,0)==owner);
    for(int i=0;i<500;i++){errno=0;if(kill((pid_t)engine,0)<0&&errno==ESRCH&&access(workspace,F_OK))break;delay();}
    errno=0;CHECK(kill((pid_t)engine,0)<0&&errno==ESRCH);CHECK(access(workspace,F_OK)&&errno==ENOENT);
    CHECK(!kill(peer,0)&&!kill(peer,SIGKILL)&&waitpid(peer,&status,0)==peer);
    InferenceConfig retry=*source;retry.admission_state=state;VisualHost recovered;
    CHECK(!visual_host_init(&recovered,&retry,engine_size,anchor_size)&&!visual_host_start(&recovered,&retry)&&revision(state)==2);
    CHECK(!visual_host_close(&recovered));
}
static char *generate(VisualHost *host,InferenceConfig *cfg,unsigned char *rgba,size_t width,size_t height,const char *prompt,const char *expected,int temporal){
    char *text=NULL,prepared[65];size_t in=0,out=0;const char *error;
    if(temporal){TemporalInput input={.count=1,.end_us=1000000,.tokens=32,.prompt=(char *)prompt};input.frames[0]=(TemporalFrame){.width=width,.height=height,.rgba=rgba};
        hash_bytes(rgba,width*height*4,input.input_sha256);error=visual_host_temporal(host,cfg,&input,&text,&in,&out);}
    else error=visual_host_image(host,cfg,rgba,width,height,prompt,32,&text,&in,&out,prepared);
    if(text)for(char *p=text;*p;p++)*p=(char)tolower((unsigned char)*p);
    if(error||!text)fprintf(stderr,"generate %s: %s\n",prompt,error?error:"missing text");
    CHECK(!error&&text&&strstr(text,expected)&&in<=131072&&out<=32);return text;
}
static unsigned char *heading(const char *path,size_t *w,size_t *h){
    FILE *file=fopen(path,"rb");CHECK(file);size_t width=0,height=0;
    CHECK(fscanf(file,"P6\n%zu %zu\n255",&width,&height)==2&&width&&height&&width<=4096&&height<=4096&&fgetc(file)=='\n');
    size_t bytes=width*height*3;unsigned char *rgb=malloc(bytes);CHECK(rgb&&fread(rgb,1,bytes,file)==bytes&&fgetc(file)==EOF&&!fclose(file));
    *w=width;*h=height;if(*w>512||*h>512){if(*w>=*h){*h=height*512/width;*w=512;}else{*w=width*512/height;*h=512;}}
    unsigned char *rgba=malloc(*w**h*4);CHECK(rgba);
    for(size_t y=0;y<*h;y++)for(size_t x=0;x<*w;x++){
        size_t source=((y*height/ *h)*width+x*width/ *w)*3,target=(y* *w+x)*4;
        memcpy(rgba+target,rgb+source,3);rgba[target+3]=255;
    }
    free(rgb);return rgba;
}
static void public_call(FILE *input,FILE *output,const char *request,const char *expected){
    CHECK(fprintf(input,"{\"contract_version\":1,\"request_id\":\"visual\",\"request\":%s}\n",request)>0&&!fflush(input));
    struct pollfd wait={fileno(output),POLLIN,0};CHECK(poll(&wait,1,30000)>0);
    char *line=NULL;size_t bytes=0;CHECK(getline(&line,&bytes,output)>0);
    if(!strstr(line,expected))fprintf(stderr,"public request %s expected %s: %s\n",request,expected,line);
    CHECK(strstr(line,"\"request_id\":\"visual\"")&&strstr(line,expected));free(line);
}
static void public_registry(const char *cli,const char *root,const InferenceConfig *cfg,size_t engine_size,size_t anchor_size,int temporal){
    char path[1024],state[1024],request[2048];snprintf(path,sizeof(path),"%s/public-%d.json",root,temporal);snprintf(state,sizeof(state),"%s/public-state-%d",root,temporal);
    FILE *file=fopen(path,"w");CHECK(file&&fchmod(fileno(file),0600)==0);
    fprintf(file,"{\"contract_version\":1,\"owner\":\"visual-owner\",\"providers\":[{\"provider_id\":\"%s\",\"kind\":\"%s\",\"state\":",cfg->provider_id,temporal?"video_analysis":"image_analysis");
    json_quote(file,state,strlen(state));fputs(",\"engine\":",file);const char *engine=temporal?cfg->temporal->executable:cfg->vision->executable;json_quote(file,engine,strlen(engine));
    const char *model=temporal?cfg->temporal->model:cfg->vision->model;fputs(",\"model\":",file);json_quote(file,model,strlen(model));
    fprintf(file,",\"engine_sha256\":\"%s\",\"model_sha256\":\"%s\",\"lifecycle\":\"%s\",\"engine_size\":%zu,\"%s\":%zu",
        temporal?cfg->temporal->executable_sha256:cfg->vision->executable_sha256,temporal?cfg->temporal->model_sha256:cfg->vision->model_sha256,
        temporal?"resident-temporal-v1":"resident-vision-v1",engine_size,temporal?"manifest_size":"config_size",anchor_size);
    if(temporal)fprintf(file,",\"manifest_sha256\":\"%s\"",cfg->temporal->manifest_sha256);
    else fprintf(file,",\"config_sha256\":\"%s\",\"tokenizer_sha256\":\"%s\",\"weights_sha256\":\"%s\"",cfg->vision->config_sha256,cfg->vision->tokenizer_sha256,cfg->vision->weights_sha256);
    fputs("}]}",file);CHECK(!fclose(file));
    int in[2],out[2];CHECK(!pipe(in)&&!pipe(out));pid_t child=fork();CHECK(child>=0);
    if(!child){CHECK(dup2(in[0],0)>=0&&dup2(out[1],1)>=0);close(in[0]);close(in[1]);close(out[0]);close(out[1]);execl(cli,cli,"--config",path,"--session",(char *)NULL);_exit(126);}
    close(in[0]);close(out[1]);FILE *input=fdopen(in[1],"w"),*output=fdopen(out[0],"r");CHECK(input&&output&&!setvbuf(output,NULL,_IONBF,0));
    snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"provider_lifecycle_status\",\"provider_id\":\"%s\"}",cfg->provider_id);public_call(input,output,request,"\"ready\":false");
    snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"provider_start\",\"provider_id\":\"%s\"}",cfg->provider_id);public_call(input,output,request,"\"ready\":true");
    if(temporal)snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"analyze_video_frames\",\"provider_id\":\"%s\",\"frames\":[{\"timestamp_us\":0,\"width\":1,\"height\":1,\"rgba_base64\":\"/wAA/w==\"}],\"start_us\":0,\"end_us\":1000000,\"max_output_tokens\":32,\"prompt\":\"color\",\"operation_id\":\"public-red\"}",cfg->provider_id);
    else snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"analyze_image\",\"provider_id\":\"%s\",\"image_rgba_base64\":\"/wAA/w==\",\"width\":1,\"height\":1,\"max_output_tokens\":32,\"prompt\":\"color\",\"operation_id\":\"public-red\"}",cfg->provider_id);
    public_call(input,output,request,"\"text\":\"red\"");public_call(input,output,request,"\"text\":\"red\"");
    snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"provider_stop\",\"provider_id\":\"%s\"}",cfg->provider_id);public_call(input,output,request,"\"ready\":false");
    snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"provider_start\",\"provider_id\":\"%s\"}",cfg->provider_id);public_call(input,output,request,"\"ready\":true");
    FILE *trace=fopen(getenv("SAMOSA_VISUAL_FIXTURE_LOG"),"r");CHECK(trace);long worker=0,current;char workspace[512];
    while(fscanf(trace,"%ld %511s",&current,workspace)==2)worker=current;CHECK(!fclose(trace)&&worker>1);
    char *slash=strrchr(workspace,'/');CHECK(slash);*slash=0;CHECK(!access(workspace,F_OK));
    CHECK(!fclose(input)&&!fclose(output));int status;CHECK(waitpid(child,&status,0)==child&&WIFEXITED(status)&&WEXITSTATUS(status)==0);
    errno=0;CHECK(kill((pid_t)worker,0)<0&&errno==ESRCH);CHECK(access(workspace,F_OK)&&errno==ENOENT);
}
int main(int argc,char **argv){
    int bridge=vision_bridge_dispatch(argc,argv);if(bridge>=0)return bridge;
    int guardian=engine_guardian_dispatch(argc,argv);if(guardian>=0)return guardian;
    if(argc==2&&getenv("SAMOSA_VISUAL_FIXTURE_LOG"))return engine_fixture(argv[1]);
    int death_owner=argc==8&&!strcmp(argv[5],"--death-owner");
    CHECK((argc==2||argc==5||argc==6||death_owner)&&argv[0][0]=='/');alarm(600);
    int real=argc>=5,real_temporal=real&&!strcmp(argv[4],"temporal");
    char root[]="/tmp/samosa-visual-host-XXXXXX";CHECK(death_owner||mkdtemp(root));
    unsigned char red[]={255,0,0,255},blue[]={0,0,255,255};
    for(int temporal=real?real_temporal:0;temporal<(real?real_temporal+1:2);temporal++){
        char state[512],model[4096],engine[4096],anchor[4096],path[4096],engine_sha[65],anchor_sha[65],log[512],cache[512];
        snprintf(state,sizeof(state),"%s/state-%d",root,temporal);snprintf(model,sizeof(model),"%s/model-%d",root,temporal);
        snprintf(engine,sizeof(engine),"%s/engine-%d",root,temporal);snprintf(log,sizeof(log),"%s/log-%d",root,temporal);snprintf(cache,sizeof(cache),"%s/cache-%d",root,temporal);
        if(real){CHECK(strlen(argv[3])<sizeof(model)&&strlen(argv[2])<sizeof(engine));strcpy(model,argv[3]);strcpy(engine,argv[2]);}
        else{
            const char *fixture=getenv("SAMOSA_VISUAL_NATIVE_FIXTURE");
            CHECK(!mkdir(model,0700));file_copy(fixture?fixture:argv[0],engine);
            if(temporal)package(model,argv[1]);
            else{const char *names[]={"config.json","tokenizer.json","model.safetensors"};for(size_t k=0;k<3;k++){snprintf(path,sizeof(path),"%s/%s",model,names[k]);file_write(path,"abc");}}
            CHECK(!setenv("SAMOSA_VISUAL_FIXTURE_LOG",log,1)&&!setenv("SAMOSA_VISUAL_CACHE_PROBE",cache,1));
            if(temporal)CHECK(!setenv("SAMOSA_VISUAL_FRAME_PROTOCOL","1",1));else CHECK(!unsetenv("SAMOSA_VISUAL_FRAME_PROTOCOL"));
        }
        file_sha(engine,engine_sha);snprintf(anchor,sizeof(anchor),"%s/%s",model,temporal?"manifest.json":"config.json");file_sha(anchor,anchor_sha);
        VisionConfig vision={.self=argv[0],.executable=engine,.model=model,.executable_sha256=engine_sha,.config_sha256=anchor_sha};
        char tokenizer[65],weights[65],identity[65];TemporalConfig video={.self=argv[0],.executable=engine,.model=model,.executable_sha256=engine_sha,.manifest_sha256=anchor_sha};
        if(temporal){temporal_package_identity(anchor_sha,identity);video.model_sha256=identity;}
        else{snprintf(path,sizeof(path),"%s/tokenizer.json",model);file_sha(path,tokenizer);snprintf(path,sizeof(path),"%s/model.safetensors",model);file_sha(path,weights);
            vision.tokenizer_sha256=tokenizer;vision.weights_sha256=weights;vision_model_identity(&vision,identity);vision.model_sha256=identity;}
        InferenceConfig cfg={.provider_id=temporal?"molmo2-native-v1":"visionpsy-native-v1",.owner="visual-owner",.admission_state=state,
            .vision=temporal?NULL:&vision,.temporal=temporal?&video:NULL};
        if(death_owner)cfg.admission_state=argv[6];
        struct stat e,a;CHECK(!stat(engine,&e)&&!stat(anchor,&a));VisualHost host;
        CHECK(!visual_host_init(&host,&cfg,(size_t)e.st_size,(size_t)a.st_size));
        if(death_owner){CHECK(!visual_host_start(&host,&cfg));int notify=atoi(argv[7]);CHECK(notify>2);
            write_all(notify,host.workspace,sizeof(host.workspace));close(notify);for(;;)pause();}
        CHECK(!strcmp(visual_host_status(&host),"provider_unavailable"));if(!real)CHECK(access(log,F_OK));
        const char *error=visual_host_start(&host,&cfg);if(error)fprintf(stderr,"start temporal=%d: %s\n",temporal,error);CHECK(!error);
        CHECK(!visual_host_status(&host)&&revision(state)==1);pid_t guardian_pid=host.activation.process.guardian;CHECK(guardian_pid>0);
        char workspace[256];strcpy(workspace,host.workspace);
        if(real&&!temporal){
            CHECK(argc==6);size_t w=0,h=0;unsigned char *rgba=heading(argv[5],&w,&h);
            char *text=generate(&host,&cfg,rgba,w,h,"Read the large heading on this page. Answer briefly.","hello",0);printf("Actual heading: %s\n",text);free(text);
            rgba[0]=255;rgba[1]=0;rgba[2]=0;
            text=generate(&host,&cfg,rgba,w,h,"Read the large heading on this page. Answer briefly.","hello",0);printf("Actual changed-pixel heading: %s\n",text);free(text);free(rgba);
        }else{
            free(generate(&host,&cfg,red,1,1,"Describe the main color briefly.","red",temporal));
            free(generate(&host,&cfg,blue,1,1,"Describe the main color briefly.","blue",temporal));
        }
        CHECK(host.activation.process.guardian==guardian_pid&&!visual_host_status(&host)&&revision(state)==1);
        if(!real)queued_cancel(&host,&cfg,log,temporal);
        InferenceConfig expired=cfg;expired.deadline_monotonic=inference_now()-1;
        char *text=NULL,prepared[65];size_t in=0,out=0;
        if(!temporal){error=visual_host_image(&host,&expired,red,1,1,"expired",32,&text,&in,&out,prepared);CHECK(error&&!strcmp(error,"deadline_exceeded")&&!text);}
        CHECK(host.activation.process.guardian==guardian_pid&&!visual_host_status(&host));
        CHECK(!visual_host_stop(&host)&&!host.attempted&&access(workspace,F_OK)&&errno==ENOENT);
        if(!real){
            struct stat st;CHECK(!stat(cache,&st)&&st.st_size==67108865);
            CHECK(!setenv("SAMOSA_VISUAL_BAD_READY","1",1));error=visual_host_start(&host,&cfg);
            CHECK(error&&!strcmp(error,"invalid_provider_response")&&!host.qualified&&revision(state)==1);
            CHECK(!unsetenv("SAMOSA_VISUAL_BAD_READY"));CHECK(!chmod(model,0777));error=visual_host_start(&host,&cfg);
            CHECK(error&&!strcmp(error,"unsupported_artifact")&&!host.qualified&&revision(state)==1);
            CHECK(!chmod(model,0700));CHECK(!visual_host_start(&host,&cfg)&&revision(state)==2);
            InferenceConfig limited=cfg;limited.deadline_monotonic=inference_now()+0.15;
            if(temporal){TemporalInput input={.count=1,.end_us=1000000,.tokens=32,.prompt="stall"};input.frames[0]=(TemporalFrame){.width=1,.height=1,.rgba=red};hash_bytes(red,4,input.input_sha256);
                error=visual_host_temporal(&host,&limited,&input,&text,&in,&out);}
            else error=visual_host_image(&host,&limited,red,1,1,"stall",32,&text,&in,&out,prepared);
            CHECK(error&&!strcmp(error,"deadline_exceeded")&&!text&&!host.attempted);
            CHECK(!visual_host_start(&host,&cfg)&&revision(state)==3);
            snprintf(path,sizeof(path),"%s/%s",model,temporal?"weights.safetensors":"model.safetensors");file_write(path,"modified fixture weights");
            error=visual_host_status(&host);CHECK(error&&!strcmp(error,"provider_artifact_changed"));
            CHECK(!visual_host_stop(&host));error=visual_host_start(&host,&cfg);
            CHECK(error&&!host.qualified&&revision(state)==3);CHECK(!visual_host_close(&host));
            file_write(path,temporal?"controlled fixture weights, not a model":"abc");
        }
        CHECK(!visual_host_close(&host));
        if(!real)parent_death(root,&cfg,(size_t)e.st_size,(size_t)a.st_size,log,temporal,argv[1]);
        const char *registry=getenv("SAMOSA_VISUAL_REGISTRY");if(registry&&!real)public_registry(registry,root,&cfg,(size_t)e.st_size,(size_t)a.st_size,temporal);
        printf("PASS %s %s resident visual owner: %u checks; actual-request readiness, changing prepared input on one guardian, stop/cleanup%s\n",
               real?"actual Mac":"native fixture",temporal?"Molmo2":"VisionPsy",checks,real?"":"; failed readiness keeps last-good, deadline invalidation, cache growth");
    }
    cleanup(root);return 0;
}
