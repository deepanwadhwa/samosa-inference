#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "speech_worker.h"
#include "speech_package_loader.h"
#include "speech_session.h"
#include "json.h"
#include "sha256.h"
#include <fcntl.h>
#include <poll.h>
#include <signal.h>
#include <spawn.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
extern char **environ;
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL speech worker %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void nap(void){struct timespec t={0,10000000};nanosleep(&t,NULL);}
static void bytes(int root,const char *name,const void *data,size_t n) {
    int fd=openat(root,name,O_WRONLY|O_CREAT|O_TRUNC|O_NOFOLLOW,0600);CHECK(fd>=0);CHECK(write(fd,data,n)==(ssize_t)n);CHECK(!close(fd));
}
static const char *names[]={"native.so","flow","main","encoder","decoder","text","vocab","scores","caro.wav","stuart.wav"};
static const char *roles[]={"library","pocket_lm_flow","pocket_lm_main","pocket_encoder","pocket_decoder","pocket_text_conditioner","pocket_vocab","pocket_token_scores","reference_caro","reference_stuart"};
static void manifest(int root,char pin[65]) {
    char *body=NULL;size_t length=0;FILE *f=open_memstream(&body,&length);CHECK(f!=NULL);
    fprintf(f,"{\"format\":\"samosa.sherpa-speech.v1\",\"abi_version\":\"1.13.4\",\"platform\":\"%s\",\"kind\":\"pocket\",\"threads\":2,\"files\":[",speech_package_platform());
    for(unsigned i=0;i<10;i++) {
        int fd=openat(root,names[i],O_RDONLY|O_NOFOLLOW);struct stat s;CHECK(fd>=0&&!fstat(fd,&s));
        Sha256 hash;sha256_init(&hash);unsigned char block[65536],digest[32];ssize_t n;
        while((n=read(fd,block,sizeof(block)))>0)sha256_update(&hash,block,(size_t)n);
        CHECK(n==0);CHECK(!close(fd));sha256_final(&hash,digest);char sha[65];sha256_hex(digest,sha);
        fprintf(f,"%s{\"name\":\"%s\",\"role\":\"%s\",\"bytes\":%llu,\"sha256\":\"%s\"}",i?",":"",names[i],roles[i],(unsigned long long)s.st_size,sha);
    }
    fputs("]}",f);CHECK(!fclose(f));hash_bytes(body,length,pin);bytes(root,"manifest.json",body,length);free(body);
}
static int lease(const char *path){int fd=open(path,O_RDWR|O_CREAT,0600);if(fd<0||flock(fd,LOCK_EX|LOCK_NB)){if(fd>=0)close(fd);return -1;}return fd;}
static int unlocked(const char *path){int fd=lease(path);if(fd<0)return 0;close(fd);return 1;}
typedef struct {SpeechProcessConfig c;EngineProcess process;int lease,root,directory,input,output;char work[4096],residency[4096];} Host;
static void setup(Host *h,const char *self,const char *base,const char *package,const char *pin,const char *request,double lifetime) {
    memset(h,0,sizeof(*h));CHECK(snprintf(h->work,sizeof(h->work),"%s/work",base)>0);CHECK(!mkdir(h->work,0700));
    h->directory=open(h->work,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(h->directory>=0);
    bytes(h->directory,"request.frame",request,strlen(request));h->input=openat(h->directory,"request.frame",O_RDONLY|O_NOFOLLOW);
    h->output=openat(h->directory,"result.frame",O_RDWR|O_CREAT|O_EXCL,0600);h->root=open(package,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);
    CHECK(snprintf(h->residency,sizeof(h->residency),"%s/residency",base)>0);h->lease=lease(h->residency);
    CHECK(h->input>=0&&h->output>=0&&h->root>=0&&h->lease>=0);
    h->process=(EngineProcess){.guardian=-1,.control=-1,.response=-1};
    h->c=(SpeechProcessConfig){.self=self,.package_path=package,.manifest_sha256=pin,.workspace_path=h->work,
        .residency_fd=h->lease,.package_fd=h->root,.workspace_fd=h->directory,.input_fd=h->input,.output_fd=h->output,
        .lifetime_seconds=lifetime,.log_limit=65536,.process=&h->process};
}
static void finish(Host *h) {
    CHECK(h->process.guardian==-1&&h->process.control==-1&&h->process.response==-1&&h->process.terminal);
    CHECK(access(h->work,F_OK)<0);close(h->input);close(h->output);close(h->directory);close(h->root);close(h->lease);CHECK(unlocked(h->residency));
}
typedef struct {int fd;const char *error,*marker;} Control;
static const char *active(void *arg) {
    Control *c=arg;struct stat s;
    if(!fstat(c->fd,&s)&&s.st_size>SPEECH_PACKET_HEADER) {
        if(c->marker){int fd=open(c->marker,O_WRONLY|O_CREAT|O_EXCL,0600);if(fd>=0)close(fd);}
        return c->error;
    }
    return NULL;
}
static const char *cancel_read(void *arg){(void)arg;return "operation_cancelled";}
typedef struct {Host *host;size_t bytes,calls;unsigned char pcm[8];int live;const char *error;} Stream;
static const char *stream_pcm(void *arg,const unsigned char *pcm,size_t bytes,int rate) {
    Stream *s=arg;CHECK(bytes&&bytes<=8192&&!(bytes%2)&&rate==24000);CHECK(s->bytes+bytes<=sizeof(s->pcm));
    memcpy(s->pcm+s->bytes,pcm,bytes);s->bytes+=bytes;s->calls++;
    CHECK(!unlocked(s->host->residency));
    if(s->host->process.guardian>0&&!s->host->process.terminal){s->live=1;
        SpeechRuntimeResult incomplete;unsigned char magic[8];CHECK(pread(s->host->output,magic,8,0)==8);
        if(!memcmp(magic,"SMPCP001",8))CHECK(!strcmp(speech_worker_result(s->host->output,NULL,NULL,&incomplete),"invalid_speech_output"));}
    return s->error;
}
/* Run fixture-only loaded-session checks in a separate process, just as native
 * loading in production belongs in a guarded worker rather than the host. */
typedef struct {size_t bytes,calls;} SessionPcm;
static const char *session_pcm(void *context,const unsigned char *data,size_t n,int rate) {
    SessionPcm *s=context;CHECK(data&&n&&rate==24000);s->bytes+=n;s->calls++;return NULL;
}
static void session_checks(int root,const char *path,const char *pin) {
    SpeechSession *session=NULL;
    CHECK(!strcmp(speech_session_open(root,path,pin,"unknown",NULL,NULL,&session),"invalid_speech_voice"));
    CHECK(!session);
    CHECK(!strcmp(speech_session_open(root,path,pin,NULL,cancel_read,NULL,&session),"operation_cancelled"));
    CHECK(!session);
    int borrowed=dup(root);CHECK(borrowed>=0);
    CHECK(!speech_session_open(borrowed,path,pin,"caro",NULL,NULL,&session));CHECK(session);
    CHECK(!close(borrowed));
    /* An attempted reopen would now fail native ABI qualification. Both
     * requests must keep using the original loaded model/runtime. */
    CHECK(!setenv("SAMOSA_TEST_SPEECH_ABI","incompatible",1));
    for(unsigned i=0;i<2;i++) {
        SpeechRuntimeResult result;SessionPcm sink={0};
        CHECK(!speech_session_generate(session,i?"final-only":"hello",i?"stuart":"caro",
            session_pcm,&sink,NULL,NULL,&result));
        CHECK(result.samples==4&&result.sample_rate==24000&&sink.bytes==8);
        CHECK(result.callbacks==(i?1u:2u)&&sink.calls==result.callbacks);
    }
    SpeechRuntimeResult result;SessionPcm sink={0};
    memset(&result,0x55,sizeof(result));
    CHECK(!strcmp(speech_session_generate(session,"hello","caro",session_pcm,&sink,
        cancel_read,NULL,&result),"operation_cancelled"));
    CHECK(!sink.bytes&&!result.samples&&!result.pcm_sha256[0]);
    CHECK(!speech_session_finish(session));session=NULL;
    CHECK(!unsetenv("SAMOSA_TEST_SPEECH_ABI"));
    CHECK(!speech_session_open(root,path,pin,NULL,NULL,NULL,&session));
    bytes(root,"flow","mutation",8);
    memset(&result,0x55,sizeof(result));
    CHECK(speech_session_generate(session,"hello","caro",session_pcm,&sink,NULL,NULL,&result));
    CHECK(!sink.bytes&&!result.samples&&!result.pcm_sha256[0]);
    speech_session_close(session);speech_session_close(NULL);
    bytes(root,"flow","artifact",8);
    printf("PASS extracted speech session: repeated generation without reopen, bound root, cancellation and mutation refusal\n");
}
typedef struct {struct timespec start;} ShortDeadline;
static const char *short_deadline(void *context) {
    ShortDeadline *d=context;struct timespec now;CHECK(!clock_gettime(CLOCK_MONOTONIC,&now));
    double elapsed=(double)(now.tv_sec-d->start.tv_sec)+(now.tv_nsec-d->start.tv_nsec)/1e9;
    return elapsed>.2?"deadline_exceeded":NULL;
}
static const char *cancel_pcm(void *context,const unsigned char *pcm,size_t bytes,int rate) {
    (void)context;CHECK(pcm&&bytes&&rate==24000);return "operation_cancelled";
}
static void resident_checks(const char *self,int root,const char *path,const char *pin,const char *base) {
    char residency[4096],log[4096];CHECK(snprintf(residency,sizeof(residency),"%s/resident-lease",base)>0);
    CHECK(snprintf(log,sizeof(log),"%s/resident-loads",base)>0);CHECK(!setenv("SAMOSA_TEST_SPEECH_LOAD_LOG",log,1));
    SpeechResident r;speech_resident_init(&r);FILE *spool=tmpfile();CHECK(spool);int fd=fileno(spool);
    SpeechProcessConfig config={.self=self,.package_path=path,.manifest_sha256=pin,.package_fd=root,.lifetime_seconds=10,.log_limit=65536};
    for(unsigned mode=0;mode<6;mode++) {
        config.residency_fd=lease(residency);CHECK(config.residency_fd>=0);
        CHECK(!speech_resident_start(&config,&r));CHECK(!close(config.residency_fd));CHECK(!unlocked(residency));
        pid_t guardian=r.process.guardian;SpeechRuntimeResult result;SessionPcm sink={0};
        const char *request="{\"text\":\"hello\",\"voice\":\"caro\"}";
        if(mode==0) {
            CHECK(!strcmp(speech_resident_run(&r,request,strlen(request),fd,session_pcm,&sink,cancel_read,NULL,&result),"operation_cancelled"));
            CHECK(!sink.bytes&&r.process.guardian==guardian);
            for(unsigned i=0;i<3;i++) {
                sink=(SessionPcm){0};const char *q=i==1?"{\"text\":\"final-only\",\"voice\":\"stuart\"}":request;
                CHECK(!speech_resident_run(&r,q,strlen(q),fd,session_pcm,&sink,NULL,NULL,&result));
                CHECK(result.samples==4&&sink.bytes==8&&r.process.guardian==guardian);
                SpeechRuntimeResult saved;CHECK(!speech_worker_result(fd,NULL,NULL,&saved));CHECK(!strcmp(saved.pcm_sha256,result.pcm_sha256));
            }
            int loads=open(log,O_RDONLY);char events[8];CHECK(loads>=0);CHECK(read(loads,events,sizeof(events))==1&&events[0]=='C');CHECK(!close(loads));
            /* Graceful request EOF closes the same loaded session; guardian
             * remains responsible for final reap and lease release. */
            CHECK(!close(r.write_fd));r.write_fd=-1;
            struct pollfd watch={r.process.response,POLLIN|POLLHUP,0};CHECK(poll(&watch,1,2000)>0);
            CHECK(!speech_resident_stop(&r));
        }else {
            const char *e;
            if(mode==1)e=speech_resident_run(&r,request,strlen(request),fd,cancel_pcm,NULL,NULL,NULL,&result);
            else if(mode==2){ShortDeadline d;CHECK(!clock_gettime(CLOCK_MONOTONIC,&d.start));request="{\"text\":\"partial-wait\"}";
                e=speech_resident_run(&r,request,strlen(request),fd,NULL,NULL,short_deadline,&d,&result);}
            else if(mode==5){bytes(root,"flow","mutation",8);e=speech_resident_run(&r,request,strlen(request),fd,NULL,NULL,NULL,NULL,&result);bytes(root,"flow","artifact",8);}
            else {request=mode==3?"{\"text\":\"nan\"}":"{\"text\":\"crash\"}";
                e=speech_resident_run(&r,request,strlen(request),fd,NULL,NULL,NULL,NULL,&result);}
            CHECK(e);if(mode==1)CHECK(!strcmp(e,"operation_cancelled"));if(mode==2)CHECK(!strcmp(e,"deadline_exceeded"));
            CHECK(!result.samples&&!result.pcm_sha256[0]);CHECK(r.process.terminal&&r.write_fd==-1&&r.read_fd==-1);
        }
        CHECK(unlocked(residency));CHECK(!speech_resident_stop(&r));
    }
    CHECK(!unsetenv("SAMOSA_TEST_SPEECH_LOAD_LOG"));CHECK(!fclose(spool));CHECK(!unlink(residency)&&!unlink(log));
    puts("PASS resident speech worker: same loaded model/guardian, provisional PCM, EOF, controls, malformed audio, crash and lease recovery");
}
static const char *resident_marker(void *context,const unsigned char *pcm,size_t bytes,int rate) {
    (void)pcm;(void)bytes;(void)rate;const char *path=context;
    int fd=open(path,O_WRONLY|O_CREAT|O_EXCL,0600);if(fd>=0)close(fd);return NULL;
}
int main(int argc,char **argv) {
    int code=engine_guardian_dispatch(argc,argv);if(code>=0)return code;
    code=speech_worker_dispatch(argc,argv);if(code>=0)return code;
    if(argc==6&&!strcmp(argv[1],"--parent-resident-speech")) {
        char residency[4096];CHECK(snprintf(residency,sizeof(residency),"%s/residency",argv[2])>0);
        int root=open(argv[3],O_RDONLY|O_DIRECTORY|O_NOFOLLOW),held=lease(residency);CHECK(root>=0&&held>=0);
        SpeechProcessConfig config={.self=argv[0],.package_path=argv[3],.manifest_sha256=argv[4],
            .package_fd=root,.residency_fd=held,.lifetime_seconds=30,.log_limit=65536};
        SpeechResident r;speech_resident_init(&r);FILE *spool=tmpfile();CHECK(spool);CHECK(!speech_resident_start(&config,&r));
        close(held);close(root);SpeechRuntimeResult result;const char *q="{\"text\":\"partial-wait\"}";
        return speech_resident_run(&r,q,strlen(q),fileno(spool),resident_marker,argv[5],NULL,NULL,&result)?3:0;
    }
    if(argc==6&&!strcmp(argv[1],"--parent-speech")) {
        Host h;setup(&h,argv[0],argv[2],argv[3],argv[4],"{\"text\":\"partial-wait\",\"voice\":\"caro\"}",30);
        Control control={.fd=h.output,.marker=argv[5]};SpeechRuntimeResult result;
        return speech_process_run(&h.c,active,&control,&result)?3:0;
    }
    int export_only=argc==3&&!strcmp(argv[1],"--export-package");
    CHECK((argc==2||export_only)&&argv[0][0]=='/');
#ifdef __APPLE__
    char base[]="/private/tmp/samosa-speech-worker.XXXXXX";
#else
    char base[]="/tmp/samosa-speech-worker.XXXXXX";
#endif
    CHECK(mkdtemp(base)!=NULL);char package[4096];CHECK(snprintf(package,sizeof(package),"%s/package",base)>0);CHECK(!mkdir(package,0700));
    int root=open(package,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(root>=0);
    int library=open(argv[export_only?2:1],O_RDONLY|O_NOFOLLOW),copy=openat(root,"native.so",O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(library>=0&&copy>=0);
    unsigned char block[65536];ssize_t n;while((n=read(library,block,sizeof(block)))>0)CHECK(write(copy,block,(size_t)n)==n);CHECK(n==0);CHECK(!close(library)&&!close(copy));
    for(unsigned i=1;i<8;i++)bytes(root,names[i],"artifact",8);
    unsigned char wav[]={ 'R','I','F','F',40,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,1,0,1,0,128,62,0,0,0,125,0,0,2,0,16,0,'d','a','t','a',4,0,0,0,0,64,0,192 };
    bytes(root,"caro.wav",wav,sizeof(wav));bytes(root,"stuart.wav",wav,sizeof(wav));char pin[65];manifest(root,pin);
    if(export_only){printf("{\"base\":\"%s\",\"package\":\"%s\",\"manifest_sha256\":\"%s\"}\n",base,package,pin);close(root);return 0;}
    pid_t session_child=fork();CHECK(session_child>=0);
    if(!session_child){session_checks(root,package,pin);exit(0);}
    int session_status;CHECK(waitpid(session_child,&session_status,0)==session_child);
    CHECK(WIFEXITED(session_status)&&!WEXITSTATUS(session_status));
    resident_checks(argv[0],root,package,pin,base);
    Host h;SpeechRuntimeResult result;
    const char *requests[]={"{\"text\":\"hello\",\"voice\":\"caro\"}","{\"text\":\"final-only\",\"voice\":\"stuart\"}"};
    for(unsigned i=0;i<2;i++) {
        setup(&h,argv[0],base,package,pin,requests[i],10);Stream streamed={.host=&h};h.c.pcm_sink=stream_pcm;h.c.pcm_context=&streamed;
        const char *e=speech_process_run(&h.c,NULL,NULL,&result);if(e)fprintf(stderr,"process result: %s\n",e);CHECK(!e);
        CHECK(result.sample_rate==24000&&result.samples==4&&result.callbacks==(i?1u:2u));
        unsigned char actual[8],expected[]={0,0,102,38,154,217,0,0};CHECK(pread(h.output,actual,sizeof(actual),SPEECH_PACKET_HEADER)==sizeof(actual));CHECK(!memcmp(actual,expected,8));
        CHECK(streamed.calls&&streamed.bytes==8&&!memcmp(streamed.pcm,expected,8));
        SpeechRuntimeResult read;CHECK(!speech_worker_result(h.output,NULL,NULL,&read));CHECK(!strcmp(read.pcm_sha256,result.pcm_sha256));
        CHECK(!strcmp(speech_worker_result(h.output,cancel_read,NULL,&read),"operation_cancelled"));CHECK(!read.samples);
        const unsigned offsets[]={0,8,19,20,24,32,72};
        for(unsigned k=0;k<sizeof(offsets)/sizeof(*offsets);k++) {
            unsigned char original,changed=255;CHECK(pread(h.output,&original,1,offsets[k])==1);
            CHECK(pwrite(h.output,&changed,1,offsets[k])==1);
            CHECK(!strcmp(speech_worker_result(h.output,NULL,NULL,&read),"invalid_speech_output"));CHECK(!read.samples);
            CHECK(pwrite(h.output,&original,1,offsets[k])==1);
        }
        unsigned char bad=42;CHECK(pwrite(h.output,&bad,1,SPEECH_PACKET_HEADER)==1);CHECK(!strcmp(speech_worker_result(h.output,NULL,NULL,&read),"speech_output_checksum"));CHECK(!read.samples);
        finish(&h);
    }
    /* A stalling native callback exposes provisional PCM while its worker is
     * alive. Sink refusal must preserve its error and reap before returning. */
    setup(&h,argv[0],base,package,pin,"{\"text\":\"partial-wait\",\"voice\":\"caro\"}",10);
    Stream rejected={.host=&h,.error="stream_sink_failed"};h.c.pcm_sink=stream_pcm;h.c.pcm_context=&rejected;
    const char *stream_error=speech_process_run(&h.c,NULL,NULL,&result);
    CHECK(stream_error&&!strcmp(stream_error,"stream_sink_failed"));CHECK(rejected.live&&rejected.calls==1&&rejected.bytes==4);CHECK(!result.samples);finish(&h);
    const char *bad[]={"{}","{\"text\":\"hello\",\"path\":\"/tmp/unpinned\"}","{\"text\":\"hello\",\"voice\":\"unknown\"}","{\"text\":\"nan\",\"voice\":\"caro\"}","{\"text\":\"partial\",\"voice\":\"caro\"}","{\"text\":\"crash\",\"voice\":\"caro\"}"};
    for(unsigned i=0;i<sizeof(bad)/sizeof(*bad);i++) {
        setup(&h,argv[0],base,package,pin,bad[i],10);const char *e=speech_process_run(&h.c,NULL,NULL,&result);CHECK(e&&!strcmp(e,"engine_failed"));CHECK(!result.samples);finish(&h);
    }
    for(unsigned i=0;i<3;i++) {
        setup(&h,argv[0],base,package,pin,"{\"text\":\"partial-wait\",\"voice\":\"caro\"}",i==2?.25:10);
        Control c={.fd=h.output,.error=i?"deadline_exceeded":"operation_cancelled"};
        const char *e=speech_process_run(&h.c,i==2?NULL:active,&c,&result);CHECK(e&&!strcmp(e,i==2?"lifetime_exceeded":c.error));CHECK(!result.samples);finish(&h);
    }
    /* Actual requesting-process death must release residency only after the
     * worker is reaped and its guardian removes the owned workspace. */
    char ready[4096],residency[4096],work[4096];snprintf(ready,sizeof(ready),"%s/ready",base);snprintf(residency,sizeof(residency),"%s/residency",base);snprintf(work,sizeof(work),"%s/work",base);
    pid_t survivor;char *survivor_args[]={"/bin/sleep","30",NULL};CHECK(!posix_spawn(&survivor,"/bin/sleep",NULL,NULL,survivor_args,environ));
    for(unsigned kind=0;kind<2;kind++) {
        pid_t parent;char *args[]={argv[0],kind?"--parent-resident-speech":"--parent-speech",base,package,pin,ready,NULL};CHECK(!posix_spawn(&parent,argv[0],NULL,NULL,args,environ));
        int seen=0;for(unsigned i=0;i<500;i++){if(!access(ready,F_OK)){seen=1;break;}nap();}CHECK(seen);CHECK(!unlocked(residency));CHECK(!kill(parent,SIGKILL));int status;CHECK(waitpid(parent,&status,0)==parent);
        seen=0;for(unsigned i=0;i<500;i++){if(access(work,F_OK)<0&&unlocked(residency)){seen=1;break;}nap();}CHECK(seen);CHECK(!unlink(ready));CHECK(!unlink(residency));CHECK(!kill(survivor,0));
    }
    CHECK(!kill(survivor,SIGTERM));int survivor_status;CHECK(waitpid(survivor,&survivor_status,0)==survivor);
    for(unsigned i=0;i<10;i++)CHECK(!unlinkat(root,names[i],0));CHECK(!unlinkat(root,"manifest.json",0));CHECK(!close(root));CHECK(!rmdir(package));CHECK(!rmdir(base));
    printf("PASS owned speech worker: %u checks; native ABI, packet integrity, controls, crash and parent-death cleanup\n",checks);return 0;
}
