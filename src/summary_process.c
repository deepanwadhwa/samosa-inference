#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/summary_process.h>
#include "json.h"
#include "controlled_pipe.h"
#include <arpa/inet.h>
#include <stdint.h>
#include <limits.h>
#include <poll.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

static void sleep_millis(long ms) {
    if (ms <= 0) return;
    struct timespec pause = {.tv_sec = ms / 1000, .tv_nsec = (ms % 1000) * 1000000L};
    while (nanosleep(&pause, &pause) && errno == EINTR) {}
}

static int regular_file(const char *path, int executable) {
    struct stat st;
    return path && !stat(path, &st) && S_ISREG(st.st_mode) &&
           (!executable || !access(path, X_OK));
}

int samosa_summary_process_init(SamosaSummaryProcess *process) {
    process->pid = 0;
    process->write_fd = process->read_fd = -1;
    process->warmed = 0;
    process->lifecycle = (SamosaSummaryLifecycle){0};
    process->error = process->cleanup_error = NULL;
    return pthread_mutex_init(&process->mutex, NULL) == 0;
}
int samosa_summary_process_bind(SamosaSummaryProcess *process,const SamosaSummaryLifecycle *lifecycle){
    if(!lifecycle||!lifecycle->open||!lifecycle->close)return 0;
    pthread_mutex_lock(&process->mutex);
    int fresh=!process->pid&&process->write_fd<0&&process->read_fd<0&&!process->lifecycle.open;
    if(fresh)process->lifecycle=*lifecycle;
    pthread_mutex_unlock(&process->mutex);return fresh;
}

void samosa_summary_process_stop_locked(SamosaSummaryProcess *process) {
    if(process->lifecycle.close){
        process->cleanup_error=process->lifecycle.close(process->lifecycle.context);
        process->warmed=0;
        if(process->cleanup_error)return;
        process->write_fd=process->read_fd=-1;process->pid=0;return;
    }
    pid_t pid = process->pid;
    if (process->write_fd >= 0) close(process->write_fd);
    if (process->read_fd >= 0) close(process->read_fd);
    process->write_fd = -1;
    process->read_fd = -1;
    process->pid = 0;
    process->warmed = 0;
    if (pid <= 0) return;
    if (waitpid(pid, NULL, WNOHANG) == pid) return;
    kill(pid, SIGTERM);
    for (int i = 0; i < 20; ++i) {
        if (waitpid(pid, NULL, WNOHANG) == pid) return;
        sleep_millis(25);
    }
    kill(pid, SIGKILL);
    while (waitpid(pid, NULL, 0) < 0 && errno == EINTR) {}
}

static void summarizer_close_inherited_fds(void) {
    long limit = sysconf(_SC_OPEN_MAX);
    if (limit <= 0 || limit > 65536) limit = 65536;
    for (int fd = 3; fd < limit; ++fd) close(fd);
}

