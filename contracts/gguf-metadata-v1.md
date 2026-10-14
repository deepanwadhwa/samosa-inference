# Bounded GGUF metadata inspection

`gguf_metadata_read` reads GGUF v3 key/value framing from a held regular read-only
descriptor. It reports the exact `general.architecture`, version, declared tensor
and key counts, and consumed metadata bytes. It preserves descriptor seek position
and leaves output untouched on failure. The caller verifies the complete model pin.
No tensor table, quantization layout, tensor values or model readiness is qualified.

The implementation follows the GGUF framing/type definitions in the already pinned
llama runtime's `ggml/include/gguf.h` and `ggml/src/gguf.cpp`. It is native C with no
GGML/Python dependency. It scans the complete key/value section, rejects duplicate
keys and requires exactly one string architecture identifier. Scalar widths,
strings and non-nested typed arrays follow the source format. Invalid boolean
values, malformed types, truncation, integer-overflow sizes and embedded NUL keys
or architecture strings refuse.

Metadata is limited to 64MiB, 8,192 keys of at most 255 bytes, architecture at most
64 bytes, and 1,048,576 array elements/declared tensors. Storage is one 64KiB read
buffer and at most 2MiB of key names. Token/value payloads are skipped without
allocating their lengths. Controls apply at entry, buffer reads, bounded metadata
progress and completion. Before/after inode, size, mode and timestamp observations
detect observed changes; this is not a future immutability guarantee.

Normal and ASan/UBSan tests cover complete metadata, architecture after arrays,
empty token strings, seek preservation, writable-descriptor refusal, cancellation,
every truncated fixture prefix, invalid types/booleans, duplicate architecture and
ordinary keys, array/key limits, oversized sparse metadata and overflowing lengths.
Log: `/private/tmp/samosa-gguf-metadata.log`.

Actual original models also pass normal and ASan/UBSan metadata inspection:

| Model identity | Architecture | Tensors | Keys | Metadata bytes |
| --- | --- | ---: | ---: | ---: |
| Ornith text GGUF | `qwen35` | 427 | 37 | 10,942,337 |
| EmbeddingGemma GGUF | `gemma-embedding2` | 413 | 46 | 15,766,055 |
| T5 summary GGUF | `t5` | 131 | 36 | 1,019,804 |

These observations supply the model side of the upcoming pinned backend descriptor
check. They do not admit an installation recipe or close component 2.
