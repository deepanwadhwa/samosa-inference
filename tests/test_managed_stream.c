#define _POSIX_C_SOURCE 200809L
#include "managed_stream.h"
#include "json.h"
#include <unistd.h>
#include <sys/wait.h>
#include <poll.h>
#include <signal.h>
#include <time.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL stream %d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct {atomic_int stop,cancel,active;} Fixture;
static int dispatch(void *opaque,const char *request,size_t length,FILE *out){
    volatile unsigned char nested_stack[2097152];nested_stack[0]=1;nested_stack[sizeof(nested_stack)-1]=2;
    if(nested_stack[0]+nested_stack[sizeof(nested_stack)-1]!=3)return 1;
    Fixture *f=opaque;Json j;if(!json_parse(&j,request,length))return 1;char *op=json_string(&j,json_get(&j,0,"op"));if(!op)return 1;
    if(!strcmp(op,"hold")){atomic_fetch_add(&f->active,1);while(!atomic_load(&f->cancel)&&!atomic_load(&f->stop)){struct timespec pause={0,1000000};nanosleep(&pause,NULL);}
        atomic_fetch_sub(&f->active,1);fputs("{\"contract_version\":1,\"status\":\"cancelled\"}",out);}
    else if(!strcmp(op,"operation_get"))fprintf(out,"{\"contract_version\":1,\"active\":%d}",atomic_load(&f->active));
    else if(!strcmp(op,"operation_cancel")){atomic_store(&f->cancel,1);fputs("{\"contract_version\":1,\"cancel_requested\":true}",out);}
    else if(!strcmp(op,"ignore")){struct timespec pause={8,0};nanosleep(&pause,NULL);fputs("{\"contract_version\":1}",out);}
    else if(!strcmp(op,"large")||!strcmp(op,"oversized")){fputs("{\"contract_version\":1,\"value\":\"",out);
        for(int i=0;i<(!strcmp(op,"large")?400000:600000);i++)fputc('a',out);
        fputs("\"}",out);}
    else fputs("{\"contract_version\":1}",out);
    free(op);return 0;
}
typedef struct {pid_t child;int input,output;} Server;
static Server server(void){int input[2],output[2];CHECK(!pipe(input));CHECK(!pipe(output));pid_t child=fork();CHECK(child>=0);
    if(!child){close(input[1]);close(output[0]);signal(SIGPIPE,SIG_IGN);Fixture f={0};ManagedStreamConfig c={.input=input[0],.output=output[1],.dispatch=dispatch,.context=&f,.stop=&f.stop};
        int rc=managed_stream_run(&c);close(input[0]);close(output[1]);if(rc==2)_exit(2);exit(rc);}
    CHECK(!close(input[0]));CHECK(!close(output[1]));return (Server){child,input[1],output[0]};}
static void send_frame(Server s,const char *id,const char *op){char frame[256];int n=snprintf(frame,sizeof(frame),"{\"contract_version\":1,\"request_id\":\"%s\",\"request\":{\"contract_version\":1,\"op\":\"%s\"}}\n",id,op);CHECK(write(s.input,frame,(size_t)n)==n);}
static char *receive(Server s){char *line=malloc(2048);CHECK(line);size_t n=0;while(n<2047){struct pollfd fd={s.output,POLLIN|POLLHUP,0};CHECK(poll(&fd,1,3000)>0);
        char byte;CHECK(read(s.output,&byte,1)==1);line[n++]=byte;if(byte=='\n'){line[n]=0;return line;}}CHECK(0);return NULL;}
static void finish(Server s,int expected){CHECK(!close(s.input));int status;CHECK(waitpid(s.child,&status,0)==s.child);CHECK(WIFEXITED(status)&&WEXITSTATUS(status)==expected);CHECK(!close(s.output));}
int main(void){Server s=server();for(int i=0;i<6;i++){char id[16];snprintf(id,sizeof(id),"work%d",i);send_frame(s,id,"hold");}
    int active=0;for(int i=0;i<100;i++){send_frame(s,"status","operation_get");char *line=receive(s);Json j;CHECK(json_parse(&j,line,strlen(line)));size_t n;
        CHECK(json_uint(&j,json_get(&j,json_get(&j,0,"response"),"active"),&n));free(line);if(n==6){active=1;break;}struct timespec pause={0,1000000};nanosleep(&pause,NULL);}CHECK(active);
    send_frame(s,"overflow","hold");char *line=receive(s);CHECK(strstr(line,"stream_busy")&&strstr(line,"overflow"));free(line);
    send_frame(s,"work0","hold");line=receive(s);CHECK(strstr(line,"duplicate_request_id")&&strstr(line,"\"request_id\":null"));free(line);
    send_frame(s,"cancel","operation_cancel");unsigned seen=0;int cancellation=0;
    for(int i=0;i<7;i++){line=receive(s);Json j;CHECK(json_parse(&j,line,strlen(line)));char *id=json_string(&j,json_get(&j,0,"request_id"));CHECK(id);
        if(!strcmp(id,"cancel")){CHECK(strstr(line,"cancel_requested"));cancellation++;}
        else{CHECK(strlen(id)==5&&!strncmp(id,"work",4)&&id[4]>='0'&&id[4]<='5');unsigned bit=1u<<(id[4]-'0');CHECK(!(seen&bit));seen|=bit;CHECK(strstr(line,"cancelled"));}
        free(id);free(line);}CHECK(cancellation==1&&seen==63);
    send_frame(s,"big","oversized");line=receive(s);CHECK(strstr(line,"response_too_large")&&strstr(line,"big"));free(line);finish(s,0);
    s=server();send_frame(s,"held","hold");send_frame(s,"status","operation_get");line=receive(s);CHECK(strstr(line,"status"));free(line);
    CHECK(!close(s.input));line=receive(s);CHECK(strstr(line,"held")&&strstr(line,"cancelled"));free(line);int status;CHECK(waitpid(s.child,&status,0)==s.child);CHECK(WIFEXITED(status)&&!WEXITSTATUS(status));CHECK(!close(s.output));
    s=server();send_frame(s,"stalled","large");struct timespec pause={3,0};nanosleep(&pause,NULL);finish(s,1);
    s=server();send_frame(s,"ignored","ignore");finish(s,2);
    printf("PASS managed stream: %u checks; concurrent work, reserved control, correlation, capacity, EOF cancellation and bounded output stall\n",checks);return 0;
}