static int summary_start_locked(SamosaSummaryProcess *process, const SamosaSummaryProcessConfig *config,const SamosaSummaryControl *control) {
    if(process->cleanup_error){process->error=process->cleanup_error;return 0;}
    if(process->lifecycle.open){
        process->error=process->lifecycle.open(process->lifecycle.context,config,control,&process->write_fd,&process->read_fd);
        return !process->error&&process->write_fd>=0&&process->read_fd>=0;
    }
    if (process->pid > 0) {
        int status = 0;
        pid_t reaped = waitpid(process->pid, &status, WNOHANG);
        if (!reaped && process->write_fd >= 0 &&
            process->read_fd >= 0) return 1;
        samosa_summary_process_stop_locked(process);
    }
    if (!config || !regular_file(config->engine, 1) || !regular_file(config->model, 0) || !config->log_path || !config->gpu_layers) return 0;
    int request_pipe[2] = {-1, -1}, reply_pipe[2] = {-1, -1};
    if (pipe(request_pipe) || pipe(reply_pipe)) {
        if (request_pipe[0] >= 0) { close(request_pipe[0]); close(request_pipe[1]); }
        if (reply_pipe[0] >= 0) { close(reply_pipe[0]); close(reply_pipe[1]); }
        return 0;
    }
    pid_t pid = fork();
    if (pid < 0) {
        close(request_pipe[0]); close(request_pipe[1]);
        close(reply_pipe[0]); close(reply_pipe[1]);
        return 0;
    }
    if (!pid) {
        int log_fd = open(config->log_path, O_WRONLY | O_CREAT | O_APPEND, 0600);
        if (dup2(request_pipe[0], STDIN_FILENO) < 0 ||
            dup2(reply_pipe[1], STDOUT_FILENO) < 0 ||
            (log_fd >= 0 && dup2(log_fd, STDERR_FILENO) < 0)) _Exit(126);
        close(request_pipe[0]); close(request_pipe[1]);
        close(reply_pipe[0]); close(reply_pipe[1]);
        summarizer_close_inherited_fds();
        if (log_fd > STDERR_FILENO) close(log_fd);
        const char *gpu = config->gpu_layers;
        execl(config->engine, config->engine,
              "--model", config->model,
              "--gpu-layers", gpu,
              "--max-tokens", "128", (char *)NULL);
        _Exit(127);
    }
    close(request_pipe[0]);
    close(reply_pipe[1]);
    process->pid = pid;
    process->write_fd = request_pipe[1];
    process->read_fd = reply_pipe[0];
    process->warmed = 0;
    return 1;
}

int samosa_summary_process_start_locked(SamosaSummaryProcess *process,const SamosaSummaryProcessConfig *config){
    return summary_start_locked(process,config,NULL);
}

void samosa_summary_process_stop(SamosaSummaryProcess *process) {
    (void)samosa_summary_process_close(process);
}
const char *samosa_summary_process_close(SamosaSummaryProcess *process) {
    pthread_mutex_lock(&process->mutex);
    samosa_summary_process_stop_locked(process);
    const char *error=process->cleanup_error;
    pthread_mutex_unlock(&process->mutex);return error;
}
void samosa_summary_process_destroy(SamosaSummaryProcess *process) {
    if(samosa_summary_process_close(process))return;
    pthread_mutex_destroy(&process->mutex);
}

static long long monotonic_millis(void) {
    struct timespec ts;
    if(clock_gettime(CLOCK_MONOTONIC,&ts))return -1;
    return (long long)ts.tv_sec*1000LL+ts.tv_nsec/1000000LL;
}
static const char *control_check(const SamosaSummaryControl *control){
    long long now=monotonic_millis();if(now<0)return "summary_clock_failed";
    if(control&&control->check){const char *error=control->check(control->context);if(error)return error;}
    return control&&control->deadline_ms>0&&now>=control->deadline_ms?"deadline_exceeded":NULL;
}
static SamosaSummaryControl budget(const SamosaSummaryControl *control,long long millis){
    SamosaSummaryControl out=control?*control:(SamosaSummaryControl){0};
    long long now=monotonic_millis();
    if(out.deadline_ms<=0||out.deadline_ms>now+millis)out.deadline_ms=now+millis;
    return out;
}
static const char *lock_controlled(SamosaSummaryProcess *process,const SamosaSummaryControl *control){
    for(;;){const char *error=control_check(control);if(error)return error;
        int locked=pthread_mutex_trylock(&process->mutex);if(!locked)return NULL;
        if(locked!=EBUSY)return "summary_resource_limit";
        sleep_millis(5);
    }
}
static const char *pipe_control(void *context){return control_check(context);}
static const char *transfer(int fd,const void *bytes,size_t length,int writing,const SamosaSummaryControl *control){
    return controlled_pipe_transfer(fd,bytes,length,writing,pipe_control,(void *)control,
        "summary_io_failed","summary_protocol_failed");
}
typedef struct {int write_flags,read_flags;} ProtocolFlags;
static const char *protocol_begin(SamosaSummaryProcess *process,ProtocolFlags *flags){
    flags->write_flags=fcntl(process->write_fd,F_GETFL);flags->read_flags=fcntl(process->read_fd,F_GETFL);
    struct stat input,output;
    if(flags->write_flags<0||flags->read_flags<0||
       (flags->write_flags&O_ACCMODE)!=O_WRONLY||(flags->read_flags&O_ACCMODE)!=O_RDONLY||
       fstat(process->write_fd,&input)||fstat(process->read_fd,&output)||!S_ISFIFO(input.st_mode)||!S_ISFIFO(output.st_mode))return "summary_io_failed";
    if(fcntl(process->write_fd,F_SETFL,flags->write_flags|O_NONBLOCK))return "summary_io_failed";
    if(fcntl(process->read_fd,F_SETFL,flags->read_flags|O_NONBLOCK)){
        (void)fcntl(process->write_fd,F_SETFL,flags->write_flags);return "summary_io_failed";
    }
    return NULL;
}
static const char *protocol_end(SamosaSummaryProcess *process,const ProtocolFlags *flags){
    int w=fcntl(process->write_fd,F_SETFL,flags->write_flags),r=fcntl(process->read_fd,F_SETFL,flags->read_flags);
    return w||r?"summary_io_failed":NULL;
}
static const char *invalidate(SamosaSummaryProcess *process,const char *error){
    samosa_summary_process_stop_locked(process);
    return process->cleanup_error?process->cleanup_error:error;
}

