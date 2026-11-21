#ifndef SAMOSA_CONTROLLED_PIPE_H
#define SAMOSA_CONTROLLED_PIPE_H
/* Extracted from the existing summary transport. Caller supplies bounded
 * control checks and O_NONBLOCK pipe endpoints; no ownership is transferred. */
#include <errno.h>
#include <poll.h>
#include <pthread.h>
#include <signal.h>
#include <unistd.h>
static inline ssize_t controlled_pipe_write(int fd,const void *bytes,size_t length){
    sigset_t blocked,old,pending;sigemptyset(&blocked);sigaddset(&blocked,SIGPIPE);
    if(pthread_sigmask(SIG_BLOCK,&blocked,&old)){errno=EIO;return -1;}
    int had=sigpending(&pending)?1:sigismember(&pending,SIGPIPE);
    ssize_t written=write(fd,bytes,length);int saved=errno;
    if(written<0&&saved==EPIPE&&!had&&!sigpending(&pending)&&sigismember(&pending,SIGPIPE)){
        int signal_number;(void)sigwait(&blocked,&signal_number);
    }
    (void)pthread_sigmask(SIG_SETMASK,&old,NULL);errno=saved;return written;
}
static inline const char *controlled_pipe_transfer(int fd,const void *bytes,size_t length,int writing,
    const char *(*check)(void *),void *context,const char *io_error,const char *protocol_error){
    unsigned char *cursor=(unsigned char *)bytes;
    while(length){const char *error=(check?check(context):NULL);if(error)return error;
        struct pollfd wait={fd,writing?POLLOUT:POLLIN,0};int ready=poll(&wait,1,10);
        if(ready<0&&errno==EINTR)continue;
        if(ready<0)return io_error;
        if(!ready)continue;
        if(wait.revents&(POLLERR|POLLNVAL))return io_error;
        error=(check?check(context):NULL);if(error)return error;
        ssize_t n=writing?controlled_pipe_write(fd,cursor,length):read(fd,cursor,length);
        if(n<0&&(errno==EINTR||errno==EAGAIN||errno==EWOULDBLOCK))continue;
        if(n<=0)return protocol_error;
        cursor+=(size_t)n;length-=(size_t)n;
    }
    return (check?check(context):NULL);
}
#endif
