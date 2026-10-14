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

## 2. Model acquisition and installation — active

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

Independent native platform checkpoint: CI run38454301972 passes every configured
normal/sanitizer controlled gate on macOS14/ARM64 and Ubuntu24.04/x86-64 at source
9df7ae0f3d29668812c9b9795c3e04b79900865a. Older SDK locale declarations and x86-64
publication/catalog/provider identity gaps were fixed with explicit foreign-host
refusal retained. This closes the controlled Linux registry build gap; it does
not close actual-model platform coverage, public speech or any full numbered area.

Owned speech worker checkpoint: the extracted native runtime now runs through
the existing process guardian with a bounded binary PCM result, package rechecks,
retained shutdown ownership and cancellation/requester-death cleanup. See
[worker evidence](SPEECH_WORKER_VERIFICATION.md). Public speech routing, immutable
package staging, persistent lifecycle/readiness and the remaining acceptance
gates stay open.

Public speech checkpoint: native speech generation and owner/provider-scoped PCM
range/release operations now share the public provider registry. Actual Pocket
range hashes and saved receipts pass normal/sanitizer public tests. See
[public speech evidence](SPEECH_PUBLIC_VERIFICATION.md). Authenticated service,
streaming, persistent lifecycle, global scheduling, immutable staging and all
remaining acceptance gates stay open; original Chat voice paths are retained.

Kokoro package checkpoint: complete existing eSpeak data is now accepted with
role-scoped literal native variant names and negative path tests. All six native
voices pass actual package/loader/PCM/cancellation/recovery checks. See
[Kokoro evidence](KOKORO_VERIFICATION.md); fresh PCM variability is documented
and exact per-request integrity/replay remains required. Other area1 and full
release gates remain open.

Actual Whisper/Vision registry checkpoint: existing quality/provenance harnesses
now route through a private combined registry configuration. Actual Vision heading
recognition, Sources-prepared Whisper audio, SDK publication/readback and66-second
windowed capture pass normal/sanitizer checks. See
[actual modality evidence](REGISTRY_ACTUAL_MODALITIES_VERIFICATION.md). Remaining
model/platform, lifecycle, service and release acceptance gates stay open.

Actual embedding/temporal/text checkpoint: the common registry now passes actual
EmbeddingGemma vectors/rankings, Molmo2 ordered observations and Ornith native
generation/cancellation/recovery in macOS normal/sanitizer clients. See
[embedding/temporal evidence](REGISTRY_EMBEDDING_TEMPORAL_VERIFICATION.md) and
[text evidence](REGISTRY_TEXT_VERIFICATION.md). Native speech streaming now passes
provisional delivery, backpressure, deadline, cancellation/disconnect and all
eight actual voices through the public interface; see
[stream evidence](SPEECH_STREAM_VERIFICATION.md). Independent CI at1c12ab6 passes
macOS14/ARM64 and Ubuntu24.04/x86-64. This establishes the listed checkpoints,
not completion of the remaining acceptance areas.

Platform policy checkpoint: trusted provider configuration can declare its
deployment target; public discovery reports native host/declaration/policy while
remaining explicitly unqualified for model availability/readiness. Incompatible
targets refuse fresh work but preserve owner-bound retained data and exact
receipts. All seven adapter kinds now coexist in the combined controlled routing
gate. See [platform policy evidence](REGISTRY_PLATFORM_POLICY_VERIFICATION.md).
Actual-model Linux coverage, production package qualification and remaining
provider/lifecycle/resource/service/release gates remain open.

Multi-file acquisition checkpoint: added a pinned native package index over the
existing hashing/catalog/platform primitives. It handles complete payload lists
without increasing the existing single-file install store's16-identity limit.
See [index evidence](PACKAGE_INDEX_VERIFICATION.md). Package acquisition,
publication, compatibility, controls and actual installation remain required;
area2 is active and no complete numbered area is closed by this foundation.

Closed package-tree checkpoint: a generic verifier now checks every indexed
payload and rejects missing/unlisted entries, links, unsafe modes and observed
mutations. It reuses artifact hashing and runs normal/sanitizer controlled gates
plus integrity observations over existing actual Pocket/Kokoro trees. See
[tree evidence](PACKAGE_TREE_VERIFICATION.md). Acquisition, coordinated quota,
private staging/publication, compatibility and installed lifecycle remain open.

Per-file staging checkpoint: verified cache descriptors can now be copied into
private indexed payload names with fixed buffers, complete hash checks,
no-replacement publication and controlled cleanup. See
[staging evidence](PACKAGE_STAGE_VERIFICATION.md). This is not complete package
installation; coordination, crash/resume ledger, backend compatibility, whole
package publication and actual installation still remain required.

Package completion checkpoint: a required trusted compatibility validator and
synced full payload checks now gate a no-replacement completion record. Retained
queries do not trust marker presence alone. See
[publication evidence](PACKAGE_PUBLICATION_VERIFICATION.md). The coordinator's
crash/resume ledger, reservations, acquisition/compatibility profiles, public
installation controls and actual installed-engine gates remain required.

Concrete compatibility checkpoint: the speech package recipe now reuses complete
native manifest and host/transitive loader inspection as a publication validator.
Unknown recipes refuse. Positive native-format fixtures and actual pinned
Pocket/Kokoro normal/sanitizer inspections pass. See
[compatibility evidence](PACKAGE_COMPATIBILITY_VERIFICATION.md). Other backend
recipes, complete acquisition/coordinator recovery and actual installation remain
required; no full numbered area is closed by this recipe checkpoint.


Package acquisition now has a connected private native assembly API: whole-source
verification, assembly lease, logical copy bound, verified-file resumption and
concrete speech completion publication. See
[contract](../contracts/package-assembly-v1.md) and
[evidence](PACKAGE_ASSEMBLY_VERIFICATION.md). Public catalog acquisition, shared
reservation and crash recovery remain open; this does not close area 2.


Acquired artifact-cache descriptors now feed native package assembly directly,
without an intermediate closed-tree copy. The bridge retains verified cache
identity, request controls and independent single-link installed payloads; normal
and sanitizer fixture/Pocket gates pass. Shared destination reservations, catalog
package acquisition and crash recovery remain required before area 2 closes.


The production cache adapter now optionally acquires missing indexed files from
trusted local source configuration, preserving store resume/capacity/byte policy
and verifying held descriptors before assembly. Controlled local acquisition and
offline cache checks pass. Per-file remote mapping, prompt control during the
legacy local installer's final hash, destination reservations and recovery remain
open; area 2 stays active.


The legacy artifact installer's final staged hash and verified offline retries
now poll installation cancellation between bounded reads. Targeted cancelled
hash/resumption/status checks and native package, remote artifact, activation and
CLI integration gates pass locally, including sanitizer variants. Per-file remote
package mapping, destination reservations and recovery remain open.


Optional native remote package-cache acquisition now maps each pinned file to
trusted per-file HTTPS configuration, preserving credential, redirect, resumed
range and controlled verification behavior. Normal/sanitizer mapping checks and
production TLS/credential gates pass locally. See
[contract](../contracts/package-remote-cache-v1.md) and
[evidence](PACKAGE_REMOTE_CACHE_VERIFICATION.md). Complete remote package
installation, reservations, coordinator recovery and public catalog integration
remain open before area 2 can close.
