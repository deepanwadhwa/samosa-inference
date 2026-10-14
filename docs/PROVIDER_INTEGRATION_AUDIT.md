# Provider integration requirement audit

This audit follows the original area1 requirements, using the current independent
source and published qualification records. It does not close areas2–7 or replace
their acceptance criteria. Qualified provider code atc6eceb3 passed independent
normal/sanitizer CI on macOS14 ARM64 and Ubuntu24.04 x86_64 in
[run38491041553](https://github.com/deepanwadhwa/samosa-inference/actions/runs/38491041553).

| Requirement | Current authoritative implementation/evidence |
| --- | --- |
| One public interface for seven modalities | `registry_main.c`, `registry_host.c`, `registry.c`; `samosa-inference-providers --config` and `inference_registry_dispatch`. All seven adapter kinds coexist in the309-check combined gate, with native text and speech entries rather than compile-time precedence. |
| Discovery, operations, input limits and platform reporting | Registry discovery plus each existing adapter descriptor; host platform, declared target and explicit unspecified/compatible/incompatible policy. Unknown provider/operation and invalid configuration cases refuse. Availability remains configured/unqualified; compatibility does not imply model readiness. |
| Common owner, operation, cancellation and deadlines | `inference.c`, `operations.c`, `budget.c`, adapter admission/guardian integration. Namespaced same-ID work/replay, foreign-owner/provider refusal, cancellation, deadlines and recovery are qualified by registry/public-speech and native adapter gates. |
| Actual text and embedding integration | `REGISTRY_TEXT_VERIFICATION.md`: Ornith counting/generation, observed active registry cancellation, survivor and offline replay. `REGISTRY_EMBEDDING_TEMPORAL_VERIFICATION.md`: actual EmbeddingGemma768-dimensional vectors,4/4 rankings, context refusal and retained replay. |
| Actual summaries, transcription, image/video analysis | `INFERENCE_MODULE_VERIFICATION.md`: T5 scalar/map-reduce/provenance/replay. `REGISTRY_ACTUAL_MODALITIES_VERIFICATION.md`: actual Whisper/Sources WAV/MP3/66-second capture, SDK publication/readback and VisionPsy heading recognition. `REGISTRY_EMBEDDING_TEMPORAL_VERIFICATION.md`: actual Molmo2 ordered frame observation and bindings. |
| Native speech output and configuration | `speech_runtime.c`, complete package/loader/worker adapters; pinned Sherpa1.13.4, original Pocket references and all six Kokoro voice mappings. `KOKORO_VERIFICATION.md`, `SPEECH_PUBLIC_VERIFICATION.md` and `SPEECH_STREAM_VERIFICATION.md` contain actual-model results and variability limits. |
| Bounded PCM streaming and validated output | `--stream` and trusted PCM sink, stable provisional prefix hashes, final whole-packet/package checks, bounded event/suffix copies, cancellation/disconnect/backpressure recovery. `SPEECH_STREAM_VERIFICATION.md` and `speech-output-v1.md` record limits and transport failure semantics. |
| Provenance and existing receipt compatibility | `SPEECH_PROVENANCE_VERIFICATION.md`: verified fresh package/input/voice provenance, declared-only descriptors, no historical audio relabeling, actual previous-executable byte-identical receipt and stream replay. Other adapters retain their original provenance DTOs. |
| Preserve Sources transports and semantics | Existing native Sources argument-file transport now selects the registry; original model assertions, retained source/operation provenance and exact publication/readback/replay were preserved. No Sources validator or product adapter was relaxed. |

The interface and currently supported native model profiles have concrete
implementation/qualification evidence. Actual-model records are bounded local
macOS gates; controlled Linux builds/fixtures do not qualify Linux model execution.
No model accuracy beyond those recorded assertions is inferred. External model
engines/libraries are not covered by client sanitizer instrumentation.

The broad release is incomplete: full multi-file installation/compatibility,
persistent engine lifecycle/readiness, global memory/residency scheduling,
incremental text/session restart coverage, authenticated service and shutdown,
actual-model platform coverage, final independent engine/package builds and
legacy Chat retirement remain required under their original areas. Provider
integration remains active while that acceptance record is finalized; installation
work can proceed without claiming a complete provider or module release.
