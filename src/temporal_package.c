#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "temporal_package.h"
#include "artifact.h"
#include "json.h"
#include "sha256.h"
#include <fcntl.h>
#include <unistd.h>
#define PACKAGE_MAX UINT64_C(4294967296)
static int sha(const char *s) {
    return s && strlen(s) == 64 && strspn(s, "0123456789abcdef") == 64;
}
void temporal_package_identity(const char *manifest, char out[65]) {
    out[0] = 0;
    if (!sha(manifest))
        return;
    char material[96];
    int n =
        snprintf(material, sizeof(material), "molmo2-package-v1:%s", manifest);
    if (n > 0 && (size_t)n < sizeof(material))
        hash_bytes(material, (size_t)n, out);
}
static int same(const struct stat *a, const struct stat *b) {
    if (a->st_dev != b->st_dev || a->st_ino != b->st_ino ||
        a->st_size != b->st_size || a->st_mode != b->st_mode ||
        a->st_uid != b->st_uid || a->st_nlink != b->st_nlink)
        return 0;
#ifdef __APPLE__
    return a->st_mtimespec.tv_sec == b->st_mtimespec.tv_sec &&
           a->st_mtimespec.tv_nsec == b->st_mtimespec.tv_nsec &&
           a->st_ctimespec.tv_sec == b->st_ctimespec.tv_sec &&
           a->st_ctimespec.tv_nsec == b->st_ctimespec.tv_nsec;
#else
    return a->st_mtim.tv_sec == b->st_mtim.tv_sec &&
           a->st_mtim.tv_nsec == b->st_mtim.tv_nsec &&
           a->st_ctim.tv_sec == b->st_ctim.tv_sec &&
           a->st_ctim.tv_nsec == b->st_ctim.tv_nsec;
#endif
}
static int equal(const Json *j, int object, const char *key,
                 const char *expected) {
    char *s = json_string(j, json_get(j, object, key));
    int ok = s && !strcmp(s, expected);
    free(s);
    return ok;
}
static int leaf(const char *name) {
    if (!name || !*name || strlen(name) > 255 || name[0] == '.')
        return 0;
    for (const unsigned char *p = (const unsigned char *)name; *p; p++)
        if (!isalnum(*p) && !strchr("-_.", *p))
            return 0;
    return 1;
}
static const char *file_verify(int directory, const char *name, uint64_t bytes,
                               const char *pin, const char *(*control)(void *),
                               void *context, struct stat *identity,
                               int recheck) {
    int fd =
        openat(directory, name, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        return "artifact_unavailable";
    struct stat before, after, named;
    const char *error = fstat(fd, &before) ? "artifact_unavailable" : NULL;
    if (!error && (!S_ISREG(before.st_mode) || before.st_uid != geteuid() ||
                   before.st_nlink != 1 || (before.st_mode & 022)))
        error = "unsupported_artifact";
    if (!error && recheck && !same(&before, identity))
        error = "artifact_changed";
    if (!error)
        error = artifact_verify_controlled(fd, bytes, PACKAGE_MAX, pin, control,
                                           context);
    if (!error && (fstat(fd, &after) ||
                   fstatat(directory, name, &named, AT_SYMLINK_NOFOLLOW) ||
                   !same(&before, &after) || !same(&before, &named)))
        error = "artifact_changed";
    close(fd);
    if (!error && !recheck)
        *identity = before;
    return error;
}
const char *
temporal_package_verify(int directory, const char *path, const char *pin,
                        const char *(*control)(void *), void *context,
                        TemporalPackageReceipt *receipt, int recheck) {
    if (directory < 0 || !path || path[0] != '/' || !sha(pin) || !receipt ||
        (recheck != 0 && recheck != 1))
        return "invalid_configuration";
    const char *error = control ? control(context) : NULL;
    struct stat before, named, manifest, after;
    if (!error && (fstat(directory, &before) || lstat(path, &named)))
        error = "artifact_unavailable";
    if (!error && (!S_ISDIR(before.st_mode) || before.st_uid != geteuid() ||
                   (before.st_mode & 022)))
        error = "unsupported_artifact";
    if (!error && (!same(&before, &named) ||
                   (recheck && !same(&before, &receipt->directory))))
        error = "artifact_changed";
    if (!error &&
        (fstatat(directory, "manifest.json", &manifest, AT_SYMLINK_NOFOLLOW) ||
         manifest.st_size <= 0 || manifest.st_size > 1048576))
        error = "invalid_model_package";
    if (!error)
        error =
            file_verify(directory, "manifest.json", (uint64_t)manifest.st_size,
                        pin, control, context, &receipt->manifest, recheck);
    if (error)
        return error;
    int fd = openat(directory, "manifest.json",
                    O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0)
        return "artifact_unavailable";
    size_t length = (size_t)manifest.st_size;
    char *body = malloc(length + 1);
    if (!body) {
        close(fd);
        return "resource_limit";
    }
    size_t used = 0;
    while (!error && used < length) {
        error = control ? control(context) : NULL;
        if (error)
            break;
        size_t want = length - used;
        if (want > 65536)
            want = 65536;
        ssize_t n = pread(fd, body + used, want, (off_t)used);
        if (n < 0 && errno == EINTR)
            continue;
        if (n <= 0) {
            error = "artifact_changed";
            break;
        }
        used += (size_t)n;
    }
    body[used] = 0;
    if (!error && (memchr(body, 0, used) || fstat(fd, &after) ||
                   !same(&manifest, &after)))
        error = "artifact_changed";
    close(fd);
    Json j;
    size_t resident = 0, bits = 0, group = 0;
    int files = -1, quant = -1;
    if (!error &&
        (!json_parse(&j, body, length) || j.tokens[0].type != JT_OBJECT))
        error = "invalid_model_package";
    if (!error) {
        files = json_get(&j, 0, "files");
        quant = json_get(&j, 0, "quantization");
        if (!equal(&j, 0, "format", "samosa.molmo2.mlx.v1") ||
            !equal(&j, 0, "package_id", "molmo2-4b-mlx-q4-v1") ||
            !equal(&j, 0, "model_id", "allenai/Molmo2-4B") ||
            !equal(&j, 0, "upstream_revision",
                   "042abfa7a38879a376cec03d949eff0aefaa0600") ||
            !equal(&j, 0, "processor_fingerprint",
                   "808de9add76144a557348c5f5180a8408b12ca83592c7a6a257ae69c968"
                   "e51df") ||
            quant < 0 || j.tokens[quant].type != JT_OBJECT ||
            !equal(&j, quant, "mode", "affine") ||
            !json_uint(&j, json_get(&j, quant, "bits"), &bits) || bits != 4 ||
            !json_uint(&j, json_get(&j, quant, "group_size"), &group) ||
            group != 64 ||
            !json_uint(&j, json_get(&j, 0, "estimated_resident_bytes"),
                       &resident) ||
            !resident || resident > UINT64_C(3932160000) || files < 0 ||
            j.tokens[files].type != JT_ARRAY || j.tokens[files].count < 4 ||
            j.tokens[files].count > 64)
            error = "invalid_model_package";
    }
    size_t count = 0;
    uint64_t total = 0;
    int config = 0, tokenizer = 0, processor = 0, weights = 0;
    if (!error && recheck && receipt->count != (size_t)j.tokens[files].count)
        error = "artifact_changed";
    for (int item = files + 1; !error && item < j.tokens[files].next;
         item = j.tokens[item].next) {
        char *name = json_string(&j, json_get(&j, item, "name")),
             *digest = json_string(&j, json_get(&j, item, "sha256")),
             *role = json_string(&j, json_get(&j, item, "role"));
        size_t bytes = 0;
        if (j.tokens[item].type != JT_OBJECT || !leaf(name) || !sha(digest) ||
            !role || !json_uint(&j, json_get(&j, item, "bytes"), &bytes) ||
            !bytes || bytes > PACKAGE_MAX || total > PACKAGE_MAX - bytes)
            error = "invalid_model_package";
        for (size_t k = 0; !error && k < count; k++)
            if (!strcmp(name, receipt->files[k].name))
                error = "invalid_model_package";
        if (!error) {
            size_t n = strlen(name);
            int tensor = n > 12 && !strcmp(name + n - 12, ".safetensors");
            if ((strcmp(role, "metadata") && strcmp(role, "tokenizer") &&
                 strcmp(role, "weights")) ||
                tensor != !strcmp(role, "weights"))
                error = "invalid_model_package";
            if (!strcmp(name, "config.json")) {
                if (strcmp(role, "metadata") ||
                    strcmp(digest, "17e072e3c3b29d9be7a348c74b88658e67ccce094c3"
                                   "1b21f87646f6cecd2a76f"))
                    error = "invalid_model_package";
                config = 1;
            }
            if (!strcmp(name, "processor.json")) {
                if (strcmp(role, "metadata") ||
                    strcmp(digest, "808de9add76144a557348c5f5180a8408b12ca83592"
                                   "c7a6a257ae69c968e51df"))
                    error = "invalid_model_package";
                processor = 1;
            }
            if (!strcmp(name, "tokenizer.json")) {
                if (strcmp(role, "tokenizer") ||
                    strcmp(digest, "95e80901c901584f416b8fd4349fd60022774b89ba4"
                                   "377626511f0562cc599f7"))
                    error = "invalid_model_package";
                tokenizer = 1;
            } else if (!strcmp(role, "tokenizer"))
                error = "invalid_model_package";
            weights |= !strcmp(role, "weights");
        }
        TemporalPackageFile *entry = &receipt->files[count];
        if (!error && recheck &&
            (strcmp(entry->name, name) || strcmp(entry->sha256, digest) ||
             entry->bytes != bytes))
            error = "artifact_changed";
        if (!error)
            error = file_verify(directory, name, bytes, digest, control,
                                context, &entry->identity, recheck);
        if (!error) {
            if (!recheck) {
                strcpy(entry->name, name);
                strcpy(entry->sha256, digest);
                entry->bytes = bytes;
            }
            total += bytes;
            count++;
        }
        free(name);
        free(digest);
        free(role);
    }
    if (!error && (!config || !tokenizer || !processor || !weights))
        error = "invalid_model_package";
    if (!error && (fstat(directory, &after) || lstat(path, &named) ||
                   !same(&before, &after) || !same(&before, &named)))
        error = "artifact_changed";
    if (!error && recheck && receipt->total_bytes != total)
        error = "artifact_changed";
    if (!error) {
        receipt->count = count;
        receipt->total_bytes = total;
        if (!recheck)
            receipt->directory = before;
        error = control ? control(context) : NULL;
    }
    free(body);
    return error;
}
