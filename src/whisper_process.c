#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "whisper_process_internal.h"
#include "engine_process.h"
#include "input_identity.h"
#include "json.h"
#include <time.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/file.h>
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

static int recovery_name(const char *name) {
    return !strncmp(name,"whisper-",8)&&strlen(name)==14&&
        strspn(name+8,"0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz")==6;
}

static void recover_preparations(int root) {
    int scan=openat(root,".",O_RDONLY|O_DIRECTORY|O_CLOEXEC);
    if(scan<0)return;
    DIR *entries=fdopendir(scan);
    if(!entries){close(scan);return;}
    for(struct dirent *entry;(entry=readdir(entries));){
        if(!recovery_name(entry->d_name))continue;
        int fd=openat(root,entry->d_name,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
        if(fd<0)continue;
        struct stat held,named,marker;
        if(!flock(fd,LOCK_EX|LOCK_NB)&&!fstat(fd,&held)&&held.st_uid==geteuid()&&!(held.st_mode&077)&&
           !fstatat(root,entry->d_name,&named,AT_SYMLINK_NOFOLLOW)&&
           held.st_dev==named.st_dev&&held.st_ino==named.st_ino&&
           fstatat(fd,ENGINE_COMMAND_ADMITTED,&marker,AT_SYMLINK_NOFOLLOW)&&errno==ENOENT){
            if(!fstatat(fd,ENGINE_COMMAND_PREPARING,&marker,AT_SYMLINK_NOFOLLOW)&&
               S_ISREG(marker.st_mode)&&marker.st_uid==geteuid()&&marker.st_nlink==1&&
               !(marker.st_mode&077)&&marker.st_size==0){
                (void)unlinkat(fd,"input.wav",0);
                (void)unlinkat(fd,ENGINE_COMMAND_PREPARING,0);
            }
            /* Without a preparation receipt only an empty reservation can go. */
            (void)unlinkat(root,entry->d_name,AT_REMOVEDIR);
        }
        close(fd);
    }
    closedir(entries);
}

/* Serialize registration and recovery under the root directory lock. The
 * workspace lock is retained through preparation and duplicated to the
 * guardian with the existing directory descriptor. */
static int prepare_workspace(const char *temporary,char workspace[PATH_MAX],int *recoverable) {
    char absolute[PATH_MAX];
    if(!temporary||!mkdirs(temporary)||!realpath(temporary,absolute))return -1;
    int root=open(absolute,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(root<0)return -1;
    struct stat identity;
    if(fstat(root,&identity)){close(root);return -1;}
    *recoverable=identity.st_uid==geteuid()&&!(identity.st_mode&022);
    if(*recoverable){
        int locked;
        do{locked=flock(root,LOCK_EX);}while(locked&&errno==EINTR);
        if(locked){close(root);return -1;}
        recover_preparations(root);
    }
    if(snprintf(workspace,PATH_MAX,"%s/%s",absolute,
                *recoverable?"whisper-XXXXXX":"result-XXXXXX")>=PATH_MAX||!mkdtemp(workspace)){
        close(root);
        return -1;
    }
    const char *leaf=strrchr(workspace,'/')+1;
    int directory=openat(root,leaf,O_RDONLY|O_DIRECTORY|O_NOFOLLOW|O_CLOEXEC);
    if(directory>=0&&*recoverable){
        int marker=flock(directory,LOCK_EX|LOCK_NB)?-1:
            openat(directory,ENGINE_COMMAND_PREPARING,O_CREAT|O_EXCL|O_WRONLY|O_CLOEXEC,0600);
        if(marker>=0)close(marker);
        else{close(directory);directory=-1;}
    }
    if(directory<0)(void)unlinkat(root,leaf,AT_REMOVEDIR);
    close(root);
    return directory;
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
    char workspace[PATH_MAX]="",owned_input[PATH_MAX]="",self[PATH_MAX];
    const char *input=wav_path;
    int directory=-1,recoverable=0;pid_t pid=0;
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
    directory=prepare_workspace(config->temporary_directory,workspace,&recoverable);
    if(directory<0||!engine_process_executable(self))goto done;
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
    if(directory>=0){
        if(!recoverable||!stopped||strcmp(stopped,"engine_guardian_failed")){
            if(out_base[0])(void)unlinkat(directory,"result",0);
            if(out_path[0])(void)unlinkat(directory,timestamped?"result.srt":"result.txt",0);
            if(owned_input[0])(void)unlinkat(directory,"input.wav",0);
            if(recoverable)(void)unlinkat(directory,ENGINE_COMMAND_PREPARING,0);
            struct stat held,named;
            if(!fstat(directory,&held)&&!lstat(workspace,&named)&&
               held.st_dev==named.st_dev&&held.st_ino==named.st_ino)(void)rmdir(workspace);
        }
        close(directory);
    }
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
