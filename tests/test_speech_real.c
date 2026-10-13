#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "speech_runtime.h"
#include "speech_package.h"
#include "speech_package_loader.h"
#include <fcntl.h>
#include <unistd.h>
#include "sha256.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
typedef struct { size_t bytes, calls, nonzero, cancel_after; int rate; Sha256 hash; } Capture;
static const char *capture(void *arg, const unsigned char *pcm, size_t bytes, int rate) {
    Capture *c = arg;
    if (!bytes || bytes > 8192 || bytes % 2 || rate < 8000 || rate > 96000 ||
        (c->rate && c->rate != rate) || c->bytes + bytes > 96000 * 45 * 2)
        return "invalid_real_speech_chunk";
    c->rate = rate; c->calls++;
    if (c->cancel_after && c->calls >= c->cancel_after) return "operation_cancelled";
    c->bytes += bytes;
    for (size_t i = 0; i < bytes; ++i) c->nonzero += pcm[i] != 0;
    sha256_update(&c->hash, pcm, bytes);
    return NULL;
}
/* Optional actual-model ABI/PCM gate. Models/runtime are supplied explicitly;
 * --package also verifies a host-pinned closed tree. Neither mode qualifies
 * arbitrary dynamic dependencies or intelligibility. */
int main(int argc, char **argv) {
    int package = argc == 4 && !strcmp(argv[1], "--package");
    if (!package && argc != 3) { fprintf(stderr, "usage: %s library model-directory | --package root manifest-sha256\n", argv[0]); return 2; }
    const char *names[] = {"lm_flow.int8.onnx", "lm_main.int8.onnx", "encoder.onnx", "decoder.int8.onnx",
        "text_conditioner.onnx", "vocab.json", "token_scores.json", "voices/caro_davy.wav", "voices/stuart_bell.wav"};
    char paths[9][4096];
    for (size_t i = 0; i < 9; ++i) {
        int n = snprintf(paths[i], sizeof(paths[i]), "%s/%s", argv[2], names[i]);
        if (n < 0 || (size_t)n >= sizeof(paths[i])) return 2;
    }
    SpeechRuntimeOptions o = {.library = argv[1], .threads = 2, .pocket = 1,
        .lm_flow = paths[0], .lm_main = paths[1], .encoder = paths[2], .decoder = paths[3],
        .text_conditioner = paths[4], .vocab = paths[5], .token_scores = paths[6]};
    SpeechPackageReceipt *receipt = NULL; int package_fd = -1;
    const char *references[] = {paths[7], paths[8]};
    const char *e = NULL;
    if (package) {
        receipt = calloc(1, sizeof(*receipt));
        if (!receipt) return 1;
        package_fd = open(argv[2], O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        e = speech_package_verify(package_fd, argv[2], argv[3], NULL, NULL, receipt, 0);
        LibraryRuntimePolicy policy;
        if (!e) e = library_runtime_host_policy(&policy);
        if (!e) e = speech_package_loader_verify(package_fd, argv[2], argv[3], &policy, NULL, NULL, receipt);
        if (!e) puts("PASS actual speech native loader graph and host compatibility");
        if (!e) e = speech_package_options(receipt, &o, &references[0], &references[1]);
        if (e) { fprintf(stderr, "%s\n", e); if (package_fd >= 0) close(package_fd); free(receipt); return 1; }
        printf("PASS actual speech package: %zu files, %llu bytes, model_identity=%s\n",
            receipt->count, (unsigned long long)receipt->total_bytes, receipt->model_identity);
    }
    SpeechRuntime *r = NULL;
    e = speech_runtime_open(&o, &r);
    if (e) { fprintf(stderr, "%s\n", e); if (package_fd >= 0) close(package_fd); free(receipt); return 1; }
    const char *pocket_voices[]={"caro","stuart"};
    const char *kokoro_voices[]={"bella","sarah","nicole","adam","michael","emma"};
    const char *const *voices=o.pocket?pocket_voices:kokoro_voices;
    const char *kind=o.pocket?"Pocket":"Kokoro";
    for (size_t i = 0; i < (o.pocket?2u:6u); ++i) {
        Capture c = {0}; sha256_init(&c.hash);
        SpeechRuntimeResult result;
        e = speech_runtime_generate(r, "The meeting starts at nine. Please bring your notes.",
            voices[i], o.pocket?references[i]:NULL, capture, &c, &result);
        unsigned char digest[32]; char hex[65]; sha256_final(&c.hash, digest); sha256_hex(digest, hex);
        if (e || c.bytes != result.samples * 2 || c.rate != result.sample_rate ||
            c.bytes < (size_t)c.rate || !c.nonzero || c.calls < 2 || strcmp(hex, result.pcm_sha256)) {
            fprintf(stderr, "actual %s failed: %s (%zu bytes, %zu chunks)\n", kind, e ? e : "invalid_pcm", c.bytes, c.calls);
            speech_runtime_close(r); if (package_fd >= 0) close(package_fd); free(receipt); return 1;
        }
        printf("PASS actual %s %s: rate=%d samples=%zu chunks=%zu callbacks=%zu pcm_sha256=%s\n",
            kind, voices[i], result.sample_rate, result.samples, c.calls, result.callbacks, hex);
        fflush(stdout);
        if (i == 0) {
            Capture cancelled = {.cancel_after = 2}; sha256_init(&cancelled.hash);
            e = speech_runtime_generate(r, "The meeting starts at nine. Please bring your notes.",
                voices[0], o.pocket?references[0]:NULL, capture, &cancelled, &result);
            if (!e || strcmp(e, "operation_cancelled") || cancelled.calls != 2 ||
                !cancelled.bytes || result.samples || result.pcm_sha256[0]) {
                fprintf(stderr, "actual %s cancellation failed\n",kind);
                speech_runtime_close(r); if (package_fd >= 0) close(package_fd); free(receipt); return 1;
            }
            printf("PASS actual %s cancellation after partial PCM; no successful receipt\n",kind);
        }
    }
    speech_runtime_close(r);
    if (package) {
        e = speech_package_verify(package_fd, argv[2], argv[3], NULL, NULL, receipt, 1);
        close(package_fd); free(receipt);
        if (e) { fprintf(stderr, "%s\n", e); return 1; }
        puts("PASS actual speech package retained identity after generation/cancellation/recovery");
    }
    return 0;
}
