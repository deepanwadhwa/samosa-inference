#define _POSIX_C_SOURCE 200809L
#define _XOPEN_SOURCE 700
#define _DARWIN_C_SOURCE
#include "speech_runtime.h"
#include "sherpa_abi.h"
#include "engine_runtime.h"
#include "json.h"
#include "sha256.h"
#include <dlfcn.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

struct SpeechRuntime {
    void *library;
    const SamosaSherpaOfflineTts *tts;
    SamosaSherpaDestroyTts destroy;
    SamosaSherpaTtsSampleRate rate;
    SamosaSherpaGenerateTts generate;
    SamosaSherpaDestroyAudio destroy_audio;
    int pocket;
};
void speech_runtime_close(SpeechRuntime *r) {
    if (!r) return;
    if (r->tts) r->destroy(r->tts);
    if (r->library) dlclose(r->library);
    free(r);
}
/* POSIX dlsym supplies function symbols. Copy their representation to avoid
 * undefined pointer casts and preserve strict C11 compiler diagnostics. */
#define LOAD(member, symbol) do { \
    void *address = dlsym(r->library, symbol); \
    _Static_assert(sizeof(member) == sizeof(address), "unsupported function ABI"); \
    if (!address) { speech_runtime_close(r); return "speech_runtime_symbols"; } \
    memcpy(&(member), &address, sizeof(member)); \
} while (0)
const char *speech_runtime_open(const SpeechRuntimeOptions *o, SpeechRuntime **out) {
    if (!out) return "invalid_speech_runtime";
    *out = NULL;
    if (!o || !o->library || o->library[0] != '/' ||
        (o->pocket != 0 && o->pocket != 1) || o->threads < 1 || o->threads > 12)
        return "invalid_speech_runtime";
    if (o->pocket ? (!o->lm_flow || !o->lm_main || !o->encoder || !o->decoder ||
        !o->text_conditioner || !o->vocab || !o->token_scores) :
        (!o->model || !o->voices || !o->tokens || !o->data_dir))
        return "invalid_speech_runtime";
    const char *error = engine_runtime_environment();
    if (error) return error;
    SpeechRuntime *r = calloc(1, sizeof(*r));
    if (!r) return "speech_resource_limit";
    r->library = dlopen(o->library, RTLD_NOW | RTLD_LOCAL);
    if (!r->library) { free(r); return "speech_runtime_unavailable"; }
    SamosaSherpaCreateTts create;
    const char *(*version)(void);
    LOAD(version, "SherpaOnnxGetVersionStr");
    const char *abi = version();
    if (!abi || strcmp(abi, "1.13.4")) { speech_runtime_close(r); return "incompatible_speech_abi"; }
    LOAD(create, "SherpaOnnxCreateOfflineTts");
    LOAD(r->destroy, "SherpaOnnxDestroyOfflineTts");
    LOAD(r->rate, "SherpaOnnxOfflineTtsSampleRate");
    LOAD(r->generate, "SherpaOnnxOfflineTtsGenerateWithConfig");
    LOAD(r->destroy_audio, "SherpaOnnxDestroyOfflineTtsGeneratedAudio");
    SamosaSherpaTtsConfig c = {0};
    c.model.num_threads = o->threads;
    c.model.provider = "cpu";
    c.max_num_sentences = 1;
    c.silence_scale = .12f;
    if (o->pocket) {
        c.model.pocket.lm_flow = o->lm_flow;
        c.model.pocket.lm_main = o->lm_main;
        c.model.pocket.encoder = o->encoder;
        c.model.pocket.decoder = o->decoder;
        c.model.pocket.text_conditioner = o->text_conditioner;
        c.model.pocket.vocab_json = o->vocab;
        c.model.pocket.token_scores_json = o->token_scores;
        c.model.pocket.voice_embedding_cache_capacity = 4;
    } else {
        c.model.kokoro.model = o->model;
        c.model.kokoro.voices = o->voices;
        c.model.kokoro.tokens = o->tokens;
        c.model.kokoro.data_dir = o->data_dir;
    }
    r->tts = create(&c);
    r->pocket = o->pocket;
    if (!r->tts) { speech_runtime_close(r); return "speech_model_unavailable"; }
    int rate = r->rate(r->tts);
    if (rate < 8000 || rate > 96000) {
        speech_runtime_close(r); return "invalid_speech_audio";
    }
    *out = r;
    return NULL;
}
#undef LOAD
static unsigned le16(const unsigned char *p) { return p[0] | ((unsigned)p[1] << 8); }
static uint32_t le32(const unsigned char *p) {
    return p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
}
/* Reuse the gateway's mono/stereo PCM16 conversion, with strict RIFF bounds.
 * Metadata chunks remain supported; duplicate format/data chunks are rejected.
 * Only a regular, non-symlink reference is accepted by this private loader. */