static void summary_normalize_punctuation(char *text) {
    if (!text) return;
    size_t read_at = 0, write_at = 0;
    while (text[read_at]) {
        if (text[read_at] == ' ' && strchr(".,;:!?", text[read_at + 1])) {
            read_at++;
            continue;
        }
        text[write_at++] = text[read_at++];
    }
    text[write_at] = 0;
}

char *samosa_summary_process_frame_controlled(SamosaSummaryProcess *process,const SamosaSummaryProcessConfig *config,
        const unsigned char *frame,size_t bytes,const SamosaSummaryControl *control,const char **result_error){
    if(result_error)*result_error=NULL;
    if(!frame||bytes<5||bytes>65536){if(result_error)*result_error="invalid_summary_input";return NULL;}
    SamosaSummaryControl scoped=budget(control,60000);const char *error=lock_controlled(process,&scoped);
    if(error){if(result_error)*result_error=error;return NULL;}
    if(process->cleanup_error){error=process->cleanup_error;pthread_mutex_unlock(&process->mutex);if(result_error)*result_error=error;return NULL;}
    process->error=NULL;
    if(!summary_start_locked(process,config,&scoped))error=process->error?process->error:"summary_engine_unavailable";
    ProtocolFlags flags;int begun=0;
    if(!error){error=protocol_begin(process,&flags);begun=!error;}
    if(!error)error=transfer(process->write_fd,frame,bytes,1,&scoped);
    uint32_t encoded=0;
    if(!error)error=transfer(process->read_fd,&encoded,sizeof(encoded),0,&scoped);
    size_t length=error?0:ntohl(encoded);char *reply=NULL;
    if(!error&&(!length||length>8192))error="invalid_summary_reply";
    if(!error&&!(reply=malloc(length+1)))error="summary_resource_limit";
    if(!error)error=transfer(process->read_fd,reply,length,0,&scoped);
    if(!error){if(!utf8_valid((const unsigned char *)reply,length))error="invalid_summary_reply";else reply[length]=0;}
    if(begun){const char *restore=protocol_end(process,&flags);if(!error)error=restore;}
    if(error){free(reply);reply=NULL;error=invalidate(process,error);}else process->warmed=1;
    pthread_mutex_unlock(&process->mutex);if(result_error)*result_error=error;return reply;
}
char *samosa_summary_process_once_controlled(SamosaSummaryProcess *process,const SamosaSummaryProcessConfig *config,
        const char *source,size_t source_len,const SamosaSummaryControl *control,const char **result_error){
    if(result_error)*result_error=NULL;
    if(!source||!source_len||source_len>60000||!utf8_valid((const unsigned char *)source,source_len)){
        if(result_error)*result_error="invalid_summary_input";return NULL;
    }
    SamosaSummaryControl scoped=budget(control,60000);
    size_t prompt_len=11+source_len;char *prompt=malloc(prompt_len);
    if(!prompt){if(result_error)*result_error="summary_resource_limit";return NULL;}
    memcpy(prompt,"summarize: ",11);memcpy(prompt+11,source,source_len);
    const char *error=lock_controlled(process,&scoped);
    if(error){free(prompt);if(result_error)*result_error=error;return NULL;}
    if(process->cleanup_error){error=process->cleanup_error;pthread_mutex_unlock(&process->mutex);free(prompt);if(result_error)*result_error=error;return NULL;}
    process->error=NULL;
    if(!summary_start_locked(process,config,&scoped))error=process->error?process->error:"summary_engine_unavailable";
    if(process->warmed)scoped=budget(&scoped,15000);
    ProtocolFlags flags;int begun=0;
    if(!error){error=protocol_begin(process,&flags);begun=!error;}
    uint32_t encoded=htonl((uint32_t)prompt_len),reply_encoded=0;
    if(!error)error=transfer(process->write_fd,&encoded,sizeof(encoded),1,&scoped);
    if(!error)error=transfer(process->write_fd,prompt,prompt_len,1,&scoped);
    free(prompt);
    if(!error)error=transfer(process->read_fd,&reply_encoded,sizeof(reply_encoded),0,&scoped);
    uint32_t reply_len=error?0:ntohl(reply_encoded);char *reply=NULL;
    if(!error&&(!reply_len||reply_len>8192))error="invalid_summary_reply";
    if(!error&&!(reply=malloc((size_t)reply_len+1)))error="summary_resource_limit";
    if(!error)error=transfer(process->read_fd,reply,reply_len,0,&scoped);
    if(!error){
        if(!utf8_valid((const unsigned char *)reply,reply_len))error="invalid_summary_reply";
        else{reply[reply_len]=0;summary_normalize_punctuation(reply);if(strlen(reply)<12)error="invalid_summary_reply";}
    }
    if(begun){const char *restore=protocol_end(process,&flags);if(!error)error=restore;}
    if(error){free(reply);reply=NULL;error=invalidate(process,error);}else process->warmed=1;
    pthread_mutex_unlock(&process->mutex);if(result_error)*result_error=error;return reply;
}
char *samosa_summary_process_once(SamosaSummaryProcess *process,const SamosaSummaryProcessConfig *config,const char *source,size_t source_len){
    return samosa_summary_process_once_controlled(process,config,source,source_len,NULL,NULL);
}

