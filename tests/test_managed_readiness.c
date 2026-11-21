#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "managed_provider.h"
#include "budget.h"
#include "sha256.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static unsigned free_port(void) {
    int fd=socket(AF_INET,SOCK_STREAM,0);CHECK(fd>=0);
    struct sockaddr_in a={.sin_family=AF_INET,.sin_addr.s_addr=htonl(INADDR_LOOPBACK)};
    CHECK(!bind(fd,(struct sockaddr*)&a,sizeof(a)));socklen_t n=sizeof(a);
    CHECK(!getsockname(fd,(struct sockaddr*)&a,&n));CHECK(!close(fd));return ntohs(a.sin_port);
}
static unsigned long long copy(const char *from,const char *to,char pin[65]) {
    int in=open(from,O_RDONLY),out=open(to,O_CREAT|O_EXCL|O_WRONLY,0700);CHECK(in>=0&&out>=0);
    Sha256 hash;sha256_init(&hash);unsigned long long total=0;unsigned char block[65536];
    for(;;){ssize_t n=read(in,block,sizeof(block));CHECK(n>=0);if(!n)break;
        CHECK(write(out,block,(size_t)n)==n);sha256_update(&hash,block,(size_t)n);total+=(unsigned long long)n;}
    CHECK(!close(in)&&!close(out));unsigned char digest[32];sha256_final(&hash,digest);sha256_hex(digest,pin);return total;
}
static char *saved(const char *state) {
    char *text=NULL;size_t n=0;FILE *f=open_memstream(&text,&n);CHECK(f);
    CHECK(!activation_get(state,f));CHECK(!fclose(f));return text;
}
static void reaped(const char *log) {
    char path[1024];snprintf(path,sizeof(path),"%s.pid",log);FILE *f=fopen(path,"r");CHECK(f);
    long pid=0;CHECK(fscanf(f,"%ld",&pid)==1&&pid>1);CHECK(!fclose(f));
    errno=0;CHECK(kill((pid_t)pid,0)==-1&&errno==ESRCH);
}
static void reached_generation(const char *log) {
    FILE *f=fopen(log,"r");CHECK(f);char line[2048];int generated=0;
    while(fgets(line,sizeof(line),f))if(strstr(line,"\"n_predict\":8"))generated=1;
    CHECK(!fclose(f)&&generated);
}
static int cancel_generation(void *context) {
    FILE *f=fopen(context,"r");if(!f)return 0;
    char line[2048];int generated=0;
    while(fgets(line,sizeof(line),f))if(strstr(line,"\"n_predict\":8"))generated=1;
    fclose(f);return generated;
}
int main(int argc,char **argv) {
    int guardian=engine_guardian_dispatch(argc,argv);if(guardian>=0)return guardian;
    CHECK(argc==2&&argv[0][0]=='/'&&argv[1][0]=='/');
    char root[]="/tmp/samosa-managed-readiness-XXXXXX";CHECK(mkdtemp(root));
    char engine[512],model[512],state[512],portfile[512],log[512],endpoint[128],args[2048],pin[65],identity[100];
    snprintf(engine,sizeof(engine),"%s/engine",root);snprintf(model,sizeof(model),"%s/model",root);snprintf(state,sizeof(state),"%s/state",root);
    unsigned long long bytes=copy(argv[1],engine,pin);int fd=open(model,O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(fd>=0);CHECK(write(fd,"abc",3)==3);CHECK(!close(fd));
    ActivationConfig activation={.state=state,.self=argv[0],.engine=engine,.model=model,.engine_sha=pin,
        .model_sha="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",.engine_size=bytes,.model_size=3,
        .byte_ceiling=bytes,.lifetime=30,.log_limit=65536,.require_system_runtime=1};
    InferenceConfig config={.endpoint=endpoint,.admission_state=state,.context_tokens=128,
        .model_alias="fixture-native.gguf",.owner="alice",.api_key="fixture-secret"};
    CHECK(!managed_provider_identity(&activation,&config,identity));config.provider_id=identity;
    pid_t unrelated=fork();CHECK(unrelated>=0);if(!unrelated){for(;;)pause();}
    const char *modes[]={"good","probe-error","probe-empty","probe-zero","usage-overflow","budget-generate","budget-generate","good"};
    const char *failures[]={NULL,"provider_http_error","invalid_provider_response","invalid_provider_response","invalid_provider_response","deadline_exceeded","cancelled",NULL};
    char *last_good=NULL;
    for(unsigned i=0;i<8;i++) {
        unsigned port=free_port();snprintf(endpoint,sizeof(endpoint),"http://127.0.0.1:%u",port);
        snprintf(portfile,sizeof(portfile),"%s/port-%u",root,i);snprintf(log,sizeof(log),"%s/log-%u",root,i);
        snprintf(args,sizeof(args),"[\"@ENGINE@\",\"--fixture\",\"%s\",\"%s\",\"%s\",\"%u\",\"@MODEL@\"]",portfile,modes[i],log,port);
        activation.argv_template=args;
        config.deadline_monotonic=i==5?inference_now()+3:0;
        config.cancelled=i==6?cancel_generation:NULL;config.cancellation=log;
        ManagedProvider provider;const char *error=managed_provider_start(&activation,&config,&provider);
        if(i==0||i==7) {
            if(error)fprintf(stderr,"managed start: %s\n",error);
            CHECK(!error);CHECK(!managed_provider_status(&provider));
            CHECK(provider.activation.revision==(i?2u:1u));
            reached_generation(log);CHECK(!managed_provider_stop(&provider));
            CHECK(managed_provider_status(&provider));activation.expected_revision=1;
            if(!i)last_good=saved(state);
        } else {
            CHECK(error&&!strcmp(error,failures[i]));
            CHECK(!provider.qualified&&provider.activation.process.guardian<=0);
            reached_generation(log);char *current=saved(state);CHECK(!strcmp(last_good,current));free(current);
        }
        reaped(log);CHECK(kill(unrelated,0)==0);
    }
    free(last_good);CHECK(!kill(unrelated,SIGTERM));int status;CHECK(waitpid(unrelated,&status,0)==unrelated);
    /* All fixture state is under this exclusive test-owned directory. */
    char command[768];snprintf(command,sizeof(command),"rm -rf '%s'",root);CHECK(system(command)==0);
    printf("PASS integrated managed readiness: %u checks; generation failure, last-good revision, reap, healthy replacement and unrelated survivor\n",checks);
    return 0;
}
