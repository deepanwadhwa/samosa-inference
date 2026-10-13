#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "remote_artifact.h"
#include "sha256.h"
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <signal.h>
#include <fcntl.h>
#include <unistd.h>
#include <time.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
static unsigned checks;
#define CHECK(x) do{checks++;if(!(x)){fprintf(stderr,"FAIL remote %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static int cancel(void *count){return ++*(int*)count>1;}
static int write_all(int fd,const void *bytes,size_t length){const char *p=bytes;while(length){ssize_t n=write(fd,p,length);if(n<0&&errno==EINTR)continue;if(n<=0)return 0;p+=n;length-=(size_t)n;}return 1;}
static void payload(int fd,char value,unsigned blocks){char block[32768];memset(block,value,sizeof(block));for(unsigned i=0;i<blocks;i++)if(!write_all(fd,block,sizeof(block)))break;}
static void serve(int listener){signal(SIGPIPE,SIG_IGN);unsigned cut=0,death=0,torn=0,big=0;for(;;){int fd=accept(listener,NULL,NULL);if(fd<0)_exit(2);char request[2048];ssize_t n=read(fd,request,sizeof(request)-1);if(n<=0){close(fd);continue;}request[n]=0;
        if(strstr(request,"/big-death ")){
            if(!big++){const char *h="HTTP/1.1 200 OK\r\nContent-Length: 2097152\r\nConnection: close\r\n\r\n";(void)write_all(fd,h,strlen(h));payload(fd,'A',32);struct timespec settle={0,100000000};nanosleep(&settle,NULL);payload(fd,'B',1);struct timespec pause={2,0};nanosleep(&pause,NULL);payload(fd,'B',31);}
            else if(strstr(request,"Range: bytes=1048576-\r\n")){const char *h="HTTP/1.1 206 Partial Content\r\nContent-Range: bytes 1048576-2097151/2097152\r\nContent-Length: 1048576\r\nConnection: close\r\n\r\n";(void)write_all(fd,h,strlen(h));payload(fd,'B',32);}
            else {const char *h="HTTP/1.1 416 Range Not Satisfiable\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";(void)write_all(fd,h,strlen(h));}
            close(fd);continue;
        }
        if(strstr(request,"/death ")&&!death++){const char *first="HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\na";
            (void)write(fd,first,strlen(first));struct timespec pause={1,0};nanosleep(&pause,NULL);(void)write(fd,"bc",2);close(fd);continue;}
        const char *response="HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nabc";
        if(strstr(request,"/death ")&&strstr(request,"Range: bytes=1-\r\n"))response="HTTP/1.1 206 Partial Content\r\nContent-Range: bytes 1-2/3\r\nContent-Length: 2\r\nConnection: close\r\n\r\nbc";
        if(strstr(request,"/checkpoint-fallback ")&&!torn++)response="HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\na";
        if(strstr(request,"/auth ")&&!strstr(request,"Authorization: Bearer private-secret\r\n"))response="HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        if(strstr(request," /range")){
            if(!strstr(request,"Range: bytes=1-\r\n"))response="HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\na";
            else {
                response="HTTP/1.1 206 Partial Content\r\nContent-Range: bytes 1-2/3\r\nContent-Length: 2\r\nConnection: close\r\n\r\nbc";
                if(strstr(request,"/range-bad "))response="HTTP/1.1 206 Partial Content\r\nContent-Range: bytes 0-1/3\r\nContent-Length: 2\r\nConnection: close\r\n\r\nbc";
                if(strstr(request,"/range-total "))response="HTTP/1.1 206 Partial Content\r\nContent-Range: bytes 1-2/4\r\nContent-Length: 2\r\nConnection: close\r\n\r\nbc";
                if(strstr(request,"/range-end "))response="HTTP/1.1 206 Partial Content\r\nContent-Range: bytes 1-1/3\r\nContent-Length: 2\r\nConnection: close\r\n\r\nbc";
                if(strstr(request,"/range-nolength "))response="HTTP/1.1 206 Partial Content\r\nContent-Range: bytes 1-2/3\r\nConnection: close\r\n\r\nbc";
                if(strstr(request,"/range-missing "))response="HTTP/1.1 206 Partial Content\r\nContent-Length: 2\r\nConnection: close\r\n\r\nbc";
                if(strstr(request,"/range-corrupt "))response="HTTP/1.1 206 Partial Content\r\nContent-Range: bytes 1-2/3\r\nContent-Length: 2\r\nConnection: close\r\n\r\nxy";
                if(strstr(request,"/range-length "))response="HTTP/1.1 206 Partial Content\r\nContent-Range: bytes 1-2/3\r\nContent-Length: 3\r\nConnection: close\r\n\r\nbcx";
                if(strstr(request,"/range-duplicate "))response="HTTP/1.1 206 Partial Content\r\nContent-Range: bytes 1-2/3\r\nContent-Range: bytes 1-2/3\r\nContent-Length: 2\r\nConnection: close\r\n\r\nbc";
                if(strstr(request,"/range-overflow "))response="HTTP/1.1 206 Partial Content\r\nContent-Range: bytes 18446744073709551616-2/3\r\nContent-Length: 2\r\nConnection: close\r\n\r\nbc";
                if(strstr(request,"/range-redirect "))response="HTTP/1.1 302 Found\r\nLocation: /range-target\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
                if(strstr(request,"/range-fallback-conflict "))response="HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nxyz";
            }
        }
        if(strstr(request,"/bad "))response="HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nxyz";
        if(strstr(request,"/short "))response="HTTP/1.1 200 OK\r\nContent-Length: 2\r\nConnection: close\r\n\r\nab";
        if(strstr(request,"/partial "))response="HTTP/1.1 206 Partial Content\r\nContent-Length: 3\r\nConnection: close\r\n\r\nabc";
        if(strstr(request,"/cut ")&&!cut++)response="HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\na";
        if(strstr(request,"/oversize "))response="HTTP/1.1 200 OK\r\nConnection: close\r\n\r\nabcx";
        if(strstr(request,"/encoding "))response="HTTP/1.1 200 OK\r\nContent-Encoding: gzip\r\nContent-Length: 3\r\nConnection: close\r\n\r\nabc";
        if(strstr(request,"/redirect "))response="HTTP/1.1 302 Found\r\nLocation: /ok\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        if(strstr(request,"/foreign "))response="HTTP/1.1 302 Found\r\nLocation: http://localhost:1/x\r\nContent-Length: 0\r\nConnection: close\r\n\r\n";
        if(strstr(request,"/slow ")){struct timespec pause={1,0};nanosleep(&pause,NULL);}
        (void)write(fd,response,strlen(response));close(fd);}}
