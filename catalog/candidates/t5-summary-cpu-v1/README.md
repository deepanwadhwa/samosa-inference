# Native T5 summary distribution candidate

The existing independently built CPU engine and qualified Q8_0 model are reused
with the `llama-gguf-package-v1` summary/T5 host policy. This adds no model
conversion, inference implementation or installer. The model is Falconsai
`text_summarization` revision `6e505f907968c4a9360773ff57885cdc6dca4bfd`, converted
previously from original safetensors to GGUF Q8_0. Its retained bytes remain pinned
at `30c73e0b2ebd4f65538d87f5157aa7193261e8ff362e441df86c6941f3e9265c`.

The nine-file index is
`8632d0fb41f034e46925e586a83b10ce347d7d0cf5e8bc7ee641922afc760458`,
with69393904 payload bytes. Only metadata is stored here; engine/model binaries
are absent from the source repository. The build tool reads retained binaries in
place and prepares a content-addressed release asset list without copying weights.
The previously converted GGUF needs a distribution host; the proposed release
will upload it directly from its retained source file.

The pinned [upstream model card](https://huggingface.co/Falconsai/text_summarization/blob/6e505f907968c4a9360773ff57885cdc6dca4bfd/README.md)
declares Apache-2.0. Its complete original card, Apache model license, explicit
conversion notice, Samosa sidecar license and MIT Prism/ggml notice are included.
Every input is independently hash-checked by `tools/prepare_t5_catalog.py`, which
shares preparation with the existing Whisper tool. The pinned native engine
source and CPU build profile are recorded in provenance.

Execution qualification is macOS26.5.1 ARM64 only; the14.0 deployment metadata is
not an actual macOS14 execution claim. Installation remains separate from loaded
readiness. Listing/public requests cannot choose private paths, credentials or
commands. App provisioning must copy catalog/source configuration into its owned
private files; ordinary checkout permissions are not trusted native configuration.

The final nine-file package passes1347 actual summary checks in each of normal
and sanitizer-equipped hosts, after full per-file HTTPS setup, policy/TLS refusal,
publication, last-good preservation, offline status/resume, completed/incomplete
removal and cache-only reinstall. Logs are
`/private/tmp/samosa-t5-distribution-final-{normal,sanitize}.log`; exact host
executable and log pins are in `qualification.json`. The shared preparation
reproduces every published Whisper metadata byte unchanged, and the shared remote
gate again passes Whisper's real published install/transcription/cache/cleanup.
The proposed release URLs are not evidence of publication. Other distributions,
full area2 acceptance and final module release remain open.