/* Original scalar/batch wire format and output partitioning, with bounded I/O. */
int samosa_summary_process_batch_controlled(SamosaSummaryProcess *process,const SamosaSummaryProcessConfig *config,
        char **parts,int count,char **replies,int concise,const SamosaSummaryControl *control,const char **result_error){
    if(result_error)*result_error=NULL;
    if(!parts||!replies||count<1||count>64){if(result_error)*result_error="invalid_summary_input";return 0;}
    for(int i=0;i<count;i++)if(!parts[i]||replies[i]||strlen(parts[i])>60000||!utf8_valid((const unsigned char *)parts[i],strlen(parts[i]))){
        if(result_error)*result_error="invalid_summary_input";return 0;
    }
    SamosaSummaryControl scoped=budget(control,180000);const char *error=lock_controlled(process,&scoped);
    if(error){if(result_error)*result_error=error;return 0;}
    if(process->cleanup_error){error=process->cleanup_error;pthread_mutex_unlock(&process->mutex);if(result_error)*result_error=error;return 0;}
    process->error=NULL;
    if(!summary_start_locked(process,config,&scoped))error=process->error?process->error:"summary_engine_unavailable";
    ProtocolFlags flags;int begun=0;
    if(!error){error=protocol_begin(process,&flags);begun=!error;}
    uint32_t encoded=htonl(0x80000000U|(concise?0x40000000U:0)|(uint32_t)count);
    if(!error)error=transfer(process->write_fd,&encoded,sizeof(encoded),1,&scoped);
    for(int i=0;!error&&i<count;i++){
        size_t bytes=strlen(parts[i]);encoded=htonl((uint32_t)(11+bytes));
        error=transfer(process->write_fd,&encoded,sizeof(encoded),1,&scoped);
        if(!error)error=transfer(process->write_fd,"summarize: ",11,1,&scoped);
        if(!error)error=transfer(process->write_fd,parts[i],bytes,1,&scoped);
    }
    for(int i=0;!error&&i<count;i++){
        error=transfer(process->read_fd,&encoded,sizeof(encoded),0,&scoped);
        uint32_t length=error?0:ntohl(encoded);
        if(!error&&(!length||length>65536))error="invalid_summary_reply";
        if(!error&&!(replies[i]=malloc((size_t)length+1)))error="summary_resource_limit";
        if(!error)error=transfer(process->read_fd,replies[i],length,0,&scoped);
        if(!error){
            if(!utf8_valid((const unsigned char *)replies[i],length))error="invalid_summary_reply";
            else{replies[i][length]=0;summary_normalize_punctuation(replies[i]);if(strlen(replies[i])<12)error="invalid_summary_reply";}
        }
    }
    if(begun){const char *restore=protocol_end(process,&flags);if(!error)error=restore;}
    if(error){for(int i=0;i<count;i++){free(replies[i]);replies[i]=NULL;}error=invalidate(process,error);}
    else process->warmed=1;
    pthread_mutex_unlock(&process->mutex);if(result_error)*result_error=error;return !error;
}
int samosa_summary_process_batch(SamosaSummaryProcess *process,const SamosaSummaryProcessConfig *config,char **parts,int count,char **replies,int concise){
    return samosa_summary_process_batch_controlled(process,config,parts,count,replies,concise,NULL,NULL);
}


