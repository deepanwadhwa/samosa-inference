#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "managed_host.h"
#include "embedding.h"
#include "inference.h"
#include "json.h"
#include "sha256.h"
#include "managed_stream.h"
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include <poll.h>
_Static_assert(ATOMIC_INT_LOCK_FREE==2,"Signal cancellation requires lock-free atomics");
static atomic_int stopping;
static atomic_int stream_stop;
static void stop_signal(int number){(void)number;atomic_store(&stopping,1);}
static int cancelled(void *unused){(void)unused;return atomic_load(&stopping)||atomic_load(&stream_stop);}
static int dispatch_request(void *context,const char *request,size_t length,FILE *out){return inference_dispatch_config(context,request,length,out);}
typedef struct {ManagedProvider *provider;} StreamOwner;
static int interrupted(void *context){StreamOwner *owner=context;struct pollfd terminal={owner->provider->activation.process.response,POLLIN|POLLHUP,0};return atomic_load(&stopping)||poll(&terminal,1,0)!=0;}
static void error(const char *code){fputs("{\"contract_version\":1,\"error\":{\"code\":",stdout);json_quote(stdout,code,strlen(code));fputs("}}\n",stdout);fflush(stdout);}
int main(int argc,char **argv){
    int guardian=engine_guardian_dispatch(argc,argv);if(guardian>=0)return guardian;
    if((argc!=3&&argc!=4)||strcmp(argv[1],"--config")||(argc==4&&strcmp(argv[3],"--single")&&strcmp(argv[3],"--single-current"))){fputs("usage: samosa-inference-managed --config ABSOLUTE_PRIVATE_JSON [--single | --single-current]\n",stderr);return 2;}
    int single=argc==4;
    int advance_current=single&&!strcmp(argv[3],"--single-current");
    struct sigaction action={.sa_handler=stop_signal};sigemptyset(&action.sa_mask);sigaction(SIGTERM,&action,NULL);sigaction(SIGINT,&action,NULL);signal(SIGPIPE,SIG_IGN);
    ManagedHost host;int rc=2;
    const char *started=managed_host_start(argv[2],argv[0],advance_current,cancelled,NULL,&host);
    if(started){error(started);return 2;}
    if(single){char *request=malloc(INFERENCE_REQUEST_MAX+1);
        if(!request){error("managed_resource_limit");goto done;}
        size_t bytes=fread(request,1,INFERENCE_REQUEST_MAX+1,stdin);
        rc=ferror(stdin)?2:inference_dispatch_config(&host.provider.inference,request,bytes,stdout);
        free(request);goto done;}
    fputs("{\"contract_version\":1,\"event\":\"provider_started\",\"provider_id\":",stdout);json_quote(stdout,host.identity,strlen(host.identity));
    fprintf(stdout,",\"activation_revision\":%u,\"runtime_policy\":\"system-runtime-v1\"}\n",host.provider.activation.revision);fflush(stdout);
    StreamOwner stream_owner={&host.provider};
    ManagedStreamConfig stream={.input=STDIN_FILENO,.output=STDOUT_FILENO,
        .dispatch=dispatch_request,.context=&host.provider.inference,.interrupted=interrupted,.interruption=&stream_owner,.stop=&stream_stop};
    rc=managed_stream_run(&stream);
    if(rc==2)_exit(1); /* Guardian lifeline closes; never free live worker state. */
done:
    {const char *e=managed_host_close(&host);if(e){error(e);rc=1;}}
    return rc;
}
