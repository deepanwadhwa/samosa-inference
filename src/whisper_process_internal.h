#ifndef SAMOSA_WHISPER_PROCESS_INTERNAL_H
#define SAMOSA_WHISPER_PROCESS_INTERNAL_H
#include <samosa_inference/whisper_process.h>
#include <stddef.h>
#include <stdatomic.h>

typedef struct {
    int lease;
    double lifetime;
    const unsigned char *probe_audio;
    size_t probe_bytes;
    const atomic_int *stop_requested;
} WhisperProcessAdmission;

int whisper_process_run_admitted(const SamosaWhisperProcessConfig *,
    const SamosaWhisperProcessIdentity *, const SamosaWhisperProcessControl *,
    const char *wav_path, int timestamped, int preserve_cancel, int own_input,
    const WhisperProcessAdmission *, char **output);
#endif