typedef struct NativeSummaryRequest {
    SamosaSummaryProcess *process;
    SamosaSummaryProcessConfig config;
    char **parts, **replies;
    int count, concise, done, ok;
    SamosaSummaryControl control;const SamosaSummaryControl *scope;const char *error;
    struct NativeSummaryRequest *next;
    void (*activity)(void *, const char *);
    void *activity_context;
} NativeSummaryRequest;
static pthread_mutex_t native_summary_queue_mu = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t native_summary_queue_cv;
static pthread_once_t native_summary_queue_once=PTHREAD_ONCE_INIT;
static int native_summary_queue_ready;
static NativeSummaryRequest *native_summary_queue;
static int native_summary_dispatching;

static void queue_init(void){
#ifdef __APPLE__
    native_summary_queue_ready=!pthread_cond_init(&native_summary_queue_cv,NULL);
#else
    pthread_condattr_t attr;if(pthread_condattr_init(&attr))return;
    native_summary_queue_ready=!pthread_condattr_setclock(&attr,CLOCK_MONOTONIC)&&!pthread_cond_init(&native_summary_queue_cv,&attr);
    pthread_condattr_destroy(&attr);
#endif
}
static int queue_wait(void){
    struct timespec until={0,20000000};
#ifdef __APPLE__
    return pthread_cond_timedwait_relative_np(&native_summary_queue_cv,&native_summary_queue_mu,&until);
#else
    if(clock_gettime(CLOCK_MONOTONIC,&until))return EIO;
    until.tv_nsec+=20000000;if(until.tv_nsec>=1000000000){until.tv_sec++;until.tv_nsec-=1000000000;}
    return pthread_cond_timedwait(&native_summary_queue_cv,&native_summary_queue_mu,&until);
#endif
}
static int queue_remove(NativeSummaryRequest *request){
    NativeSummaryRequest **at=&native_summary_queue;
    while(*at&&*at!=request)at=&(*at)->next;
    if(!*at)return 0;
    *at=request->next;return 1;
}
/* Original 4ms/64-frame grouping and in-order demultiplexing. Each queue head
 * coordinates the batch containing itself, then hands off to the next waiter;
 * a completed caller no longer drains unrelated work beyond its own deadline. */
