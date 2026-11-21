#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/whisper_process.h>
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

int samosa_whisper_process_run(const SamosaWhisperProcessConfig *config,
        const SamosaWhisperProcessControl *control, const char *wav_path,
        int timestamped, int preserve_cancel, char **output) {
    if (!output) return SAMOSA_WHISPER_OUTPUT_INVALID;
    *output = NULL;
    if (!config || !config->engine || !config->model || !wav_path ||
        (control && (!control->acquire || !control->started || !control->reaped ||
                     !control->release || !control->cancelled)))
        return SAMOSA_WHISPER_NOT_READY;
    if (control && !control->acquire(control->context, preserve_cancel))
        return SAMOSA_WHISPER_BUSY;
    int result = SAMOSA_WHISPER_PREPARE_FAILED;
    char out_base[PATH_MAX] = "", out_path[PATH_MAX] = "";
    int placeholder = -1;
    if (!config->temporary_directory ||
        !mkdirs(config->temporary_directory) ||
        snprintf(out_base, sizeof(out_base), "%s/result-XXXXXX", config->temporary_directory) >=
            (int)sizeof(out_base))
        goto done;
    placeholder = mkstemp(out_base);
    if (placeholder < 0) goto done;
    close(placeholder); placeholder = -1;
    unlink(out_base);
    if (snprintf(out_path, sizeof(out_path), "%s.%s", out_base,
                 timestamped ? "srt" : "txt") >= (int)sizeof(out_path))
        goto done;
    if ((control && control->cancelled(control->context))) {
        result = SAMOSA_WHISPER_CANCELLED;
        goto done;
    }

    pid_t pid = fork();
    if (pid < 0) { result = SAMOSA_WHISPER_PROCESS_FAILED; goto done; }
    if (pid == 0) {
        int devnull = open("/dev/null", O_WRONLY);
        if (devnull >= 0) {
            dup2(devnull, STDOUT_FILENO);
            dup2(devnull, STDERR_FILENO);
            close(devnull);
        }
        if (timestamped)
            execl(config->engine, config->engine, "-m", config->model, "-f",
                  wav_path, "-l", "en", "-nt", "-osrt", "-of", out_base,
                  (char *)NULL);
        else
            execl(config->engine, config->engine, "-m", config->model, "-f",
                  wav_path, "-l", "en", "-nt", "-otxt", "-of", out_base,
                  (char *)NULL);
        _exit(127);
    }
    if (control) control->started(control->context, pid);
    int status = 0;
    pid_t waited;
    do { waited = waitpid(pid, &status, 0); } while (waited < 0 && errno == EINTR);
    if (control) control->reaped(control->context, pid);
    if (waited != pid || !WIFEXITED(status) || WEXITSTATUS(status)) {
        result = (control && control->cancelled(control->context))
               ? SAMOSA_WHISPER_CANCELLED
               : SAMOSA_WHISPER_PROCESS_FAILED;
        goto done;
    }
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
    if (placeholder >= 0) close(placeholder);
    if (out_base[0]) unlink(out_base);
    if (out_path[0]) unlink(out_path);
    if (control) control->release(control->context);
    return result;
}
