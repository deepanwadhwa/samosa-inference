#define _POSIX_C_SOURCE 200809L
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/resource.h>
int main(int argc,char **argv){if(argc!=6)return 2;int fd=open(argv[2],O_RDONLY|O_NONBLOCK);if(fd<0||dup2(fd,5)<0)return 3;if(fd!=5)close(fd);if(fcntl(5,F_SETFD,0))return 4;if(!getenv("SAMOSA_DIAG_NO_CAP")){struct rlimit cap={67108864,67108864};if(setrlimit(RLIMIT_FSIZE,&cap))return 5;}int in=open("/dev/null",O_RDONLY);if(in<0||dup2(in,0)<0)return 6;if(in)close(in);char *args[]={argv[1],"-m","/dev/fd/5","-f",argv[3],"-l","en","-osrt","-of",argv[4],NULL};execv(argv[1],args);perror("exec");return 7;}
