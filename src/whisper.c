#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "whisper.h"
#include "artifact.h"
#include "admission.h"
#include "budget.h"
#include "engine_process.h"
#include "sha256.h"
#include <fcntl.h>
#include <poll.h>
#include <sys/stat.h>
#include <unistd.h>

#define WAV_MAX 960044u
#define SRT_MAX 16384u
static int fail(FILE *out,const char *error){fprintf(out,"{\"contract_version\":1,\"error\":{\"code\":\"%s\"}}\n",error);return 1;}
static int fields(const Json *j,const char *allowed){
    for(int t=1;t<j->tokens[0].next;t=j->tokens[j->tokens[t].next].next){char *key=json_string(j,t),match[128];int n=snprintf(match,sizeof(match),"|%s|",key?key:"");free(key);if(n<0||(size_t)n>=sizeof(match)||!strstr(allowed,match))return 0;}return 1;
}
static const char *allowed="|contract_version|op|provider_id|audio_wav_base64|operation_id|parent_operation_id|timeout_ms|";
char *whisper_normalized(const InferenceConfig *cfg,const Json *j){
    if(!cfg||!cfg->provider_id||!cfg->whisper||!cfg->whisper->model_sha256||!cfg->whisper->executable_sha256)return NULL;
    if(!fields(j,allowed))return NULL;
    char *provider=json_string(j,json_get(j,0,"provider_id")),*audio=json_string(j,json_get(j,0,"audio_wav_base64"));
    char *parent=json_string(j,json_get(j,0,"parent_operation_id"));int has_parent=json_get(j,0,"parent_operation_id")>=0;
    size_t budget=0;int token=json_get(j,0,"timeout_ms"),valid=provider&&audio&&(!has_parent||(parent&&*parent&&strlen(parent)<=128))&&(token<0||(json_uint(j,token,&budget)&&budget&&budget<=60000));
    if(parent)for(const char *p=parent;*p;p++)if(!isalnum((unsigned char)*p)&&!strchr("-_:.",*p))valid=0;
    char *body=NULL;size_t length=0;FILE *f=valid?open_memstream(&body,&length):NULL;
    if(f){fputs("{\"op\":\"transcribe\",\"provider_id\":",f);json_quote(f,provider,strlen(provider));fputs(",\"audio\":",f);json_quote(f,audio,strlen(audio));
        fputs(",\"configured_provider\":",f);json_quote(f,cfg->provider_id,strlen(cfg->provider_id));
        fputs(",\"engine_sha256\":",f);json_quote(f,cfg->whisper->executable_sha256,strlen(cfg->whisper->executable_sha256));fputs(",\"model_sha256\":",f);json_quote(f,cfg->whisper->model_sha256,strlen(cfg->whisper->model_sha256));
        if(parent){fputs(",\"parent_operation_id\":",f);json_quote(f,parent,strlen(parent));}if(token>=0)fprintf(f,",\"timeout_ms\":%zu",budget);fputc('}',f);
        if(fclose(f)){free(body);body=NULL;}}
    free(provider);free(audio);free(parent);return body;
}
static int base64_value(unsigned char c){if(c>='A'&&c<='Z')return c-'A';if(c>='a'&&c<='z')return c-'a'+26;if(c>='0'&&c<='9')return c-'0'+52;return c=='+'?62:c=='/'?63:-1;}
static unsigned char *decode(const char *text,size_t *length){
    size_t n=strlen(text);if(!n||n%4||n>((WAV_MAX+2)/3)*4)return NULL;unsigned char *out=malloc(n/4*3);if(!out)return NULL;size_t used=0;
    for(size_t i=0;i<n;i+=4){int a=base64_value((unsigned char)text[i]),b=base64_value((unsigned char)text[i+1]),c=base64_value((unsigned char)text[i+2]),d=base64_value((unsigned char)text[i+3]);
        int pad_c=text[i+2]=='=',pad_d=text[i+3]=='=';
        if(a<0||b<0||(!pad_c&&c<0)||(!pad_d&&d<0)||(pad_c&&!pad_d)||((pad_c||pad_d)&&i+4!=n)||(pad_c&&(b&15))||(!pad_c&&pad_d&&(c&3))){free(out);return NULL;}
        out[used++]=(unsigned char)((a<<2)|(b>>4));if(!pad_c)out[used++]=(unsigned char)((b<<4)|(c>>2));if(!pad_d)out[used++]=(unsigned char)((c<<6)|d);}
    if(used>WAV_MAX){free(out);return NULL;}*length=used;return out;
}
static uint32_t u32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static uint16_t u16(const unsigned char *p){return (uint16_t)((unsigned)p[0]|(unsigned)p[1]<<8);}
static int wav(const unsigned char *p,size_t n){return n>=76&&n<=WAV_MAX&&!((n-44)%32)&&!memcmp(p,"RIFF",4)&&u32(p+4)==n-8&&!memcmp(p+8,"WAVEfmt ",8)&&u32(p+16)==16&&u16(p+20)==1&&u16(p+22)==1&&u32(p+24)==16000&&u32(p+28)==32000&&u16(p+32)==2&&u16(p+34)==16&&!memcmp(p+36,"data",4)&&u32(p+40)==n-44;}
static const char *verified(const char *path,const char *digest,uint64_t ceiling,int executable,int *descriptor){
    *descriptor=-1;if(!path||path[0]!='/')return "invalid_configuration";
    int fd=open(path,O_RDONLY|O_NOFOLLOW|O_CLOEXEC|O_NONBLOCK);if(fd<0)return "artifact_unavailable";struct stat s;
    const char *error=fstat(fd,&s)||!S_ISREG(s.st_mode)||s.st_size<=0||s.st_nlink!=1||s.st_uid!=geteuid()||(s.st_mode&022)||(executable&&!(s.st_mode&0111))?"unsupported_artifact":NULL;
    if(!error)error=artifact_verify(fd,(uint64_t)s.st_size,ceiling,digest);
    if(error)close(fd);else *descriptor=fd;return error;
}
/* Same SRT timing convention as audio_srt_timestamp in the monolith, with
 * integer milliseconds and full-line validation at the new public boundary. */