static const char *reference_read(const char *path, float **out, int32_t *count, int32_t *rate) {
    if (!path || path[0] != '/') return "invalid_speech_reference";
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_CLOEXEC);
    struct stat s;
    if (fd < 0) return "invalid_speech_reference";
    if (fstat(fd, &s) || !S_ISREG(s.st_mode) || s.st_size < 44 || s.st_size > (16 << 20)) {
        close(fd); return "invalid_speech_reference";
    }
    size_t size = (size_t)s.st_size, used = 0;
    unsigned char *b = malloc(size);
    if (!b) { close(fd); return "speech_resource_limit"; }
    while (used < size) {
        ssize_t n = read(fd, b + used, size - used);
        if (n < 0 && errno == EINTR) continue;
        if (n <= 0) break;
        used += (size_t)n;
    }
    close(fd);
    const char *error = "invalid_speech_reference";
    if (used != size || memcmp(b, "RIFF", 4) || memcmp(b + 8, "WAVE", 4) ||
        (uint64_t)le32(b + 4) + 8 != size) goto done;
    unsigned channels = 0, sample_rate = 0;
    const unsigned char *data = NULL;
    size_t length = 0, at = 12;
    while (at < size) {
        if (size - at < 8) goto done;
        size_t n = le32(b + at + 4), padded = n + (n & 1);
        if (padded > size - at - 8) goto done;
        if (!memcmp(b + at, "fmt ", 4)) {
            if (channels || n < 16 || le16(b + at + 8) != 1) goto done;
            channels = le16(b + at + 10); sample_rate = le32(b + at + 12);
            if ((channels != 1 && channels != 2) || sample_rate < 8000 || sample_rate > 96000 ||
                le16(b + at + 22) != 16 || le16(b + at + 20) != 2 * channels ||
                le32(b + at + 16) != sample_rate * 2 * channels) goto done;
        } else if (!memcmp(b + at, "data", 4)) {
            if (data || !n) goto done;
            data = b + at + 8; length = n;
        }
        at += 8 + padded;
    }
    if (!channels || !data || length % (2 * channels)) goto done;
    *count = (int32_t)(length / (2 * channels));
    *out = malloc((size_t)*count * sizeof(**out));
    if (!*out) { error = "speech_resource_limit"; goto done; }
    for (int32_t i = 0; i < *count; ++i) {
        int sum = 0;
        for (unsigned c = 0; c < channels; ++c) {
            unsigned v = le16(data + ((size_t)i * channels + c) * 2);
            sum += v >= 32768 ? (int)v - 65536 : (int)v;
        }
        (*out)[i] = (float)sum / (32768.f * channels);
    }
    *rate = (int32_t)sample_rate;
    error = NULL;
