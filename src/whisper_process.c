#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "whisper_process_internal.h"
#include "engine_process.h"
#include "input_identity.h"
#include "json.h"
#include <time.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>

/* Original gateway helpers, retained locally for an independent owner build. */
static int path_copy(char *out, size_t cap, const char *value) {
    int n = snprintf(out, cap, "%s", value ? value : "");
    return n >= 0 && (size_t)n < cap;
}

static int mkdirs(const char *path) {
    char copy[PATH_MAX];
    if (!path_copy(copy, sizeof(copy), path)) return 0;
    for (char *p = copy + 1; *p; ++p) {
        if (*p != '/') continue;
        *p = 0;
        if (mkdir(copy, 0700) && errno != EEXIST) return 0;
        *p = '/';
    }
    return !mkdir(copy, 0700) || errno == EEXIST;
}

static unsigned char *read_file_bytes_limit(const char *path, size_t limit, size_t *out_len) {
    if (out_len) *out_len = 0;
    int fd = open(path, O_RDONLY | O_NOFOLLOW);
    if (fd < 0) return NULL;
    struct stat st;
    if (fstat(fd, &st) || !S_ISREG(st.st_mode) || st.st_size < 0 || (size_t)st.st_size > limit) {
        close(fd); return NULL;
    }
    unsigned char *data = malloc((size_t)st.st_size + 1);
    if (!data) { close(fd); return NULL; }
    size_t used = 0, size = (size_t)st.st_size;
    while (used < size) {
        ssize_t n = read(fd, data + used, size - used);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) { free(data); close(fd); return NULL; }
        used += (size_t)n;
    }
    close(fd); data[size] = 0;
    if (out_len) *out_len = size;
    return data;
}

static char *read_file_limit(const char *path, size_t limit) {
    return (char *)read_file_bytes_limit(path, limit, NULL);
}

static long utf8_scalar_count(const unsigned char *data, size_t len) {
    size_t i = 0; long count = 0;
    while (i < len) {
        unsigned char c = data[i++];
        int cont; uint32_t cp;
        if (c < 0x80) {
            if (c < 0x20 && c != 9 && c != 10 && c != 13) return -1;
            count++; continue;
        }
        if (c >= 0xc2 && c <= 0xdf) { cont = 1; cp = c & 0x1f; }
        else if (c >= 0xe0 && c <= 0xef) { cont = 2; cp = c & 0x0f; }
        else if (c >= 0xf0 && c <= 0xf4) { cont = 3; cp = c & 0x07; }
        else return -1;
        if ((size_t)cont > len - i) return -1;
        if (c == 0xe0 && data[i] < 0xa0) return -1;
        if (c == 0xed && data[i] >= 0xa0) return -1;
        if (c == 0xf0 && data[i] < 0x90) return -1;
        if (c == 0xf4 && data[i] >= 0x90) return -1;
        while (cont--) {
            if ((data[i] & 0xc0) != 0x80) return -1;
            cp = (cp << 6) | (data[i++] & 0x3f);
        }
        (void)cp;
        count++;
    }
    return count;
}

static size_t trim_ascii_ws(char *s) {
    size_t len = strlen(s);
    size_t start = 0;
    while (start < len && (s[start] == ' ' || s[start] == '\t' ||
                           s[start] == '\n' || s[start] == '\r')) start++;
    size_t end = len;
    while (end > start && (s[end - 1] == ' ' || s[end - 1] == '\t' ||
                           s[end - 1] == '\n' || s[end - 1] == '\r')) end--;
    size_t out_len = end - start;
    if (start) memmove(s, s + start, out_len);
    s[out_len] = 0;
    return out_len;
}

int samosa_whisper_process_inspect(const SamosaWhisperProcessConfig *config,
        off_t expected_model_size, SamosaWhisperProcessIdentity *identity) {
    return config && config->engine && config->model && identity &&
           expected_model_size >= 0 &&
           !stat(config->engine, &identity->engine) &&
           S_ISREG(identity->engine.st_mode) && identity->engine.st_size > 0 &&
           !stat(config->model, &identity->model) &&
           S_ISREG(identity->model.st_mode) && identity->model.st_size > 0 &&
           (!expected_model_size || identity->model.st_size == expected_model_size);
}

static int inputs_unchanged(const SamosaWhisperProcessConfig *config,
        const SamosaWhisperProcessIdentity *before) {
    SamosaWhisperProcessIdentity now;
    return samosa_whisper_process_inspect(config, 0, &now) &&
           inference_same_input(&before->engine, &now.engine) &&
           inference_same_input(&before->model, &now.model);
}

