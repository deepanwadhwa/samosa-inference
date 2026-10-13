#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "temporal.h"
#include "temporal_package.h"
#include "whisper_fixture.h"
static char *request(size_t count,const char *pixels,size_t width,int duplicate){
    char *body=NULL;size_t bytes=0;FILE *out=open_memstream(&body,&bytes);CHECK(out);
    fputs("{\"contract_version\":1,\"op\":\"analyze_video_frames\",\"provider_id\":\"molmo2-native-v1\",\"start_us\":0,\"end_us\":2000000,\"prompt\":\"Describe the change.\",\"max_output_tokens\":64,\"frames\":[",out);
    for(size_t k=0;k<count;k++){if(k)fputc(',',out);fprintf(out,"{\"timestamp_us\":%zu,\"width\":%zu,\"height\":1,\"rgba_base64\":",duplicate?0:k*100000,width);json_quote(out,pixels,strlen(pixels));fputc('}',out);}fputs("]}",out);CHECK(!fclose(out));return body;
}
static void check_input(const InferenceConfig *cfg,char *body,const char *expected,size_t count){Json j;CHECK(json_parse(&j,body,strlen(body)));TemporalInput input;const char *error=temporal_input(cfg,&j,&input);if(expected)CHECK(error&&!strcmp(error,expected)&&!input.count&&!input.provider);else{CHECK(!error&&input.count==count&&strlen(input.input_sha256)==64&&strlen(input.prepared_sha256)==64);temporal_input_free(&input);}free(body);}
int main(void){
    const char *pin="0000000000000000000000000000000000000000000000000000000000000000";char identity[65];temporal_package_identity(pin,identity);
    TemporalConfig temporal={.self="/missing/self",.executable="/missing/engine",.model="/missing/model",.executable_sha256=pin,.manifest_sha256=pin,.model_sha256=identity};InferenceConfig cfg={.provider_id="molmo2-native-v1",.admission_state="/missing/state",.temporal=&temporal};CHECK(temporal_configured(&cfg));
    check_input(&cfg,request(1,"/wAA/w==",1,0),NULL,1);check_input(&cfg,request(16,"/wAA/w==",1,0),NULL,16);check_input(&cfg,request(17,"/wAA/w==",1,0),"invalid_request",0);check_input(&cfg,request(0,"/wAA/w==",1,0),"invalid_request",0);check_input(&cfg,request(2,"/wAA/w==",1,1),"invalid_request",0);check_input(&cfg,request(1,"/wAA/w==",513,0),"invalid_request",0);
    check_input(&cfg,request(1,"/wAA/wAA",1,0),"invalid_image",0);check_input(&cfg,request(1,"/wAA/x==",1,0),"invalid_image",0);check_input(&cfg,request(1,"/wAA/w=",1,0),"invalid_image",0);
    char *a=request(2,"/wAA/w==",1,0),*b=request(2,"AAD//w==",1,0);Json ja,jb;CHECK(json_parse(&ja,a,strlen(a))&&json_parse(&jb,b,strlen(b)));char *na=temporal_normalized(&cfg,&ja),*nb=temporal_normalized(&cfg,&jb);CHECK(na&&nb&&strcmp(na,nb)&&strlen(na)<2048&&!strstr(na,"/wAA/w=="));
    TemporalConfig other=temporal;other.executable_sha256="1111111111111111111111111111111111111111111111111111111111111111";InferenceConfig changed=cfg;changed.temporal=&other;char *nc=temporal_normalized(&changed,&ja);CHECK(nc&&strcmp(na,nc));free(nc);
    TemporalInput input;CHECK(!temporal_input(&cfg,&ja,&input));size_t bytes=0;char *ppm=temporal_ppm(&input.frames[0],&bytes);CHECK(ppm&&bytes==14&&!memcmp(ppm,"P6\n1 1\n255\n",11)&&(unsigned char)ppm[11]==255&&ppm[12]==0&&ppm[13]==0);free(ppm);temporal_input_free(&input);
    free(a);free(b);free(na);free(nb);puts("PASS temporal input: bounded16-frame canonical pixels, timestamps, padding refusal, model/runtime binding and normalized hash-only operation identity");return 0;
}
