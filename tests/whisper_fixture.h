#ifndef WHISPER_FIXTURE_H
#define WHISPER_FIXTURE_H
#include "json.h"
#include "sha256.h"
#include <fcntl.h>
#include <signal.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>
#include <ftw.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL %d: %s\n",__LINE__,#x);exit(1);}}while(0)
typedef struct {const char *cli,*engine,*model,*state,*owner;char engine_sha[65],model_sha[65];} FixtureConfig;
typedef struct {pid_t pid;int output;} Invocation;
static inline void delay(void){struct timespec t={0,10000000};nanosleep(&t,NULL);}
static inline void write_all(int fd,const void *bytes,size_t n){size_t done=0;while(done<n){ssize_t used=write(fd,(const char*)bytes+done,n-done);if(used<0&&errno==EINTR)continue;CHECK(used>0);done+=(size_t)used;}}
static inline char *read_all(int fd,size_t limit){char *body=malloc(limit+1);CHECK(body);size_t n=0;for(;;){ssize_t used=read(fd,body+n,limit-n);if(used<0&&errno==EINTR)continue;CHECK(used>=0);if(!used)break;n+=(size_t)used;CHECK(n<limit);}body[n]=0;return body;}
static inline char *file_read(const char *path,size_t limit){int fd=open(path,O_RDONLY);CHECK(fd>=0);char *text=read_all(fd,limit);CHECK(!close(fd));return text;}
static inline void file_write(const char *path,const char *text){int fd=open(path,O_WRONLY|O_CREAT|O_TRUNC,0600);CHECK(fd>=0);write_all(fd,text,strlen(text));CHECK(!close(fd));}
static inline void file_sha(const char *path,char sha[65]){int fd=open(path,O_RDONLY);CHECK(fd>=0);Sha256 hash;sha256_init(&hash);unsigned char bytes[65536];for(;;){ssize_t n=read(fd,bytes,sizeof(bytes));if(n<0&&errno==EINTR)continue;CHECK(n>=0);if(!n)break;sha256_update(&hash,bytes,(size_t)n);}unsigned char digest[32];sha256_final(&hash,digest);sha256_hex(digest,sha);CHECK(!close(fd));}
static inline void file_copy(const char *source,const char *target){int input=open(source,O_RDONLY),output=open(target,O_WRONLY|O_CREAT|O_EXCL,0700);CHECK(input>=0&&output>=0);unsigned char bytes[65536];for(;;){ssize_t n=read(input,bytes,sizeof(bytes));if(n<0&&errno==EINTR)continue;CHECK(n>=0);if(!n)break;write_all(output,bytes,(size_t)n);}CHECK(!close(input));CHECK(!close(output));}
static inline Invocation spawn_request(char *const argv[],const char *request){int input[2],output[2];CHECK(!pipe(input));CHECK(!pipe(output));pid_t pid=fork();CHECK(pid>=0);if(!pid){close(input[1]);close(output[0]);if(dup2(input[0],STDIN_FILENO)<0||dup2(output[1],STDOUT_FILENO)<0)_exit(2);close(input[0]);close(output[1]);execv(argv[0],argv);_exit(2);}close(input[0]);close(output[1]);write_all(input[1],request,strlen(request));CHECK(!close(input[1]));return (Invocation){pid,output[0]};}
static inline char *finish(Invocation p,int expected){char *body=read_all(p.output,2097152);CHECK(!close(p.output));int status;CHECK(waitpid(p.pid,&status,0)==p.pid);if(!WIFEXITED(status)||WEXITSTATUS(status)!=expected)fprintf(stderr,"child status: %d; response: %s\n",status,body);CHECK(WIFEXITED(status)&&WEXITSTATUS(status)==expected);return body;}
/* Actual-model gates may select the common registry without replacing their
 * quality/provenance assertions or the legacy standalone fixture transport. */