static int cancelled(const SamosaWhisperProcessControl *control, const WhisperProcessAdmission *admission) {
    return (admission && admission->stop_requested && atomic_load(admission->stop_requested)) ||
           (control && control->cancelled(control->context));
}

int whisper_process_run_admitted(const SamosaWhisperProcessConfig *config,
        const SamosaWhisperProcessIdentity *expected,
        const SamosaWhisperProcessControl *control, const char *wav_path,
        int timestamped, int preserve_cancel, int own_input,
        const WhisperProcessAdmission *admission, char **output) {
    if (!output) return SAMOSA_WHISPER_OUTPUT_INVALID;
    *output = NULL;
    const unsigned char *probe = admission ? admission->probe_audio : NULL;
    if (!config || !config->engine || !config->model || (!wav_path && !probe) ||
        (control && (!control->acquire || !control->started || !control->reaped ||
                     !control->release || !control->cancelled)))
        return SAMOSA_WHISPER_NOT_READY;
    if (control && !control->acquire(control->context, preserve_cancel))
        return SAMOSA_WHISPER_BUSY;
    int result = SAMOSA_WHISPER_PREPARE_FAILED;
    char out_base[PATH_MAX] = "", out_path[PATH_MAX] = "";
    char created_path[PATH_MAX]="",workspace[PATH_MAX]="",owned_input[PATH_MAX]="",self[PATH_MAX];
    const char *input=wav_path;
    int directory=-1,created=0;pid_t pid=0;
    EngineProcess process={.guardian=-1,.control=-1,.response=-1};
    char *command=NULL;size_t command_size=0;
    SamosaWhisperProcessIdentity observed;
    if (expected) {
        observed = *expected;
        if (!inputs_unchanged(config, &observed)) {
            result = SAMOSA_WHISPER_NOT_READY;
            goto done;
        }
    }
    if (!config->temporary_directory || !mkdirs(config->temporary_directory) ||
        snprintf(created_path,sizeof(created_path),"%s/result-XXXXXX",config->temporary_directory)>=(int)sizeof(created_path))goto done;
    if(!mkdtemp(created_path))goto done;
    created=1;
    if(!realpath(created_path,workspace)||!engine_process_executable(self))goto done;
    directory=open(workspace,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(directory<0||snprintf(out_base,sizeof(out_base),"%s/result",workspace)>=(int)sizeof(out_base))goto done;
    if (snprintf(out_path, sizeof(out_path), "%s.%s", out_base,
                 timestamped ? "srt" : "txt") >= (int)sizeof(out_path))
        goto done;
    if (cancelled(control,admission)) {
        result = SAMOSA_WHISPER_CANCELLED;
        goto done;
    }

    /* Explicit ownership handoff for host-prepared temporary audio. Durable or
     * borrowed audio uses the original entry point and is never renamed. */
    if (probe) {
        if (snprintf(owned_input,sizeof(owned_input),"%s/input.wav",workspace)>=(int)sizeof(owned_input))
            goto done;
        int audio=openat(directory,"input.wav",O_WRONLY|O_CREAT|O_EXCL|O_CLOEXEC,0600);
        if (audio<0) goto done;
        size_t written=0;
        while (written<admission->probe_bytes) {
            ssize_t count=write(audio,probe+written,admission->probe_bytes-written);
            if (count<0 && errno==EINTR) continue;
            if (count<=0) break;
            written+=(size_t)count;
        }
        if (close(audio) || written!=admission->probe_bytes) goto done;
        input=owned_input;
    } else if(own_input){struct stat audio;
        if(lstat(wav_path,&audio)||!S_ISREG(audio.st_mode)||audio.st_uid!=geteuid()||
           audio.st_nlink!=1||
           snprintf(owned_input,sizeof(owned_input),"%s/input.wav",workspace)>=(int)sizeof(owned_input)||
           rename(wav_path,owned_input)||chmod(owned_input,0600))goto done;
        input=owned_input;
    }
    /* Original CLI argv; only lifecycle/workspace ownership changes. */
    const char *args[]={config->engine,"-m",config->model,"-f",input,"-l","en","-nt",
                        timestamped?"-osrt":"-otxt","-of",out_base};
    FILE *encoded=open_memstream(&command,&command_size);
    if(!encoded){result=SAMOSA_WHISPER_PROCESS_FAILED;goto done;}
    fputc('[',encoded);for(size_t i=0;i<sizeof(args)/sizeof(args[0]);i++){
        if(i)fputc(',',encoded);json_quote(encoded,args[i],strlen(args[i]));}
    fputc(']',encoded);
    if(fclose(encoded)){result=SAMOSA_WHISPER_PROCESS_FAILED;goto done;}
    if (!expected && !samosa_whisper_process_inspect(config, 0, &observed)) {
        result = SAMOSA_WHISPER_PROCESS_FAILED;
        goto done;
    }
    if (expected && !inputs_unchanged(config, &observed)) {
        result = SAMOSA_WHISPER_NOT_READY;
        goto done;
    }
    const char *error=engine_process_start_command(self,command,workspace,directory,
        admission?admission->lease:-1,admission?admission->lifetime:0,&process);
    if(error){result=cancelled(control,admission)?SAMOSA_WHISPER_CANCELLED:SAMOSA_WHISPER_PROCESS_FAILED;goto done;}
    pid=process.engine;
    if(control)control->started(control->context,pid);
    while(engine_process_running(&process)){
        /* The guardian holds the actual PID until acknowledgement. Finish a
         * cancelled TERM-resistant CLI without signalling any unrelated PID.
         * It then finishes the owned group and reserves identity/workspace. */
        if(cancelled(control,admission))(void)kill(pid,SIGKILL);
        struct timespec delay={0,10000000};while(nanosleep(&delay,&delay)&&errno==EINTR){}
    }
    if(cancelled(control,admission)){result=SAMOSA_WHISPER_CANCELLED;goto done;}
    if(strcmp(process.reason,"completed")){result=SAMOSA_WHISPER_PROCESS_FAILED;goto done;}
    *output = read_file_limit(out_path, 16 << 20);
    if (!*output) { result = SAMOSA_WHISPER_OUTPUT_INVALID; goto done; }
    trim_ascii_ws(*output);
    if (!(*output)[0] ||
        utf8_scalar_count((const unsigned char *)*output, strlen(*output)) < 0) {
        free(*output); *output = NULL;
        result = SAMOSA_WHISPER_OUTPUT_INVALID;
        goto done;
    }
    result = SAMOSA_WHISPER_OK;

done:
    free(command);
    /* Withdraw cancellation/tracking while the guardian still reserves the
     * child identity; keep the operation reservation until full cleanup. */
    if(pid>0&&control)control->reaped(control->context,pid);
    const char *stopped;
    do{stopped=engine_process_stop(&process);
        if(stopped&&!strcmp(stopped,"engine_shutdown_pending")){struct timespec delay={0,10000000};nanosleep(&delay,NULL);}
    }while(stopped&&!strcmp(stopped,"engine_shutdown_pending"));
    if(directory>=0)close(directory);
    if (out_base[0]) unlink(out_base);
    if (out_path[0]) unlink(out_path);
    if(owned_input[0])unlink(owned_input);
    if(created)rmdir(created_path);
    if (result == SAMOSA_WHISPER_OK) {
        if (stopped) result = SAMOSA_WHISPER_PROCESS_FAILED;
        else if (cancelled(control,admission)) result = SAMOSA_WHISPER_CANCELLED;
        else if (!inputs_unchanged(config, &observed)) result = SAMOSA_WHISPER_NOT_READY;
        if (result != SAMOSA_WHISPER_OK) {
            free(*output);
            *output = NULL;
        }
    }
    if (admission && stopped && !strcmp(stopped,"engine_guardian_failed"))
        result = SAMOSA_WHISPER_GUARDIAN_FAILED;
    else if (admission && stopped && !strcmp(stopped,"lifetime_exceeded"))
        result = SAMOSA_WHISPER_DEADLINE_EXCEEDED;
    if (control) control->release(control->context);
    return result;
}

int samosa_whisper_process_dispatch(int argc,char **argv) {
    return engine_guardian_dispatch(argc,argv);
}

int samosa_whisper_process_run(const SamosaWhisperProcessConfig *config,
        const SamosaWhisperProcessControl *control,const char *wav_path,
        int timestamped,int preserve_cancel,char **output) {
    return whisper_process_run_admitted(config,NULL,control,wav_path,timestamped,preserve_cancel,0,NULL,output);
}
int samosa_whisper_process_run_owned(const SamosaWhisperProcessConfig *config,
        const SamosaWhisperProcessControl *control,const char *wav_path,
        int timestamped,int preserve_cancel,char **output) {
    return whisper_process_run_admitted(config,NULL,control,wav_path,timestamped,preserve_cancel,1,NULL,output);
}
int samosa_whisper_process_run_checked(const SamosaWhisperProcessConfig *config,
        const SamosaWhisperProcessIdentity *expected, const SamosaWhisperProcessControl *control,
        const char *wav_path, int timestamped, int preserve_cancel, int own_input, char **output) {
    if (!expected) {
        if (output) *output = NULL;
        return SAMOSA_WHISPER_NOT_READY;
    }
    return whisper_process_run_admitted(config,expected,control,wav_path,timestamped,preserve_cancel,own_input,NULL,output);
}
