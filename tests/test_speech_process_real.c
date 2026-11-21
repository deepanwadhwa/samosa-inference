#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "speech_worker.h"
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL real speech worker %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static const char *cancel_partial(void *context) {
    int fd=*(int *)context;struct stat size;
    return !fstat(fd,&size)&&size.st_size>SPEECH_PACKET_HEADER?"operation_cancelled":NULL;
}
typedef struct {size_t bytes,chunks;int cancel;} ResidentAudio;
static const char *resident_pcm(void *context,const unsigned char *pcm,size_t bytes,int rate) {
    ResidentAudio *a=context;CHECK(pcm&&bytes&&bytes<=8192&&rate==24000);a->bytes+=bytes;a->chunks++;
    return a->cancel&&a->chunks==2?"operation_cancelled":NULL;
}
static void resident_actual(const char *worker,const char *package,const char *pin,int root,int lease) {
    SpeechResident resident;speech_resident_init(&resident);FILE *spool=tmpfile();CHECK(spool);
    SpeechProcessConfig config={.self=worker,.package_path=package,.manifest_sha256=pin,
        .package_fd=root,.residency_fd=lease,.lifetime_seconds=120,.log_limit=65536};
    CHECK(!speech_resident_start(&config,&resident));pid_t guardian=resident.process.guardian;
    for(unsigned i=0;i<4;i++) {
        const char *request=i==1||i==3?"{\"text\":\"The meeting starts at nine. Please bring your notes.\",\"voice\":\"stuart\"}":
            "{\"text\":\"The meeting starts at nine. Please bring your notes.\",\"voice\":\"caro\"}";
        if(i==3){CHECK(!speech_resident_start(&config,&resident));guardian=resident.process.guardian;}
        ResidentAudio audio={.cancel=i==2};SpeechRuntimeResult result;
        const char *e=speech_resident_run(&resident,request,strlen(request),fileno(spool),resident_pcm,&audio,NULL,NULL,&result);
        if(i==2){CHECK(e&&!strcmp(e,"operation_cancelled"));CHECK(audio.chunks==2&&!result.samples&&resident.process.terminal);
            puts("PASS actual resident Pocket interrupted PCM: worker invalidated and reaped");continue;}
        if(e)fprintf(stderr,"resident actual error: %s\n",e);CHECK(!e);
        const char *expected=i==0?"5a854797b1ebc66817950c875966dc8eefbd0ae10c143e0964546fbbdd7270c4":
            "0687ebda438c0f27ae2279306a51b8c2ceae8b0c02bdf4880c6a842a6cadb3fc";
        CHECK(!strcmp(result.pcm_sha256,expected)&&audio.bytes==result.samples*2&&audio.chunks>1);
        CHECK(resident.process.guardian==guardian&&!resident.process.terminal);
        SpeechRuntimeResult saved;CHECK(!speech_worker_result(fileno(spool),NULL,NULL,&saved));CHECK(!strcmp(saved.pcm_sha256,result.pcm_sha256));
        printf("PASS actual resident Pocket %s: same guardian, samples=%zu chunks=%zu pcm_sha256=%s\n",i==0?"caro":"stuart",result.samples,audio.chunks,result.pcm_sha256);
    }
    CHECK(!speech_resident_stop(&resident));CHECK(!fclose(spool));
}
int main(int argc,char **argv) {
    if(argc!=4){fprintf(stderr,"usage: %s ABSOLUTE_WORKER ABSOLUTE_PACKAGE MANIFEST_SHA256\n",argv[0]);return 2;}
#ifdef __APPLE__
    char base[]="/private/tmp/samosa-real-speech-process.XXXXXX";
#else
    char base[]="/tmp/samosa-real-speech-process.XXXXXX";
#endif
    CHECK(mkdtemp(base)!=NULL);char work[4096],lease[4096];snprintf(work,sizeof(work),"%s/work",base);snprintf(lease,sizeof(lease),"%s/residency",base);
    int root=open(argv[2],O_RDONLY|O_DIRECTORY|O_NOFOLLOW),residency=open(lease,O_RDWR|O_CREAT|O_EXCL,0600);
    CHECK(root>=0&&residency>=0&&!flock(residency,LOCK_EX|LOCK_NB));
    for(unsigned i=0;i<3;i++) {
        CHECK(!mkdir(work,0700));int dir=open(work,O_RDONLY|O_DIRECTORY|O_NOFOLLOW);CHECK(dir>=0);
        int input=openat(dir,"request.frame",O_WRONLY|O_CREAT|O_EXCL,0600);CHECK(input>=0);
        const char *request=i==2?"{\"text\":\"The meeting starts at nine. Please bring your notes.\",\"voice\":\"stuart\"}":
            "{\"text\":\"The meeting starts at nine. Please bring your notes.\",\"voice\":\"caro\"}";
        CHECK(write(input,request,strlen(request))==(ssize_t)strlen(request));CHECK(!close(input));input=openat(dir,"request.frame",O_RDONLY|O_NOFOLLOW);
        int output=openat(dir,"result.frame",O_RDWR|O_CREAT|O_EXCL,0600);CHECK(input>=0&&output>=0);
        EngineProcess process={.guardian=-1,.control=-1,.response=-1};
        SpeechProcessConfig config={.self=argv[1],.package_path=argv[2],.manifest_sha256=argv[3],.workspace_path=work,
            .residency_fd=residency,.package_fd=root,.workspace_fd=dir,.input_fd=input,.output_fd=output,.lifetime_seconds=30,.log_limit=65536,.process=&process};
        SpeechRuntimeResult result;const char *e=speech_process_run(&config,i==1?cancel_partial:NULL,&output,&result);
        if(i==1){CHECK(e&&!strcmp(e,"operation_cancelled"));CHECK(!result.samples&&!result.pcm_sha256[0]);puts("PASS actual owned Pocket cancellation after PCM; no successful receipt");}
        else {
            if(e)fprintf(stderr,"actual worker error: %s\n",e);CHECK(!e);
            CHECK(result.sample_rate==24000&&result.samples>12000&&result.callbacks>=2);
            const char *expected=i==2?"0687ebda438c0f27ae2279306a51b8c2ceae8b0c02bdf4880c6a842a6cadb3fc":
                "5a854797b1ebc66817950c875966dc8eefbd0ae10c143e0964546fbbdd7270c4";
            CHECK(!strcmp(expected,result.pcm_sha256));
            printf("PASS actual owned Pocket %s: samples=%zu callbacks=%zu pcm_sha256=%s\n",i==2?"stuart":"caro",result.samples,result.callbacks,result.pcm_sha256);
        }
        CHECK(process.guardian==-1&&process.control==-1&&process.response==-1&&process.terminal);
        CHECK(access(work,F_OK)<0);CHECK(!close(input)&&!close(output)&&!close(dir));fflush(stdout);
    }
    resident_actual(argv[1],argv[2],argv[3],root,residency);
    CHECK(!close(root)&&!close(residency));CHECK(!unlink(lease)&&!rmdir(base));
    printf("PASS actual speech guardian: %u checks; package/ABI verification, output integrity, cancellation, cleanup and recovery\n",checks);return 0;
}
