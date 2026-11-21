#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/backend_process.h>
#include "engine_process.h"
#include "json.h"
#include <sys/file.h>
#ifdef __APPLE__
#include <mach-o/dyld.h>
#endif
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/time.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

/* Existing primary-backend implementation, with explicit trusted owner inputs. */
static int backend_write_small_file(const char *path, const char *text) {
    char temp[PATH_MAX];
    if (snprintf(temp, sizeof(temp), "%s.tmp.%ld", path, (long)getpid()) >=
        (int)sizeof(temp)) return 0;
    int out = open(temp, O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW, 0600);
    if (out < 0) return 0;
    size_t length = strlen(text), written = 0;
    int ok = 1;
    while (ok && written < length) {
        ssize_t n = write(out, text + written, length - written);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { ok = 0; break; }
        written += (size_t)n;
    }
    if (ok) ok = fsync(out) == 0;
    if (close(out)) ok = 0;
    if (ok) ok = rename(temp, path) == 0;
    if (!ok) unlink(temp);
    return ok;
}

static int backend_tcp_connect(int port) {
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return -1;
    struct sockaddr_in address = {0};
    address.sin_family = AF_INET;
    address.sin_port = htons((uint16_t)port);
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
    if (connect(fd, (struct sockaddr *)&address, sizeof(address))) {
        close(fd);
        return -1;
    }
    return fd;
}

static int backend_send_all(int fd, const void *data, size_t size) {
    const char *cursor=(const char *)data;
    while (size) {
#ifdef MSG_NOSIGNAL
        ssize_t n=send(fd,cursor,size,MSG_NOSIGNAL);
#else
        ssize_t n=send(fd,cursor,size,0);
#endif
        if (n<0 && errno==EINTR) continue;
        if (n<=0) return 0;
        cursor+=n; size-=(size_t)n;
    }
    return 1;
}

/* Private same-executable bootstrap transport, never a public request format.
 * The existing launch body below runs in its original child context. This avoids
 * changing the host environment or rebuilding its engine-specific argv rules. */
