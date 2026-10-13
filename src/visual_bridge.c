#define _POSIX_C_SOURCE 200809L
#define _DARWIN_C_SOURCE
#include "artifact.h"
#include "sha256.h"
#include "vision.h"
#include <fcntl.h>
#include <limits.h>
#include <sys/stat.h>
#include <unistd.h>
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
/* The existing bridge belongs to the guardian's process group. Input/output
 * are bound owned files; fd5 is the already verified model directory. */
int vision_bridge_dispatch(int argc, char **argv) {
    if (argc < 2 || strcmp(argv[1], "--vision-engine-bridge"))
        return -1;
    if (argc != 6 || strlen(argv[3]) != 64 ||
        strspn(argv[3], "0123456789abcdef") != 64 || argv[2][0] != '/' ||
        argv[4][0] != '/' || argv[5][0] != '/')
        return 2;
    int executable = open(argv[2], O_RDONLY | O_NOFOLLOW | O_NONBLOCK),
        input = open(argv[4], O_RDONLY | O_NOFOLLOW),
        output = open(argv[5], O_WRONLY | O_NOFOLLOW);
    if (executable < 0 || input < 0 || output < 0)
        return 2;
    struct stat engine, a, b;
    if (fstat(executable, &engine) || !S_ISREG(engine.st_mode) ||
        engine.st_uid != geteuid() || engine.st_nlink != 1 ||
        (engine.st_mode & 022) || !(engine.st_mode & 0111) ||
        engine.st_size <= 0 ||
        artifact_verify(executable, (uint64_t)engine.st_size, 67108864u,
                        argv[3]) ||
        fstat(input, &a) || fstat(output, &b) || !S_ISREG(a.st_mode) ||
        !S_ISREG(b.st_mode) || a.st_uid != geteuid() || b.st_uid != geteuid() ||
        a.st_nlink != 1 || b.st_nlink != 1 || (a.st_mode & 077) ||
        (b.st_mode & 077) || dup2(input, 0) < 0 || dup2(output, 1) < 0)
        return 2;
    if (input > 2)
        close(input);
    if (output > 2)
        close(output);
#ifdef __APPLE__
    struct stat held, named;
    char model[PATH_MAX];
    if (fstat(executable, &held) || lstat(argv[2], &named) ||
        !same(&held, &engine) || !same(&held, &named) ||
        fcntl(5, F_GETPATH, model) < 0 || fstat(5, &held) ||
        lstat(model, &named) || !same(&held, &named))
        return 2;
    char *args[] = {argv[2], model, NULL};
    execv(argv[2], args);
#else
    struct stat held;
    if (fstat(executable, &held) || !same(&held, &engine))
        return 2;
    char path[64];
    snprintf(path, sizeof(path), "/dev/fd/%d", executable);
    char *args[] = {path, "/dev/fd/5", NULL};
    execv(path, args);
#endif
    return 2;
}
