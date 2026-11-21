#define _POSIX_C_SOURCE 200809L
#include <samosa_inference/llama_launch.h>
#include <assert.h>
#include <errno.h>
#include <stdio.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

static void check(SamosaLlamaLaunch config, const char *expected) {
    int output[2]; assert(!pipe(output));
    pid_t pid = fork(); assert(pid >= 0);
    if (!pid) {
        close(output[0]);
        if (dup2(output[1], STDOUT_FILENO) < 0) _exit(125);
        close(output[1]);
        (void)samosa_llama_launch(&config); _exit(126);
    }
    close(output[1]);
    char observed[2048] = {0}; size_t at = 0;
    for (;;) {
        ssize_t n = read(output[0], observed + at, sizeof(observed) - at - 1);
        if (n < 0 && errno == EINTR) continue;
        assert(n >= 0); if (!n) break;
        at += (size_t)n; assert(at < sizeof(observed) - 1);
    }
    close(output[0]);
    int status; assert(waitpid(pid, &status, 0) == pid);
    assert(WIFEXITED(status) && WEXITSTATUS(status) == 0);
    if (strcmp(observed, expected)) fprintf(stderr, "unexpected argv:\n%s", observed);
    assert(!strcmp(observed, expected));
}
int main(int argc, char **argv) {
    if (argc > 1 && !strcmp(argv[1], "-m")) {
        for (int i = 1; i < argc; i++) puts(argv[i]);
        return 0;
    }
    assert(argc == 2);
    SamosaLlamaLaunch config = {
        .engine = argv[1], .model = "/owned/model with spaces.gguf", .alias = "configured-text",
        .port = 23456, .context_tokens = 8192, .threads = 0,
        .ram_bytes = 8589934592ULL, .perf_cores = 4
    };
    check(config, "-m\n/owned/model with spaces.gguf\n-ngl\n99\n-c\n8192\n-np\n1\n--cache-ram\n0\n--host\n127.0.0.1\n--port\n23456\n--no-ui\n--alias\nconfigured-text\n");
    config.context_override = "4096"; config.gpu_override = "0";
    config.threads_override = "2"; config.projector = "/owned/projector with spaces.gguf";
    check(config, "-m\n/owned/model with spaces.gguf\n-ngl\n0\n-c\n4096\n-t\n2\n-np\n1\n--cache-ram\n0\n--host\n127.0.0.1\n--port\n23456\n--no-ui\n--alias\nconfigured-text\n--mmproj\n/owned/projector with spaces.gguf\n");
    config.context_override = "0"; config.gpu_override = "-1";
    config.threads_override = "-2"; config.threads = 3; config.projector = NULL;
    check(config, "-m\n/owned/model with spaces.gguf\n-ngl\n99\n-c\n8192\n-t\n3\n-np\n1\n--cache-ram\n0\n--host\n127.0.0.1\n--port\n23456\n--no-ui\n--alias\nconfigured-text\n");
    config.engine = "/missing-owned-engine";
    errno = 0; assert(samosa_llama_launch(&config) == -1 && errno == ENOENT);
    puts("llama launch: PASS (original flags, overrides, projector, literal paths, exec refusal)");
    return 0;
}
