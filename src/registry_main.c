#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "registry_host.h"
#include "engine_process.h"
#include "speech_worker.h"
#include "budget.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <poll.h>
#include <signal.h>
#include <unistd.h>

typedef struct {const char *provider,*operation;size_t sequence,offset;int broken;} SpeechEvents;
static int stream_disconnected(void *context){
    /* Darwin reports a closed pipe reader only when writability is requested. */
    SpeechEvents *s=context;struct pollfd watch={STDOUT_FILENO,POLLOUT,0};
    int ready=poll(&watch,1,0);
    if(ready>0&&(watch.revents&(POLLERR|POLLHUP|POLLNVAL))){s->broken=1;return 1;}
    return 0;
}
static const char *event_write(const char *body,size_t length,const char *(*control)(void *),void *context,size_t *delivered) {
    *delivered=0;
    if(length>16384)return "stream_event_limit";
    int flags=fcntl(STDOUT_FILENO,F_GETFL);
    if(flags<0||fcntl(STDOUT_FILENO,F_SETFL,flags|O_NONBLOCK))return "stream_output_failed";
    const char *e=NULL,*aborted=NULL;size_t at=0;double deadline=inference_now()+2;
    /* Generation supplies its full tracked deadline/cancellation hook. The
     * terminal frame has a separate bounded delivery window after cleanup. */
    while(at<length){
        e=aborted?(inference_now()>=deadline?"stream_delivery_timeout":NULL):
            control?control(context):(inference_now()>=deadline?"stream_delivery_timeout":NULL);
        if(e){
            if(control&&!aborted&&at){aborted=e;e=NULL;deadline=inference_now()+2;}
            else break;
        }
        /* Finish an already-started JSON frame within a two-second grace
         * window before returning cancellation. Never concatenate a terminal
         * frame onto an incomplete PCM frame when delivery itself fails. */
        ssize_t n=write(STDOUT_FILENO,body+at,length-at);
        if(n>0){at+=(size_t)n;continue;}
        if(n<0&&errno==EINTR)continue;
        if(n<0&&(errno==EAGAIN||errno==EWOULDBLOCK)){
            struct pollfd watch={STDOUT_FILENO,POLLOUT,0};int ready=poll(&watch,1,10);
            if(ready<0&&errno==EINTR)continue;
            if(ready>=0)continue;
        }
        e="stream_output_failed";break;
    }
    *delivered=at;
    if(fcntl(STDOUT_FILENO,F_SETFL,flags)&&!e)e="stream_output_failed";
    return aborted&&e&&!strcmp(e,"stream_delivery_timeout")?aborted:e?e:aborted;
}
static void event_ids(FILE *f,const SpeechEvents *s){
    fprintf(f,",\"sequence\":%zu,\"provider_id\":",s->sequence);json_quote(f,s->provider?s->provider:"",s->provider?strlen(s->provider):0);
    fputs(",\"operation_id\":",f);json_quote(f,s->operation?s->operation:"",s->operation?strlen(s->operation):0);
}
static const char *speech_event(void *context,const unsigned char *pcm,size_t bytes,int rate,
    const char *(*control)(void *),void *control_context){
    SpeechEvents *s=context;char *body=NULL;size_t length=0;FILE *f=open_memstream(&body,&length);
    if(!f)return "stream_resource_limit";
    fputs("{\"contract_version\":1,\"event\":\"speech_pcm\",\"provisional\":true",f);event_ids(f,s);
    fprintf(f,",\"format\":\"pcm_s16le\",\"channels\":1,\"sample_rate\":%d,\"offset\":%zu,\"bytes\":%zu,\"pcm_base64\":\"",rate,s->offset,bytes);
    static const char abc[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    for(size_t i=0;i<bytes;i+=3){unsigned v=(unsigned)pcm[i]<<16;if(i+1<bytes)v|=(unsigned)pcm[i+1]<<8;if(i+2<bytes)v|=pcm[i+2];
        fputc(abc[v>>18],f);fputc(abc[(v>>12)&63],f);fputc(i+1<bytes?abc[(v>>6)&63]:'=',f);fputc(i+2<bytes?abc[v&63]:'=',f);}
    fputs("\"}\n",f);if(fclose(f)){free(body);return "stream_resource_limit";}
    size_t delivered=0;const char *e=event_write(body,length,control,control_context,&delivered);free(body);
    if(delivered==length){s->sequence++;s->offset+=bytes;}
    else if(delivered||(e&&!strcmp(e,"stream_output_failed")))s->broken=1;
    return e;
}
static int stream_dispatch(InferenceRegistryHost *host,const char *request,size_t bytes){
    SpeechEvents events={0};Json j;char *provider=NULL,*operation=NULL,*op=NULL;
    if(bytes<=INFERENCE_REQUEST_MAX&&json_parse(&j,request,bytes)){
        provider=json_string(&j,json_get(&j,0,"provider_id"));operation=json_string(&j,json_get(&j,0,"operation_id"));op=json_string(&j,json_get(&j,0,"op"));
    }
    events.provider=provider&&strlen(provider)<=128?provider:NULL;events.operation=operation&&strlen(operation)<=128?operation:NULL;
    for(size_t i=0;i<host->registry.count;i++)if(host->registry.providers[i].speech){host->speeches[i].pcm_sink=speech_event;host->speeches[i].pcm_context=&events;
        host->providers[i].cancelled=stream_disconnected;host->providers[i].cancellation=&events;}
    char *response=NULL;size_t length=0;FILE *f=open_memstream(&response,&length);int rc=2;
    if(f){
        if(!op||!strcmp(op,"speech_generate"))rc=inference_registry_dispatch(&host->registry,request,bytes,f);
        else{fputs("{\"contract_version\":1,\"error\":{\"code\":\"unsupported_stream_operation\"}}\n",f);rc=1;}
        if(fclose(f))rc=2;
    }
    char *terminal=NULL;size_t terminal_length=0;f=rc!=2&&!events.broken?open_memstream(&terminal,&terminal_length):NULL;
    if(f){fprintf(f,"{\"contract_version\":1,\"event\":\"%s\"",rc?"failed":"completed");event_ids(f,&events);
        if(!rc)fprintf(f,",\"replayed\":%s",events.sequence?"false":"true");
        /* Stored receipts end in a newline; the envelope itself must occupy
         * one line without changing the retained response's JSON value. */
        while(length&&isspace((unsigned char)response[length-1]))length--;
        fputs(",\"response\":",f);fwrite(response,1,length,f);fputs("}\n",f);
        size_t delivered=0;if(fclose(f)||event_write(terminal,terminal_length,NULL,NULL,&delivered))rc=2;
    }else rc=2;
    free(terminal);free(response);free(provider);free(operation);free(op);return rc;
}

int main(int argc,char **argv) {
    int worker=speech_worker_dispatch(argc,argv);if(worker>=0)return worker;
    int bridge=summary_bridge_dispatch(argc,argv);if(bridge>=0)return bridge;
    bridge=vision_bridge_dispatch(argc,argv);if(bridge>=0)return bridge;
    int guardian=engine_guardian_dispatch(argc,argv);if(guardian>=0)return guardian;
    int stream=argc==4&&!strcmp(argv[3],"--stream");
    if((argc!=3&&!stream)||strcmp(argv[1],"--config")){fputs("usage: samosa-inference-providers --config ABSOLUTE_PRIVATE_JSON [--stream]\n",stderr);return 2;}
    char self[PATH_MAX];if(!realpath(argv[0],self))return 2;
    InferenceRegistryHost host;const char *error=inference_registry_host_load(argv[2],self,&host);
    if(error){fprintf(stdout,"{\"contract_version\":1,\"error\":{\"code\":\"%s\"}}\n",error);return 2;}
    signal(SIGPIPE,SIG_IGN);
    char *request=malloc(INFERENCE_REQUEST_MAX+1);if(!request){inference_registry_host_free(&host);return 2;}
    size_t bytes=fread(request,1,INFERENCE_REQUEST_MAX+1,stdin);
    int rc=ferror(stdin)?2:stream?stream_dispatch(&host,request,bytes):inference_registry_dispatch(&host.registry,request,bytes,stdout);
    free(request);inference_registry_host_free(&host);return rc;
}
