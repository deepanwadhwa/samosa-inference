#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "speech_runtime.h"
#include "sha256.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
static unsigned checks;
#define CHECK(x) do { ++checks; if (!(x)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #x); exit(1); } } while (0)
typedef struct { unsigned char pcm[64]; size_t used, calls; const char *failure; } Sink;
static const char *receive(void *context, const unsigned char *pcm, size_t bytes, int rate) {
    Sink *s = context;
    CHECK(rate == 24000); CHECK(bytes > 0 && bytes <= 8192 && bytes % 2 == 0);
    s->calls++;
    if (s->failure) return s->failure;
    CHECK(s->used + bytes <= sizeof(s->pcm));
    memcpy(s->pcm + s->used, pcm, bytes); s->used += bytes;
    return NULL;
}
static void wav(const char *path, int channels) {
    unsigned char b[] = {
        'R','I','F','F',40,0,0,0,'W','A','V','E','f','m','t',' ',16,0,0,0,
        1,0,1,0,128,62,0,0,0,125,0,0,2,0,16,0,'d','a','t','a',4,0,0,0,0,64,0,192
    };
    if (channels == 2) { b[22] = 2; b[28] = 0; b[29] = 250; b[32] = 4; }
    FILE *f = fopen(path, "wb"); CHECK(f != NULL);
    CHECK(fwrite(b, 1, sizeof(b), f) == sizeof(b)); CHECK(fclose(f) == 0);
}
static void generation(SpeechRuntime *r, const char *reference) {
    SpeechRuntimeResult out;
    Sink s = {0};
    CHECK(!speech_runtime_generate(r, "hello", "sarah", reference, receive, &s, &out));
    CHECK(out.sample_rate == 24000 && out.samples == 4 && out.callbacks == 2);
    CHECK(s.calls == 2 && s.used == 8);
    const unsigned char expected[] = {0,0,102,38,154,217,0,0};
    CHECK(!memcmp(s.pcm, expected, sizeof(expected)));
    char digest[65]; hash_bytes(s.pcm, s.used, digest);
    CHECK(!strcmp(digest, out.pcm_sha256));
    s = (Sink){0};
    CHECK(!speech_runtime_generate(r, "final-only", "sarah", reference, receive, &s, &out));
    CHECK(s.calls == 1 && out.samples == 4 && out.callbacks == 1 && s.used == 8);
    s = (Sink){0};
    CHECK(!speech_runtime_generate(r, "clip", "sarah", reference, receive, &s, &out));
    CHECK(s.pcm[0] == 255 && s.pcm[1] == 127 && s.pcm[2] == 1 && s.pcm[3] == 128);
    const char *requests[] = {"nan", "bad-rate", "empty", "null", "partial", "mismatch", "limit"};
    const char *errors[] = {"invalid_speech_audio", "invalid_speech_audio", "invalid_speech_audio",
                           "invalid_speech_audio", "speech_stream_mismatch", "speech_stream_mismatch", "speech_audio_limit"};
    for (size_t i = 0; i < sizeof(requests)/sizeof(*requests); ++i) {
        s = (Sink){0}; memset(&out, 0x55, sizeof(out));
        const char *e = speech_runtime_generate(r, requests[i], "sarah", reference, receive, &s, &out);
        CHECK(e && !strcmp(e, errors[i])); CHECK(out.samples == 0 && !out.pcm_sha256[0]);
    }
    for (unsigned i = 0; i < 2; ++i) {
        s = (Sink){.failure = i ? "operation_cancelled" : "output_unavailable"};
        CHECK(speech_runtime_generate(r, "hello", "sarah", reference, receive, &s, &out) == s.failure);
        CHECK(s.calls == 1 && s.used == 0 && out.samples == 0);
    }
    s = (Sink){0};
    CHECK(!strcmp(speech_runtime_generate(r, "", "sarah", reference, receive, &s, &out), "invalid_speech_request"));
    CHECK(!strcmp(speech_runtime_generate(r, "\xc0\x80", "sarah", reference, receive, &s, &out), "invalid_speech_request"));
    char text[2402]; memset(text, 'a', sizeof(text)-1); text[sizeof(text)-1] = 0;
    CHECK(!strcmp(speech_runtime_generate(r, text, "sarah", reference, receive, &s, &out), "invalid_speech_request"));
    CHECK(s.calls == 0);
}
int main(int argc, char **argv) {
    CHECK(argc == 2);
    SpeechRuntimeOptions o = {.library = argv[1], .threads = 6, .model = "model",
        .voices = "voices", .tokens = "tokens", .data_dir = "data"};
    SpeechRuntime *r = NULL;
    CHECK(!speech_runtime_open(&o, &r)); CHECK(r != NULL);
    generation(r, NULL);
    SpeechRuntimeResult out; Sink s = {0};
    CHECK(!strcmp(speech_runtime_generate(r, "hello", "unknown", NULL, receive, &s, &out), "invalid_speech_voice"));
    CHECK(s.calls == 0); speech_runtime_close(r);
    char directory[] = "/tmp/samosa-speech-runtime.XXXXXX"; CHECK(mkdtemp(directory) != NULL);
    char reference[512], alias[512];
    snprintf(reference, sizeof(reference), "%s/reference.wav", directory);
    snprintf(alias, sizeof(alias), "%s/alias.wav", directory);
    o = (SpeechRuntimeOptions){.library = argv[1], .threads = 2, .pocket = 1,
        .lm_flow = "flow", .lm_main = "main", .encoder = "encoder", .decoder = "decoder",
        .text_conditioner = "text", .vocab = "vocab", .token_scores = "scores"};
    CHECK(!speech_runtime_open(&o, &r));
    wav(reference, 1); generation(r, reference);
    wav(reference, 2); generation(r, reference);
    CHECK(symlink(reference, alias) == 0);
    CHECK(!strcmp(speech_runtime_generate(r, "hello", NULL, alias, receive, &s, &out), "invalid_speech_reference"));
    CHECK(!strcmp(speech_runtime_generate(r, "hello", NULL, directory, receive, &s, &out), "invalid_speech_reference"));
    /* Corrupt bounds, alignment and format fields; each refusal precedes PCM. */
    const int offsets[] = {4, 16, 20, 22, 24, 28, 32, 34, 40};
    for (size_t i = 0; i < sizeof(offsets)/sizeof(*offsets); ++i) {
        wav(reference, 1);
        FILE *f = fopen(reference, "r+b"); CHECK(f != NULL);
        CHECK(fseek(f, offsets[i], SEEK_SET) == 0); CHECK(fputc(255, f) == 255); CHECK(fclose(f) == 0);
        s = (Sink){0};
        CHECK(!strcmp(speech_runtime_generate(r, "hello", NULL, reference, receive, &s, &out), "invalid_speech_reference"));
        CHECK(s.calls == 0 && out.samples == 0);
    }
    speech_runtime_close(r); speech_runtime_close(NULL);
    o.threads = 0; r = (SpeechRuntime *)1;
    CHECK(!strcmp(speech_runtime_open(&o, &r), "invalid_speech_runtime")); CHECK(r == NULL);
    o.threads = 2; o.library = "/missing/speech-library";
    CHECK(!strcmp(speech_runtime_open(&o, &r), "speech_runtime_unavailable")); CHECK(r == NULL);
    o.library = argv[1];
    CHECK(setenv("DYLD_LIBRARY_PATH", directory, 1) == 0);
    CHECK(speech_runtime_open(&o, &r) != NULL && r == NULL);
    CHECK(unsetenv("DYLD_LIBRARY_PATH") == 0);
    CHECK(unlink(alias) == 0); CHECK(unlink(reference) == 0); CHECK(rmdir(directory) == 0);
    printf("PASS native speech runtime: %u checks; Kokoro/Pocket ABI, PCM, fallback, cancellation and malformed audio\n", checks);
    return 0;
}
