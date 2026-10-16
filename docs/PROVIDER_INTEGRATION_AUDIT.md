# Provider integration requirement audit

## Current expanded inventory — 2026-10-16

The original seven-modality completion decision below predates the required
OpenDecision text/image inventory. The expanded area1 audit is now complete:
all local and hosted final checks passed. Public text decisions are
qualified in [the text record](OPENDECISION_PUBLIC_INTEGRATION.md). Native image
decisions now reuse the same registry/operation/guardian path; all twenty pinned
actual-model image cases pass through the public adapter normally and under
ASan/UBSan on macOS ARM64. See [image evidence](OPENDECISION_IMAGE_INTEGRATION.md).
Text/image coexistence, image limits and common controls pass on macOS and Linux;
Linux model quality and downstream areas remain outside those fixture checks.
The historical foundation audit below remains evidence, not a completion claim
for the newly expanded inventory; the decision below supersedes it.

| Expanded requirement | Current evidence | Gate outcome / retained scope |
| --- | --- | --- |
| Preserve existing provider/model implementations | Original seven adapters and qualified model records below; OpenDecision reuses native ORT, the locked tokenizer, typed decoder/result types and common provider controls | Passed final independent and hosted source checks |
| One discovery/dispatch interface with coexisting selection | All nine provider kinds coexist in one registry; normal/SAN registry gates pass459 checks on macOS and Linux | Passed final hosted checks |
| Actual OpenDecision text behavior | Ten pinned public actual-model cases, including8x512 relations, still pass after image integration; maximum typed error0.000583839146 | Broader platform/release coverage remains area7 |
| Actual OpenDecision image behavior and limits | Original eleven cases plus nine extended cases pass public normal/SAN adapters with unchanged executable pins;128-candidate Choice/Score, all relation categories, fallback/ties, Unicode and extreme frame dimensions; maximum typed error0.0000595896272 | Passed final hosted checks; broader platform/release coverage remains area7 |
| Exact native processor/tokenizer reuse | Twenty-case private native pipeline passes normal/SAN; every expanded processor tensor matches exactly in the sanitizer gate; all51 native tokenizer matrices match on both platforms; twelve model-free pixel tensors pass normal/SAN on both | Passed final source qualification |
| Common owner, retry, cancellation, deadlines and cleanup | Public text/image control gates exercise NUL/order, retained offline replay, provider/owner isolation, malformed results, worker death, mutation, full-frame bounds, pixel/dimension conflicts and requester-death raw-file cleanup on both hosts | Service-wide and global runtime requirements remain areas3–6 |
| Preserve original transports, receipts and modality behavior | Sources public transports/validators are unchanged. Original modality evidence below remains applicable; minimal registry, native registry CLI, VisionPsy and guardian normal/SAN regressions pass on both hosts | Passed full shared script at final source revision |
| Accurate availability and platform claims | Registry still declares configured/unqualified availability. Actual image/text model execution is qualified on macOS ARM64; Linux fixture/tokenizer/build checks are explicitly scoped | Actual supported-platform release matrix remains area7 |

Runtime implementation is at `5f84d7a`; expanded qualification source is
`46612a4`. The complete `tools/check_native.sh` passed from exported source on
macOS ARM64 and independent Ubuntu ARM64; logs are
`/private/tmp/samosa-image-final-independent-macos.log` and
`/private/tmp/samosa-image-final-independent-linux.log`. The source archive SHA256
is `dfd590c3112a2ced7e5b1898c7a95984733401203777f952aa5c93a713504250`.
Fresh hosted macOS/Linux checks also completed successfully for exactly
`46612a4e492dc14701255a039c3c5f6d2b48d6e9` in
[run38973353427](https://github.com/deepanwadhwa/samosa-inference/actions/runs/38973353427).
The public implementation's prior hosted run38972535169 also passed.

**Expanded area1 completion decision, 2026-10-16:** every provider integration
requirement in the original foundation and expanded table has direct evidence.
Area1 is complete; area2 is active. This is1/7 Inference areas complete. Actual
model/platform release breadth, production catalogs, persistent readiness,
global scheduling, sessions/streaming, authenticated service, pinned App assembly
and legacy retirement retain all original area2–7 requirements. The full goal
remains incomplete.
The next installation task is mapped to existing code in
[OpenDecision package follow-through](OPENDECISION_PACKAGE_INTEGRATION.md);
it preserves the original remaining area2–7 scope.

## Original modality foundation

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
