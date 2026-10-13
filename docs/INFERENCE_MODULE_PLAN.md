# Independent Inference module acceptance

Active goal, 2026-10-12. Sources has completed its separate seven-area release.
This checklist owns Inference completion; Sources closure does not close these
gates. Existing S02–S13 documents describe qualified subsets and remain evidence.
No area is closed by a fixture, a descriptor, or a file move alone.

## 1. Provider and modality integration — active

Reuse the existing native text, EmbeddingGemma2, T5 summary, Whisper,
VisionPsy and Molmo2 implementations and their public contracts. Audit native
Pocket/Kokoro speech output in the gateway before extracting its provider adapter.
Do not introduce a shipped Python runtime.

- One public provider discovery and dispatch surface exposes configured provider
  identities, operations, modality/input limits and platform availability.
- Multiple configured providers coexist without compile-time dispatch precedence
  hiding another adapter. Unknown providers/operations refuse explicitly.
- Text, embeddings, summaries, transcription, image/video analysis and speech
  output use common owner, operation, cancellation and deadline controls.
- Speech output preserves bounded native PCM generation and streaming, verified
  package identity, voice configuration and output validation.
- Preserve Sources public transports and existing receipts/replay semantics.
- Qualify actual supported models and combined-provider routing, malformed input,
  provenance, unavailable providers and platform-specific capability reporting.

## 2. Model acquisition and installation — pending

- Assemble trusted catalogs over the existing verified artifact, HTTPS download
  and setup primitives; pin full engine/model packages and provenance.
- Qualify credentials, redirects, quota, resumable interruption, corrupt partial
  state, concurrent installation, offline retry and last-good preservation.
- Enforce package architecture, minimum OS, loader/backend compatibility and
  declared native dependencies before activation; installation alone is not ready.
- Publish production catalog entries only after their actual package gates pass.

## 3. Engine lifecycle and readiness — pending

- Extend existing guardian/activation ownership to the supported provider set.
- Start, verify, load, unload and stop through public controls; readiness reflects
  current identity, health and a successful provider qualification request.
- Qualify crash recovery, failed replacement, engine/owner death, stale readiness,
  pinned package mutation and unrelated-process survival.
- Retain last-good configuration while refusing ambiguous interrupted generation.

## 4. Resource scheduling and admission — pending

- Coordinate memory reservations and residency across text and modality engines.
- Bound active work, queues, results and persistent state globally; expose typed
  admission failures and observed/reserved resources.
- Fairly schedule interactive/background work with bounded starvation; queued
  cancellation and deadlines release exactly their own reservations.
- Qualify concurrent mixed providers, residency transitions, memory refusal,
  process death and recovery without leaked leases or oversubscription.

## 5. Generation, streaming and sessions — pending

- Preserve native template/token counting and exact context/output reservations.
- Add real incremental generation events; the existing correlated managed
  request/response stream is not token streaming.
- Bound events/backpressure; qualify disconnect, partial output, cancellation,
  timeout, UTF-8 boundaries and exactly one terminal outcome.
- Preserve opaque owner-bound sessions, revisions and last successful state
  through restart, failures and concurrent access; declare semantic versus KV reuse.

## 6. Authenticated service, controls and shutdown — pending

- Provide bounded authenticated public ingress and trusted owner mapping over the
  common provider interface; public requests cannot choose private paths/commands.
- Preserve owner-scoped status, cancellation, retry identity and deadlines across
  modalities, queued work, streaming and engine transitions.
- Qualify authentication refusal, cross-owner access, malformed/oversized ingress,
  socket backpressure, background ownership and service/worker crashes.
- Stop admission, cancel/drain according to declared policy, reap engines and
  leave recoverable state during EOF, signals and explicit shutdown.

## 7. Final qualification and release — pending

- Maintain an exact acceptance record with commands, artifact/runtime hashes,
  platforms, results and explicit unsupported combinations.
- Run actual-model quality/control/recovery gates and macOS/Linux native normal
  and sanitizer gates; protocol fixtures do not qualify model availability.
- Build/test/release a clean archive outside this checkout using declared
  dependencies only, including the complete public interface and optional adapters.
- Create the independent repository and pin its revision in product integration.
- Retire replaced gateway inference routes, model installers, engine ownership
  and private tests after replacement integration qualifies; preserve App-owned
  policy and browser-local capabilities at their proper boundary.

## Execution and evidence

Complete area 1 before claiming its closure, then qualify each subsequent area.
Reuse existing implementations and tests before creating replacement machinery.
Record concrete failures and passes here or in the accompanying acceptance record;
historical checkpoint prose cannot substitute for current release qualification.
Keep unrelated working changes and the Sources release intact.

2026-10-12 provider integration checkpoint: the public registry now routes six
existing adapter kinds through one native executable/C API with private host
configuration and separate owner-bound provider namespaces. Core, linked modality,
native process/control, sanitizer and actual T5 map/reduce/replay gates pass on
macOS. See [current qualification](INFERENCE_MODULE_VERIFICATION.md). Speech output
and the remaining acceptance rows stay open; Docker Linux qualification is not
currently available. Do not close area1 from the registry subset.

2026-10-13: independent repository is established at
https://github.com/deepanwadhwa/samosa-inference.git and Samosa Chat pins it as a
submodule. The private speech runtime now reuses the existing native ABI and
PCM streaming implementation; see [speech evidence](SPEECH_RUNTIME_VERIFICATION.md).
Public speech provider/package/guardian/artifact gates and all remaining areas
stay open. Repository creation alone does not close area 7.

Speech package checkpoint: the private closed-tree verifier reuses existing
artifact hashing/identity primitives and passes controlled normal/sanitizer and
actual pinned-package Pocket generation/recheck gates. See
[package evidence](SPEECH_PACKAGE_VERIFICATION.md). This does not close public
speech routing, production acquisition, loader compatibility or engine ownership.

Speech loader checkpoint: actual macOS Pocket package passes native graph/host
compatibility and loaded ABI checks. Normal/sanitizer format and transitive graph
gates cover both Mach-O/ELF and ARM64/x86-64 under explicit private test policies.
See [loader evidence](SPEECH_LOADER_VERIFICATION.md). Guardian/staging, public
speech routing, actual Linux/Kokoro and final release gates remain open.
