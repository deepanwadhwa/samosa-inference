#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "whisper_fixture.h"
#include <dlfcn.h>
static const char *media_helper;
static const char *chutni_library,*chutni_worker;
static char *chutni_store_id;
static char chutni_store[512];
static void symbol(void *library,const char *name,void *target,size_t n){void *p=dlsym(library,name);CHECK(p&&n==sizeof(p));memcpy(target,&p,n);}
static void create_store(const char *root){
    snprintf(chutni_store,sizeof(chutni_store),"%s/memory.chutni",root);void *library=dlopen(chutni_library,RTLD_NOW|RTLD_LOCAL);CHECK(library);
    int (*create)(const char*,const char*,void**)=NULL;int (*call)(void*,const char*,const char*,char**)=NULL;void (*release)(void*)=NULL;void (*close_store)(void*)=NULL;
    symbol(library,"chutni_create",&create,sizeof(create));symbol(library,"chutni_call",&call,sizeof(call));symbol(library,"chutni_free",&release,sizeof(release));symbol(library,"chutni_close",&close_store,sizeof(close_store));
    void *store=NULL;CHECK(!create(chutni_store,"Actual Whisper publication qualification",&store));char *info=NULL;CHECK(!call(store,"store_info","{}",&info));chutni_store_id=string_field(info,"store_id");release(info);close_store(store);CHECK(!dlclose(library));
}
static char *catalog_transport(const char *exe,const char *root,const char *state,const FixtureConfig *cfg,const char *request,int offline,const char *arguments){
    char *args[36]={(char*)exe,"--root",(char*)root,"--state",(char*)state,"--owner","actual-whisper-test",
        "--inference-provider-id","whisper-base-en-v1","--inference-engine-sha256",(char*)cfg->engine_sha,"--inference-model-sha256",(char*)cfg->model_sha,
        "--chutni-library",(char*)chutni_library,"--chutni-store",chutni_store,"--chutni-store-id",chutni_store_id,"--chutni-helper",offline?"/missing/chutni-worker":(char*)chutni_worker,"--chutni-publish",NULL};
    size_t at=22;if(media_helper){args[at++]="--media-helper";args[at++]=(char*)media_helper;}
    if(arguments){args[at++]="--inference-executable";args[at++]=(char*)cfg->cli;args[at++]="--inference-arguments-file";args[at++]=(char*)arguments;}
    args[at]=NULL;return finish(spawn_request(args,request),0);
}
static char *catalog_call(const char *exe,const char *root,const char *state,const FixtureConfig *cfg,const char *request,int offline){return catalog_transport(exe,root,state,cfg,request,offline,NULL);}
static int same_json(const Json *a,int x,const Json *b,int y){
    if(x<0||y<0||a->tokens[x].type!=b->tokens[y].type||a->tokens[x].count!=b->tokens[y].count)return 0;
    JsonType kind=a->tokens[x].type;if(kind==JT_STRING){char *left=json_string(a,x),*right=json_string(b,y);int same=left&&right&&!strcmp(left,right);free(left);free(right);return same;}
    if(kind==JT_OBJECT){for(int t=x+1;t<a->tokens[x].next;t=a->tokens[a->tokens[t].next].next){char *key=json_string(a,t);int matched=key&&same_json(a,a->tokens[t].next,b,json_get(b,y,key));free(key);if(!matched)return 0;}return 1;}
    if(kind==JT_ARRAY){int right=y+1;for(int t=x+1;t<a->tokens[x].next;t=a->tokens[t].next){if(!same_json(a,t,b,right))return 0;right=b->tokens[right].next;}return 1;}
    size_t n=a->tokens[x].end-a->tokens[x].begin;return n==b->tokens[y].end-b->tokens[y].begin&&!memcmp(a->text+a->tokens[x].begin,b->text+b->tokens[y].begin,n);
}
static char *source_call(const char *exe,const char *root,const char *state,const char *request){char *args[12]={(char*)exe,"--root",(char*)root,"--state",(char*)state,"--owner","actual-whisper-test",NULL};if(media_helper){args[7]="--media-helper";args[8]=(char*)media_helper;args[9]=NULL;}return finish(spawn_request(args,request),0);}
static char *source_capture(const char *exe,const char *state,const FixtureConfig *cfg,const char *arguments,const char *request,int online){
    char *args[24]={(char*)exe,"--state",(char*)state,"--owner","actual-whisper-test",
        "--inference-provider-id","whisper-base-en-v1","--inference-engine-sha256",(char*)cfg->engine_sha,
        "--inference-model-sha256",(char*)cfg->model_sha,NULL};
    size_t at=11;if(media_helper){args[at++]="--media-helper";args[at++]=(char*)media_helper;}
    if(online){args[at++]="--inference-executable";args[at++]=(char*)cfg->cli;args[at++]="--inference-arguments-file";args[at++]=(char*)arguments;}args[at]=NULL;
    return finish(spawn_request(args,request),0);
}
static int full_duration(char **argv){
    CHECK(chutni_library&&chutni_worker);
    char root[]="/tmp/samosa-whisper-full-native-XXXXXX";CHECK(mkdtemp(root));
    char files[512],input[540],state[512],inference[512],arguments[512],request[1024];
    snprintf(files,sizeof(files),"%s/files",root);CHECK(!mkdir(files,0700));
    snprintf(input,sizeof(input),"%s/speech.wav",files);file_copy(media_helper?argv[7]:argv[5],input);
    struct stat reference_stat;CHECK(!stat(argv[5],&reference_stat)&&reference_stat.st_size>=44&&reference_stat.st_size<4194304);
    char *reference=file_read(argv[5],4194304);size_t data_offset=0,data_bytes=0;
    for(size_t offset=12;offset+8<=(size_t)reference_stat.st_size;){size_t chunk=get32((unsigned char*)reference+offset+4);CHECK(chunk<=(size_t)reference_stat.st_size-offset-8);if(!memcmp(reference+offset,"data",4)){data_bytes=chunk;data_offset=offset+8;break;}offset+=8+chunk+(chunk&1);}
    CHECK(data_offset&&data_bytes>=66000u*32u);
    snprintf(state,sizeof(state),"%s/sources",root);snprintf(inference,sizeof(inference),"%s/inference",root);
    FixtureConfig cfg={.cli=argv[1],.engine=argv[3],.model=argv[4],.state=inference,.owner="actual-whisper-test"};
    file_sha(cfg.engine,cfg.engine_sha);file_sha(cfg.model,cfg.model_sha);create_store(root);
    snprintf(arguments,sizeof(arguments),"%s/arguments.json",root);
    FILE *out=fopen(arguments,"w");CHECK(out);CHECK(!chmod(arguments,0600));const char *values[]={"--whisper-cli",cfg.engine,"--whisper-model",cfg.model,"--whisper-cli-sha256",cfg.engine_sha,"--whisper-model-sha256",cfg.model_sha,"--admission-state",cfg.state,"--owner",cfg.owner};
    fputc('[',out);for(size_t i=0;i<12;i++){if(i)fputc(',',out);json_quote(out,values[i],strlen(values[i]));}fputc(']',out);CHECK(!fclose(out));
    char *registered=source_call(argv[2],files,state,media_helper?"{\"contract_version\":1,\"op\":\"register_media\",\"path\":\"speech.wav\"}":"{\"contract_version\":1,\"op\":\"register_audio\",\"path\":\"speech.wav\"}");
    char *source=string_field(registered,"source_id"),*version=string_field(registered,"version");free(registered);
    snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"audio_index_create\",\"source_id\":\"%s\",\"version\":\"%s\",\"start_ms\":0,\"end_ms\":66000}",source,version);
    char *body=catalog_call(argv[2],files,state,&cfg,request,0),*job=string_field(body,"job_id");CHECK(strstr(body,"\"total_windows\":3"));free(body);
    char *captures[3]={0};size_t starts[]={0,29000,58000},ends[]={30000,59000,66000};
    for(size_t k=0;k<3;k++){
        snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"audio_index_step\",\"job_id\":\"%s\",\"timeout_ms\":60000}",job);
        body=catalog_transport(argv[2],files,state,&cfg,request,0,arguments);CHECK(strstr(body,"\"phase\":\"captured\""));free(body);
        char read[512];snprintf(read,sizeof(read),"{\"contract_version\":1,\"op\":\"audio_index_read\",\"job_id\":\"%s\",\"ordinal\":%zu}",job,k);
        captures[k]=catalog_call(argv[2],files,state,&cfg,read,0);
        CHECK(strstr(captures[k],"coriander")&&strstr(captures[k],"sample")&&strstr(captures[k],"speech_recognition_uncertain")&&strstr(captures[k],"source_time_ms"));
        Json j;parse(captures[k],&j);int segments=json_get(&j,0,"segments");CHECK(segments>=0&&j.tokens[segments].count>0);
        size_t wave_bytes=44+(ends[k]-starts[k])*32;unsigned char *wave=calloc(wave_bytes,1);CHECK(wave);memcpy(wave,"RIFF",4);put32(wave+4,(uint32_t)wave_bytes-8);memcpy(wave+8,"WAVEfmt ",8);put32(wave+16,16);wave[20]=1;wave[22]=1;put32(wave+24,16000);put32(wave+28,32000);wave[32]=2;wave[34]=16;memcpy(wave+36,"data",4);put32(wave+40,(uint32_t)wave_bytes-44);memcpy(wave+44,reference+data_offset+starts[k]*32,wave_bytes-44);
        char expected_sha[65];hash_bytes(wave,wave_bytes,expected_sha);free(wave);
        char *input_sha=json_string(&j,json_get(&j,json_get(&j,0,"provenance"),"input_sha256"));CHECK(input_sha&&!strcmp(input_sha,expected_sha));free(input_sha);
        size_t last=starts[k];for(int t=segments+1;t<j.tokens[segments].next;t=j.tokens[t].next){size_t a=0,b=0;CHECK(json_uint(&j,json_get(&j,t,"start_ms"),&a)&&json_uint(&j,json_get(&j,t,"end_ms"),&b)&&a>=starts[k]&&a>=last&&a<b&&b<=ends[k]);last=a;}
        /* Publication and a fresh-process retry use the saved capture, no model. */
        body=catalog_call(argv[2],files,state,&cfg,request,0);CHECK(strstr(body,"\"phase\":\"published\""));free(body);
    }
    snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"audio_index_get\",\"job_id\":\"%s\"}",job);
    body=catalog_call(argv[2],files,state,&cfg,request,0);CHECK(strstr(body,"\"status\":\"completed\"")&&strstr(body,"\"captured_windows\":3")&&strstr(body,"\"published_windows\":3"));free(body);
    CHECK(!unlink(input));body=catalog_call(argv[2],files,state,&cfg,request,1);CHECK(strstr(body,"\"status\":\"completed\""));free(body);
    for(size_t k=0;k<3;k++){snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"audio_index_read\",\"job_id\":\"%s\",\"ordinal\":%zu}",job,k);body=catalog_call(argv[2],files,state,&cfg,request,1);CHECK(!strcmp(body,captures[k]));free(body);free(captures[k]);}
    free(reference);free(source);free(version);free(job);free(chutni_store_id);cleanup(root);
    printf("PASS actual full-duration %s speech: %u checks;66-second recording, three overlapping windows, independent PCM/input hashes, initial/middle/late speech, source-time bounds, native model capture, SDK publication and origin/model-free retained readback\n",media_helper?"encoded":"WAV",checks);return 0;
}
int main(int argc,char **argv){
    CHECK(argc==6||argc==8);for(int i=1;i<argc;i++)CHECK(argv[i][0]=='/');if(argc==8)media_helper=argv[6];
    chutni_library=getenv("SAMOSA_TEST_CHUTNI_LIBRARY");chutni_worker=getenv("SAMOSA_TEST_CHUTNI_WORKER");CHECK((!chutni_library&&!chutni_worker)||(chutni_library&&chutni_library[0]=='/'&&chutni_worker&&chutni_worker[0]=='/'));
    if(getenv("SAMOSA_TEST_FULL_DURATION"))return full_duration(argv);
    char root[]="/tmp/samosa-whisper-real-native-XXXXXX";CHECK(mkdtemp(root));char files[512],input[540],source_state[512],inference_state[512];
    snprintf(files,sizeof(files),"%s/files",root);snprintf(input,sizeof(input),"%s/speech.wav",files);snprintf(source_state,sizeof(source_state),"%s/source-state",root);snprintf(inference_state,sizeof(inference_state),"%s/inference-state",root);CHECK(!mkdir(files,0700));file_copy(argc==8?argv[7]:argv[5],input);
    struct stat s;CHECK(!stat(argv[5],&s)&&s.st_size>=44&&s.st_size<1048576);char *raw=file_read(argv[5],1048576);size_t duration=0,data_offset=0;
    for(size_t offset=12;offset+8<=(size_t)s.st_size;){size_t chunk=get32((unsigned char*)raw+offset+4);CHECK(chunk<=(size_t)s.st_size-offset-8);if(!memcmp(raw+offset,"data",4)){duration=chunk/32;data_offset=offset+8;break;}offset+=8+chunk+(chunk&1);}
    CHECK(duration>=500&&duration<=30000);size_t wave_bytes=44+duration*32;unsigned char *canonical=calloc(wave_bytes,1);CHECK(canonical);memcpy(canonical,"RIFF",4);put32(canonical+4,(uint32_t)wave_bytes-8);memcpy(canonical+8,"WAVEfmt ",8);put32(canonical+16,16);canonical[20]=1;canonical[22]=1;put32(canonical+24,16000);put32(canonical+28,32000);canonical[32]=2;canonical[34]=16;memcpy(canonical+36,"data",4);put32(canonical+40,(uint32_t)wave_bytes-44);memcpy(canonical+44,raw+data_offset,duration*32);char expected_sha[65];hash_bytes(canonical,wave_bytes,expected_sha);free(canonical);free(raw);
    char *registered=source_call(argv[2],files,source_state,media_helper?"{\"contract_version\":1,\"op\":\"register_media\",\"path\":\"speech.wav\"}":"{\"contract_version\":1,\"op\":\"register_audio\",\"path\":\"speech.wav\"}");char *source=string_field(registered,"source_id"),*version=string_field(registered,"version");free(registered);
    char request[1024];snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"prepare_audio\",\"source_id\":\"%s\",\"version\":\"%s\",\"start_ms\":0,\"end_ms\":%zu}",source,version,duration);
    char *prepared=source_call(argv[2],files,source_state,request);CHECK(strstr(prepared,"prepared_audio")&&strstr(prepared,"derived"));char *audio=string_field(prepared,"data");free(prepared);
    FixtureConfig cfg={.cli=argv[1],.engine=argv[3],.model=argv[4],.state=inference_state,.owner="actual-whisper-test"};file_sha(cfg.engine,cfg.engine_sha);file_sha(cfg.model,cfg.model_sha);
    char *transcript_request=transcribe(audio,"actual-transcript",60000),*result=invoke(&cfg,transcript_request,NULL),*text=string_field(result,"text"),*input_sha=string_field(result,"input_sha256");CHECK(!strcmp(input_sha,expected_sha));free(input_sha);CHECK(strstr(text,"coriander")&&strstr(text,"sample"));CHECK(strstr(result,"speech_recognition_uncertain")&&strstr(result,"derived"));Json j;parse(result,&j);size_t reported=0;CHECK(json_uint(&j,json_get(&j,0,"duration_ms"),&reported)&&reported==duration);
    int segments=json_get(&j,0,"segments");CHECK(segments>=0&&j.tokens[segments].type==JT_ARRAY&&j.tokens[segments].count>0);size_t previous=0;
    for(int t=segments+1;t<j.tokens[segments].next;t=j.tokens[t].next){size_t start=0,end=0;CHECK(json_uint(&j,json_get(&j,t,"start_ms"),&start)&&json_uint(&j,json_get(&j,t,"end_ms"),&end)&&start<end&&end<=duration&&start>=previous);previous=start;}
    char arguments[512];snprintf(arguments,sizeof(arguments),"%s/provider-arguments.json",root);
    char *argument_json=NULL;size_t argument_bytes=0;FILE *args_out=open_memstream(&argument_json,&argument_bytes);CHECK(args_out);
    const char *values[]={"--whisper-cli",cfg.engine,"--whisper-model",cfg.model,"--whisper-cli-sha256",cfg.engine_sha,"--whisper-model-sha256",cfg.model_sha,"--admission-state",cfg.state,"--owner",cfg.owner};
    fputc('[',args_out);for(size_t i=0;i<12;i++){if(i)fputc(',',args_out);json_quote(args_out,values[i],strlen(values[i]));}fputc(']',args_out);CHECK(!fclose(args_out));file_write(arguments,argument_json);free(argument_json);
    snprintf(request,sizeof(request),"{\"contract_version\":1,\"op\":\"transcribe_audio\",\"source_id\":\"%s\",\"version\":\"%s\",\"start_ms\":0,\"end_ms\":%zu}",source,version,duration);
    char *captured=source_capture(argv[2],source_state,&cfg,arguments,request,1);CHECK(strstr(captured,"coriander")&&strstr(captured,"sample")&&strstr(captured,"speech_recognition_uncertain"));
    Json capture;parse(captured,&capture);int provenance=json_get(&capture,0,"provenance");char *captured_sha=json_string(&capture,json_get(&capture,provenance,"input_sha256"));CHECK(captured_sha&&!strcmp(captured_sha,expected_sha));free(captured_sha);CHECK(!strstr(captured,"evidence_id"));CHECK(strstr(captured,"whisper_20ms_end_clipped_to_input"));
    char publication_request[1024],read_request[1024],queue_state[512],queue_read[1024],queue_get[1024],*queued_capture=NULL,*published=NULL;
    if(chutni_library){create_store(root);
        /* A new owner state has no prepared/transcript cache: capture must invoke
           the configured public Inference process with the actual installed model. */
        snprintf(queue_state,sizeof(queue_state),"%s/audio-queue-state",root);
        registered=source_call(argv[2],files,queue_state,media_helper?"{\"contract_version\":1,\"op\":\"register_media\",\"path\":\"speech.wav\"}":"{\"contract_version\":1,\"op\":\"register_audio\",\"path\":\"speech.wav\"}");
        char *queue_source=string_field(registered,"source_id"),*queue_version=string_field(registered,"version");CHECK(!strcmp(queue_source,source)&&!strcmp(queue_version,version));free(queue_source);free(queue_version);free(registered);
        char queue_request[1024];snprintf(queue_request,sizeof(queue_request),"{\"contract_version\":1,\"op\":\"audio_index_create\",\"source_id\":\"%s\",\"version\":\"%s\",\"start_ms\":0,\"end_ms\":%zu}",source,version,duration);
        char *created=catalog_call(argv[2],files,queue_state,&cfg,queue_request,0),*job=string_field(created,"job_id");CHECK(strstr(created,"\"total_windows\":1")&&strstr(created,"\"captured_windows\":0"));free(created);
        snprintf(queue_request,sizeof(queue_request),"{\"contract_version\":1,\"op\":\"audio_index_step\",\"job_id\":\"%s\",\"timeout_ms\":60000}",job);
        char *stepped=catalog_transport(argv[2],files,queue_state,&cfg,queue_request,0,arguments);CHECK(strstr(stepped,"\"phase\":\"captured\""));free(stepped);
        snprintf(queue_read,sizeof(queue_read),"{\"contract_version\":1,\"op\":\"audio_index_read\",\"job_id\":\"%s\",\"ordinal\":0}",job);
        queued_capture=catalog_call(argv[2],files,queue_state,&cfg,queue_read,0);Json queued;parse(queued_capture,&queued);int queued_provenance=json_get(&queued,0,"provenance");char *queued_sha=json_string(&queued,json_get(&queued,queued_provenance,"input_sha256"));CHECK(queued_sha&&!strcmp(queued_sha,expected_sha));free(queued_sha);
        CHECK(strstr(queued_capture,"coriander")&&strstr(queued_capture,"sample")&&strstr(queued_capture,"speech_recognition_uncertain")&&!strstr(queued_capture,"evidence_id"));
        /* Publication and completion reopen durable state without model transport. */
        stepped=catalog_call(argv[2],files,queue_state,&cfg,queue_request,0);CHECK(strstr(stepped,"\"phase\":\"published\""));free(stepped);
        snprintf(queue_get,sizeof(queue_get),"{\"contract_version\":1,\"op\":\"audio_index_get\",\"job_id\":\"%s\"}",job);free(job);
        char *completed=catalog_call(argv[2],files,queue_state,&cfg,queue_get,0);CHECK(strstr(completed,"\"status\":\"completed\"")&&strstr(completed,"\"captured_windows\":1")&&strstr(completed,"\"published_windows\":1")&&strstr(completed,"selected_audio_windows"));free(completed);
        snprintf(publication_request,sizeof(publication_request),"{\"contract_version\":1,\"op\":\"publish_transcript\",\"source_id\":\"%s\",\"version\":\"%s\",\"start_ms\":0,\"end_ms\":%zu}",source,version,duration);published=catalog_call(argv[2],files,source_state,&cfg,publication_request,0);Json receipt;parse(published,&receipt);int item=json_get(&receipt,0,"publication");CHECK(item>=0&&!strstr(published,"evidence_id"));char *external_source=json_string(&receipt,json_get(&receipt,item,"external_source_id")),*external_artifact=json_string(&receipt,json_get(&receipt,item,"external_artifact_id")),*artifact=json_string(&receipt,json_get(&receipt,item,"artifact_id")),*captured_artifact=string_field(captured,"artifact_id");CHECK(external_source&&external_artifact&&artifact&&!strcmp(artifact,captured_artifact));free(artifact);free(captured_artifact);
        snprintf(read_request,sizeof(read_request),"{\"contract_version\":1,\"op\":\"memory_read\",\"store_id\":\"%s\",\"external_source_id\":\"%s\",\"external_artifact_id\":\"%s\"}",chutni_store_id,external_source,external_artifact);free(external_source);free(external_artifact);
        char *read=catalog_call(argv[2],files,source_state,&cfg,read_request,0),*capture_text=string_field(captured,"text");Json observed;parse(read,&observed);int observed_result=json_get(&observed,json_get(&observed,0,"memory"),"result");char *read_text=json_string(&observed,json_get(&observed,observed_result,"text"));CHECK(read_text&&!strcmp(read_text,capture_text));free(read_text);free(capture_text);int proof=json_get(&observed,observed_result,"provenance"),derivation=json_get(&observed,proof,"derivation"),parameters=json_get(&observed,derivation,"parameters"),saved_capture=json_get(&observed,parameters,"sources_capture");CHECK(same_json(&capture,0,&observed,saved_capture));CHECK(strstr(read,"model_generated")&&strstr(read,"\"producer_kind\":\"model\"")&&strstr(read,"source_time_ms")&&strstr(read,"not_performed"));free(read);
    }
    CHECK(!unlink(input));if(!chutni_library)CHECK(!rmdir(files));char *offline=source_capture(argv[2],source_state,&cfg,arguments,request,0);CHECK(!strcmp(offline,captured));free(offline);
    if(chutni_library){char *saved=catalog_call(argv[2],files,queue_state,&cfg,queue_read,1);CHECK(!strcmp(saved,queued_capture));free(saved);free(queued_capture);char *completed=catalog_call(argv[2],files,queue_state,&cfg,queue_get,1);CHECK(strstr(completed,"\"status\":\"completed\""));free(completed);
        char *receipt=catalog_call(argv[2],files,source_state,&cfg,publication_request,1);CHECK(!strcmp(receipt,published));free(receipt);free(published);char *read=catalog_call(argv[2],files,source_state,&cfg,read_request,0);Json observed;parse(read,&observed);int observed_result=json_get(&observed,json_get(&observed,0,"memory"),"result"),proof=json_get(&observed,observed_result,"provenance"),parameters=json_get(&observed,json_get(&observed,proof,"derivation"),"parameters");CHECK(same_json(&capture,0,&observed,json_get(&observed,parameters,"sources_capture")));free(read);CHECK(!rmdir(files));free(chutni_store_id);}
    free(captured);free(source);free(version);
    char *retry=invoke(&cfg,transcript_request,NULL);CHECK(!strcmp(retry,result));free(retry);free(text);free(audio);free(transcript_request);
    printf("PASS actual Sources %s capture/cache -> installed Whisper public Inference%s: %u checks; exact prepared input, timed derived text, uncertainty and offline cache/replay\n%s",media_helper?"compressed-audio":"WAV",chutni_library?" -> durable audio queue and public Chutni publication/readback":"",checks,result);free(result);cleanup(root);return 0;
}
