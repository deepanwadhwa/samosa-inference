#ifndef SAMOSA_INFERENCE_SPEECH_SESSION_H
#define SAMOSA_INFERENCE_SPEECH_SESSION_H
#include "speech_runtime.h"
/* Private worker-side extraction of package verification and native loading.
 * Call only inside the existing guardian boundary. The session duplicates the
 * bound root fd and copies trusted host paths/pin. It provides neither process
 * ownership nor public readiness. One caller at a time; close after any failed
 * generation before considering the worker reusable. PCM stays provisional
 * until generation and the final package recheck both succeed. */
typedef struct SpeechSession SpeechSession;
const char *speech_session_open(int root_fd, const char *root_path,
    const char *manifest_sha256, const char *initial_voice,
    const char *(*control)(void *), void *, SpeechSession **);
const char *speech_session_generate(SpeechSession *, const char *text,
    const char *voice, SpeechPcmSink, void *,
    const char *(*control)(void *), void *, SpeechRuntimeResult *);
/* Successful worker completion rechecks after native destruction as before.
 * Consumes the session on every result; close is the failure cleanup path. */
const char *speech_session_finish(SpeechSession *);
void speech_session_close(SpeechSession *);
#endif
