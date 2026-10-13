#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#include "whisper_fixture.h"
#include <arpa/inet.h>
static void frame(FILE *out,const char *body){uint32_t bytes=htonl((uint32_t)strlen(body));CHECK(fwrite(&bytes,1,4,out)==4);CHECK(fwrite(body,1,strlen(body),out)==strlen(body));}
static char *reply(FILE *in){uint32_t bytes;CHECK(fread(&bytes,1,4,in)==4);size_t n=ntohl(bytes);CHECK(n&&n<32768);char *body=calloc(n+1,1);CHECK(body&&fread(body,1,n,in)==n);return body;}
int main(int argc,char **argv){
    CHECK(argc==4&&argv[1][0]=='/'&&argv[2][0]=='/'&&argv[3][0]=='/');CHECK(!mkdir(argv[3],0700));
    char red[4096],blue[4096],request_path[4096],result_path[4096];
    CHECK(snprintf(red,sizeof(red),"%s/red.ppm",argv[3])<(int)sizeof(red));CHECK(snprintf(blue,sizeof(blue),"%s/blue.ppm",argv[3])<(int)sizeof(blue));
    CHECK(snprintf(request_path,sizeof(request_path),"%s/request.frame",argv[3])<(int)sizeof(request_path));CHECK(snprintf(result_path,sizeof(result_path),"%s/result.frame",argv[3])<(int)sizeof(result_path));
    for(size_t k=0;k<2;k++){FILE *out=fopen(k?blue:red,"wb");CHECK(out);CHECK(fputs("P6\n64 64\n255\n",out)>=0);unsigned char pixel[3]={0};pixel[k?2:0]=255;for(size_t p=0;p<4096;p++)CHECK(fwrite(pixel,1,3,out)==3);CHECK(!fclose(out));}
    FILE *input=fopen(request_path,"wb"),*output=fopen(result_path,"w+b");CHECK(input&&output);
    frame(input,"{\"command\":\"hello\"}");
    char request[12288];
    CHECK(snprintf(request,sizeof(request),"{\"command\":\"analyze\",\"id\":\"bad-times\",\"media_kind\":\"video_frames\",\"media_paths\":[\"%s\",\"%s\"],\"timestamps_seconds\":[10,10],\"prompt\":\"Name the colors in temporal order.\",\"max_tokens\":64}",red,blue)<(int)sizeof(request));frame(input,request);
    CHECK(snprintf(request,sizeof(request),"{\"command\":\"analyze\",\"id\":\"temporal-colors\",\"media_kind\":\"video_frames\",\"media_paths\":[\"%s\",\"%s\"],\"timestamps_seconds\":[10.125,11.375],\"prompt\":\"Describe the color change in this video in temporal order.\",\"max_tokens\":64}",red,blue)<(int)sizeof(request));frame(input,request);
    frame(input,"{\"command\":\"quit\"}");CHECK(!fclose(input));
    int request_fd=open(request_path,O_RDONLY);CHECK(request_fd>=0);
    pid_t child=fork();CHECK(child>=0);if(!child){if(setpgid(0,0)||dup2(request_fd,0)<0||dup2(fileno(output),1)<0)_exit(2);execl(argv[1],argv[1],argv[2],(char*)NULL);_exit(2);}close(request_fd);
    struct timespec start,now;CHECK(!clock_gettime(CLOCK_MONOTONIC,&start));int status=0;
    while(waitpid(child,&status,WNOHANG)==0){CHECK(!clock_gettime(CLOCK_MONOTONIC,&now));if(now.tv_sec-start.tv_sec>120){kill(-child,SIGKILL);CHECK(waitpid(child,&status,0)==child);CHECK(0);}delay();}
    CHECK(WIFEXITED(status)&&!WEXITSTATUS(status));CHECK(!fseek(output,0,SEEK_SET));
    char *hello=reply(output);CHECK(strstr(hello,"samosa.multimodal.v1"));free(hello);
    char *bad=reply(output);CHECK(strstr(bad,"molmo2_media_invalid"));free(bad);
    char *body=reply(output);Json j;parse(body,&j);char *state=string_field(body,"status"),*id=string_field(body,"id"),*text=string_field(body,"observation");CHECK(!strcmp(state,"ok")&&!strcmp(id,"temporal-colors"));
    size_t frames=0,tokens=0;CHECK(json_uint(&j,json_get(&j,0,"frames"),&frames)&&frames==2);CHECK(json_uint(&j,json_get(&j,0,"generated_tokens"),&tokens)&&tokens&&tokens<=64);
    int times=json_get(&j,0,"sample_times_seconds");CHECK(times>=0&&j.tokens[times].count==2);int first=times+1,second=j.tokens[first].next;CHECK(j.tokens[first].end-j.tokens[first].begin==6&&!memcmp(j.text+j.tokens[first].begin,"10.125",6));CHECK(j.tokens[second].end-j.tokens[second].begin==6&&!memcmp(j.text+j.tokens[second].begin,"11.375",6));
    char *lower=strdup(text);CHECK(lower);for(char *p=lower;*p;p++)*p=(char)tolower((unsigned char)*p);char *red_word=strstr(lower,"red"),*blue_word=strstr(lower,"blue");if(!red_word||!blue_word||red_word>=blue_word)fprintf(stderr,"observation: %s\n",text);CHECK(red_word&&blue_word&&red_word<blue_word);free(lower);
    char json_path[4096];CHECK(snprintf(json_path,sizeof(json_path),"%s/observation.json",argv[3])<(int)sizeof(json_path));file_write(json_path,body);free(body);free(text);free(state);free(id);
    char *quit=reply(output);CHECK(strstr(quit,"ok"));free(quit);CHECK(fgetc(output)==EOF&&!ferror(output));CHECK(!fclose(output));
    printf("PASS actual native temporal engine: %u checks; duplicate timestamp refusal, two timestamped prepared frames, ordered red-to-blue observation; %s\n",checks,argv[3]);return 0;
}
