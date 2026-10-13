#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "speech_worker.h"
#include "speech_package_loader.h"
#include "json.h"
#include "sha256.h"
#include <fcntl.h>
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
int main(int argc,char **argv) {
    int code=engine_guardian_dispatch(argc,argv);if(code>=0)return code;
    code=speech_worker_dispatch(argc,argv);if(code>=0)return code;
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
    Host h;SpeechRuntimeResult result;
    const char *requests[]={"{\"text\":\"hello\",\"voice\":\"caro\"}","{\"text\":\"final-only\",\"voice\":\"stuart\"}"};
    for(unsigned i=0;i<2;i++) {
        setup(&h,argv[0],base,package,pin,requests[i],10);const char *e=speech_process_run(&h.c,NULL,NULL,&result);if(e)fprintf(stderr,"process result: %s\n",e);CHECK(!e);
        CHECK(result.sample_rate==24000&&result.samples==4&&result.callbacks==(i?1u:2u));
        unsigned char actual[8],expected[]={0,0,102,38,154,217,0,0};CHECK(pread(h.output,actual,sizeof(actual),SPEECH_PACKET_HEADER)==sizeof(actual));CHECK(!memcmp(actual,expected,8));
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
    pid_t parent;char *args[]={argv[0],"--parent-speech",base,package,pin,ready,NULL};CHECK(!posix_spawn(&parent,argv[0],NULL,NULL,args,environ));
    int seen=0;for(unsigned i=0;i<500;i++){if(!access(ready,F_OK)){seen=1;break;}nap();}CHECK(seen);CHECK(!unlocked(residency));CHECK(!kill(parent,SIGKILL));int status;CHECK(waitpid(parent,&status,0)==parent);
    seen=0;for(unsigned i=0;i<500;i++){if(access(work,F_OK)<0&&unlocked(residency)){seen=1;break;}nap();}CHECK(seen);CHECK(!unlink(ready));CHECK(!unlink(residency));
    for(unsigned i=0;i<10;i++)CHECK(!unlinkat(root,names[i],0));CHECK(!unlinkat(root,"manifest.json",0));CHECK(!close(root));CHECK(!rmdir(package));CHECK(!rmdir(base));
    printf("PASS owned speech worker: %u checks; native ABI, packet integrity, controls, crash and parent-death cleanup\n",checks);return 0;
}
