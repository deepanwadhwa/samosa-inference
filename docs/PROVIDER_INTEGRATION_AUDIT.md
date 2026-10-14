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
legacy Chat retirement remain required under their original areas. Those are
separate downstream acceptance requirements; they do not reopen the implemented
provider integration contract. Area 1 is complete as audited below. The module
release and areas 2–7 remain incomplete.


## Area 1 completion decision — 2026-10-14

Each original area-1 requirement has implementation and qualification evidence in
the table above: one discovery/dispatch surface, coexisting provider selection,
common owner/operation/cancel/deadline controls, bounded verified native speech,
preserved Sources transports/receipts, and supported-model/routing/input/provenance/
platform reporting gates. No downstream installation, persistent lifecycle,
global scheduling, token streaming, authenticated ingress or product retirement
requirement is being substituted for this contract or removed from its own area.

Current source comparison from `c6eceb3` to `b1df2a3` reports no changes in
`registry.c`, `registry_host.c`, `inference.c`, `speech.c`, `embedding.c`,
`whisper.c`, `vision.c`, `temporal.c`, `summary.c`, `artifact.c`,
`engine_process.c`, `operations.c` or `budget.c`. The actual-model records remain
applicable to those unchanged provider/control implementations: Ornith,
EmbeddingGemma, T5, Whisper, VisionPsy, Molmo2 and Pocket/Kokoro, including all
supported Kokoro voice mappings. The evidence records retain model/runtime hashes,
commands, bounded quality assertions and replay/provenance limits. Actual-model
coverage is macOS arm64; Linux fixtures do not become Linux model qualification.
That distinction remains explicit in capability reporting and final area-7 gates.

Current component checks were rerun:

- `make test-registry-modalities sanitize-registry-modalities`: 309 checks each.
- `make test-registry-cli sanitize-registry-cli`: 584 normal / 587 sanitizer checks.

Log: `/private/tmp/samosa-provider-foundation-closure.log`. These qualify the
unchanged public routing/control implementation together with the existing
actual-model evidence, rather than treating fixtures as actual models. Area 1
is closed; the next active component is model acquisition/installation, whose
full original criteria remain open. This supersedes historical statements that
area 1 remained active merely because downstream release work was unfinished.
