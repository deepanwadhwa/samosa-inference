#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "speech_worker.h"
#include "speech_package_loader.h"
#include "json.h"
#include "sha256.h"
#include <errno.h>
#include <fcntl.h>
#include <poll.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
static uint32_t u32(const unsigned char *p) {return p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint64_t u64(const unsigned char *p) {return u32(p)|(uint64_t)u32(p+4)<<32;}
static void w32(unsigned char *p,uint32_t v) {for(unsigned i=0;i<4;i++)p[i]=(unsigned char)(v>>(i*8));}
static void w64(unsigned char *p,uint64_t v) {for(unsigned i=0;i<8;i++)p[i]=(unsigned char)(v>>(i*8));}
static int same(const struct stat *a,const struct stat *b) {
    if(a->st_dev!=b->st_dev||a->st_ino!=b->st_ino||a->st_size!=b->st_size||a->st_mode!=b->st_mode||a->st_uid!=b->st_uid||a->st_nlink!=b->st_nlink)return 0;
#ifdef __APPLE__
    return a->st_mtimespec.tv_sec==b->st_mtimespec.tv_sec&&a->st_mtimespec.tv_nsec==b->st_mtimespec.tv_nsec&&a->st_ctimespec.tv_sec==b->st_ctimespec.tv_sec&&a->st_ctimespec.tv_nsec==b->st_ctimespec.tv_nsec;
#else
    return a->st_mtim.tv_sec==b->st_mtim.tv_sec&&a->st_mtim.tv_nsec==b->st_mtim.tv_nsec&&a->st_ctim.tv_sec==b->st_ctim.tv_sec&&a->st_ctim.tv_nsec==b->st_ctim.tv_nsec;
#endif
}
static int read_at(int fd,void *bytes,size_t count,off_t offset) {
    unsigned char *p=bytes;
    while(count){ssize_t n=pread(fd,p,count,offset);if(n<0&&errno==EINTR)continue;if(n<=0)return 0;
        p+=n;offset+=n;count-=(size_t)n;}return 1;
}
static int write_at(int fd,const void *bytes,size_t count,off_t offset) {
    const unsigned char *p=bytes;
    while(count){ssize_t n=pwrite(fd,p,count,offset);if(n<0&&errno==EINTR)continue;if(n<=0)return 0;
        p+=n;offset+=n;count-=(size_t)n;}return 1;
}
typedef struct {size_t bytes,chunks;Sha256 hash;} Output;
static const char *pcm(void *context,const unsigned char *bytes,size_t count,int rate) {
    Output *out=context;
    if(!count||count>8192||count%2||rate<8000||rate>96000||count>SPEECH_PACKET_LIMIT-SPEECH_PACKET_HEADER-out->bytes)
        return "speech_audio_limit";
    if(!write_at(STDOUT_FILENO,bytes,count,(off_t)(SPEECH_PACKET_HEADER+out->bytes)))return "speech_output_failed";
    sha256_update(&out->hash,bytes,count);out->bytes+=count;out->chunks++;
    unsigned char header[SPEECH_PACKET_HEADER]={0},digest[32];Sha256 snapshot=out->hash;
    sha256_final(&snapshot,digest);
    memcpy(header,"SMPCP001",8);w64(header+8,out->bytes);w32(header+16,(uint32_t)rate);
    w64(header+24,out->bytes/2);w64(header+32,out->chunks);memcpy(header+40,digest,32);
    /* This prefix is provisional, never a completed packet. Readers accept a
     * stable header only after verifying its entire newly appended PCM suffix. */
    if(!write_at(STDOUT_FILENO,header,sizeof(header),0))return "speech_output_failed";
    return NULL;
}
int speech_worker_dispatch(int argc,char **argv) {
    if(argc<2||strcmp(argv[1],"--inference-speech-worker"))return -1;
    if(argc!=4)return 2;
    struct stat input,output,after,root;
    if(fstat(5,&root)||!S_ISDIR(root.st_mode)||(fcntl(5,F_GETFL)&O_ACCMODE)!=O_RDONLY||
        fstat(STDIN_FILENO,&input)||!S_ISREG(input.st_mode)||input.st_uid!=geteuid()||(input.st_mode&077)||input.st_nlink!=1||
        input.st_size<=0||input.st_size>16384||(fcntl(STDIN_FILENO,F_GETFL)&O_ACCMODE)!=O_RDONLY||
        fstat(STDOUT_FILENO,&output)||!S_ISREG(output.st_mode)||output.st_uid!=geteuid()||(output.st_mode&077)||output.st_nlink!=1||
        output.st_size!=0||(fcntl(STDOUT_FILENO,F_GETFL)&O_ACCMODE)!=O_RDWR)return 2;
    char body[16384];
    if(!read_at(STDIN_FILENO,body,(size_t)input.st_size,0)||fstat(STDIN_FILENO,&after)||!same(&input,&after))return 2;
    Json *j=malloc(sizeof(*j));if(!j)return 2;
    const char *e=NULL;char *text=NULL,*voice=NULL;
    if(!json_parse(j,body,(size_t)input.st_size)||j->tokens[0].type!=JT_OBJECT||j->tokens[0].count<1||j->tokens[0].count>2)e="invalid_speech_request";
    if(!e) {
        text=json_string(j,json_get(j,0,"text"));
        int v=json_get(j,0,"voice");if(v>=0)voice=json_string(j,v);
        if(!text||!*text||strlen(text)>2400||(v>=0&&!voice)||
            (j->tokens[0].count==2&&v<0))e="invalid_speech_request";
        /* The only two accepted fields are explicit; no request path escapes
         * the host's package configuration or inherited descriptor boundary. */
        if(!e&&j->tokens[0].count!=(voice?2:1))e="invalid_speech_request";
    }
    free(j);
    SpeechPackageReceipt *package=NULL;SpeechRuntime *runtime=NULL;
    SpeechRuntimeOptions options;const char *caro=NULL,*stuart=NULL;
    if(!e){package=calloc(1,sizeof(*package));if(!package)e="speech_resource_limit";}
    if(!e)e=speech_package_verify(5,argv[2],argv[3],NULL,NULL,package,0);
    LibraryRuntimePolicy policy;
    if(!e)e=library_runtime_host_policy(&policy);
    if(!e)e=speech_package_loader_verify(5,argv[2],argv[3],&policy,NULL,NULL,package);
    if(!e)e=speech_package_options(package,&options,&caro,&stuart);
    if(!e&&options.pocket&&voice&&strcmp(voice,"caro")&&strcmp(voice,"stuart"))e="invalid_speech_voice";
    if(!e)e=speech_runtime_open(&options,&runtime);
    Output out={0};SpeechRuntimeResult result={0};
    sha256_init(&out.hash);
    unsigned char header[SPEECH_PACKET_HEADER]={0};
    if(!e&&!write_at(STDOUT_FILENO,header,sizeof(header),0))e="speech_output_failed";
    if(!e)e=speech_runtime_generate(runtime,text,voice,
        options.pocket?(voice&&!strcmp(voice,"stuart")?stuart:caro):NULL,pcm,&out,&result);
    speech_runtime_close(runtime);
    if(!e)e=speech_package_verify(5,argv[2],argv[3],NULL,NULL,package,1);
    if(!e&&out.bytes!=result.samples*2)e="speech_stream_mismatch";
    if(!e) {
        memcpy(header,"SMPCM001",8);w64(header+8,out.bytes);w32(header+16,(uint32_t)result.sample_rate);
        w64(header+24,result.samples);w64(header+32,result.callbacks);
        for(unsigned i=0;i<32;i++)header[40+i]=(unsigned char)(json_hex(result.pcm_sha256[i*2])*16+json_hex(result.pcm_sha256[i*2+1]));
        /* Publish terminal metadata last, after complete generation and package
         * recheck. Zero/partial headers cannot be decoded as successful audio. */
        if(!write_at(STDOUT_FILENO,header,sizeof(header),0)||fsync(STDOUT_FILENO))e="speech_output_failed";
    }
    if(e)fprintf(stderr,"speech worker: %s\n",e);
    free(package);free(text);free(voice);return e?2:0;
}
const char *speech_worker_result(int fd,const char *(*control)(void *),void *context,SpeechRuntimeResult *result) {
    if(!result)return "invalid_configuration";
    memset(result,0,sizeof(*result));const char *e=control?control(context):NULL;
    struct stat before,after;unsigned char header[SPEECH_PACKET_HEADER];
    if(!e&&(fstat(fd,&before)||!S_ISREG(before.st_mode)||before.st_uid!=geteuid()||(before.st_mode&077)||before.st_nlink>1||
        before.st_size<SPEECH_PACKET_HEADER||before.st_size>SPEECH_PACKET_LIMIT||!read_at(fd,header,sizeof(header),0)))e="invalid_speech_output";
    uint64_t bytes=0,samples=0,callbacks=0;uint32_t rate=0;
    if(!e) {
        bytes=u64(header+8);rate=u32(header+16);samples=u64(header+24);callbacks=u64(header+32);
        if(memcmp(header,"SMPCM001",8)||u32(header+20)||u64(header+72)||rate<8000||rate>96000||
            !samples||samples>(uint64_t)rate*45||bytes!=samples*2||bytes!=(uint64_t)before.st_size-SPEECH_PACKET_HEADER||
            !callbacks||callbacks>samples)e="invalid_speech_output";
    }
    Sha256 hash;sha256_init(&hash);unsigned char data[65536],digest[32];
    for(uint64_t at=0;!e&&at<bytes;) {
        e=control?control(context):NULL;if(e)break;
        size_t n=(size_t)(bytes-at);if(n>sizeof(data))n=sizeof(data);
        if(!read_at(fd,data,n,(off_t)(SPEECH_PACKET_HEADER+at)))e="speech_output_changed";
        else sha256_update(&hash,data,n);
        at+=n;
    }
    if(!e)e=control?control(context):NULL;
    if(!e&&(fstat(fd,&after)||!same(&before,&after)))e="speech_output_changed";
    if(!e){sha256_final(&hash,digest);if(memcmp(digest,header+40,32))e="speech_output_checksum";}
    if(!e){result->sample_rate=(int)rate;result->samples=(size_t)samples;result->callbacks=(size_t)callbacks;sha256_hex(digest,result->pcm_sha256);}
    return e;
}
typedef struct {size_t bytes;int rate;Sha256 hash;} Progress;
static const char *progress(int fd,Progress *p,SpeechPcmSink sink,void *sink_context,
    const char *(*control)(void *),void *context,int final) {
    struct stat s;unsigned char header[SPEECH_PACKET_HEADER],after[SPEECH_PACKET_HEADER];
    if(fstat(fd,&s)||!S_ISREG(s.st_mode)||s.st_uid!=geteuid()||(s.st_mode&077)||s.st_nlink>1||s.st_size>SPEECH_PACKET_LIMIT)
        return "invalid_speech_output";
    if(s.st_size<SPEECH_PACKET_HEADER||!read_at(fd,header,sizeof(header),0))return final?"invalid_speech_output":NULL;
    int complete=!memcmp(header,"SMPCM001",8);
    if(!complete&&memcmp(header,"SMPCP001",8))return final?"invalid_speech_output":NULL;
    uint64_t bytes=u64(header+8),samples=u64(header+24),callbacks=u64(header+32);uint32_t rate=u32(header+16);
    /* Header writes may overlap a live observation. A transient inconsistent
     * snapshot delivers nothing; terminal validation remains strict. */
    if(u32(header+20)||u64(header+72)||rate<8000||rate>96000||!samples||samples>(uint64_t)rate*45||
        bytes!=samples*2||bytes>(uint64_t)s.st_size-SPEECH_PACKET_HEADER||!callbacks||callbacks>samples)
        return final?"invalid_speech_output":NULL;
    if(bytes<p->bytes||(p->rate&&p->rate!=(int)rate))return "speech_stream_mismatch";
    size_t count=(size_t)bytes-p->bytes;unsigned char *data=count?malloc(count):NULL;
    if(count&&!data)return "speech_resource_limit";
    const char *e=control?control(context):NULL;
    if(!e&&count&&!read_at(fd,data,count,(off_t)(SPEECH_PACKET_HEADER+p->bytes)))e="speech_output_changed";
    Sha256 candidate=p->hash,snapshot;unsigned char digest[32];
    if(!e){if(count)sha256_update(&candidate,data,count);snapshot=candidate;sha256_final(&snapshot,digest);
        if(!read_at(fd,after,sizeof(after),0)||memcmp(header,after,sizeof(header))||memcmp(digest,header+40,32)){
            free(data);return final?"speech_output_checksum":NULL;}}
    for(size_t at=0;!e&&at<count;){
        e=control?control(context):NULL;if(e)break;
        size_t n=count-at;if(n>8192)n=8192;
        e=sink(sink_context,data+at,n,(int)rate);at+=n;
    }
    if(!e){p->hash=candidate;p->bytes=(size_t)bytes;p->rate=(int)rate;}
    free(data);return e;
}
const char *speech_process_run(const SpeechProcessConfig *c,const char *(*control)(void *),void *context,SpeechRuntimeResult *result) {
    if(!result)return "invalid_configuration";
    memset(result,0,sizeof(*result));
    if(!c||!c->self||!c->package_path||!c->manifest_sha256||!c->workspace_path||!c->process)return "invalid_configuration";
    if(c->process->guardian>0)return "speech_process_active";
    const char *e=control?control(context):NULL;if(e)return e;
    struct stat directory,named,input,output;
    if(c->workspace_fd<0||c->input_fd<0||c->output_fd<0||c->package_fd<0||c->residency_fd<0||
        fstat(c->workspace_fd,&directory)||!S_ISDIR(directory.st_mode)||directory.st_uid!=geteuid()||(directory.st_mode&077)||
        lstat(c->workspace_path,&named)||!same(&directory,&named)||
        fstat(c->input_fd,&input)||!S_ISREG(input.st_mode)||input.st_uid!=geteuid()||(input.st_mode&077)||input.st_nlink!=1||
        (fcntl(c->input_fd,F_GETFL)&O_ACCMODE)!=O_RDONLY||
        fstatat(c->workspace_fd,"request.frame",&named,AT_SYMLINK_NOFOLLOW)||!same(&input,&named)||
        fstat(c->output_fd,&output)||!S_ISREG(output.st_mode)||output.st_uid!=geteuid()||(output.st_mode&077)||output.st_nlink!=1||output.st_size!=0||
        (fcntl(c->output_fd,F_GETFL)&O_ACCMODE)!=O_RDWR||
        fstatat(c->workspace_fd,"result.frame",&named,AT_SYMLINK_NOFOLLOW)||!same(&output,&named))return "invalid_speech_workspace";
    char *argv=NULL;size_t bytes=0;FILE *f=open_memstream(&argv,&bytes);if(!f)return "speech_resource_limit";
    fputc('[',f);json_quote(f,c->self,strlen(c->self));fputs(",\"--inference-speech-worker\",",f);
    json_quote(f,c->package_path,strlen(c->package_path));fputc(',',f);json_quote(f,c->manifest_sha256,strlen(c->manifest_sha256));fputc(']',f);
    if(fclose(f)){free(argv);return "speech_resource_limit";}
    EngineProcess *p=c->process;
    e=engine_process_start_bound_protocol_sized(c->self,argv,c->lifetime_seconds,c->log_limit,c->residency_fd,c->package_fd,
        c->workspace_path,c->workspace_fd,c->input_fd,c->output_fd,SPEECH_PACKET_LIMIT,p);
    free(argv);
    if(e)return e;
    Progress streamed={0};sha256_init(&streamed.hash);
    for(;;) {
        e=control?control(context):NULL;if(e)break;
        if(c->pcm_sink){e=progress(c->output_fd,&streamed,c->pcm_sink,c->pcm_context,control,context,0);if(e)break;}
        struct pollfd watch={p->response,POLLIN|POLLHUP,0};int n=poll(&watch,1,10);
        if(n<0&&errno==EINTR)continue;
        if(n<0){e="speech_process_failed";break;}
        if(n>0)break;
    }
    const char *stopped=engine_process_stop(p);
    if(stopped&&!strcmp(stopped,"engine_shutdown_pending"))return stopped;
    if(e)return e;
    if(stopped)return stopped;
    if(strcmp(p->reason,"completed"))return "speech_process_failed";
    e=speech_worker_result(c->output_fd,control,context,result);
    if(!e&&c->pcm_sink)e=progress(c->output_fd,&streamed,c->pcm_sink,c->pcm_context,control,context,1);
    if(e)memset(result,0,sizeof(*result));
    return e;
}
