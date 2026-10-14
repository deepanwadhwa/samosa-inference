#ifndef SAMOSA_INFERENCE_GGUF_METADATA_H
#define SAMOSA_INFERENCE_GGUF_METADATA_H
#include <stdint.h>
typedef struct {char architecture[65];uint32_t version;uint64_t tensors,pairs,metadata_bytes;} GgufMetadata;
/* Bounded GGUF v3 key/value inspection, not tensor/quantization validation or
 * model readiness. Held regular read-only descriptor; seek position preserved.
 * Output untouched on failure. The caller binds full file bytes to its pin. */
const char *gguf_metadata_read(int,const char *(*)(void *),void *,GgufMetadata *);
#endif
