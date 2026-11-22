#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include <samosa_inference/primary_host.h>
#include <samosa_inference/backend_json.h>
#include "activation.h"
#include "artifact.h"
#include "backend_process_internal.h"
#include "json.h"
#include "library_runtime.h"
#include "sha256.h"
#include <arpa/inet.h>
#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <time.h>
#include <unistd.h>
#ifdef __APPLE__
#include <libproc.h>
#include <sys/proc_info.h>
#endif

enum { MAX_INPUTS = 16, MAX_LIBRARIES = 8, PROBE_TOKENS = 4 };
typedef struct {
    int fd;
    struct stat identity;
} RetainedInput;

struct SamosaPrimaryHost {
    SamosaPrimaryConfig config;
    Activation activation;
    struct stat engine, model;
    RetainedInput inputs[MAX_INPUTS], libraries[MAX_LIBRARIES], directory;
    const char *model_path;
    int launched, ready;
};

static int unavailable_cleanup(const char *error) {
    return error && (!strcmp(error, "engine_shutdown_pending") ||
                     !strcmp(error, "engine_guardian_failed"));
}

static int cancelled(const SamosaPrimaryHost *host) {
    return host->config.cancelled && host->config.cancelled(host->config.cancellation);
}

static const char *check_cancel(void *context) {
    return cancelled(context) ? "cancelled" : NULL;
}

static int absolute(const char *path) {
    return path && path[0] == '/' && path[1] && strlen(path) < PATH_MAX;
}

static int valid_pin(const SamosaPrimaryInput *pin) {
    return absolute(pin->path) && pin->size && pin->size <= INT64_MAX &&
        pin->sha256 && strlen(pin->sha256) == 64 &&
        strspn(pin->sha256, "0123456789abcdef") == 64;
}

static int has_input(const SamosaPrimaryConfig *config, const char *path) {
    if (!absolute(path)) return 0;
    for (size_t i = 0; i < config->input_count; i++)
        if (!strcmp(config->inputs[i].path, path)) return 1;
    return 0;
}

static int joined(char path[PATH_MAX], const char *directory, const char *file) {
    int count = snprintf(path, PATH_MAX, "%s/%s", directory, file);
    return count > 0 && count < PATH_MAX;
}

static int valid_config(const SamosaPrimaryConfig *c) {
    if (!c || !c->owner.process || !c->owner.mutex || !absolute(c->owner.pid_file) ||
        !absolute(c->state) || !c->gate || !c->upstream_fd ||
        !valid_pin(&c->engine) || !valid_pin(&c->model) ||
        c->input_count > MAX_INPUTS || c->library_count > MAX_LIBRARIES ||
        (c->input_count && !c->inputs) || (c->library_count && !c->libraries) ||
        c->startup_seconds < 1 || c->startup_seconds > 120 ||
        c->generation_seconds < 1 || c->generation_seconds > 120 ||
        c->launch.port < 1 || c->launch.port > 65535 ||
        !c->launch.backend || !absolute(c->launch.log))
        return 0;
    for (size_t i = 0; i < c->input_count; i++)
        if (!valid_pin(&c->inputs[i])) return 0;
    for (size_t i = 0; i < c->library_count; i++)
        if (!valid_pin(&c->libraries[i])) return 0;
    int qwen = !strcmp(c->launch.backend, "qwen");
    int maple = !strcmp(c->launch.backend, "maple");
    if (qwen || maple) {
        if (!absolute(c->launch.model) || !absolute(c->launch.engine) ||
            strcmp(c->engine.path, c->launch.engine)) return 0;
        char path[PATH_MAX];
        if (!joined(path, c->launch.model, qwen ? "manifest.json" : "maple-manifest.json") ||
            strcmp(path, c->model.path)) return 0;
        const char *files[] = {
            "config.json", qwen ? "experts.bin" : "maple-experts.bin",
            qwen ? NULL : "maple-resident.safetensors",
            qwen ? NULL : "tokenizer.json"
        };
        for (size_t i = 0; i < sizeof(files) / sizeof(files[0]); i++)
            if (files[i] && (!joined(path, c->launch.model, files[i]) || !has_input(c, path))) return 0;
        return !qwen || (has_input(c, c->launch.tokenizer) && absolute(c->launch.chats) &&
                         c->launch.runtime.cpu_source && c->launch.runtime.context_source);
    }
    return (!strcmp(c->launch.backend, "bonsai") || !strcmp(c->launch.backend, "ornith")) &&
        absolute(c->launch.llama.engine) && absolute(c->launch.llama.model) &&
        !strcmp(c->engine.path, c->launch.llama.engine) &&
        !strcmp(c->model.path, c->launch.llama.model) &&
        c->launch.llama.alias && c->launch.llama.port == c->launch.port &&
        (!c->launch.llama.projector || has_input(c, c->launch.llama.projector));
}

