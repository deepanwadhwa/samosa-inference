#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <stdlib.h>
#include <errno.h>
#include <sys/wait.h>
/* Native test launcher. Parent waits for exec handshake before exiting, avoiding
 * the shell's background-job exit race; launch an owned isolated engine only. */
int main(int argc,char **argv) {
    if(argc<3)return 2;
    int handshake[2];if(pipe(handshake))return 2;
    if(fcntl(handshake[1],F_SETFD,FD_CLOEXEC))return 2;
    pid_t pid=fork();if(pid<0)return 2;
    if(!pid) {
        close(handshake[0]);if(setsid()<0)_exit(126);signal(SIGHUP,SIG_IGN);
        int log=open(argv[1],O_WRONLY|O_CREAT|O_TRUNC,0600),input=open("/dev/null",O_RDONLY);
        if(log<0||input<0||dup2(log,STDOUT_FILENO)<0||dup2(log,STDERR_FILENO)<0||dup2(input,STDIN_FILENO)<0)_exit(126);
        close(log);close(input);execvp(argv[2],argv+2);int error=errno;(void)write(handshake[1],&error,sizeof(error));_exit(127);
    }
    close(handshake[1]);int error=0;ssize_t got;do{got=read(handshake[0],&error,sizeof(error));}while(got<0&&errno==EINTR);close(handshake[0]);
    if(got!=0){waitpid(pid,NULL,0);return 2;}printf("%d\n",pid);return 0;
}
