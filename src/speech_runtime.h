#ifndef SAMOSA_INFERENCE_SPEECH_RUNTIME_H
#define SAMOSA_INFERENCE_SPEECH_RUNTIME_H
#include <stddef.h>
#include <stdint.h>
/* Private native primitive, called by one worker at a time. The host must
 * verify the complete runtime/model package before opening it and isolate the
 * worker. Paths and callbacks are trusted host inputs, never public requests.
 * This API alone does not provide package provenance, ownership or deadlines. */
typedef struct SpeechRuntime SpeechRuntime;
typedef struct {
    int pocket, threads;
    const char *library;
    const char *lm_flow, *lm_main, *encoder, *decoder, *text_conditioner;
    const char *vocab, *token_scores;
    const char *model, *voices, *tokens, *data_dir;
} SpeechRuntimeOptions;
typedef const char *(*SpeechPcmSink)(void *context, const unsigned char *pcm,
                                   size_t bytes, int sample_rate);
typedef struct {
    int sample_rate;
    size_t samples, callbacks;
    char pcm_sha256[65];
} SpeechRuntimeResult;
const char *speech_runtime_open(const SpeechRuntimeOptions *, SpeechRuntime **);
void speech_runtime_close(SpeechRuntime *);
/* Pocket uses the selected host-owned PCM16 WAV reference. Kokoro accepts
 * bella/sarah/nicole/adam/michael/emma. Sink failure stops generation and its
 * error is returned unchanged; partial bytes never imply a completed result. */
const char *speech_runtime_generate(SpeechRuntime *, const char *text,
    const char *voice, const char *reference_wav, SpeechPcmSink, void *,
    SpeechRuntimeResult *);
#endif
