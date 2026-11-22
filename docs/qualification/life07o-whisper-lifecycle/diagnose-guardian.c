#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "engine_process.h"
#include "json.h"
#include <fcntl.h>
#include <poll.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
int main(int argc,char **argv){int dispatch=engine_guardian_dispatch(argc,argv);if(dispatch>=0)return dispatch;if(argc!=4)return 2;
 char workspace[]="/tmp/samosa-whisper-guard-diag-XXXXXX",input[512],prefix[512],result[512];if(!mkdtemp(workspace))return 3;snprintf(input,sizeof(input),"%s/input.wav",workspace);snprintf(prefix,sizeof(prefix),"%s/result",workspace);snprintf(result,sizeof(result),"%s/result.srt",workspace);
 int in=open(argv[3],O_RDONLY),out=open(input,O_WRONLY|O_CREAT|O_EXCL,0600);char bytes[65536];ssize_t n;while((n=read(in,bytes,sizeof(bytes)))>0)if(write(out,bytes,(size_t)n)!=n)return 4;close(in);close(out);
 int fd=open(result,O_RDWR|O_CREAT|O_EXCL,0600),model=open(argv[2],O_RDONLY|O_NONBLOCK|O_CLOEXEC),directory=open(workspace,O_RDONLY|O_DIRECTORY|O_CLOEXEC);FILE *lease=tmpfile();if(fd<0||model<0||directory<0||!lease)return 5;
 const char *args[]={argv[1],"-m","/dev/fd/5","-f",input,"-l","en","-osrt","-of",prefix};char *command=NULL;size_t count=0;FILE *f=open_memstream(&command,&count);fputc('[',f);for(size_t i=0;i<10;i++){if(i)fputc(',',f);json_quote(f,args[i],strlen(args[i]));}fputc(']',f);fclose(f);
 EngineProcess process;const char *error=engine_process_start_bound_workspace(argv[0],command,60,262144,fileno(lease),model,workspace,directory,&process);fprintf(stderr,"start=%s guardian=%ld\n",error?error:"ok",(long)process.guardian);if(!error){struct pollfd ready={process.response,POLLIN|POLLHUP,0};poll(&ready,1,65000);error=engine_process_stop(&process);}fprintf(stderr,"stop=%s reason=%s\n",error?error:"ok",process.reason);struct stat status;fstat(fd,&status);fprintf(stderr,"result=%lld bytes nlink=%lld\n",(long long)status.st_size,(long long)status.st_nlink);lseek(fd,0,SEEK_SET);while((n=read(fd,bytes,sizeof(bytes)))>0)write(STDERR_FILENO,bytes,(size_t)n);free(command);close(fd);close(directory);close(model);fclose(lease);return error?1:0;}