static const char *open_input(SamosaPrimaryHost *host, const SamosaPrimaryInput *pin,
                              RetainedInput *retained) {
    retained->fd = open(pin->path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (retained->fd < 0 || fstat(retained->fd, &retained->identity))
        return "artifact_unavailable";
    const struct stat *st = &retained->identity;
    if (!S_ISREG(st->st_mode) || st->st_uid != geteuid() || st->st_nlink != 1 || (st->st_mode & 022))
        return "unsafe_activation_input";
    return artifact_verify_controlled(retained->fd, pin->size, pin->size, pin->sha256,
                                      check_cancel, host);
}

static const char *inputs_unchanged(const SamosaPrimaryHost *host) {
    const SamosaPrimaryConfig *c = &host->config;
    if (!activation_input_unchanged(host->activation.engine_fd, c->engine.path, &host->engine) ||
        !activation_input_unchanged(host->activation.model_fd, c->model.path, &host->model))
        return "provider_artifact_changed";
    if (host->directory.fd >= 0 &&
        !activation_input_unchanged(host->directory.fd, host->model_path, &host->directory.identity))
        return "provider_artifact_changed";
    for (size_t i = 0; i < c->input_count; i++)
        if (!activation_input_unchanged(host->inputs[i].fd, c->inputs[i].path, &host->inputs[i].identity))
            return "provider_artifact_changed";
    for (size_t i = 0; i < c->library_count; i++)
        if (!activation_input_unchanged(host->libraries[i].fd, c->libraries[i].path, &host->libraries[i].identity))
            return "provider_artifact_changed";
    return NULL;
}

/* Qwen's existing st_init opens every .safetensors entry in SNAP. */
static const char *qwen_shards(const SamosaPrimaryHost *host) {
    int fd = openat(host->directory.fd, ".", O_RDONLY | O_DIRECTORY | O_CLOEXEC);
    if (fd < 0) return "artifact_unavailable";
    DIR *directory = fdopendir(fd);
    if (!directory) { close(fd); return "artifact_unavailable"; }
    const char *error = NULL;
    size_t count = 0;
    for (;;) {
        errno = 0;
        struct dirent *entry = readdir(directory);
        if (!entry) {
            if (errno) error = "artifact_unavailable";
            break;
        }
        const char *ext = strrchr(entry->d_name, '.');
        if (!ext || strcmp(ext, ".safetensors")) continue;
        char path[PATH_MAX];
        if (!joined(path, host->model_path, entry->d_name) || !has_input(&host->config, path)) {
            error = "unbound_primary_input";
            break;
        }
        count++;
    }
    closedir(directory);
    return error ? error : count ? NULL : "unbound_primary_input";
}

/* Keep the release streaming profile bound to the pinned expert file. */
static const char *maple_manifest(const SamosaPrimaryConfig *c, int fd) {
    if (c->model.size > 65536) return "primary_configuration_limit";
    size_t bytes = (size_t)c->model.size;
    char *body = malloc(bytes);
    if (!body) return "resource_limit";
    Json json;
    const char *error = "invalid_primary_configuration";
    if (pread(fd, body, bytes, 0) == (ssize_t)bytes && json_parse(&json, body, bytes) &&
        json.tokens[0].type == JT_OBJECT) {
        int field = json_get(&json, 0, "experts_file");
        char *name = json_string(&json, field);
        if (field < 0 || (name && !strcmp(name, "maple-experts.bin"))) error = NULL;
        free(name);
    }
    free(body);
    return error;
}

static const char *verify(void *context, int engine, int model) {
    SamosaPrimaryHost *host = context;
    const SamosaPrimaryConfig *c = &host->config;
    if (fstat(engine, &host->engine) || fstat(model, &host->model)) return "artifact_unavailable";
    if (!(host->engine.st_mode & 0111)) return "unsafe_activation_input";
    const char *error = check_cancel(host);
    if (!error && host->model_path) {
        host->directory.fd = open(host->model_path, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        if (host->directory.fd < 0 || fstat(host->directory.fd, &host->directory.identity) ||
            host->directory.identity.st_uid != geteuid() || (host->directory.identity.st_mode & 022))
            return "unsafe_activation_input";
        if (!strcmp(c->launch.backend, "qwen")) {
            char generation[PATH_MAX];
            struct stat found;
            if (!joined(generation, host->model_path, "generation_config.json"))
                return "invalid_primary_configuration";
            if (!has_input(c, generation) && (!lstat(generation, &found) || errno != ENOENT))
                return "unbound_primary_input";
            if (getenv("REFINE_DIR") || getenv("REFINE_MODE")) return "unbound_primary_input";
            error = qwen_shards(host);
        } else {
            const char *full = getenv("SAMOSA_MAPLE_ALLOW_FULL_RESIDENT");
            if (full && !strcmp(full, "1")) return "unbound_primary_input";
            error = maple_manifest(c, model);
        }
    }
    LibraryRuntimeFile files[MAX_LIBRARIES + 1] = {{c->engine.path, LIBRARY_RUNTIME_EXECUTABLE}};
    LibraryRuntimeImage *images = calloc(c->library_count + 1, sizeof(*images));
    if (!images) return "resource_limit";
    LibraryRuntimePolicy policy;
    if (!error) error = library_runtime_host_policy(&policy);
    if (!error) error = library_runtime_inspect_declared(engine, &policy, &images[0], 1, c->engine.path);
    for (size_t i = 0; !error && i < c->library_count; i++) {
        const SamosaPrimaryInput *pin = &c->libraries[i];
        error = open_input(host, pin, &host->libraries[i]);
        if (!error) error = library_runtime_inspect_declared(host->libraries[i].fd, &policy, &images[i+1], 0, pin->path);
        files[i+1] = (LibraryRuntimeFile){pin->path, LIBRARY_RUNTIME_LIBRARY};
    }
    if (!error) error = library_runtime_graph_declared(&policy, files, images, c->library_count+1, check_cancel, host);
    free(images);
    for (size_t i = 0; !error && i < c->input_count; i++)
        error = open_input(host, &c->inputs[i], &host->inputs[i]);
    return error ? error : inputs_unchanged(host);
}

static const char *launch(void *context, const char *self, const char *args,
                          double lifetime, size_t limit, int lease, int model, EngineProcess *process) {
    (void)self; (void)args; (void)lifetime; (void)limit; (void)lease; (void)model;
    SamosaPrimaryHost *host = context;
    const char *error = inputs_unchanged(host);
    if (!error) error = check_cancel(host);
    if (!error) {
        error = backend_process_start_delegated(&host->config.owner, &host->config.launch, process);
        pthread_mutex_lock(host->config.owner.mutex);
        host->launched = host->config.owner.process->guardian != NULL &&
            (!error || unavailable_cleanup(error));
        pthread_mutex_unlock(host->config.owner.mutex);
    }
    return error;
}

static const char *owned(SamosaPrimaryHost *host, pid_t *pid) {
    const SamosaPrimaryConfig *c = &host->config;
    const char *error = samosa_backend_process_status(&c->owner, pid);
    if (!error && *pid != host->activation.process.engine) error = "provider_identity_changed";
    if (!error) error = activation_owned_status(&host->activation, c->state, c->engine.path, c->model.path,
                                                &host->engine, &host->model);
    if (!error) error = inputs_unchanged(host);
    if (error) *pid = 0;
    return error;
}

static int owns_listener(pid_t pid, int port) {
#ifdef __APPLE__
    /* Reserve a bounded descriptor snapshot while the guardian still owns
     * this PID. The engine's default listener is IPv4 loopback. */
    struct proc_fdinfo descriptors[1024];
    int bytes = proc_pidinfo(pid, PROC_PIDLISTFDS, 0, descriptors, sizeof(descriptors));
    if (bytes <= 0 || (size_t)bytes >= sizeof(descriptors)) return 0;
    size_t count = (size_t)bytes / sizeof(descriptors[0]);
    for (size_t i = 0; i < count; i++) {
        if (descriptors[i].proc_fdtype != PROX_FDTYPE_SOCKET) continue;
        struct socket_fdinfo info;
        if (proc_pidfdinfo(pid, descriptors[i].proc_fd, PROC_PIDFDSOCKETINFO, &info, sizeof(info)) != sizeof(info)) continue;
        if (info.psi.soi_family != AF_INET || info.psi.soi_kind != SOCKINFO_TCP) continue;
        const struct tcp_sockinfo *tcp = &info.psi.soi_proto.pri_tcp;
        if (tcp->tcpsi_state == TSI_S_LISTEN && tcp->tcpsi_ini.insi_lport == htons((uint16_t)port) &&
            tcp->tcpsi_ini.insi_laddr.ina_46.i46a_addr4.s_addr == htonl(INADDR_LOOPBACK))
            return 1;
    }
#else
    (void)pid; (void)port;
#endif
    return 0;
}

static int live_probe(void *context) {
    SamosaPrimaryHost *host = context;
    const SamosaBackendLaunch *launch_config = &host->config.launch;
    pid_t pid;
    return !cancelled(host) && !owned(host, &pid) && owns_listener(pid, launch_config->port) &&
        samosa_backend_process_probe(&host->config.owner, launch_config->backend, launch_config->port);
}

static int generation_observed(const char *body, const char *backend) {
    Json j;
    if (!body || !json_parse(&j, body, strlen(body)) || j.tokens[0].type != JT_OBJECT ||
        json_get(&j, 0, "error") >= 0) return 0;
    int choices = json_get(&j, 0, "choices");
    if (choices < 0 || j.tokens[choices].type != JT_ARRAY || j.tokens[choices].count != 1) return 0;
    int message = json_get(&j, choices+1, "message");
    int usage = json_get(&j, 0, "usage");
    size_t generated = 0;
    char *content = json_string(&j, json_get(&j, message, "content"));
    int valid = content && content[strspn(content, " \t\r\n")];
    if (!strcmp(backend, "maple")) {
        /* The original Maple non-streaming schema has a finish reason and
         * no usage field. The request applies the model's own token bound. */
        char *reason = json_string(&j, json_get(&j, choices+1, "finish_reason"));
        valid = valid && reason && (!strcmp(reason, "stop") || !strcmp(reason, "length"));
        free(reason);
    } else {
        valid = valid && json_uint(&j, json_get(&j, usage, "completion_tokens"), &generated) &&
            generated > 0 && generated <= PROBE_TOKENS;
    }
    free(content);
    return valid;
}

static const char *qualify(void *context) {
    SamosaPrimaryHost *host = context;
    const SamosaPrimaryConfig *c = &host->config;
    struct timespec start, now;
    if (clock_gettime(CLOCK_MONOTONIC, &start)) return "provider_clock_failed";
    pid_t pid;
    for (;;) {
        if (cancelled(host)) return "cancelled";
        const char *error = owned(host, &pid);
        if (error) return error;
        if (live_probe(host)) break;
        if (clock_gettime(CLOCK_MONOTONIC, &now)) return "provider_clock_failed";
        if (now.tv_sec - start.tv_sec >= c->startup_seconds) return "provider_startup_deadline";
        struct timespec wait = {0, 50000000};
        nanosleep(&wait, NULL);
    }
    if (samosa_turn_generating(c->gate)) return "provider_busy";
    SamosaBackendJson request = {
        .port=c->launch.port, .timeout_seconds=c->generation_seconds, .gate=c->gate,
        .mutex=c->owner.mutex, .upstream_fd=c->upstream_fd, .probe=live_probe,
        .context=host, .require_complete=1
    };
    /* The original small stateless Qwen qualification request. The selected
     * llama alias is explicit for the existing file-model launch profiles. */
    char *payload = NULL;
    size_t bytes;
    FILE *out = open_memstream(&payload, &bytes);
    if (!out) return "resource_limit";
    fputc('{', out);
    if (!host->model_path) {
        fputs("\"model\":", out);
        json_quote(out, c->launch.llama.alias, strlen(c->launch.llama.alias));
        fputc(',', out);
    }
    fprintf(out, "\"messages\":[{\"role\":\"user\",\"content\":\"Say hello.\"}],"
                 "\"max_tokens\":%d,\"thinking\":false,\"stream\":false}", PROBE_TOKENS);
    if (fclose(out)) { free(payload); return "resource_limit"; }
    char *reply = samosa_backend_json(&request, payload);
    free(payload);
    int observed = generation_observed(reply, c->launch.backend);
    free(reply);
    const char *error = owned(host, &pid);
    if (error) return error;
    if (cancelled(host)) return "cancelled";
    return observed && owns_listener(pid, c->launch.port) ? NULL : "provider_qualification_failed";
}

static void write_pin(FILE *out, const SamosaPrimaryInput *pin) {
    fputs("{\"path\":", out);
    json_quote(out, pin->path, strlen(pin->path));
    fprintf(out, ",\"sha256\":\"%s\",\"size\":%llu}", pin->sha256, (unsigned long long)pin->size);
}

static const char *snapshot(const SamosaPrimaryConfig *c, char **body, char identity[65]) {
    char *settings = NULL;
    const char *error = backend_process_launch_snapshot(&c->launch, &settings);
    if (error) return error;
    size_t bytes;
    FILE *out = open_memstream(body, &bytes);
    if (!out) { free(settings); return "resource_limit"; }
    fprintf(out, "{\"profile\":\"native-primary-v1\",\"launch\":%s,\"inputs\":[", settings);
    free(settings);
    for (size_t i = 0; i < c->input_count; i++) {
        if (i) fputc(',', out);
        write_pin(out, &c->inputs[i]);
    }
    fputs("],\"libraries\":[", out);
    for (size_t i = 0; i < c->library_count; i++) {
        if (i) fputc(',', out);
        write_pin(out, &c->libraries[i]);
    }
    fputs("]}", out);
    if (fclose(out) || bytes > 4096) return "primary_configuration_limit";
    hash_bytes(*body, bytes, identity);
    return NULL;
}

const char *samosa_primary_start(SamosaPrimaryHost **result, const SamosaPrimaryConfig *config) {
#ifndef __APPLE__
    return "unsupported_provider_platform";
#endif
    if (!result || !valid_config(config)) return "invalid_primary_configuration";
    if (*result) return "backend_already_started";
    pthread_mutex_lock(config->owner.mutex);
    int retained = config->owner.process->guardian != NULL;
    pthread_mutex_unlock(config->owner.mutex);
    if (retained) return "backend_already_started";
    SamosaPrimaryHost *host = calloc(1, sizeof(*host));
    if (!host) return "resource_limit";
    host->config = *config;
    host->directory.fd = -1;
    for (size_t i = 0; i < MAX_INPUTS; i++) host->inputs[i].fd = -1;
    for (size_t i = 0; i < MAX_LIBRARIES; i++) host->libraries[i].fd = -1;
    host->activation = (Activation){.process={.guardian=-1,.control=-1,.response=-1},
        .root=-1,.lease=-1,.engine_fd=-1,.model_fd=-1};
    if (!strcmp(config->launch.backend, "qwen") || !strcmp(config->launch.backend, "maple"))
        host->model_path = config->launch.model;
    *result = host;
    char *bundle = NULL, identity[65], self[PATH_MAX];
    const char *error = snapshot(config, &bundle, identity);
    if (!error && !engine_process_executable(self)) error = "engine_io_failed";
    if (!error) {
        ActivationConfig activation = {
            .state=config->state, .self=self, .engine=config->engine.path, .model=config->model.path,
            .engine_sha=config->engine.sha256, .model_sha=config->model.sha256,
            .engine_size=config->engine.size, .model_size=config->model.size, .byte_ceiling=INT64_MAX,
            .argv_template="[\"@ENGINE@\",\"@MODEL@\"]",
            .model_argument=host->model_path ? host->model_path : config->model.path,
            .advance_current=1, .readable_inputs=1, .launch=launch, .launch_context=host,
            .verify_runtime=verify, .runtime_context=host, .qualify=qualify, .qualification=host,
            .bundle_identity=identity, .bundle_config=bundle
        };
        error = activation_start(&activation, &host->activation);
    }
    free(bundle);
    if (error) {
        const char *cleanup = samosa_primary_stop(result, NULL);
        return unavailable_cleanup(cleanup) ? cleanup : error;
    }
    host->ready = 1;
    return NULL;
}

const char *samosa_primary_status(SamosaPrimaryHost *host, pid_t *pid, unsigned *revision) {
    if (!pid || !revision) return "invalid_primary_configuration";
    *pid = 0;
    *revision = 0;
    if (!host) return "provider_unavailable";
    if (atomic_load(&host->activation.cleanup_failed)) return "engine_guardian_failed";
    if (!host->ready) return "provider_unavailable";
    const char *error = owned(host, pid);
    if (!error && !live_probe(host)) error = "provider_unavailable";
    if (!error) error = owned(host, pid);
    if (error) {
        *pid = 0;
        if (strcmp(error,"provider_unavailable")) host->ready = 0;
        return error;
    }
    *revision = host->activation.revision;
    return NULL;
}

const char *samosa_primary_stop(SamosaPrimaryHost **result, const SamosaBackendStopControl *control) {
    if (!result || !*result) return NULL;
    SamosaPrimaryHost *host = *result;
    host->ready = 0;
    const char *error = host->launched ? samosa_backend_process_stop_owned(&host->config.owner, control) : NULL;
    if (error && !strcmp(error, "engine_guardian_failed")) activation_cleanup_failed(&host->activation);
    if (unavailable_cleanup(error)) return error;
    host->launched = 0;
    const char *cleanup = activation_stop(&host->activation);
    if (unavailable_cleanup(cleanup)) return cleanup;
    if (host->directory.fd >= 0) close(host->directory.fd);
    for (size_t i = 0; i < host->config.input_count; i++)
        if (host->inputs[i].fd >= 0) close(host->inputs[i].fd);
    for (size_t i = 0; i < host->config.library_count; i++)
        if (host->libraries[i].fd >= 0) close(host->libraries[i].fd);
    free(host);
    *result = NULL;
    return error ? error : cleanup;
}

const char *samosa_primary_last_good(const char *state, FILE *out) {
    if (!absolute(state) || !out) return "invalid_primary_configuration";
    return activation_get(state, out);
}
