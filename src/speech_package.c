#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "speech_package.h"
#include "artifact.h"
#include "json.h"
#include "sha256.h"
#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#define PACKAGE_BYTES UINT64_C(8589934592)
/* The regular-file identity and controlled hashing rules are reused from the
 * existing temporal package verifier; nested closed-tree checking adds support
 * for native runtime libraries, voice references and Kokoro espeak resources. */
static int same(const struct stat *a, const struct stat *b) {
    if (a->st_dev != b->st_dev || a->st_ino != b->st_ino || a->st_size != b->st_size ||
        a->st_mode != b->st_mode || a->st_uid != b->st_uid || a->st_nlink != b->st_nlink) return 0;
#ifdef __APPLE__
    return a->st_mtimespec.tv_sec == b->st_mtimespec.tv_sec &&
        a->st_mtimespec.tv_nsec == b->st_mtimespec.tv_nsec &&
        a->st_ctimespec.tv_sec == b->st_ctimespec.tv_sec && a->st_ctimespec.tv_nsec == b->st_ctimespec.tv_nsec;
#else
    return a->st_mtim.tv_sec == b->st_mtim.tv_sec && a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
        a->st_ctim.tv_sec == b->st_ctim.tv_sec && a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
#endif
}
static int digest(const char *s) { return s && strlen(s) == 64 && strspn(s, "0123456789abcdef") == 64; }
static int relative(const char *s) {
    if (!s || !*s || strlen(s) > 255) return 0;
    unsigned components = 0;
    for (const char *p = s; *p;) {
        const char *begin = p;
        if (++components > 8 || *p == '.') return 0;
        /* eSpeak's literal variant directory is handled as one component.
         * Manifest roles below constrain it to Kokoro data/voices/!v files. */
        if (p[0] == '!' && p[1] == 'v' && (!p[2] || p[2] == '/')) p += 2;
        /* Preserve the upstream variant's one space-containing filename. */
        else if (p - s >= 3 && !memcmp(p - 3, "!v/", 3) &&
                 !strncmp(p, "Mr serious", 10) && (!p[10] || p[10] == '/')) p += 10;
        else while (*p && *p != '/') {
            unsigned char c = (unsigned char)*p++;
            if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') ||
                  (c >= '0' && c <= '9') || strchr("-_.", c))) return 0;
        }
        if (p == begin) return 0;
        if (*p && !*++p) return 0;
    }
    return 1;
}
static int keys(const Json *j, int object, const char *const *names, size_t count) {
    if (object < 0 || j->tokens[object].type != JT_OBJECT) return 0;
    for (int t = object + 1; t < j->tokens[object].next;) {
        char *key = json_string(j, t); size_t i = 0;
        while (key && i < count && strcmp(key, names[i])) ++i;
        int valid = key && i < count; free(key); if (!valid) return 0;
        t = j->tokens[j->tokens[t].next].next;
    }
    return 1;
}
static int equal(const Json *j, int o, const char *key, const char *value) {
    char *s = json_string(j, json_get(j, o, key)); int ok = s && !strcmp(s, value); free(s); return ok;
}
const char *speech_package_platform(void) {
#if defined(__APPLE__) && defined(__aarch64__)
    return "macos-arm64";
#elif defined(__APPLE__) && defined(__x86_64__)
    return "macos-x86_64";
#elif defined(__linux__) && defined(__aarch64__)
    return "linux-arm64";
#elif defined(__linux__) && defined(__x86_64__)
    return "linux-x86_64";
#else
    return NULL;
#endif
}
void speech_package_identity(const char *pin, char out[65]) {
    out[0] = 0;
    if (!digest(pin)) return;
    char material[96]; int n = snprintf(material, sizeof(material), "sherpa-speech-package-v1:%s", pin);
    if (n > 0 && (size_t)n < sizeof(material)) hash_bytes(material, (size_t)n, out);
}
/* Slots 0..13 correspond to the required library/models/reference paths. Extra
 * pinned dependencies and metadata have roles 14 and 15, data files role 16. */
static const char *const roles[] = {"library", "pocket_lm_flow", "pocket_lm_main", "pocket_encoder",
    "pocket_decoder", "pocket_text_conditioner", "pocket_vocab", "pocket_token_scores",
    "reference_caro", "reference_stuart", "kokoro_model", "kokoro_voices", "kokoro_tokens",
    "reserved", "dependency", "metadata", "kokoro_data"};
