#ifndef SAMOSA_INFERENCE_AUDIO_STORE_H
#define SAMOSA_INFERENCE_AUDIO_STORE_H
#include <stddef.h>
#include <stdint.h>
/* Host configuration only. Public callers cannot choose owner/provider/state. */
typedef struct {
    const char *state, *owner, *provider;
} AudioStoreConfig;
typedef struct {
    char id[65], pcm_sha256[65];
    uint64_t bytes, samples;
    int sample_rate;
} AudioReceipt;
#define AUDIO_RANGE_MAX 32768u
/* Publish a completed, verified worker packet atomically. The operation and
 * normalized request pin identify replay; a conflicting pin never overwrites.
 * Native packet descriptors remain caller-owned. */
const char *audio_store_publish(const AudioStoreConfig *, const char *operation,
    const char *request_sha256, int packet_fd, const char *(*control)(void *),
    void *, AudioReceipt *);
/* Validate the complete stored audio before returning a bounded PCM range.
 * Foreign handles are unknown; failures clear both receipt and range buffer. */
const char *audio_store_read(const AudioStoreConfig *, const char *id,
    uint64_t offset, size_t length, unsigned char *output,
    const char *(*control)(void *), void *, AudioReceipt *);
/* Owner/provider-scoped release. Saved operation receipts may then reference
 * unavailable audio; replay must not silently generate replacement bytes. */
const char *audio_store_delete(const AudioStoreConfig *, const char *id);
#endif