typedef struct { EngineProcess child; } BackendGuard;
typedef struct {
    uint32_t magic; int port,runtime[8],llama[4]; uint64_t ram;
    uint32_t lengths[15];
} PrimaryWire;
static void string_slots(SamosaBackendLaunch *c,const char ***v) {
    const char **slots[]={&c->backend,&c->engine,&c->model,&c->tokenizer,&c->chats,&c->log,
        &c->runtime.cpu_source,&c->runtime.context_source,&c->llama.engine,&c->llama.model,
        &c->llama.alias,&c->llama.projector,&c->llama.context_override,&c->llama.gpu_override,&c->llama.threads_override};
    memcpy(v,slots,sizeof(slots));
}
static int configuration_write(FILE *out,const SamosaBackendLaunch *c) {
    PrimaryWire wire={.magic=0x53425031,.port=c->port,
        .runtime={c->runtime.cpu_auto,c->runtime.context_auto,c->runtime.auto_compact,c->runtime.compact_threshold_percent,
                  c->runtime.cpu_locked,c->runtime.context_locked,c->runtime.cpu_effective,c->runtime.context_effective},
        .llama={c->llama.port,c->llama.context_tokens,c->llama.threads,c->llama.perf_cores},.ram=c->llama.ram_bytes};
    SamosaBackendLaunch copy=*c;const char **slots[15];string_slots(&copy,slots);
    for(size_t i=0;i<15;i++){size_t n=*slots[i]?strlen(*slots[i])+1:0;if(n>1048576)return 0;wire.lengths[i]=(uint32_t)n;}
    if(fwrite(&wire,sizeof(wire),1,out)!=1)return 0;
    for(size_t i=0;i<15;i++)if(wire.lengths[i]&&fwrite(*slots[i],wire.lengths[i],1,out)!=1)return 0;
    return !fflush(out)&&!fseek(out,0,SEEK_SET);
}
static int configuration_read(SamosaBackendLaunch *c) {
    int flags=fcntl(9,F_GETFL);struct stat st;
    if(flags<0||(flags&O_ACCMODE)!=O_RDWR||fcntl(9,F_SETFD,FD_CLOEXEC)||fstat(9,&st)||
       !S_ISREG(st.st_mode)||st.st_uid!=geteuid()||(st.st_mode&077)||st.st_nlink||st.st_size>(off_t)(sizeof(PrimaryWire)+15*1048576))return 0;
    FILE *in=fdopen(9,"r");if(!in)return 0;PrimaryWire w;int ok=fread(&w,sizeof(w),1,in)==1&&w.magic==0x53425031;
    if(ok){*c=(SamosaBackendLaunch){.port=w.port,
        .runtime={w.runtime[0],w.runtime[1],w.runtime[2],w.runtime[3],w.runtime[4],w.runtime[5],w.runtime[6],w.runtime[7],NULL,NULL},
        .llama={.port=w.llama[0],.context_tokens=w.llama[1],.threads=w.llama[2],.perf_cores=w.llama[3],.ram_bytes=w.ram}};
        const char **slots[15];string_slots(c,slots);
        for(size_t i=0;ok&&i<15;i++){if(w.lengths[i]>1048576){ok=0;break;}
            if(!w.lengths[i])continue;
            char *text=malloc(w.lengths[i]);if(!text){ok=0;break;}*slots[i]=text;
            ok=fread(text,w.lengths[i],1,in)==1&&text[w.lengths[i]-1]==0&&strlen(text)==w.lengths[i]-1;
        }
        if(ok)ok=fgetc(in)==EOF&&!ferror(in)&&c->backend&&c->log&&
            ((!strcmp(c->backend,"qwen")||!strcmp(c->backend,"maple"))?c->engine&&c->model:c->llama.engine&&c->llama.model&&c->llama.alias);
    }
    fclose(in);return ok;
}
static int executable_path(char self[PATH_MAX]) {
#ifdef __APPLE__
    uint32_t size=PATH_MAX;char path[PATH_MAX];
    return !_NSGetExecutablePath(path,&size)&&realpath(path,self)!=NULL;
#else
    ssize_t n=readlink("/proc/self/exe",self,PATH_MAX-1);if(n<=0)return 0;self[n]=0;return 1;
#endif
}
static int owner_lease(const char *marker) {
    char path[PATH_MAX];if(snprintf(path,sizeof(path),"%s.lock",marker)>=(int)sizeof(path))return -1;
    int fd=open(path,O_RDWR|O_CREAT|O_NOFOLLOW|O_NONBLOCK|O_CLOEXEC,0600);if(fd<0)return -1;
    struct stat st;if(fstat(fd,&st)||!S_ISREG(st.st_mode)||st.st_uid!=geteuid()||st.st_nlink!=1||(st.st_mode&077)||flock(fd,LOCK_EX|LOCK_NB)){close(fd);return -1;}
    return fd;
}

pid_t samosa_backend_process_pid(const SamosaBackendOwner *owner) {
    pthread_mutex_lock(owner->mutex);
    pid_t pid = owner->process->pid;
    pthread_mutex_unlock(owner->mutex);
    return pid;
}

int samosa_backend_process_running(const SamosaBackendOwner *owner) {
    pthread_mutex_lock(owner->mutex);
    pid_t pid = owner->process->pid;
    if (pid <= 0) {
        pthread_mutex_unlock(owner->mutex);
        return 0;
    }
    BackendGuard *guard=owner->process->guardian;
    int running=guard&&engine_process_running(&guard->child);
    if(!running){
        owner->process->pid=owner->process->pgid=0;
        unlink(owner->pid_file);
        if(guard){const char *error=engine_process_stop(&guard->child);
            if(!error||strcmp(error,"engine_shutdown_pending")){free(guard);owner->process->guardian=NULL;}}
    }
    pthread_mutex_unlock(owner->mutex);
    return running;
}

