#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "managed_stream.h"
#include "json.h"
#include "inference.h"
#include <pthread.h>
#include <poll.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <errno.h>
#define RESPONSE_MAX 524288u
typedef struct {
    pthread_t thread;atomic_int done;int occupied,started;
    char id[65],*request,*response;size_t length,sent;
    const ManagedStreamConfig *config;
} Worker;
static double clock_now(void){struct timespec t;if(clock_gettime(CLOCK_MONOTONIC,&t))return -1;return (double)t.tv_sec+(double)t.tv_nsec/1e9;}
static char *failure(const char *id,const char *code,size_t *length){
    char *out=malloc(256);if(!out)return NULL;
    int n=snprintf(out,256,"{\"contract_version\":1,\"request_id\":%s%s%s,\"response\":{\"contract_version\":1,\"error\":{\"code\":\"%s\"}}}\n",id?"\"":"",id?id:"null",id?"\"":"",code);
    if(n<0||n>=256){free(out);return NULL;}*length=(size_t)n;return out;
}
static void *dispatch(void *opaque){
    Worker *w=opaque;char *body=calloc(RESPONSE_MAX+1,1);FILE *out=body?fmemopen(body,RESPONSE_MAX+1,"w"):NULL;
    const char *e=out?NULL:"stream_resource_limit";long n=0;
    if(out){(void)w->config->dispatch(w->config->context,w->request,strlen(w->request),out);
        if(fflush(out)||ferror(out)||(n=ftell(out))<0||n>RESPONSE_MAX)e="response_too_large";
        if(fclose(out))e="response_too_large";}
    if(!e){Json j;if(!json_parse(&j,body,(size_t)n)||j.tokens[0].type!=JT_OBJECT)e="invalid_component_response";}
    if(!e){while(n&&isspace((unsigned char)body[n-1]))n--;
        size_t capacity=(size_t)n+160;w->response=malloc(capacity);
        if(!w->response)e="stream_resource_limit";
        else{int prefix=snprintf(w->response,capacity,"{\"contract_version\":1,\"request_id\":\"%s\",\"response\":",w->id);
            memcpy(w->response+prefix,body,(size_t)n);memcpy(w->response+prefix+n,"}\n",2);w->length=(size_t)prefix+(size_t)n+2;}}
    if(e)w->response=failure(w->id,e,&w->length);
    free(body);free(w->request);w->request=NULL;atomic_store_explicit(&w->done,1,memory_order_release);return NULL;
}
static int envelope(const char *frame,size_t length,char id[65],char **request,int *control){
    Json j;size_t version;*request=NULL;
    if(!json_parse(&j,frame,length)||j.tokens[0].type!=JT_OBJECT||j.tokens[0].count!=3||
        !json_uint(&j,json_get(&j,0,"contract_version"),&version)||version!=1)return 0;
    char *name=json_string(&j,json_get(&j,0,"request_id"));int body=json_get(&j,0,"request");
    if(!name||!name[0]||strlen(name)>64||body<0||j.tokens[body].type!=JT_OBJECT){free(name);return 0;}
    for(char *p=name;*p;p++)if(!isalnum((unsigned char)*p)&&!strchr("-_.:",*p)){free(name);return 0;}
    strcpy(id,name);free(name);char *op=json_string(&j,json_get(&j,body,"op"));
    *control=op&&(!strcmp(op,"operation_cancel")||!strcmp(op,"operation_get")||!strcmp(op,"describe"));free(op);
    size_t n=j.tokens[body].end-j.tokens[body].begin;*request=malloc(n+1);if(!*request)return 0;
    memcpy(*request,frame+j.tokens[body].begin,n);(*request)[n]=0;return 1;
}
static void synthetic(Worker *w,const char *id,const char *code){
    w->occupied=1;w->started=0;w->sent=0;w->response=failure(id,code,&w->length);atomic_store(&w->done,1);
}
int managed_stream_run(const ManagedStreamConfig *c){
    if(!c||!c->dispatch||!c->stop)return 1;
    Worker *workers=calloc(9,sizeof(*workers));char *frame=malloc(INFERENCE_REQUEST_MAX+1);
    int input_flags=fcntl(c->input,F_GETFL),output_flags=fcntl(c->output,F_GETFL);
    if(!workers||!frame||input_flags<0||output_flags<0||fcntl(c->input,F_SETFL,input_flags|O_NONBLOCK)||fcntl(c->output,F_SETFL,output_flags|O_NONBLOCK)){
        if(input_flags>=0)(void)fcntl(c->input,F_SETFL,input_flags);
        if(output_flags>=0)(void)fcntl(c->output,F_SETFL,output_flags);
        free(workers);free(frame);return 1;}
    for(int i=0;i<9;i++)atomic_init(&workers[i].done,0);
    size_t used=0,chunk_size=0,cursor=0;unsigned char chunk[4096];int closing=0,rc=0,writing=-1;
    double deadline=0,output_progress=clock_now();
    for(;;){double now=clock_now();if(now<0)return 2;
        if(!closing&&(atomic_load(c->stop)||(c->interrupted&&c->interrupted(c->interruption))))closing=1;
        if(closing&&!deadline){atomic_store(c->stop,1);deadline=now+5;}
        int occupied=0;
        for(int i=0;i<9;i++){Worker *w=workers+i;if(!w->occupied)continue;occupied++;
            if(atomic_load_explicit(&w->done,memory_order_acquire)&&!w->response){rc=1;closing=1;}
            if(writing<0&&atomic_load_explicit(&w->done,memory_order_acquire)&&w->response){writing=i;output_progress=now;}}
        if(closing&&!occupied)break;
        if(closing&&now>=deadline){rc=2;break;}
        if(writing>=0){Worker *w=workers+writing;ssize_t n=write(c->output,w->response+w->sent,w->length-w->sent);
            if(n>0){w->sent+=(size_t)n;output_progress=now;
                if(w->sent==w->length){if(w->started)pthread_join(w->thread,NULL);free(w->response);w->response=NULL;w->occupied=0;w->started=0;writing=-1;}}
            else if(n<0&&errno!=EINTR&&errno!=EAGAIN&&errno!=EWOULDBLOCK){rc=1;closing=1;}
            if(now-output_progress>=2){rc=1;closing=1;}
        }
        /* A closed/stalled output cannot deliver outstanding results; cancel
         * workers, join those completed, and let the owner reconcile durable ops. */
        if(closing&&rc){for(int i=0;i<9;i++){Worker *w=workers+i;if(w->occupied&&atomic_load_explicit(&w->done,memory_order_acquire)){
                    if(w->started)pthread_join(w->thread,NULL);
                    free(w->response);w->response=NULL;w->occupied=0;w->started=0;}}
            writing=-1;}
        if(!closing&&!workers[8].occupied){
            if(cursor==chunk_size){ssize_t n=read(c->input,chunk,sizeof(chunk));
                if(n>0){chunk_size=(size_t)n;cursor=0;}
                else if(!n){closing=1;if(used){synthetic(workers+8,NULL,"incomplete_frame");rc=1;}}
                else if(errno!=EAGAIN&&errno!=EWOULDBLOCK&&errno!=EINTR){closing=1;rc=1;}}
            while(!closing&&!workers[8].occupied&&cursor<chunk_size){unsigned char ch=chunk[cursor++];
                if(ch!='\n'){if(used==INFERENCE_REQUEST_MAX){synthetic(workers+8,NULL,"request_too_large");closing=1;rc=1;}else frame[used++]=(char)ch;continue;}
                char id[65],*request=NULL;int control=0;
                if(!envelope(frame,used,id,&request,&control)){synthetic(workers+8,NULL,"invalid_stream_frame");used=0;continue;}used=0;
                int duplicate=0;for(int i=0;i<8;i++)if(workers[i].occupied&&!strcmp(workers[i].id,id))duplicate=1;
                if(duplicate){free(request);synthetic(workers+8,NULL,"duplicate_request_id");continue;}
                int first=control?6:0,last=control?8:6,index=-1;
                for(int i=first;i<last;i++)if(!workers[i].occupied){index=i;break;}
                if(index<0){free(request);synthetic(workers+8,id,"stream_busy");continue;}
                Worker *w=workers+index;w->occupied=1;w->started=0;w->sent=0;w->request=request;w->config=c;strcpy(w->id,id);atomic_store(&w->done,0);
                pthread_attr_t attributes;int initialized=pthread_attr_init(&attributes)==0;
                int created=initialized&&!pthread_attr_setstacksize(&attributes,4u*1048576u)&&!pthread_create(&w->thread,&attributes,dispatch,w);
                if(initialized)pthread_attr_destroy(&attributes);
                if(!created){free(request);w->request=NULL;synthetic(w,id,"stream_resource_limit");}
                else w->started=1;
            }
        }
        struct pollfd fds[2]={{!closing&&!workers[8].occupied?c->input:-1,POLLIN,0},{closing&&rc?-1:c->output,writing>=0?POLLOUT:0,0}};
        /* Completed workers have no pipe wakeup;20ms bounds collection latency.
         * Nonblocking reads above also observe Darwin FIFO EOF without POLLHUP. */
        if(poll(fds,2,20)<0&&errno!=EINTR){rc=1;closing=1;}
        if(fds[1].revents&(POLLERR|POLLHUP|POLLNVAL)){rc=1;closing=1;}
    }
    if(rc==2)return 2;
    (void)fcntl(c->input,F_SETFL,input_flags);(void)fcntl(c->output,F_SETFL,output_flags);
    free(workers);free(frame);return rc;
}
