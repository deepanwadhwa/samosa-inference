/* Shared original native HTTP fixture; no model execution claim. */
#ifndef SAMOSA_TEST_LLAMA_HTTP_FIXTURE_H
#define SAMOSA_TEST_LLAMA_HTTP_FIXTURE_H
#include "json.h"
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <time.h>
#include <unistd.h>
static int send_all(int fd,const void *data,size_t n) {
    const char *p=data;while(n){ssize_t got=send(fd,p,n,0);if(got<0&&errno==EINTR)continue;if(got<=0)return 0;p+=got;n-=(size_t)got;}return 1;
}
static int llama_fixture_server(const char *portfile,const char *mode,const char *logfile,unsigned port,const char *key) {
    signal(SIGPIPE,SIG_IGN);
    int listener=socket(AF_INET,SOCK_STREAM,0);if(listener<0)return 3;
    int reuse=1;if(setsockopt(listener,SOL_SOCKET,SO_REUSEADDR,&reuse,sizeof(reuse)))return 3;
    struct sockaddr_in address={.sin_family=AF_INET,.sin_addr.s_addr=htonl(INADDR_LOOPBACK)};
    address.sin_port=htons((uint16_t)port);
    if(bind(listener,(struct sockaddr*)&address,sizeof(address))||listen(listener,8))return 3;
    socklen_t n=sizeof(address);if(getsockname(listener,(struct sockaddr*)&address,&n))return 3;
    FILE *file=fopen(portfile,"w");if(!file)return 3;fprintf(file,"%u\n",ntohs(address.sin_port));fclose(file);
    unsigned completions=0,budget_blocked=0,embeddings=0;
    for(;;) {
        int fd=accept(listener,NULL,NULL);if(fd<0){if(errno==EINTR)continue;return 3;}
        char headers[4096];size_t used=0;int complete=0;
        while(used<sizeof(headers)-1){ssize_t got=recv(fd,headers+used,1,0);if(got!=1)break;used++;if(used>=4&&!memcmp(headers+used-4,"\r\n\r\n",4)){complete=1;break;}}
        headers[used]=0;if(!complete)return 3;
        char authorization[256];
        snprintf(authorization,sizeof(authorization),"Authorization: Bearer %s\r\n",key?key:"fixture-secret");
        int unauthorized=(key||!strcmp(mode,"auth"))&&!strstr(headers,authorization);
        int props=strstr(headers,"GET /props ")!=NULL;
        char *length=strstr(headers,"Content-Length:");if(!length&&!props)return 3;
        unsigned long bytes=length?strtoul(length+15,NULL,10):0;if(bytes>1048576)return 3;
        char *body=malloc(bytes+1);if(!body)return 3;
        size_t read_bytes=0;while(read_bytes<bytes){ssize_t got=recv(fd,body+read_bytes,bytes-read_bytes,0);if(got<=0)return 3;read_bytes+=(size_t)got;}body[bytes]=0;
        Json request;if(!props&&!json_parse(&request,body,bytes))return 3;
        file=fopen(logfile,"a");if(!file)return 3;fprintf(file,"%s\n",body);fclose(file);
        if(!strcmp(mode,"budget-aggregate")){struct timespec wait={0,150000000};nanosleep(&wait,NULL);}
        int stage=(!strcmp(mode,"budget-props")&&props)||(!strcmp(mode,"budget-template")&&strstr(headers,"POST /apply-template "))||(!strcmp(mode,"budget-tokenize")&&strstr(headers,"POST /tokenize "))||(!strcmp(mode,"budget-generate")&&strstr(headers,"POST /completion "));
        if(!strcmp(mode,"work-block")&&strstr(headers,"POST /completion ")){
            size_t predicted=0;stage=json_uint(&request,json_get(&request,0,"n_predict"),&predicted)&&predicted!=8;
        }
        if(!strcmp(mode,"embedding-work-block")&&strstr(headers,"POST /v1/embeddings "))stage=embeddings++>0;
        if(stage&&!budget_blocked++){
            if(!strcmp(mode,"embedding-work-block")){
                char marker[1024];snprintf(marker,sizeof(marker),"%s.blocked",logfile);
                FILE *started=fopen(marker,"w");if(!started)return 3;fclose(started);
            }
            struct timeval timeout={5,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));char byte;ssize_t closed=recv(fd,&byte,1,0);free(body);close(fd);if(closed!=0)return 3;continue;
        }
        char *reply=NULL;size_t reply_bytes=0;FILE *out=open_memstream(&reply,&reply_bytes);if(!out)return 3;
        if(!strcmp(mode,"oversize")){for(size_t i=0;i<1048577;i++)fputc('x',out);}
        else if(props){
            if(!strcmp(mode,"props-missing"))fputs("{}",out);
            else if(!strcmp(mode,"props-malformed"))fputs("[]",out);
            else fprintf(out,"{\"model_alias\":\"%s\",\"default_generation_settings\":{\"n_ctx\":%u}}",!strcmp(mode,"model-changed")?"foreign.gguf":"fixture-native.gguf",!strcmp(mode,"context-changed")?256:128);
        }
        else if(strstr(headers,"POST /apply-template ")) {
            int thinking=json_get(&request,0,"enable_thinking");
            if(thinking<0||request.tokens[thinking].type!=JT_BOOL||memcmp(body+request.tokens[thinking].begin,"false",5))return 3;
            int messages=json_get(&request,0,"messages");if(messages<0||request.tokens[messages].type!=JT_ARRAY||request.tokens[messages].count!=1)return 3;
            char *prompt=json_string(&request,json_get(&request,messages+1,"content"));if(!prompt)return 3;
            fputs("{\"prompt\":",out);size_t total=strlen(prompt)+10;char *rendered=malloc(total);if(!rendered)return 3;snprintf(rendered,total,"RENDERED %s",prompt);json_quote(out,rendered,strlen(rendered));fputc('}',out);free(prompt);free(rendered);
        } else if(strstr(headers,"POST /tokenize ")) {
            char *content=json_string(&request,json_get(&request,0,"content"));if(!content)return 3;int embedding=!strncmp(content,"task: search result | query: ",28)||!strncmp(content,"title: ",7);if(!embedding&&strncmp(content,"RENDERED ",9))return 3;free(content);
            int special=json_get(&request,0,"add_special"),parse=json_get(&request,0,"parse_special");if(special<0||parse<0||memcmp(body+request.tokens[special].begin,"true",4)||memcmp(body+request.tokens[parse].begin,embedding?"false":"true",embedding?5:4))return 3;
            fputs(!strcmp(mode,"negative-tokens")?"{\"tokens\":[-1,2]}":"{\"tokens\":[10,20,30]}",out);
        } else if(strstr(headers,"POST /v1/embeddings ")) {
            int input=json_get(&request,0,"input");if(input<0||request.tokens[input].type!=JT_ARRAY||request.tokens[input].count!=3)return 3;
            if(!strcmp(mode,"embedding-zero"))fputs("{\"data\":[{\"index\":0,\"embedding\":[0,0]}],\"usage\":{\"prompt_tokens\":3}}",out);
            else if(!strcmp(mode,"embedding-overflow"))fputs("{\"data\":[{\"index\":0,\"embedding\":[1e100,0]}],\"usage\":{\"prompt_tokens\":3}}",out);
            else if(!strcmp(mode,"embedding-clipped"))fputs("{\"data\":[{\"index\":0,\"embedding\":[1,0]}],\"usage\":{\"prompt_tokens\":2}}",out);
            else fputs("{\"data\":[{\"index\":0,\"embedding\":[1,0]}],\"usage\":{\"prompt_tokens\":3}}",out);

        } else if(strstr(headers,"POST /completion ")) {

            if(!strcmp(mode,"cancel-active")&&++completions==1){
                char marker[1024];snprintf(marker,sizeof(marker),"%s.blocked",logfile);
                FILE *started=fopen(marker,"w");if(!started)return 3;fclose(started);
                struct timeval timeout={10,0};setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout));
                char byte;ssize_t closed=recv(fd,&byte,1,0);
                fclose(out);free(reply);free(body);close(fd);
                if(closed!=0)return 3;
                continue;
            }
            int tokens=json_get(&request,0,"prompt");if(tokens<0||request.tokens[tokens].type!=JT_ARRAY||request.tokens[tokens].count!=3)return 3;
            size_t expected=10;for(int t=tokens+1;t<request.tokens[tokens].next;t=request.tokens[t].next){size_t value;if(!json_uint(&request,t,&value)||value!=expected)return 3;expected+=10;}
            if(!strcmp(mode,"probe-empty"))fputs("{\"content\":\"\",\"tokens_predicted\":1,\"timings\":{\"prompt_n\":2}}",out);
            else if(!strcmp(mode,"probe-zero"))fputs("{\"content\":\"ready\",\"tokens_predicted\":0,\"timings\":{\"prompt_n\":2}}",out);
            else fputs(!strcmp(mode,"usage-overflow")?"{\"content\":\"7319\",\"tokens_predicted\":999,\"timings\":{\"prompt_n\":3}}":"{\"content\":\"7319\",\"tokens_predicted\":4,\"timings\":{\"prompt_n\":2}}",out);
        } else return 3;
        fclose(out);char response_header[256];int h=snprintf(response_header,sizeof(response_header),"HTTP/1.1 %s\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",!strcmp(mode,"http-error")?"503 Unavailable":"200 OK",reply_bytes);
        if(!strcmp(mode,"probe-error")&&strstr(headers,"POST /completion "))h=snprintf(response_header,sizeof(response_header),"HTTP/1.1 503 Unavailable\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",reply_bytes);
        if(!strcmp(mode,"embedding-http-error")&&strstr(headers,"POST /v1/embeddings "))h=snprintf(response_header,sizeof(response_header),"HTTP/1.1 503 Unavailable\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",reply_bytes);
        if(unauthorized)h=snprintf(response_header,sizeof(response_header),"HTTP/1.1 401 Unauthorized\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",reply_bytes);
        int sent=send_all(fd,response_header,(size_t)h)&&send_all(fd,reply,reply_bytes);close(fd);free(body);free(reply);if(!sent&&strcmp(mode,"budget-aggregate"))return 3;
    }
}
#endif
