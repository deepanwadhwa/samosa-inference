#define _POSIX_C_SOURCE 200809L
#include "speech_runtime.h"
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
 * this does not qualify package provenance or intelligibility. */
int main(int argc, char **argv) {
    if (argc != 3) { fprintf(stderr, "usage: %s library model-directory\n", argv[0]); return 2; }
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
    SpeechRuntime *r = NULL;
    const char *e = speech_runtime_open(&o, &r);
    if (e) { fprintf(stderr, "%s\n", e); return 1; }
    for (size_t i = 0; i < 2; ++i) {
        Capture c = {0}; sha256_init(&c.hash);
        SpeechRuntimeResult result;
        e = speech_runtime_generate(r, "The meeting starts at nine. Please bring your notes.",
            i ? "stuart" : "caro", paths[7+i], capture, &c, &result);
        unsigned char digest[32]; char hex[65]; sha256_final(&c.hash, digest); sha256_hex(digest, hex);
        if (e || c.bytes != result.samples * 2 || c.rate != result.sample_rate ||
            c.bytes < (size_t)c.rate || !c.nonzero || c.calls < 2 || strcmp(hex, result.pcm_sha256)) {
            fprintf(stderr, "actual Pocket failed: %s (%zu bytes, %zu chunks)\n", e ? e : "invalid_pcm", c.bytes, c.calls);
            speech_runtime_close(r); return 1;
        }
        printf("PASS actual Pocket %s: rate=%d samples=%zu chunks=%zu callbacks=%zu pcm_sha256=%s\n",
            i ? "stuart" : "caro", result.sample_rate, result.samples, c.calls, result.callbacks, hex);
        fflush(stdout);
        if (i == 0) {
            Capture cancelled = {.cancel_after = 2}; sha256_init(&cancelled.hash);
            e = speech_runtime_generate(r, "The meeting starts at nine. Please bring your notes.",
                "caro", paths[7], capture, &cancelled, &result);
            if (!e || strcmp(e, "operation_cancelled") || cancelled.calls != 2 ||
                !cancelled.bytes || result.samples || result.pcm_sha256[0]) {
                fprintf(stderr, "actual Pocket cancellation failed\n");
                speech_runtime_close(r); return 1;
            }
            puts("PASS actual Pocket cancellation after partial PCM; no successful receipt");
        }
    }
    speech_runtime_close(r);
    return 0;
}