static const char *regular_file(int parent, const char *name, uint64_t bytes, const char *pin,
    const char *(*control)(void *), void *ctx, struct stat *identity, const struct stat *prior) {
    int fd = openat(parent, name, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return "artifact_unavailable";
    struct stat before, after, named;
    const char *e = fstat(fd, &before) ? "artifact_unavailable" : NULL;
    if (!e && (!S_ISREG(before.st_mode) || before.st_uid != geteuid() ||
        before.st_nlink != 1 || (before.st_mode & 022))) e = "unsupported_artifact";
    if (!e && prior && !same(&before, prior)) e = "artifact_changed";
    if (!e) e = artifact_verify_controlled(fd, bytes, PACKAGE_BYTES, pin, control, ctx);
    if (!e && (fstat(fd, &after) || fstatat(parent, name, &named, AT_SYMLINK_NOFOLLOW) ||
        !same(&before, &after) || !same(&before, &named))) e = "artifact_changed";
    if (!e) *identity = before;
    close(fd); return e;
}
typedef struct {
    SpeechPackageReceipt *next;
    const SpeechPackageReceipt *prior;
    const char *(*control)(void *);
    void *context;
    unsigned char found[SPEECH_PACKAGE_FILES];
    size_t visited;
} Walk;
static const char *walk_tree(int fd, const char *prefix, Walk *w) {
    /* Opening '.' instead of dup gives each traversal its own directory offset,
     * including rechecks on a caller-retained descriptor. */
    int copy = openat(fd, ".", O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (copy < 0) return "artifact_unavailable";
    DIR *d = fdopendir(copy);
    if (!d) { close(copy); return "artifact_unavailable"; }
    const char *e = NULL;
    for (;;) {
        if ((e = w->control ? w->control(w->context) : NULL)) break;
        errno = 0; struct dirent *entry = readdir(d);
        if (!entry) { if (errno) e = "artifact_unavailable"; break; }
        if (!strcmp(entry->d_name, ".") || !strcmp(entry->d_name, "..")) continue;
        if (++w->visited > SPEECH_PACKAGE_FILES + SPEECH_PACKAGE_DIRECTORIES + 1) { e = "speech_package_limit"; break; }
        char name[256];
        int n = snprintf(name, sizeof(name), "%s%s%s", prefix, *prefix ? "/" : "", entry->d_name);
        if (n < 0 || (size_t)n >= sizeof(name) || !relative(name)) { e = "invalid_speech_package"; break; }
        struct stat before, after, named;
        if (fstatat(fd, entry->d_name, &before, AT_SYMLINK_NOFOLLOW)) { e = "artifact_changed"; break; }
        if (!*prefix && !strcmp(name, "manifest.json")) {
            if (!same(&before, &w->next->manifest)) { e = "artifact_changed"; break; }
            continue;
        }
        if (S_ISDIR(before.st_mode)) {
            if (before.st_uid != geteuid() || (before.st_mode & 022)) { e = "unsupported_artifact"; break; }
            int needed = 0;
            for (size_t i = 0; i < w->next->count; ++i)
                needed |= !strncmp(name, w->next->files[i].name, (size_t)n) && w->next->files[i].name[n] == '/';
            if (!needed) { e = "unlisted_speech_artifact"; break; }
            if (w->next->directory_count == SPEECH_PACKAGE_DIRECTORIES) { e = "speech_package_limit"; break; }
            SpeechPackageDirectory *item = &w->next->directories[w->next->directory_count++];
            strcpy(item->name, name); item->identity = before;
            if (w->prior) {
                size_t i = 0;
                while (i < w->prior->directory_count && strcmp(w->prior->directories[i].name, name)) ++i;
                if (i == w->prior->directory_count || !same(&before, &w->prior->directories[i].identity)) {
                    e = "artifact_changed"; break;
                }
            }
            int child = openat(fd, entry->d_name, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
            if (child < 0) { e = "artifact_changed"; break; }
            if (fstat(child, &after) || !same(&before, &after)) e = "artifact_changed";
            if (!e) e = walk_tree(child, name, w);
            if (!e && (fstat(child, &after) || fstatat(fd, entry->d_name, &named, AT_SYMLINK_NOFOLLOW) ||
                !same(&before, &after) || !same(&before, &named))) e = "artifact_changed";
            close(child);
            if (e) break;
        } else {
            size_t i = 0;
            while (i < w->next->count && strcmp(w->next->files[i].name, name)) ++i;
            if (i == w->next->count || w->found[i]) { e = "unlisted_speech_artifact"; break; }
            SpeechPackageFile *item = &w->next->files[i];
            e = regular_file(fd, entry->d_name, item->bytes, item->sha256,
                w->control, w->context, &item->identity, w->prior ? &w->prior->files[i].identity : NULL);
            if (e) break;
            w->found[i] = 1;
        }
    }
    closedir(d); return e;
}
const char *speech_package_verify(int directory, const char *path, const char *pin,
    const char *(*control)(void *), void *context, SpeechPackageReceipt *receipt, int recheck) {
    if (directory < 0 || !path || path[0] != '/' || strlen(path) > 3800 || !digest(pin) ||
        !receipt || (recheck != 0 && recheck != 1) || (recheck && !receipt->verified)) return "invalid_configuration";
    if (!recheck) receipt->verified = 0;
    const char *e = control ? control(context) : NULL;
    if (e) return e;
    SpeechPackageReceipt *next = calloc(1, sizeof(*next));
    if (!next) return "resource_limit";
    struct stat named, after, manifest;
    char canonical[SPEECH_PACKAGE_PATH];
    if (fstat(directory, &next->root) || lstat(path, &named)) e = "artifact_unavailable";
    if (!e && (!S_ISDIR(next->root.st_mode) || next->root.st_uid != geteuid() || (next->root.st_mode & 022)))
        e = "unsupported_artifact";
    if (!e && (!realpath(path, canonical) || strcmp(canonical, path) || !same(&next->root, &named) ||
        (recheck && (!same(&next->root, &receipt->root) || strcmp(path, receipt->root_path))))) e = "artifact_changed";
    if (!e && (fstatat(directory, "manifest.json", &manifest, AT_SYMLINK_NOFOLLOW) ||
        manifest.st_size <= 0 || manifest.st_size > 1048576)) e = "invalid_speech_package";
    if (!e) e = regular_file(directory, "manifest.json", (uint64_t)manifest.st_size, pin,
        control, context, &next->manifest, recheck ? &receipt->manifest : NULL);
    char *body = NULL; Json *j = NULL;
    if (!e) {
        body = malloc((size_t)manifest.st_size + 1); j = malloc(sizeof(*j));
        if (!body || !j) e = "resource_limit";
    }
    if (!e) {
        int fd = openat(directory, "manifest.json", O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) e = "artifact_changed";
        size_t used = 0, length = (size_t)manifest.st_size;
        while (!e && used < length) {
            e = control ? control(context) : NULL;
            if (e) break;
            size_t want = length - used; if (want > 65536) want = 65536;
            ssize_t n = pread(fd, body + used, want, (off_t)used);
            if (n < 0 && errno == EINTR) continue;
            if (n <= 0) { e = "artifact_changed"; break; }
            used += (size_t)n;
        }
        if (!e && (fstat(fd, &after) || !same(&next->manifest, &after) ||
            fstatat(directory, "manifest.json", &named, AT_SYMLINK_NOFOLLOW) || !same(&after, &named))) e = "artifact_changed";
        if (fd >= 0) close(fd);
        if (!e && (!json_parse(j, body, length) || j->tokens[0].type != JT_OBJECT)) e = "invalid_speech_package";
    }
    const char *const root_keys[] = {"format", "abi_version", "platform", "kind", "threads", "data_dir", "files"};
    const char *const file_keys[] = {"name", "role", "bytes", "sha256"};
    unsigned seen[14] = {0}; int files = -1; size_t threads = 0;
    if (!e) {
        const char *platform = speech_package_platform();
        if (!keys(j, 0, root_keys, 7) || !equal(j, 0, "format", "samosa.sherpa-speech.v1") ||
            !equal(j, 0, "abi_version", "1.13.4")) e = "invalid_speech_package";
        else if (!platform || !equal(j, 0, "platform", platform)) e = "incompatible_speech_package";
        next->pocket = equal(j, 0, "kind", "pocket");
        if (!e && ((!next->pocket && !equal(j, 0, "kind", "kokoro")) ||
            !json_uint(j, json_get(j, 0, "threads"), &threads) || !threads || threads > (next->pocket ? 8u : 12u))) e = "invalid_speech_package";
        next->threads = (int)threads;
        files = json_get(j, 0, "files");
        if (!e && (files < 0 || j->tokens[files].type != JT_ARRAY || j->tokens[files].count < 1 ||
            j->tokens[files].count > SPEECH_PACKAGE_FILES)) e = "invalid_speech_package";
        if (!e && next->pocket && json_get(j, 0, "data_dir") >= 0) e = "invalid_speech_package";
        if (!e && !next->pocket) {
            char *data = json_string(j, json_get(j, 0, "data_dir"));
            if (!relative(data) || strchr(data, '!')) e = "invalid_speech_package";
            else strcpy(next->data_dir, data);
            free(data);
        }
    }
    for (int t = files + 1; !e && t < j->tokens[files].next; t = j->tokens[t].next) {
        char *name = json_string(j, json_get(j, t, "name"));
        char *hash = json_string(j, json_get(j, t, "sha256"));
        char *role = json_string(j, json_get(j, t, "role")); size_t bytes = 0, slot = 0;
        while (role && slot < 17 && strcmp(role, roles[slot])) ++slot;
        if (!keys(j, t, file_keys, 4) || !relative(name) || !strcmp(name, "manifest.json") ||
            !digest(hash) || !role || slot == 17 || slot == 13 ||
            !json_uint(j, json_get(j, t, "bytes"), &bytes) || !bytes || bytes > PACKAGE_BYTES ||
            next->total_bytes > PACKAGE_BYTES - bytes) e = "invalid_speech_package";
        if (!e && strchr(name, '!')) {
            char prefix[256];
            int n = snprintf(prefix, sizeof(prefix), "%s/voices/!v/", next->data_dir);
            /* Preserve the complete native data tree without admitting ! in
             * libraries, other data paths, nested variants or Pocket packages. */
            if (next->pocket || slot != 16 || n < 0 || (size_t)n >= sizeof(prefix) ||
                strncmp(name, prefix, (size_t)n) || !name[n] ||
                strchr(name + n, '/') || strchr(name + n, '!')) e = "invalid_speech_package";
        }
        for (size_t k = 0; !e && k < next->count; ++k)
            if (!strcmp(name, next->files[k].name)) e = "invalid_speech_package";
        if (!e && ((next->pocket && slot >= 10 && slot <= 12) ||
            (!next->pocket && slot >= 1 && slot <= 9) || (next->pocket && slot == 16))) e = "invalid_speech_package";
        if (!e && slot == 16 && (strncmp(name, next->data_dir, strlen(next->data_dir)) ||
            name[strlen(next->data_dir)] != '/')) e = "invalid_speech_package";
        if (!e && slot < 14 && seen[slot]++) e = "invalid_speech_package";
        if (!e) {
            SpeechPackageFile *f = &next->files[next->count++];
            strcpy(f->name, name); strcpy(f->sha256, hash); f->bytes = bytes; f->role = (unsigned)slot;
            next->total_bytes += bytes;
            if (slot < 14) snprintf(next->paths[slot], SPEECH_PACKAGE_PATH, "%s/%s", path, name);
        }
        free(name); free(hash); free(role);
    }
    if (!e && !seen[0]) e = "invalid_speech_package";
    for (unsigned i = next->pocket ? 1 : 10; !e && i <= (next->pocket ? 9u : 12u); ++i)
        if (!seen[i]) e = "invalid_speech_package";
    if (!e && recheck) {
        if (next->count != receipt->count || next->total_bytes != receipt->total_bytes) e = "artifact_changed";
        for (size_t i = 0; !e && i < next->count; ++i)
            if (strcmp(next->files[i].name, receipt->files[i].name) || strcmp(next->files[i].sha256, receipt->files[i].sha256) ||
                next->files[i].bytes != receipt->files[i].bytes || next->files[i].role != receipt->files[i].role) e = "artifact_changed";
    }
    if (!e) {
        Walk w = {.next = next, .prior = recheck ? receipt : NULL, .control = control, .context = context};
        e = walk_tree(directory, "", &w);
        for (size_t i = 0; !e && i < next->count; ++i) if (!w.found[i]) e = "artifact_unavailable";
        if (!e && !next->pocket) {
            int found = 0;
            for (size_t i = 0; i < next->directory_count; ++i) found |= !strcmp(next->directories[i].name, next->data_dir);
            if (!found) e = "invalid_speech_package";
        }
    }
    if (!e && recheck && next->directory_count != receipt->directory_count) e = "artifact_changed";
    if (!e && (fstat(directory, &after) || lstat(path, &named) || !same(&next->root, &after) || !same(&after, &named))) e = "artifact_changed";
    if (!e) e = control ? control(context) : NULL;
    if (!e) {
        strcpy(next->root_path, path); speech_package_identity(pin, next->model_identity);
        if (!next->pocket) snprintf(next->paths[13], SPEECH_PACKAGE_PATH, "%s/%s", path, next->data_dir);
        next->verified = 1; *receipt = *next;
    } else if (!recheck) receipt->verified = 0;
    free(body); free(j); free(next); return e;
}
const char *speech_package_options(const SpeechPackageReceipt *r, SpeechRuntimeOptions *o,
    const char **caro, const char **stuart) {
    if (!r || !r->verified || !o || !caro || !stuart) return "invalid_configuration";
    *o = (SpeechRuntimeOptions){.pocket = r->pocket, .threads = r->threads, .library = r->paths[0],
        .lm_flow = r->paths[1], .lm_main = r->paths[2], .encoder = r->paths[3], .decoder = r->paths[4],
        .text_conditioner = r->paths[5], .vocab = r->paths[6], .token_scores = r->paths[7],
        .model = r->paths[10], .voices = r->paths[11], .tokens = r->paths[12], .data_dir = r->paths[13]};
    /* Slot 13 is reserved for the verified Kokoro data directory, not a file. */
    if (!r->pocket) {
        /* Populated during verification so options point into the receipt. */
        if (!*r->paths[13]) return "invalid_configuration";
    }
    *caro = r->pocket ? r->paths[8] : NULL; *stuart = r->pocket ? r->paths[9] : NULL;
    return NULL;
}