int samosa_summary_process_coalesce_controlled(SamosaSummaryProcess *process,const SamosaSummaryProcessConfig *config,
        char **parts,int count,char **replies,int concise,void (*activity)(void *,const char *),void *activity_context,
        const SamosaSummaryControl *control,const char **result_error){
    if(result_error)*result_error=NULL;
    if(!config||!parts||!replies||count<1||count>64){if(result_error)*result_error="invalid_summary_input";return 0;}
    for(int i=0;i<count;i++)if(!parts[i]||replies[i]||strlen(parts[i])>60000||!utf8_valid((const unsigned char *)parts[i],strlen(parts[i]))){
        if(result_error)*result_error="invalid_summary_input";return 0;
    }
    if(pthread_once(&native_summary_queue_once,queue_init)||!native_summary_queue_ready){if(result_error)*result_error="summary_resource_limit";return 0;}
    NativeSummaryRequest request={.process=process,.config=*config,.parts=parts,.replies=replies,
        .count=count,.concise=concise,.activity=activity,.activity_context=activity_context,
        .control=budget(control,180000),.scope=control};
    const char *error=control_check(&request.control);if(error){if(result_error)*result_error=error;return 0;}
    if(activity)activity(activity_context,"summary_queued");
    pthread_mutex_lock(&native_summary_queue_mu);
    NativeSummaryRequest **tail=&native_summary_queue;while(*tail)tail=&(*tail)->next;*tail=&request;
    while(!request.done){
        error=control_check(&request.control);
        if(error&&queue_remove(&request)){request.done=1;request.error=error;pthread_cond_broadcast(&native_summary_queue_cv);break;}
        if(!native_summary_dispatching&&native_summary_queue==&request){
            native_summary_dispatching=1;
            pthread_mutex_unlock(&native_summary_queue_mu);sleep_millis(4);pthread_mutex_lock(&native_summary_queue_mu);
            error=control_check(&request.control);
            if(error){(void)queue_remove(&request);request.done=1;request.error=error;}
            else{
                NativeSummaryRequest *group[64];char *inputs[64]={0},*outputs[64]={0};int groups=0,frames=0;
                SamosaSummaryControl batch_control=request.control;
                NativeSummaryRequest **at=&native_summary_queue;
                while(*at){NativeSummaryRequest *candidate=*at;
                    if(candidate->process==process&&candidate->concise==concise&&candidate->scope==request.scope&&frames+candidate->count<=64){
                        *at=candidate->next;group[groups++]=candidate;
                        for(int i=0;i<candidate->count;i++)inputs[frames++]=candidate->parts[i];
                        if(candidate->control.deadline_ms<batch_control.deadline_ms)batch_control.deadline_ms=candidate->control.deadline_ms;
                    }else at=&candidate->next;
                }
                pthread_mutex_unlock(&native_summary_queue_mu);
                for(int i=0;i<groups;i++)if(group[i]->activity)group[i]->activity(group[i]->activity_context,"summarizing");
                const char *failure=NULL;
                int ok=samosa_summary_process_batch_controlled(process,config,inputs,frames,outputs,concise,&batch_control,&failure);
                pthread_mutex_lock(&native_summary_queue_mu);int offset=0;
                for(int i=0;i<groups;i++){
                    for(int j=0;j<group[i]->count;j++)group[i]->replies[j]=outputs[offset++];
                    group[i]->ok=ok;group[i]->error=failure;group[i]->done=1;
                }
            }
            native_summary_dispatching=0;pthread_cond_broadcast(&native_summary_queue_cv);
        }else{
            int waited=queue_wait();
            if(waited&&waited!=ETIMEDOUT&&queue_remove(&request)){
                request.done=1;request.error="summary_resource_limit";pthread_cond_broadcast(&native_summary_queue_cv);
            }
        }
    }
    pthread_mutex_unlock(&native_summary_queue_mu);if(result_error)*result_error=request.error;return request.ok;
}
int samosa_summary_process_coalesce(SamosaSummaryProcess *process,const SamosaSummaryProcessConfig *config,
        char **parts,int count,char **replies,int concise,void (*activity)(void *,const char *),void *context){
    return samosa_summary_process_coalesce_controlled(process,config,parts,count,replies,concise,activity,context,NULL,NULL);
}