static int timestamp(const char *line,size_t *start,size_t *end){
    unsigned sh,sm,ss,sms,eh,em,es,ems;int consumed=0;
    if(sscanf(line,"%u:%u:%u,%u --> %u:%u:%u,%u%n",&sh,&sm,&ss,&sms,&eh,&em,&es,&ems,&consumed)!=8||line[consumed]||sh>0||eh>0||sm>59||em>59||ss>59||es>59||sms>999||ems>999)return 0;
    *start=((size_t)sm*60+ss)*1000+sms;*end=((size_t)em*60+es)*1000+ems;return *end>*start;
}
static char *segments(char *srt,size_t duration,char **transcript){
    char *body=NULL,*text=NULL;size_t bytes=0,text_bytes=0;FILE *out=open_memstream(&body,&bytes),*plain=open_memstream(&text,&text_bytes);
    if(!out||!plain){if(out)fclose(out);if(plain)fclose(plain);free(body);free(text);return NULL;}
    fputc('[',out);char *cursor=srt;size_t count=0,previous=0;int valid=1;
    while(*cursor&&valid){while(*cursor=='\r'||*cursor=='\n')cursor++;if(!*cursor)break;
        char *line=cursor,*newline=strchr(cursor,'\n');if(!newline){valid=0;break;}*newline=0;cursor=newline+1;size_t n=strlen(line);if(n&&line[n-1]=='\r')line[--n]=0;
        if(!n){valid=0;break;}for(size_t i=0;i<n;i++)if(!isdigit((unsigned char)line[i]))valid=0;
        line=cursor;newline=strchr(cursor,'\n');if(!newline){valid=0;break;}*newline=0;cursor=newline+1;n=strlen(line);if(n&&line[n-1]=='\r')line[--n]=0;
        size_t start=0,end=0;if(!timestamp(line,&start,&end)||start<previous||start>=duration||++count>128){valid=0;break;}
        /* Whisper timestamp tokens advance in20ms steps. Only the first grid
         * boundary beyond the input end may be clipped; larger overruns refuse. */
        if(end>duration){if(end!=((duration+19)/20)*20){valid=0;break;}end=duration;}
        previous=start;
        char cue[SRT_MAX+1];size_t used=0;
        while(*cursor){line=cursor;newline=strchr(cursor,'\n');if(newline){*newline=0;cursor=newline+1;}else cursor+=strlen(cursor);n=strlen(line);if(n&&line[n-1]=='\r')line[--n]=0;if(!n)break;
            if(n+(used?1:0)>SRT_MAX-used){valid=0;break;}if(used)cue[used++]='\n';memcpy(cue+used,line,n);used+=n;}
        if(!valid||!used){valid=0;break;}cue[used]=0;if(count>1){fputc(',',out);fputc('\n',plain);}fprintf(out,"{\"start_ms\":%zu,\"end_ms\":%zu,\"text\":",start,end);json_quote(out,cue,used);fputc('}',out);fwrite(cue,1,used,plain);
    }
    fputc(']',out);if(fclose(out))valid=0;if(fclose(plain))valid=0;
    if(!valid||!count){free(body);free(text);return NULL;}*transcript=text;return body;
}
static const char *run(const InferenceConfig *cfg,const unsigned char *audio,size_t bytes,char **result,char **text){
    int lease=-1,model=-1,executable=-1;const char *error=inference_interrupted(cfg);if(!error)error=inference_schedule(cfg,&lease);
    if(!error)error=verified(cfg->whisper->model,cfg->whisper->model_sha256,536870912u,0,&model);
    if(!error)error=inference_interrupted(cfg);
    if(!error)error=verified(cfg->whisper->executable,cfg->whisper->executable_sha256,67108864u,1,&executable);
    if(!error)error=inference_interrupted(cfg);
    char workspace[]="/tmp/samosa-inference-whisper-XXXXXX",input[256]={0},output[256]={0},srt_path[260]={0};int created=0,directory=-1,result_fd=-1;
    if(!error){created=mkdtemp(workspace)!=NULL;if(!created)error="engine_io_failed";else{snprintf(input,sizeof(input),"%s/input.wav",workspace);snprintf(output,sizeof(output),"%s/result",workspace);snprintf(srt_path,sizeof(srt_path),"%s.srt",output);}}
    if(!error){int fd=open(input,O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);if(fd<0)error="engine_io_failed";else{size_t used=0;while(used<bytes){ssize_t n=write(fd,audio+used,bytes-used);if(n<0&&errno==EINTR)continue;if(n<=0){error="engine_io_failed";break;}used+=(size_t)n;}close(fd);}}
    if(!error){directory=open(workspace,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);result_fd=open(srt_path,O_RDWR|O_CREAT|O_EXCL|O_CLOEXEC,0600);if(directory<0||result_fd<0)error="engine_io_failed";}
    char *argv=NULL;size_t argv_bytes=0;FILE *f=error?NULL:open_memstream(&argv,&argv_bytes);if(!error&&!f)error="resource_limit";
    if(f){const char *args[]={cfg->whisper->executable,"-m","/dev/fd/5","-f",input,"-l","en","-osrt","-of",output};fputc('[',f);for(size_t i=0;i<sizeof(args)/sizeof(args[0]);i++){if(i)fputc(',',f);json_quote(f,args[i],strlen(args[i]));}fputc(']',f);if(fclose(f))error="resource_limit";}
    EngineProcess process={.guardian=-1,.control=-1,.response=-1};
    if(!error){double remaining=cfg->deadline_monotonic?cfg->deadline_monotonic-inference_now():60;
        if(remaining<=0)error="deadline_exceeded";else error=engine_process_start_bound_workspace(cfg->whisper->self,argv,remaining,262144,lease,model,workspace,directory,&process);}
    while(!error&&process.guardian>0){error=inference_interrupted(cfg);if(error)break;struct stat size;if(fstat(result_fd,&size)||size.st_size>SRT_MAX){error="invalid_provider_response";break;}struct pollfd watch={process.response,POLLIN|POLLHUP,0};int ready=poll(&watch,1,10);if(ready<0&&errno==EINTR)continue;if(ready<0){error="engine_io_failed";break;}if(ready>0){const char *terminal=engine_process_stop(&process);if(terminal)error=!strcmp(terminal,"lifetime_exceeded")?"deadline_exceeded":"provider_process_failed";if(!error&&strcmp(process.reason,"completed"))error="provider_process_failed";break;}}
    if(process.guardian>0){const char *stopped=engine_process_stop(&process);if(stopped&&!strcmp(stopped,"engine_shutdown_pending"))error=stopped;}
    if(!error)error=inference_interrupted(cfg);
    if(!error){struct stat s;if(fstat(model,&s)||s.st_size<=0)error="artifact_changed";else error=artifact_verify(model,(uint64_t)s.st_size,536870912u,cfg->whisper->model_sha256);}
    if(!error)error=inference_interrupted(cfg);
    if(!error){int fd=result_fd;struct stat s;char *srt=NULL;
        if(fd<0||fstat(fd,&s)||!S_ISREG(s.st_mode)||s.st_uid!=geteuid()||s.st_nlink!=0||s.st_size<=0||s.st_size>SRT_MAX)error="invalid_provider_response";
        else{srt=malloc((size_t)s.st_size+1);if(!srt)error="resource_limit";else{size_t used=0;while(used<(size_t)s.st_size){ssize_t n=read(fd,srt+used,(size_t)s.st_size-used);if(n<0&&errno==EINTR)continue;if(n<=0){error="invalid_provider_response";break;}used+=(size_t)n;}srt[used]=0;if(!error&&(!utf8_valid((unsigned char*)srt,used)||!(*result=segments(srt,(bytes-44)/32,text))))error="invalid_provider_response";}}
        free(srt);}
    free(argv);if(created){unlink(srt_path);unlink(input);rmdir(workspace);}if(result_fd>=0)close(result_fd);if(directory>=0)close(directory);if(model>=0)close(model);if(executable>=0)close(executable);if(lease>=0)close(lease);return error;
}
int whisper_dispatch(const InferenceConfig *cfg,const Json *j,FILE *out){
    if(!cfg||!cfg->whisper||!cfg->whisper->self||!cfg->whisper->executable||!cfg->whisper->model||!cfg->whisper->executable_sha256||!cfg->whisper->model_sha256||!cfg->admission_state)return fail(out,"invalid_configuration");
    char *op=json_string(j,json_get(j,0,"op"));if(!op)return fail(out,"invalid_request");
    if(!strcmp(op,"describe")){free(op);if(!fields(j,"|contract_version|op|"))return fail(out,"unknown_field");fprintf(out,"{\"contract_version\":1,\"component\":\"samosa-inference\",\"provider_id\":\"%s\",\"capabilities\":[\"transcription\",\"operations\",\"cancellation\",\"request_budgets\"],\"language\":\"en\",\"input\":\"canonical_pcm16_mono_16000_wav_base64\",\"max_duration_ms\":30000,\"quality\":\"whisper_cpp\"}\n",cfg->provider_id);return 0;}
    if(strcmp(op,"transcribe")){free(op);return fail(out,"unsupported_operation");}free(op);
    if(!fields(j,allowed))return fail(out,"unknown_field");
    char *provider=json_string(j,json_get(j,0,"provider_id")),*encoded=json_string(j,json_get(j,0,"audio_wav_base64"));
    const char *error=!provider||!encoded?"invalid_request":strcmp(provider,cfg->provider_id)?"provider_unavailable":NULL;
    size_t bytes=0;unsigned char *audio=error?NULL:decode(encoded,&bytes);if(!error&&(!audio||!wav(audio,bytes)))error="invalid_audio";
    char *items=NULL,*text=NULL;if(!error)error=run(cfg,audio,bytes,&items,&text);
    char digest[65]={0};if(audio)hash_bytes(audio,bytes,digest);
    if(!error){fprintf(out,"{\"contract_version\":1,\"provider_id\":\"%s\",\"kind\":\"derived\",\"artifact_type\":\"transcript\",\"language\":\"en\",\"input_sha256\":\"%s\",\"duration_ms\":%zu,\"text\":",cfg->provider_id,digest,(bytes-44)/32);json_quote(out,text,strlen(text));fprintf(out,",\"segments\":%s,\"coverage\":{\"status\":\"partial\",\"reason\":\"speech_recognition_uncertain\"},\"provenance\":{\"engine\":\"whisper.cpp\",\"engine_sha256\":\"%s\",\"model_sha256\":\"%s\",\"coordinate_space\":\"input_audio_milliseconds\",\"timestamp_policy\":\"whisper_20ms_end_clipped_to_input\"}}\n",items,cfg->whisper->executable_sha256,cfg->whisper->model_sha256);}
    free(provider);free(encoded);free(audio);free(items);free(text);return error?fail(out,error):0;
}
