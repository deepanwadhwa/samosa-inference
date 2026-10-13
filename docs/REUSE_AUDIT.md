# Inference reuse audit

2026-10-12. Start with the working implementation, extract its provider mechanics,
and qualify the new boundary. Do not rewrite engines to achieve independence.
Paths below are migration references, not permitted sibling build dependencies.

| Area | Existing implementation to reuse | Remaining boundary work |
| --- | --- | --- |
| Provider integration | Inference `inference.c`, `llama.c`, `embedding.c`, `summary.c`, `whisper.c`, `vision.c`, `temporal.c`; parent `samosa_summarizer.cpp`, `visionpsy/`, `molmo2/`, `maple/`, `qwen36b.c` | Common provider registry/dispatch; explicit supported-engine inventory; speech adapter; independently packaged native engines |
| Acquisition | Inference `artifact.c`, `install.c`, `remote_artifact.c`, catalog/setup; parent `assets/models.json`, gateway catalog/install handlers, voice runtime scripts | Translate proven artifact entries into qualified package manifests; retain existing acquisition primitives and recovery |
| Lifecycle | Inference guardian, activation, managed engine/runtime; gateway `backend_probe`, `backend_start`, `backend_stop`, VisionPsy/Molmo2 sessions, native summary supervisor | Extend existing ownership to all supported engines and verify current readiness |
| Scheduling | Inference admission and activation residency lease; gateway native summary queue and runtime context/thread policy | Shared cross-provider memory/residency admission and interactive/background fairness |
| Generation/sessions | Inference exact native tokenization/generation, sessions, operations, managed stream; gateway backend streaming and native engine generation loops | Extract incremental events; preserve context/session behavior with scoped controls |
| Service/shutdown | Inference managed transport and budget/operations; Sources qualified native service; gateway `samosa_http.h`, UI/LAN authentication, shutdown/install-stop paths | Reuse transport patterns with Inference owner/request policy; isolate controls and qualify shutdown |
| Release | Existing Inference standalone Makefile/contracts/tests/release, Sources independent archive/repository integration | Complete engine dependency packaging, actual-model/platform acceptance and pinned replacement integration before retirement |

## First extraction: existing native speech output

The gateway already implements both Pocket and Kokoro through Sherpa-ONNX's
native public C ABI. `src/samosa_kokoro.h` declares the pinned v1.13.4 layout.
`kokoro_native_start_locked` resolves the five required symbols, constructs
Pocket/Kokoro configurations and initializes the selected model. Reuse that ABI
and loader/configuration behavior rather than introducing another speech engine.

`neural_generation_config_locked` preserves Kokoro voice IDs, speed1.15 and
silence0.12; Pocket loads the selected reference voice, uses five flow steps and
seed42. `kokoro_synthesize` validates rate/count and emits bounded mono PCM16
WAV. `kokoro_pcm_progress` and `voice_tts_stream_handler` already emit incremental
PCM and handle engines returning final audio without callbacks. Preserve their
sample bounds and avoid duplicated final audio when callbacks produced output.

Move these mechanics behind trusted package/voice configuration and the existing
Inference guardian, operation and admission controls. Gateway UI voice selection,
HTTP response headers and tracing remain product concerns. Audit non-finite
samples, native package dependencies and callback cancellation during extraction;
existing in-process mutex ownership alone does not prove the new service controls.

Reuse `tests/fake_kokoro_native.c`, `tests/test_kokoro_native_gateway.sh` and
`tests/test_voice_gateway.sh` for behavioral cases. Port provider checks to the
independent contract without including the gateway's private implementation.
`tools/samosa_kokoro_runtime.sh` contains the current Pocket runtime/model URLs
and installation layout; verified/resumable installation must use the existing
Inference acquisition layer rather than copying its shell downloader unchanged.

## Current observations

- `make -C components/samosa-inference test` passes on the current macOS host:
  core contracts, sessions, admission, operations, artifacts/install, guardian,
  activation/runtime, managed transport and catalog fixtures. This run does not
  qualify optional models, Linux, sanitizer builds or the seven-area release.
- The current config has one provider identity. Modality adapters are selected
  in compile-time order and several return directly whenever configured. Audit
  and test this before advertising simultaneous provider support.
- Managed stream qualification is concurrent correlated whole-response transport;
  it does not establish incremental token generation.
- Existing parent code is substantial. An unimplemented common boundary is not
  evidence that the corresponding engine or behavior needs to be rebuilt.