int samosa_backend_process_probe(const SamosaBackendOwner *owner, const char *backend, int port) {
    if (!samosa_backend_process_running(owner)) return 0;
    int fd = backend_tcp_connect(port);
    if (fd < 0) return 0;
    /* A process can accept the TCP connection before its HTTP health handler
       is usable (or accept and then wedge). Without socket deadlines the
       selection watchdog blocks forever inside recv(), so the UI remains on
       "Starting..." and its own readiness deadline is never observed. Keep
       each probe bounded; the watchdog will retry until its overall deadline. */
    struct timeval timeout = {.tv_sec = 1, .tv_usec = 0};
    setsockopt(fd, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    setsockopt(fd, SOL_SOCKET, SO_SNDTIMEO, &timeout, sizeof(timeout));
    const char *path = !strcmp(backend, "qwen") ? "/healthz" :
                       !strcmp(backend, "maple") ? "/healthz" : "/health";
    char request[256];
    int n = snprintf(request, sizeof(request),
                     "GET %s HTTP/1.1\r\nHost: 127.0.0.1:%d\r\nConnection: close\r\n\r\n",
                     path, port);
    char response[64] = {0};
    int ok = n > 0 && backend_send_all(fd, request, (size_t)n) &&
             recv(fd, response, sizeof(response) - 1, 0) > 0 &&
             strstr(response, " 200 ") != NULL;
    close(fd);
    return ok;
}

void samosa_backend_process_stop(const SamosaBackendOwner *owner, const SamosaBackendStopControl *control) {
    pthread_mutex_lock(owner->mutex);
    BackendGuard *guard=owner->process->guardian;
    owner->process->pid=owner->process->pgid=0;
    if(guard)unlink(owner->pid_file);
    if(control&&control->detach)control->detach(control->context);
    pthread_mutex_unlock(owner->mutex);
    if(control&&control->interrupted)control->interrupted(control->context);
    if(!guard)return;
    const char *error=engine_process_stop(&guard->child);
    if(error&&!strcmp(error,"engine_shutdown_pending"))return;
    pthread_mutex_lock(owner->mutex);
    owner->process->guardian=NULL;
    pthread_mutex_unlock(owner->mutex);
    free(guard);
}

int samosa_backend_process_start(const SamosaBackendOwner *owner, const SamosaBackendLaunch *config) {
    /* Never launch into a port already owned by an untracked process. Before
       this guard, the new child would fail its bind while backend_probe()
       happily accepted the old process's /health response. That made every
       model switch fail and then falsely reported Qwen as healthy. */
    int occupied = backend_tcp_connect(config->port);
    if (occupied >= 0) {
        close(occupied);
        fprintf(stderr, "samosa-gateway: refusing to start %s: private backend port %d "
                        "is already occupied by an untracked process\n",
                config->backend, config->port);
        return 0;
    }
    /* The caller serializes whole start/stop. A timed-out guardian keeps its
     * ownership lease; never overwrite that handle or publish a replacement. */
    pthread_mutex_lock(owner->mutex);
    int retained=owner->process->guardian!=NULL;
    pthread_mutex_unlock(owner->mutex);
    if(retained)return 0;
    int lease=owner_lease(owner->pid_file);if(lease<0)return 0;
    char self[PATH_MAX];FILE *configuration=tmpfile();
    if(!configuration){close(lease);return 0;}
    int fd=fileno(configuration);
    int ok=fcntl(fd,F_SETFD,FD_CLOEXEC)==0&&executable_path(self)&&configuration_write(configuration,config);
    char *command=NULL;size_t length=0;FILE *args=ok?open_memstream(&command,&length):NULL;
    if(args){fputc('[',args);json_quote(args,self,strlen(self));fputs(",\"--inference-primary-backend\"]",args);ok=!fclose(args);}else ok=0;
    BackendGuard *guard=ok?calloc(1,sizeof(*guard)):NULL;
    EngineServer server={fd,STDOUT_FILENO,STDERR_FILENO,owner->pid_file};
    const char *error=guard?engine_process_start_server(self,command,lease,&server,&guard->child):"engine_io_failed";
    free(command);fclose(configuration);close(lease);
    if(error){
        if(guard&&guard->child.guardian>0){pthread_mutex_lock(owner->mutex);owner->process->guardian=guard;pthread_mutex_unlock(owner->mutex);}else free(guard);
        return 0;
    }
    pthread_mutex_lock(owner->mutex);
    owner->process->guardian=guard;
    owner->process->pid=owner->process->pgid=guard->child.engine;
    char pid_text[32];snprintf(pid_text,sizeof(pid_text),"%ld\n",(long)guard->child.engine);
    if(!backend_write_small_file(owner->pid_file,pid_text))
        fprintf(stderr,"samosa-gateway: could not persist backend pid marker %s\n",owner->pid_file);
    pthread_mutex_unlock(owner->mutex);
    return 1;
}

static void configuration_free(SamosaBackendLaunch *c) {
    const char **slots[15];string_slots(c,slots);for(size_t i=0;i<15;i++)free((void *)*slots[i]);
}
int samosa_backend_process_dispatch(int argc,char **argv) {
    int dispatched=engine_guardian_dispatch(argc,argv);if(dispatched>=0)return dispatched;
    if(argc!=2||strcmp(argv[1],"--inference-primary-backend"))return -1;
    SamosaBackendLaunch storage={0};if(!configuration_read(&storage)){configuration_free(&storage);return 127;}
    const SamosaBackendLaunch *config=&storage;
    const SamosaBackendRuntime *runtime=&config->runtime;
    /* SAMOSA_BIND controls only the authenticated public gateway. Native
       qwen/maple backends also use samosa_http.h and inherit the gateway
       environment, so pin the child back to loopback before exec. Without
       this, `--lan` would accidentally expose the raw model port + 1. */
    setenv("SAMOSA_BIND", "127.0.0.1", 1);
    unsetenv("SAMOSA_LAN");
    unsetenv("SAMOSA_LAN_PASSWORD");
    int log = open(config->log, O_WRONLY | O_CREAT | O_APPEND, 0600);
    if (log >= 0) { dup2(log, STDOUT_FILENO); dup2(log, STDERR_FILENO); close(log); }
    char port[16];
    snprintf(port, sizeof(port), "%d", config->port);
    if (!strcmp(config->backend, "qwen")) {
        setenv("SNAP", config->model, 1);
        setenv("TOKENIZER", config->tokenizer, 1);
        setenv("SAMOSA_CHATS_DIR", config->chats, 1);
        char threads[16], context[16], threshold[16];
        if (!runtime->cpu_locked && !runtime->cpu_auto) {
            snprintf(threads, sizeof(threads), "%d", runtime->cpu_effective);
            setenv("OMP_NUM_THREADS", threads, 1);
        }
        if (!runtime->context_locked) {
            if (runtime->context_auto) setenv("SAMOSA_CONTEXT_TOKENS", "auto", 1);
            else {
                snprintf(context, sizeof(context), "%d", runtime->context_effective);
                setenv("SAMOSA_CONTEXT_TOKENS", context, 1);
            }
        }
        snprintf(threshold, sizeof(threshold), "%d", runtime->compact_threshold_percent);
        setenv("SAMOSA_AUTO_COMPACT", runtime->auto_compact ? "1" : "0", 1);
        setenv("SAMOSA_COMPACT_THRESHOLD", threshold, 1);
        fprintf(stderr, "[samosa] runtime: qwen threads=%d (%s) context=%d (%s) compact=%s@%d%%\n",
                runtime->cpu_effective, runtime->cpu_source,
                runtime->context_effective, runtime->context_source,
                runtime->auto_compact ? "on" : "off", runtime->compact_threshold_percent);
        execl(config->engine, config->engine, "--serve", "--port", port,
              "--tokenizer", config->tokenizer, (char *)NULL);
    } else if (!strcmp(config->backend, "maple")) {
        fprintf(stderr, "[samosa] runtime: starting maple backend on port %s\n", port);
        execl(config->engine, config->engine, "--model-dir", config->model, "--port", port, (char *)NULL);
    } else {
        SamosaLlamaLaunch launch = config->llama;
        (void)samosa_llama_launch(&launch);
    }
    configuration_free(&storage);return 127;
}
