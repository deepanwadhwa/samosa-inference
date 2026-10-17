# General text distribution candidate

The seven-file candidate reuses the already-qualified static CPU llama.cpp engine
`958601dbd22ab2f5857e09860247f30c74153cb0f3914b8d62f2bacbba9aab97`
and retained Ornith1.0 9B Q4_K_M GGUF
`5720d1f671b4996481274fffe01868c3c36e87c135cc8538471cc7bd6087b106`.
The existing native GGUF reader reports version3, architecture `qwen35`,
427 tensors,37 keys and10942337 metadata bytes. No new parser, inference runtime,
model download or conversion is used for staging.

`tools/prepare_text_catalog.py` reuses the common release helper and existing
`llama-gguf-package-v1` text descriptor. The candidate index is
`f5a6faa589c9478f29cb5fe343be224152780fc0264a20a3ff5d9a8a0d3a7c22`,
with5645083556 payload bytes. Weights use the original App catalog's immutable
upstream URL/revision directly, rather than this release's assets. The original
source namespace redirects to `ornith-ai`; revision and content pins remain the
same. The pinned original model card explicitly declares MIT and is retained
unchanged. That pinned repository contains no separate LICENSE file despite the
card's link; no upstream copyright notice or license file is invented. The full
MIT native-runtime notice accompanies the package.

The shared HTTPS installer gate now invokes the existing `managed_native.sh`
against the freshly installed engine/model, with the existing registry and
offline adapters. It preserves the complete acquisition/refusal/recovery/removal
checks before actual model execution. Both normal and sanitizer candidate gates pass, including
native token count24, arithmetic answer4, observed active cancellation, survivor,
owner-scoped status, foreign-owner refusal, FIFO EOF shutdown, private credential
cleanup and exact runtime-free offline replay. The seven unique release assets
passed download-back hashes before publication. Cold published-URL qualification
is running; its production catalog entry remains pending. The underlying engine itself is
a release build.

Logs: `/private/tmp/samosa-text-distribution-v1-{normal,sanitize}.log`.
Qualification is bounded arithmetic/control behavior on macOS26.5.1 arm64, not a
general coding benchmark, incremental token streaming, common-registry owned
readiness, KV-session semantics, or another platform's model execution.