done:
    free(b); return error;
}
typedef struct {
    SpeechPcmSink sink;
    void *context;
    int rate;
    size_t count, callbacks, maximum;
    Sha256 hash;
    const char *error;
} PcmStream;
static int convert_pcm(const float *samples, size_t n, unsigned char *pcm) {
    for (size_t i = 0; i < n; ++i) {
        float v = samples[i];
        if (!isfinite(v)) return 0;
        if (v > 1) v = 1;
        if (v < -1) v = -1;
        int value = (int)(v * 32767.f + (v >= 0 ? .5f : -.5f));
        pcm[i * 2] = (unsigned char)value;
        pcm[i * 2 + 1] = (unsigned char)((unsigned)(uint16_t)value >> 8);
    }
    return 1;
}
static int32_t pcm_progress(const float *samples, int32_t n, float progress, void *arg) {
    PcmStream *s = arg;
    (void)progress;
    if (s->error) return 0;
    if (n == 0) return 1;
    if (n < 0 || !samples) { s->error = "invalid_speech_audio"; return 0; }
    if ((size_t)n > s->maximum - s->count) { s->error = "speech_audio_limit"; return 0; }
    s->callbacks++;
    unsigned char pcm[8192];
    for (size_t at = 0; at < (size_t)n;) {
        size_t chunk = (size_t)n - at;
        if (chunk > 4096) chunk = 4096;
        if (!convert_pcm(samples + at, chunk, pcm)) { s->error = "invalid_speech_audio"; return 0; }
        s->error = s->sink(s->context, pcm, chunk * 2, s->rate);
        if (s->error) return 0;
        sha256_update(&s->hash, pcm, chunk * 2);
        s->count += chunk; at += chunk;
    }
    return 1;
}
const char *speech_runtime_generate(SpeechRuntime *r, const char *text, const char *voice,
    const char *reference, SpeechPcmSink sink, void *context, SpeechRuntimeResult *result) {
    if (!result) return "invalid_speech_request";
    memset(result, 0, sizeof(*result));
    size_t bytes = text ? strnlen(text, 2401) : 0;
    if (!r || !sink || !bytes || bytes > 2400 || !utf8_valid((const unsigned char *)text, bytes))
        return "invalid_speech_request";
    SamosaSherpaGenerationConfig c = {0};
    float *ref = NULL;
    if (r->pocket) {
        const char *e = reference_read(reference, &ref, &c.reference_audio_len, &c.reference_sample_rate);
        if (e) return e;
        c.reference_audio = ref; c.speed = 1;
        /* Pocket 1.13.4 scales silence only in the final buffer, after sending
         * raw progress chunks. Keep the gateway's streamed PCM authoritative
         * by disabling that final-only transform; receipts then match bytes. */
        c.silence_scale = 1;
        /* Preserve the gateway's five-step quality setting and stable seed. */
        c.num_steps = 5; c.extra = "{\"max_reference_audio_len\":10.0,\"seed\":42}";
    } else {
        const char *names[] = {"bella", "sarah", "nicole", "adam", "michael", "emma"};
        const int ids[] = {1, 3, 2, 5, 6, 7};
        c.sid = 1;
        if (voice) {
            size_t i = 0;
            while (i < 6 && strcmp(voice, names[i])) ++i;
            if (i == 6) return "invalid_speech_voice";
            c.sid = ids[i];
        }
        c.speed = 1.15f; c.silence_scale = .12f;
    }
    int rate = r->rate(r->tts);
    if (rate < 8000 || rate > 96000) { free(ref); return "invalid_speech_audio"; }
    size_t seconds = 8 + bytes / 8;
    if (seconds < 12) seconds = 12;
    if (seconds > 45) seconds = 45;
    PcmStream s = { .sink = sink, .context = context, .rate = rate, .maximum = seconds * (size_t)rate };
    sha256_init(&s.hash);
    const SamosaSherpaGeneratedAudio *audio = r->generate(r->tts, text, &c, pcm_progress, &s);
    if (!s.error && (!audio || !audio->samples || audio->n <= 0 || audio->sample_rate != rate))
        s.error = "invalid_speech_audio";
    if (!s.error && (size_t)audio->n > s.maximum) s.error = "speech_audio_limit";
    if (!s.error && !s.count) pcm_progress(audio->samples, audio->n, 1, &s);
    unsigned char digest[32], expected[32];
    if (!s.error) {
        sha256_final(&s.hash, digest);
        /* A successful terminal result must describe exactly the delivered
         * PCM, including callback runtimes which return a final audio buffer. */
        Sha256 final_hash; sha256_init(&final_hash);
        unsigned char pcm[8192];
        for (size_t at = 0; !s.error && at < (size_t)audio->n;) {
            size_t n = (size_t)audio->n - at;
            if (n > 4096) n = 4096;
            if (!convert_pcm(audio->samples + at, n, pcm)) s.error = "invalid_speech_audio";
            else sha256_update(&final_hash, pcm, n * 2);
            at += n;
        }
        sha256_final(&final_hash, expected);
        if (!s.error && (s.count != (size_t)audio->n || memcmp(digest, expected, 32)))
            s.error = "speech_stream_mismatch";
        if (!s.error) {
            result->sample_rate = rate; result->samples = s.count; result->callbacks = s.callbacks;
            sha256_hex(digest, result->pcm_sha256);
        }
    }
    if (audio) r->destroy_audio(audio);
    free(ref);
    return s.error;
}
