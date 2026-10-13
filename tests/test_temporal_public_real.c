#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "whisper_fixture.h"
static char *invoke_temporal(char **argv,const char *state,const char *engine_pin,const char *manifest_pin,const char *request,int offline,const char *owner,const char *error){
    if(getenv("SAMOSA_TEST_USE_REGISTRY")){
        FixtureConfig c={.cli=argv[1],.engine=offline?"/missing/temporal-engine":argv[2],.model=offline?"/missing/temporal-model":argv[3],.state=state,.owner=owner};
        strcpy(c.engine_sha,engine_pin);char material[96],extra[128];
        int n=snprintf(material,sizeof(material),"molmo2-package-v1:%s",manifest_pin);CHECK(n>0&&(size_t)n<sizeof(material));hash_bytes(material,(size_t)n,c.model_sha);
        snprintf(extra,sizeof(extra),",\"manifest_sha256\":\"%s\"",manifest_pin);
        /* Reconfigure trusted owner/paths for the original offline and foreign
         * tests; public JSON never chooses them. Model identity stays pinned. */
        actual_registry(&c,argv[4],"molmo2-native-v1","video_analysis",extra);
        char *body=finish(registry_launch(&c,request,"molmo2-native-v1"),error?1:0);if(error)error_check(body,error);return body;
    }
    char *args[]={argv[1],"--temporal-engine",offline?"/missing/temporal-engine":argv[2],"--temporal-model",offline?"/missing/temporal-model":argv[3],"--temporal-engine-sha256",(char *)engine_pin,"--temporal-manifest-sha256",(char *)manifest_pin,"--admission-state",(char *)state,"--owner",(char *)owner,NULL};
    char *body=finish(spawn_request(args,request),error?1:0);if(error)error_check(body,error);return body;
}
int main(int argc,char **argv){
    CHECK(argc==5);for(int k=1;k<argc;k++)CHECK(argv[k][0]=='/');CHECK(!mkdir(argv[4],0700));
    char state[4096],manifest[4096],engine_pin[65],manifest_pin[65],model_pin[65],material[96],request_path[4096],response_path[4096];
    CHECK(snprintf(state,sizeof(state),"%s/admission",argv[4])<(int)sizeof(state));CHECK(snprintf(manifest,sizeof(manifest),"%s/manifest.json",argv[3])<(int)sizeof(manifest));file_sha(argv[2],engine_pin);file_sha(manifest,manifest_pin);int model_bytes=snprintf(material,sizeof(material),"molmo2-package-v1:%s",manifest_pin);CHECK(model_bytes>0&&model_bytes<(int)sizeof(material));hash_bytes(material,(size_t)model_bytes,model_pin);
    unsigned char *rgba=malloc(512u*512u*4u);CHECK(rgba);char *encoded[2]={0},pixel_hash[2][65],ppm_hash[2][65];
    for(size_t k=0;k<2;k++){for(size_t p=0;p<512u*512u;p++){memset(rgba+p*4,0,4);rgba[p*4+(k?2:0)]=255;rgba[p*4+3]=255;}hash_bytes(rgba,512u*512u*4u,pixel_hash[k]);encoded[k]=encode(rgba,512u*512u*4u);
        size_t bytes=0;char *ppm=NULL;FILE *out=open_memstream(&ppm,&bytes);CHECK(out);fputs("P6\n512 512\n255\n",out);for(size_t p=0;p<512u*512u;p++)CHECK(fwrite(rgba+p*4,1,3,out)==3);CHECK(!fclose(out));hash_bytes(ppm,bytes,ppm_hash[k]);free(ppm);}
    free(rgba);char *request=NULL;size_t bytes=0;FILE *out=open_memstream(&request,&bytes);CHECK(out);
    fputs("{\"contract_version\":1,\"op\":\"analyze_video_frames\",\"provider_id\":\"molmo2-native-v1\",\"start_us\":10000000,\"end_us\":12000000,\"prompt\":\"Describe the color change in this video in temporal order.\",\"max_output_tokens\":64,\"operation_id\":\"public-temporal-colors\",\"parent_operation_id\":\"source-video-qa\",\"frames\":[",out);
    for(size_t k=0;k<2;k++){if(k)fputc(',',out);fprintf(out,"{\"timestamp_us\":%u,\"width\":512,\"height\":512,\"rgba_base64\":",k?11375000u:10125000u);json_quote(out,encoded[k],strlen(encoded[k]));fputc('}',out);}fputs("]}",out);CHECK(!fclose(out));CHECK(bytes>2097152u&&bytes<25165824u);free(encoded[0]);free(encoded[1]);
    CHECK(snprintf(request_path,sizeof(request_path),"%s/request.json",argv[4])<(int)sizeof(request_path));file_write(request_path,request);
    char *body=invoke_temporal(argv,state,engine_pin,manifest_pin,request,0,"actual-temporal-test",NULL);Json j;parse(body,&j);char *kind=string_field(body,"kind"),*type=string_field(body,"artifact_type"),*text=string_field(body,"text");CHECK(!strcmp(kind,"derived")&&!strcmp(type,"video_observation"));free(kind);free(type);
    int frames=json_get(&j,0,"frames");CHECK(frames>=0&&j.tokens[frames].count==2);size_t k=0;for(int t=frames+1;t<j.tokens[frames].next;t=j.tokens[t].next,k++){size_t time=0,width=0,height=0;CHECK(json_uint(&j,json_get(&j,t,"timestamp_us"),&time)&&time==(k?11375000u:10125000u));CHECK(json_uint(&j,json_get(&j,t,"width"),&width)&&width==512&&json_uint(&j,json_get(&j,t,"height"),&height)&&height==512);char *pixels=json_string(&j,json_get(&j,t,"rgba_sha256")),*ppm=json_string(&j,json_get(&j,t,"prepared_sha256"));CHECK(pixels&&ppm&&!strcmp(pixels,pixel_hash[k])&&!strcmp(ppm,ppm_hash[k]));free(pixels);free(ppm);}
    int provenance=json_get(&j,0,"provenance");char *model=json_string(&j,json_get(&j,provenance,"model_sha256")),*engine=json_string(&j,json_get(&j,provenance,"engine_sha256")),*manifest_sha=json_string(&j,json_get(&j,provenance,"manifest_sha256"));CHECK(model&&engine&&manifest_sha&&!strcmp(model,model_pin)&&!strcmp(engine,engine_pin)&&!strcmp(manifest_sha,manifest_pin));free(model);free(engine);free(manifest_sha);CHECK(strstr(body,"sampled_video_analysis_uncertain")&&strstr(body,"molmo2_one_decimal_seconds"));
    char *lower=strdup(text);CHECK(lower);for(char *p=lower;*p;p++)*p=(char)tolower((unsigned char)*p);char *red=strstr(lower,"red"),*blue=strstr(lower,"blue");if(!red||!blue||red>=blue)fprintf(stderr,"observation: %s\n",text);CHECK(red&&blue&&red<blue);free(lower);free(text);
    CHECK(snprintf(response_path,sizeof(response_path),"%s/response.json",argv[4])<(int)sizeof(response_path));file_write(response_path,body);
    char *replay=invoke_temporal(argv,state,engine_pin,manifest_pin,request,1,"actual-temporal-test",NULL);CHECK(!strcmp(replay,body));free(replay);
    char *foreign=invoke_temporal(argv,state,engine_pin,manifest_pin,"{\"contract_version\":1,\"op\":\"operation_get\",\"operation_id\":\"public-temporal-colors\"}",1,"foreign-owner","unknown_operation");free(foreign);
    free(request);free(body);printf("PASS public actual temporal adapter: %u checks; two512px frames, >2MiB request, native ordered observation, per-frame pixel/PPM/time binding, model/runtime pins, foreign owner refusal and exact model/runtime-free replay; %s\n",checks,argv[4]);return 0;
}