static inline Invocation registry_launch(const FixtureConfig *c,const char *request,const char *provider){
    const char *path=getenv("SAMOSA_TEST_REGISTRY_CONFIG");CHECK(path);
    Json j;CHECK(json_parse(&j,request,strlen(request)));
    char *forward=NULL;const char *body=request;
    if(json_get(&j,0,"provider_id")<0){size_t length=strlen(request);CHECK(length&&request[length-1]=='}');
        forward=malloc(length+strlen(provider)+32);CHECK(forward);
        snprintf(forward,length+strlen(provider)+32,"%.*s,\"provider_id\":\"%s\"}",(int)length-1,request,provider);body=forward;}
    char *args[]={(char*)c->cli,"--config",(char*)path,NULL};Invocation result=spawn_request(args,body);free(forward);return result;
}
static inline void actual_registry(const FixtureConfig *c,const char *root,const char *provider,const char *kind,const char *extra){
    char path[1024];snprintf(path,sizeof(path),"%s/registry.json",root);
    char *body=NULL;size_t bytes=0;FILE *f=open_memstream(&body,&bytes);CHECK(f);
    fputs("{\"contract_version\":1,\"owner\":",f);json_quote(f,c->owner,strlen(c->owner));
    fprintf(f,",\"providers\":[{\"kind\":\"%s\",\"provider_id\":\"%s\",\"state\":",kind,provider);json_quote(f,c->state,strlen(c->state));
    fputs(",\"engine\":",f);json_quote(f,c->engine,strlen(c->engine));fputs(",\"model\":",f);json_quote(f,c->model,strlen(c->model));
    fprintf(f,",\"engine_sha256\":\"%s\",\"model_sha256\":\"%s\"%s},{\"kind\":\"fake_text\",\"provider_id\":\"contract-text\",\"state\":\"%s/text-state\"}]}",c->engine_sha,c->model_sha,extra?extra:"",root);
    CHECK(!fclose(f));file_write(path,body);free(body);CHECK(!setenv("SAMOSA_TEST_REGISTRY_CONFIG",path,1));
    char *args[]={(char*)c->cli,"--config",path,NULL};
    char *discovery=finish(spawn_request(args,"{\"contract_version\":1,\"op\":\"describe\"}"),0);CHECK(strstr(discovery,provider)&&strstr(discovery,"contract-text"));free(discovery);
    char *text=finish(spawn_request(args,"{\"contract_version\":1,\"op\":\"count_tokens\",\"provider_id\":\"contract-text\",\"prompt\":\"hello\"}"),0);CHECK(strstr(text,"fake-utf8-byte-v1"));free(text);
}
static inline void actual_arguments(const FixtureConfig *c,const char *path){
    const char *registry=getenv("SAMOSA_TEST_REGISTRY_CONFIG");
    const char *legacy[]={"--whisper-cli",c->engine,"--whisper-model",c->model,"--whisper-cli-sha256",c->engine_sha,"--whisper-model-sha256",c->model_sha,"--admission-state",c->state,"--owner",c->owner};
    const char *r[]={"--config",registry};const char *const *values=registry?r:legacy;size_t count=registry?2:12;
    char *body=NULL;size_t n=0;FILE *f=open_memstream(&body,&n);CHECK(f);fputc('[',f);
    for(size_t i=0;i<count;i++){if(i)fputc(',',f);json_quote(f,values[i],strlen(values[i]));}fputc(']',f);CHECK(!fclose(f));file_write(path,body);free(body);
}
static inline Invocation launch(const FixtureConfig *c,const char *request){if(getenv("SAMOSA_TEST_REGISTRY_CONFIG"))return registry_launch(c,request,"whisper-base-en-v1");char *args[]={(char*)c->cli,"--whisper-cli",(char*)c->engine,"--whisper-model",(char*)c->model,"--whisper-cli-sha256",(char*)c->engine_sha,"--whisper-model-sha256",(char*)c->model_sha,"--admission-state",(char*)c->state,"--owner",(char*)c->owner,NULL};return spawn_request(args,request);}
static inline void parse(const char *body,Json *j){CHECK(json_parse(j,body,strlen(body)));}
static inline char *string_field(const char *body,const char *field){Json j;parse(body,&j);char *value=json_string(&j,json_get(&j,0,field));CHECK(value);return value;}
static inline void error_check(const char *body,const char *error){Json j;parse(body,&j);char *value=json_string(&j,json_get(&j,json_get(&j,0,"error"),"code"));if(!value||strcmp(value,error))fprintf(stderr,"response: %s\n",body);CHECK(value&&!strcmp(value,error));free(value);}
static inline char *invoke(const FixtureConfig *c,const char *request,const char *error){char *body=finish(launch(c,request),error?1:0);if(error)error_check(body,error);return body;}
static inline char *transcribe(const char *audio,const char *id,size_t timeout){char *body=NULL;size_t n;FILE *out=open_memstream(&body,&n);CHECK(out);fputs("{\"contract_version\":1,\"op\":\"transcribe\",\"provider_id\":\"whisper-base-en-v1\",\"audio_wav_base64\":",out);json_quote(out,audio,strlen(audio));if(id){fputs(",\"operation_id\":",out);json_quote(out,id,strlen(id));fputs(",\"parent_operation_id\":\"sources-parent\"",out);}if(timeout)fprintf(out,",\"timeout_ms\":%zu",timeout);fputc('}',out);CHECK(!fclose(out));return body;}
static inline char *control(const char *op,const char *id){char body[512];snprintf(body,sizeof(body),"{\"contract_version\":1,\"op\":\"%s\",\"operation_id\":\"%s\"}",op,id);return strdup(body);}
static inline char *encode(const unsigned char *data,size_t n){static const char alphabet[]="ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";char *out=malloc((n+2)/3*4+1);CHECK(out);size_t used=0;for(size_t i=0;i<n;i+=3){unsigned a=data[i],b=i+1<n?data[i+1]:0,c=i+2<n?data[i+2]:0;out[used++]=alphabet[a>>2];out[used++]=alphabet[((a&3)<<4)|(b>>4)];out[used++]=i+1<n?alphabet[((b&15)<<2)|(c>>6)]:'=';out[used++]=i+2<n?alphabet[c&63]:'=';}out[used]=0;return out;}
static inline void put32(unsigned char *p,uint32_t n){for(size_t i=0;i<4;i++)p[i]=(unsigned char)(n>>(i*8));}
static inline uint32_t get32(const unsigned char *p){return (uint32_t)p[0]|(uint32_t)p[1]<<8|(uint32_t)p[2]<<16|(uint32_t)p[3]<<24;}
static inline char *silence(size_t ms){size_t bytes=44+ms*32;unsigned char *wav=calloc(bytes,1);CHECK(wav);memcpy(wav,"RIFF",4);put32(wav+4,(uint32_t)bytes-8);memcpy(wav+8,"WAVEfmt ",8);put32(wav+16,16);wav[20]=1;wav[22]=1;put32(wav+24,16000);put32(wav+28,32000);wav[32]=2;wav[34]=16;memcpy(wav+36,"data",4);put32(wav+40,(uint32_t)bytes-44);char *out=encode(wav,bytes);free(wav);return out;}
static inline int remove_file(const char *path,const struct stat *info,int type,struct FTW *f){(void)info;(void)type;(void)f;return remove(path);}
static inline void cleanup(const char *root){CHECK(!nftw(root,remove_file,32,FTW_DEPTH|FTW_PHYS));}
#endif
