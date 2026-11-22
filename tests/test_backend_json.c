#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/backend_json.h>
#include <arpa/inet.h>
#include <errno.h>
#include <signal.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
static atomic_uint checks;
#define CHECK(x) do { atomic_fetch_add(&checks,1); if (!(x)) { fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x); exit(1); } } while (0)
static const char payload[]="{\"model\":\"original\",\"messages\":[{\"role\":\"user\",\"content\":\"hello é😀\"}],\"stream\":false}";
static void sleep_ms(long ms) { struct timespec t={ms/1000,(ms%1000)*1000000}; while (nanosleep(&t,&t)&&errno==EINTR) {} }
static int write_bytes(int fd,const char *p,size_t n) { while(n) { ssize_t got=send(fd,p,n,0); if(got<0&&errno==EINTR)continue; if(got<=0)return 0; p+=got;n-=(size_t)got; } return 1; }
typedef struct { SamosaBackendJson request; SamosaTurnGate gate; pthread_mutex_t mutex; int upstream,ready; unsigned requests,responses,errors; char error[256]; } Fixture;
static int probe(void *context) { return ((Fixture *)context)->ready; }
static void trace_payload(void *context,const char *event,const char *source,const char *data,size_t n) {
    Fixture *f=context;CHECK(f->upstream==-1&&!samosa_turn_generating(&f->gate));
    if(!strcmp(event,"internal_model_request")) { CHECK(!strcmp(source,"chat_backend")&&n==strlen(payload)&&!memcmp(data,payload,n));f->requests++; }
    else { CHECK(!strcmp(event,"internal_model_response")&&!strcmp(source,"chat_backend_http")&&n>0);f->responses++; }
}
static void trace_event(void *context,const char *event,const char *fields) {
    Fixture *f=context;CHECK(f->upstream==-1&&!samosa_turn_generating(&f->gate));CHECK(!strcmp(event,"internal_model_error"));f->errors++;CHECK(strlen(fields)<sizeof(f->error));strcpy(f->error,fields);
}
static void init(Fixture *f) {
    memset(f,0,sizeof(*f));f->upstream=-1;f->ready=1;CHECK(!pthread_mutex_init(&f->mutex,NULL));CHECK(samosa_turn_gate_init(&f->gate));
    f->request=(SamosaBackendJson){.timeout_seconds=1,.gate=&f->gate,.mutex=&f->mutex,.upstream_fd=&f->upstream,.probe=probe,.trace_payload=trace_payload,.trace_event=trace_event,.context=f};
}
static void done(Fixture *f) { CHECK(f->upstream==-1&&!samosa_turn_generating(&f->gate));samosa_turn_gate_destroy(&f->gate);CHECK(!pthread_mutex_destroy(&f->mutex)); }
static int listener(int *port) {
    int fd=socket(AF_INET,SOCK_STREAM,0);CHECK(fd>=0);struct sockaddr_in a={.sin_family=AF_INET,.sin_addr.s_addr=htonl(INADDR_LOOPBACK)};
    CHECK(!bind(fd,(struct sockaddr *)&a,sizeof(a))&&!listen(fd,4));socklen_t n=sizeof(a);CHECK(!getsockname(fd,(struct sockaddr *)&a,&n));*port=ntohs(a.sin_port);return fd;
}
static void serve(int listen_fd,int port,int mode,int notice) {
    int fd=accept(listen_fd,NULL,NULL);CHECK(fd>=0);CHECK(!close(listen_fd));struct timeval timeout={3,0};CHECK(!setsockopt(fd,SOL_SOCKET,SO_RCVTIMEO,&timeout,sizeof(timeout)));
    char header[1024];size_t n=0;while(n<sizeof(header)-1) { CHECK(recv(fd,header+n,1,0)==1);n++;if(n>=4&&!memcmp(header+n-4,"\r\n\r\n",4))break; }header[n]=0;
    char expected[512];int hn=snprintf(expected,sizeof(expected),"POST /v1/chat/completions HTTP/1.1\r\nHost: 127.0.0.1:%d\r\nContent-Type: application/json\r\nContent-Length: %zu\r\nConnection: close\r\n\r\n",port,strlen(payload));CHECK(hn>0&&!strcmp(header,expected));
    char input[sizeof(payload)];size_t used=0;while(used<strlen(payload)) { ssize_t got=recv(fd,input+used,strlen(payload)-used,0);CHECK(got>0);used+=(size_t)got; }input[used]=0;CHECK(!strcmp(input,payload));
    if(mode==5) { CHECK(write(notice,"r",1)==1);char byte;CHECK(recv(fd,&byte,1,0)==0); }
    else if(mode==4) { CHECK(write_bytes(fd,"HTTP/1.1 200 OK\r\n\r\n",19));for(int i=0;i<15;i++) { if(!write_bytes(fd,"x",1))break;sleep_ms(150); } }
    else if(mode==6) { CHECK(write_bytes(fd,"HTTP/1.1 200 OK\r\n\r\n",19));char block[65536];memset(block,'x',sizeof(block));for(unsigned i=0;i<72;i++)if(!write_bytes(fd,block,sizeof(block)))break; }
    else if(mode==7) { CHECK(write_bytes(fd,"HTTP/1.1 200 OK\r\n\r\npartial",26));sleep_ms(1300); }
    else { const char *reply=mode==0?"HTTP/1.1 200 OK\r\nContent-Length: 13\r\n\r\n{\"text\":\"é\"}":mode==1?"HTTP/1.1 503 Unavailable\r\n\r\nfailed":mode==2?"HTTP/1.1 200 OK\nmissing-separator":"";
        /* Split the normal header/body over real socket writes. */
        if(mode==0) { CHECK(write_bytes(fd,reply,5));sleep_ms(10);CHECK(write_bytes(fd,reply+5,strlen(reply)-5)); }
        else if(*reply)CHECK(write_bytes(fd,reply,strlen(reply)));
    }
    CHECK(!close(fd));_exit(0);
}
typedef struct { Fixture *fixture;int notice; } Cancellation;
static void *cancel_request(void *context) {
    Cancellation *c=context;char byte;CHECK(read(c->notice,&byte,1)==1);Fixture *f=c->fixture;CHECK(samosa_turn_generating(&f->gate));CHECK(!pthread_mutex_lock(&f->mutex));CHECK(f->upstream>=0);CHECK(!shutdown(f->upstream,SHUT_RDWR));CHECK(!pthread_mutex_unlock(&f->mutex));return NULL;
}
int main(int argc,char **argv) {
    if(argc==3&&!strcmp(argv[1],"--request")) {
        /* Native transport client for the existing retained-Qwen qualifier.
         * Its driver owns/pins/guards the engine; this client does not claim
         * lifecycle readiness or adopt an external PID. */
        Fixture f;init(&f);f.request.port=atoi(argv[2]);f.request.timeout_seconds=120;
        CHECK(f.request.port>0&&f.request.port<=65535);
        f.request.trace_payload=NULL;f.request.trace_event=NULL;
        char *body=samosa_backend_json(&f.request,"{\"messages\":[{\"role\":\"user\",\"content\":\"Say hello.\"}],\"max_tokens\":4,\"thinking\":false,\"stream\":false}");
        CHECK(body);puts(body);free(body);done(&f);return 0;
    }
    CHECK(argc==1);
    alarm(30);signal(SIGPIPE,SIG_IGN);Fixture f;init(&f);
    f.ready=0;CHECK(!samosa_backend_json(&f.request,payload));CHECK(strstr(f.error,"backend_loading")&&f.requests==1&&!f.responses&&f.errors==1);done(&f);
    init(&f);int fd=listener(&f.request.port);CHECK(!close(fd));CHECK(!samosa_backend_json(&f.request,payload));CHECK(strstr(f.error,"backend_connect_failed")&&f.errors==1);done(&f);
    for(int mode=0;mode<8;mode++) {
        init(&f);int pipefd[2];CHECK(!pipe(pipefd));fd=listener(&f.request.port);pid_t child=fork();CHECK(child>=0);
        if(!child) { close(pipefd[0]);serve(fd,f.request.port,mode,pipefd[1]); }
        CHECK(!close(fd)&&!close(pipefd[1]));pthread_t thread;Cancellation cancellation={&f,pipefd[0]};if(mode==5)CHECK(!pthread_create(&thread,NULL,cancel_request,&cancellation));
        char *body=samosa_backend_json(&f.request,payload);
        if(mode==0)CHECK(body&&!strcmp(body,"{\"text\":\"é\"}")&&f.responses==1&&!f.errors);
        else if(mode==6)CHECK(body&&strlen(body)>=SAMOSA_BACKEND_JSON_MAX_BODY&&strlen(body)<SAMOSA_BACKEND_JSON_MAX_BODY+SAMOSA_BACKEND_JSON_MAX_HEADER+65536&&!f.errors);
        else if(mode==7)CHECK(body&&!strcmp(body,"partial")&&!f.errors); /* Preserve original partial-on-recv-timeout behavior. */
        else { CHECK(!body);if(mode!=2)CHECK(f.errors==1&&strstr(f.error,"non_200_response"));else CHECK(f.responses==1&&!f.errors); }
        if(mode==5)CHECK(!pthread_join(thread,NULL));free(body);CHECK(!close(pipefd[0]));int status;CHECK(waitpid(child,&status,0)==child&&WIFEXITED(status)&&!WEXITSTATUS(status));CHECK(f.requests==1);done(&f);
    }
    init(&f);f.request.trace_payload=NULL;f.request.trace_event=NULL;f.ready=0;CHECK(!samosa_backend_json(&f.request,payload));done(&f);
    printf("PASS extracted backend JSON: %u checks; original wire/trace/status/header/partial/deadline/size/cancellation behavior and scope cleanup\n",atomic_load(&checks));return 0;
}