static ArtifactConfig config(const char *root,const char *name){static char states[32][4096];static unsigned slot;CHECK(slot<32);snprintf(states[slot],4096,"%s/%s",root,name);
    return (ArtifactConfig){.state_path=states[slot++],.sha256="ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad",.size=3,.byte_ceiling=3,.disk_ceiling=96};}
static void credential_redirect(const char *root){
    int listeners[2];unsigned ports[2];pid_t children[2];
    for(unsigned i=0;i<2;i++){listeners[i]=socket(AF_INET,SOCK_STREAM,0);CHECK(listeners[i]>=0);
        struct sockaddr_in a={.sin_family=AF_INET,.sin_addr.s_addr=htonl(INADDR_LOOPBACK)};CHECK(!bind(listeners[i],(struct sockaddr*)&a,sizeof(a)));CHECK(!listen(listeners[i],4));socklen_t n=sizeof(a);CHECK(!getsockname(listeners[i],(struct sockaddr*)&a,&n));ports[i]=ntohs(a.sin_port);}
    for(unsigned i=0;i<2;i++){children[i]=fork();CHECK(children[i]>=0);if(!children[i]){close(listeners[i^1]);signal(SIGPIPE,SIG_IGN);
        for(;;){int fd=accept(listeners[i],NULL,NULL);if(fd<0)_exit(2);char request[2048];ssize_t n=read(fd,request,sizeof(request)-1);if(n<=0){close(fd);continue;}request[n]=0;
            char response[512];if(!i&&strstr(request,"Authorization: Bearer private-secret\r\n"))snprintf(response,sizeof(response),"HTTP/1.1 302 Found\r\nLocation: http://127.0.0.1:%u/target\r\nContent-Length: 0\r\nConnection: close\r\n\r\n",ports[1]);
            else if(i&&!strstr(request,"Authorization:"))strcpy(response,"HTTP/1.1 200 OK\r\nContent-Length: 3\r\nConnection: close\r\n\r\nabc");
            else strcpy(response,"HTTP/1.1 401 Unauthorized\r\nContent-Length: 0\r\nConnection: close\r\n\r\n");
            (void)write_all(fd,response,strlen(response));close(fd);}
    }}
    CHECK(!close(listeners[0]));CHECK(!close(listeners[1]));char url[256];snprintf(url,sizeof(url),"http://127.0.0.1:%u/source",ports[0]);
    RemoteArtifactSource source={.url=url,.loopback_http=1,.bearer_key="private-secret",.timeout_ms=2000};ArtifactConfig c=config(root,"credential-redirect");CHECK(!artifact_install_remote(&c,&source));
    for(unsigned i=0;i<2;i++){CHECK(!kill(children[i],SIGKILL));int status;CHECK(waitpid(children[i],&status,0)==children[i]&&WIFSIGNALED(status));}
}
int main(void){char root[]="/tmp/samosa-remote-artifact.XXXXXX";CHECK(mkdtemp(root));int listener=socket(AF_INET,SOCK_STREAM,0);CHECK(listener>=0);
    struct sockaddr_in address={.sin_family=AF_INET,.sin_addr.s_addr=htonl(INADDR_LOOPBACK)};CHECK(!bind(listener,(struct sockaddr*)&address,sizeof(address)));CHECK(!listen(listener,8));socklen_t length=sizeof(address);CHECK(!getsockname(listener,(struct sockaddr*)&address,&length));
    pid_t server=fork();CHECK(server>=0);if(!server)serve(listener);CHECK(!close(listener));char url[256];snprintf(url,sizeof(url),"http://127.0.0.1:%u/ok",ntohs(address.sin_port));
    RemoteArtifactSource source={.url=url,.loopback_http=1,.timeout_ms=2000};ArtifactConfig c=config(root,"ok");CHECK(!artifact_install_remote(&c,&source));const char *status;CHECK(!artifact_status(&c,&status)&&!strcmp(status,"verified"));
    CHECK(!kill(server,SIGKILL));int result;CHECK(waitpid(server,&result,0)==server);CHECK(!artifact_install_remote(&c,&source));
    listener=socket(AF_INET,SOCK_STREAM,0);CHECK(listener>=0);address.sin_port=0;CHECK(!bind(listener,(struct sockaddr*)&address,sizeof(address)));CHECK(!listen(listener,8));length=sizeof(address);CHECK(!getsockname(listener,(struct sockaddr*)&address,&length));
    server=fork();CHECK(server>=0);if(!server)serve(listener);CHECK(!close(listener));unsigned port=ntohs(address.sin_port);
    c=config(root,"bad");snprintf(url,sizeof(url),"http://127.0.0.1:%u/bad",port);CHECK(!strcmp(artifact_install_remote(&c,&source),"artifact_checksum_mismatch"));
    c=config(root,"short");snprintf(url,sizeof(url),"http://127.0.0.1:%u/short",port);CHECK(!strcmp(artifact_install_remote(&c,&source),"artifact_size_mismatch"));
    c=config(root,"partial");snprintf(url,sizeof(url),"http://127.0.0.1:%u/partial",port);CHECK(!strcmp(artifact_install_remote(&c,&source),"artifact_range_mismatch"));
    c=config(root,"redirect");snprintf(url,sizeof(url),"http://127.0.0.1:%u/redirect",port);CHECK(!artifact_install_remote(&c,&source));
    c=config(root,"foreign");snprintf(url,sizeof(url),"http://127.0.0.1:%u/foreign",port);CHECK(!strcmp(artifact_install_remote(&c,&source),"unsupported_artifact_url"));
    c=config(root,"resume");snprintf(url,sizeof(url),"http://127.0.0.1:%u/cut",port);CHECK(!strcmp(artifact_install_remote(&c,&source),"artifact_transfer_failed"));
    CHECK(!strcmp(artifact_install_remote(&c,&source),"artifact_resume_required"));c.resume=1;
    snprintf(url,sizeof(url),"http://127.0.0.1:%u/ok",port);CHECK(!strcmp(artifact_install_remote(&c,&source),"artifact_source_changed"));
    snprintf(url,sizeof(url),"http://127.0.0.1:%u/cut",port);CHECK(!artifact_install_remote(&c,&source));
    c=config(root,"oversize");snprintf(url,sizeof(url),"http://127.0.0.1:%u/oversize",port);CHECK(!strcmp(artifact_install_remote(&c,&source),"artifact_size_mismatch"));
    c=config(root,"encoding");snprintf(url,sizeof(url),"http://127.0.0.1:%u/encoding",port);CHECK(!strcmp(artifact_install_remote(&c,&source),"unsupported_artifact_encoding"));
    c=config(root,"cancel");int counter=0;c.cancelled=cancel;c.cancellation=&counter;
    snprintf(url,sizeof(url),"http://127.0.0.1:%u/slow",port);CHECK(!strcmp(artifact_install_remote(&c,&source),"artifact_interrupted"));
    c=config(root,"death");source.checkpoint_bytes=1;snprintf(url,sizeof(url),"http://127.0.0.1:%u/death",port);pid_t downloader=fork();CHECK(downloader>=0);
    if(!downloader)_exit(artifact_install_remote(&c,&source)?1:0);
    char stage_path[4096],checkpoint_path[4096];CHECK(snprintf(stage_path,sizeof(stage_path),"%s/%s/stage",c.state_path,c.sha256)<(int)sizeof(stage_path));
    CHECK(snprintf(checkpoint_path,sizeof(checkpoint_path),"%s/%s/remote-checkpoint",c.state_path,c.sha256)<(int)sizeof(checkpoint_path));int partial=0;
    for(unsigned i=0;i<200;i++){struct stat st;char record[144],digest[65];int fd=open(checkpoint_path,O_RDONLY);ssize_t n=fd>=0?pread(fd,record,sizeof(record),144):-1;if(fd>=0)close(fd);
        if(n==144){hash_bytes(record,80,digest);if(!stat(stage_path,&st)&&st.st_size==1&&!memcmp(record,"0000000000000001",16)&&!memcmp(record+80,digest,64)){partial=1;break;}}
        struct timespec pause={0,10000000};nanosleep(&pause,NULL);}CHECK(partial);
    CHECK(!kill(downloader,SIGKILL));CHECK(waitpid(downloader,&result,0)==downloader&&WIFSIGNALED(result));
    CHECK(!artifact_status(&c,&status)&&!strcmp(status,"interrupted"));
    CHECK(!strcmp(artifact_install_remote(&c,&source),"artifact_resume_required"));c.resume=1;CHECK(!artifact_install_remote(&c,&source));
    source.checkpoint_bytes=0;
    c=config(root,"slow");snprintf(url,sizeof(url),"http://127.0.0.1:%u/slow",port);source.timeout_ms=50;CHECK(!strcmp(artifact_install_remote(&c,&source),"artifact_deadline_exceeded"));
    source.timeout_ms=2000;
    const char *paths[]={"range","range-bad","range-total","range-end","range-nolength","range-missing","range-corrupt","range-length","range-duplicate","range-overflow","range-redirect","range-fallback-conflict"};
    const char *errors[]={NULL,"artifact_range_mismatch","artifact_range_mismatch","artifact_range_mismatch",NULL,"artifact_range_mismatch","artifact_checksum_mismatch","artifact_size_mismatch","artifact_range_mismatch","artifact_range_mismatch",NULL,"artifact_resume_conflict"};
    for(size_t i=0;i<sizeof(paths)/sizeof(paths[0]);i++){
        c=config(root,paths[i]);snprintf(url,sizeof(url),"http://127.0.0.1:%u/%s",port,paths[i]);
        CHECK(!strcmp(artifact_install_remote(&c,&source),"artifact_transfer_failed"));c.resume=1;
        const char *error=artifact_install_remote(&c,&source);CHECK(errors[i]?error&&!strcmp(error,errors[i]):!error);
        if(!errors[i])CHECK(!artifact_status(&c,&status)&&!strcmp(status,"verified"));
        else {CHECK(!artifact_status(&c,&status)&&strcmp(status,"verified"));}
    }
    c=config(root,"changed-prefix");snprintf(url,sizeof(url),"http://127.0.0.1:%u/range",port);
    CHECK(!strcmp(artifact_install_remote(&c,&source),"artifact_transfer_failed"));
    CHECK(snprintf(stage_path,sizeof(stage_path),"%s/%s/stage",c.state_path,c.sha256)<(int)sizeof(stage_path));int changed=open(stage_path,O_WRONLY);CHECK(changed>=0);CHECK(pwrite(changed,"x",1,0)==1);CHECK(!close(changed));
    c.resume=1;CHECK(!strcmp(artifact_install_remote(&c,&source),"artifact_checkpoint_mismatch"));
    c=config(root,"torn-checkpoint");snprintf(url,sizeof(url),"http://127.0.0.1:%u/checkpoint-fallback",port);
    CHECK(!strcmp(artifact_install_remote(&c,&source),"artifact_transfer_failed"));
    CHECK(snprintf(checkpoint_path,sizeof(checkpoint_path),"%s/%s/remote-checkpoint",c.state_path,c.sha256)<(int)sizeof(checkpoint_path));changed=open(checkpoint_path,O_WRONLY);CHECK(changed>=0);CHECK(pwrite(changed,"!",1,224)==1);CHECK(!close(changed));
    c.resume=1;CHECK(!artifact_install_remote(&c,&source));CHECK(!artifact_status(&c,&status)&&!strcmp(status,"verified"));
    c=config(root,"private-checkpoint");snprintf(url,sizeof(url),"http://127.0.0.1:%u/range",port);CHECK(!strcmp(artifact_install_remote(&c,&source),"artifact_transfer_failed"));
    CHECK(snprintf(checkpoint_path,sizeof(checkpoint_path),"%s/%s/remote-checkpoint",c.state_path,c.sha256)<(int)sizeof(checkpoint_path));CHECK(!chmod(checkpoint_path,0644));c.resume=1;
    CHECK(!strcmp(artifact_install_remote(&c,&source),"unsafe_artifact_checkpoint"));CHECK(!chmod(checkpoint_path,0600));
    CHECK(!artifact_install_remote(&c,&source));
    c=config(root,"auth");snprintf(url,sizeof(url),"http://127.0.0.1:%u/auth",port);CHECK(!strcmp(artifact_install_remote(&c,&source),"artifact_http_error"));
    source.bearer_key="private-secret";CHECK(!artifact_install_remote(&c,&source));source.bearer_key=NULL;
    c=config(root,"big-death");Sha256 hash;sha256_init(&hash);char block[32768],big_digest[65];unsigned char raw[32];
    memset(block,'A',sizeof(block));for(unsigned i=0;i<32;i++)sha256_update(&hash,block,sizeof(block));
    memset(block,'B',sizeof(block));for(unsigned i=0;i<32;i++)sha256_update(&hash,block,sizeof(block));sha256_final(&hash,raw);sha256_hex(raw,big_digest);
    c.sha256=big_digest;c.size=2097152;c.byte_ceiling=c.size;c.disk_ceiling=2*c.size;source.timeout_ms=5000;
    snprintf(url,sizeof(url),"http://127.0.0.1:%u/big-death",port);downloader=fork();CHECK(downloader>=0);if(!downloader)_exit(artifact_install_remote(&c,&source)?1:0);
    CHECK(snprintf(stage_path,sizeof(stage_path),"%s/%s/stage",c.state_path,c.sha256)<(int)sizeof(stage_path));
    CHECK(snprintf(checkpoint_path,sizeof(checkpoint_path),"%s/%s/remote-checkpoint",c.state_path,c.sha256)<(int)sizeof(checkpoint_path));partial=0;
    for(unsigned i=0;i<200;i++){struct stat st;char record[144],digest[65];int fd=open(checkpoint_path,O_RDONLY);ssize_t n=fd>=0?pread(fd,record,sizeof(record),144):-1;if(fd>=0)close(fd);
        if(n==144){hash_bytes(record,80,digest);if(!stat(stage_path,&st)&&st.st_size==1081344&&!memcmp(record,"0000000000100000",16)&&!memcmp(record+80,digest,64)){partial=1;break;}}
        struct timespec pause={0,10000000};nanosleep(&pause,NULL);}CHECK(partial);
    CHECK(!kill(downloader,SIGKILL));CHECK(waitpid(downloader,&result,0)==downloader&&WIFSIGNALED(result));
    CHECK(!artifact_status(&c,&status)&&!strcmp(status,"interrupted"));c.resume=1;
    changed=open(stage_path,O_WRONLY);CHECK(changed>=0);CHECK(pwrite(changed,"x",1,1048576)==1);CHECK(!close(changed));
    CHECK(!strcmp(artifact_install_remote(&c,&source),"artifact_resume_conflict"));
    changed=open(stage_path,O_WRONLY);CHECK(changed>=0);CHECK(pwrite(changed,"B",1,1048576)==1);CHECK(!close(changed));
    CHECK(!artifact_install_remote(&c,&source));CHECK(!artifact_status(&c,&status)&&!strcmp(status,"verified"));
    CHECK(!kill(server,SIGKILL));CHECK(waitpid(server,&result,0)==server);
    credential_redirect(root);
    source.loopback_http=0;CHECK(!strcmp(artifact_install_remote(&c,&source),"invalid_remote_artifact_configuration"));
    source.url="file:///foreign";CHECK(!strcmp(artifact_install_remote(&c,&source),"invalid_remote_artifact_configuration"));
    source.url="https://user:secret@example.invalid/x";CHECK(!strcmp(artifact_install_remote(&c,&source),"invalid_remote_artifact_configuration"));
    source.url="https://example.invalid/x#fragment";CHECK(!strcmp(artifact_install_remote(&c,&source),"invalid_remote_artifact_configuration"));
    source.url="https://example.invalid/x";source.bearer_key="unsafe\r\nkey";CHECK(!strcmp(artifact_install_remote(&c,&source),"invalid_remote_artifact_configuration"));
    printf("PASS remote artifacts: %u checks; staged integrity, offline retry, redirect/protocol/header/deadline refusal\n",checks);return 0;
}
