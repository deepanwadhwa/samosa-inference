# Lifecycle implementation inventory

Checkpoint 2026-10-25: LIFE-07a is in progress. The existing SamosaMmSession
now has additive request cancellation/deadline control for both original protocols,
with interrupted streams invalidated and leases retained until existing close/reap.
See `qualification/life07a-specialist-control/README.md` for transport evidence.
Next: borrowed guardian endpoints/retryable close and workspace/duplex ownership;
then original visual preparation/verification and real generation readiness.
This does not close visual lifecycle, area3 or the full repo-split goal.

Area3 is active following area2 closure on2026-10-17. This inventory identifies
existing implementations to reuse; it is not lifecycle acceptance or a
replacement design for the existing code.

| Existing component | Proven responsibility to retain | Extension needed for component3 |
| --- | --- | --- |
| `engine_process.{c,h}` | Guardian owns its engine process group, bounded logs/output, lifetime, inherited model descriptors, residency lease and request workspace cleanup; caller death does not release residency before child reap | Reuse these ownership rules for provider lifecycle transitions and crash recovery |
| `activation.{c,h}` | Verified engine/model admission, exclusive residency lease, revisioned last-good activation, adapter qualification callback, stop/reap and passive committed-state inspection | Extend existing activation semantics to installed multi-file bundles and the supported provider set |
| `managed_provider.{c,h}` | Text/embedding startup, authenticated endpoint checks, identity/model binding, guardian liveness and retained artifact/state identity; stop clears qualification | Retain it as the owned text/embedding adapter; audit successful generation qualification as well as tokenization for text |
| `managed_main.c`, `managed_stream.c` | Existing private host config, owned provider lifetime, correlated request dispatch, cancellation and EOF shutdown | Reuse the owner loop/transport while exposing common lifecycle controls; request correlation alone is not token streaming |
| `managed_host.{c,h}` (LIFE-02 extraction) | Reusable existing config/key/start/close ownership with stable provider backing storage and explicit cancellation | Consume it from common registry lifecycle; keep signals and transport in the host application |
| `registry_host.{c,h}`, `registry.c` | Immutable trusted provider configuration and common dispatch; host loader explicitly does not install/start engines | Add lifecycle ownership around this registry without turning public request fields into commands, credentials or paths |
| Summary/Whisper/Vision/temporal/decision adapters | Existing guarded model execution and package/identity checks; each calls existing guardian APIs | Preserve inference implementations and extend residency/readiness transitions rather than writing replacement inference backends |
| `speech_worker`, `speech_runtime`, `speech.c` | Existing native library/model handling, bound package directory, guardian cleanup, provisional PCM streaming and final verified result | Reuse native worker/runtime model handling for lifecycle control and actual readiness |

Initial area3 audit (historical; later qualified slices are recorded below):

- `managed_provider_start` passes its own qualification callback into
  `activation_start` and installs `managed_provider_status` as `owned_status`.
- The qualifier checks endpoint properties/authentication and native tokenization;
  embedding additionally runs an actual embedding probe. Text generation probe
  coverage must be resolved against the component3 readiness requirement.
- `managed_provider_status` checks a live guardian channel, bound residency/state
  identity and unchanged retained engine/model identities. A saved activation is
  explicitly not a live-engine claim.
- The common registry host is immutable and has no equivalent all-provider
  start/load/unload/stop ownership API yet. One-shot modality workers do not by
  themselves establish persistent loaded residency.
- Existing source/actual-model tests already cover useful ownership and control
  behavior. Those tests must remain; full component3 acceptance also requires
  common public transitions, actual qualification, crash/owner death, mutation,
  failed replacement, stale readiness and unrelated-process survival.

Implementation must preserve the full requirement in
[the module plan](INFERENCE_MODULE_PLAN.md#3-engine-lifecycle-and-readiness--active).
Authenticated multi-owner ingress, global resource scheduling and generation/KV
session acceptance remain their own later requirements, not implicit outcomes of
this inventory.

First concrete task LIFE-01: extend existing `managed_provider.c:qualify` text
readiness using the existing `llama_generate` transport. Current text readiness
checks properties/authentication and tokenization, while embedding already runs
an actual vector request. Require a bounded successful text generation before
committing readiness; retain existing activation/guardian ownership and last-good
rollback. Inspect `tests/managed_cli.sh`, `tests/test_activation.c` and their
existing fixtures before extending tests. Prove tokenization-success/generation-
failure refuses readiness, a healthy provider still qualifies, cancellation and
deadline remain scoped, and failed replacement preserves last-good state.
Requalify actual installed text using the retained runtime/model pins after the
focused native normal/SAN tests. Then complete the rest of the original area3
checklist; this first task does not replace common provider lifecycle work.

LIFE-01 generation readiness/rollback and LIFE-02 managed-host extraction are
qualified. LIFE-03 text and LIFE-03E embedding registry-owned lifecycle slices
also qualify on both-platform native normal/SAN and actual macOS model gates.
LIFE-04a guardian duplex and LIFE-04b controlled summary transport are also
qualified. Current next task is LIFE-04c public pinned T5 activation/registry
lifecycle. Read REGISTRY_LIFECYCLE.md
and EXTRACTION_STATUS.md for exact evidence/scope. The earlier first-task audit
above is historical, not a direction to restart LIFE-01.

The extracted summary_process.c retains original scalar/batch/coalesced framing
and the default App launch path, and now supports external guardian endpoints/
lifecycle with controlled queue/mutex and nonblocking write/read I/O. Native
normal/SAN, original App/legacy tests and actual macOS pinned T5 corpus qualify
that transport integration. The existing guardian owns duplex pipes, model/
residency/group/parent death and survives orphan-group HUP with worker defaults
preserved. See REGISTRY_LIFECYCLE.md for terminal source-bound evidence.

Next reuse activation.c verified admission, revisioned last-good store and
qualification callback, extending its trusted launch boundary to duplex.
Connect a production pinned resident summary owner to summary.c's existing
normalization, opening/map-reduce, provenance and receipts, then registry
lifecycle. Require an actual warm request for public loaded readiness; a test
host or raw warm flag alone does not prove it. Preserve IDs/pins/controls and
uncertain coverage. Other providers, full area3 and all later gates remain open.


LIFE-04c public resident T5 ownership is locally qualified (2026-10-24).
Existing activation admission/revision/last-good store and guardian own the
worker; existing summary_process transport and summary.c normalization, opening
map/reduce, provenance and operation receipts remain authoritative. summary_host
is the lifecycle adapter, not a replacement backend. Shared owned-status checks
are extracted from managed_provider. The same native T5 engine parses a minimal
budget-v1 envelope so requests16/64/128/256 use one resident activation; legacy
frames and the generation algorithm remain intact. Actual bounded calibration
must succeed before activation commit; incompatible old engines fail closed.

Final macOS and independent Debian ARM64 Linux normal/SAN native lifecycle gates
pass11 scenarios each. Actual retained T5 macOS normal/SAN passes5 scenarios each,
including real16-versus256 output differences, unchanged activation revision,
opening reduction, exact stopped-worker/offline replay and owner isolation.
Native failure/cancellation/deadline/mutation/crash/owner-death gates preserve
last-good ownership and release only after confirmed reap. Existing activation,
managed readiness, text/embedding lifecycle, one-shot summary, controlled
transport and nine-provider registry regressions pass both platforms. Original
actual scalar/batch/coalesced corpus passes against the new engine normal/SAN.
Final lifecycle handles67823/68730 and regressions89750/56935/95323 are terminal0.
Actual-model scope is macOS; native C hosts are instrumented in SAN runs, external
model engine remains a release build. Initial build-overlap actual-model evidence
is excluded; final harness pins its CLI before/after execution.

Candidate engine SHA256:
179efc64fa559a4a5ebc5ece2cb2b70b54b0784e264af7d2c84083e0dae606a1
Model SHA256 remains:
30c73e0b2ebd4f65538d87f5157aa7193261e8ff362e441df86c6941f3e9265c
Evidence: docs/qualification/life04c-summary-registry (component-relative).
Published legacy catalog is unchanged. Local resident qualification does not
admit a new production distribution or close area3/areas4–7/C01–C14.

User reaffirmed the purpose: split existing code into separate individual repos.
For each remaining slice, inventory the monolith/component implementation, move
or wire that implementation and its tests, and add only necessary ownership/API
boundary changes. Preserve existing behavior and licenses. Package qualification
uses existing helpers and remains a separate supporting gate; do not let new
packaging work displace the extraction. Next: audit the existing speech worker/
runtime and other modality owners against the remaining area3 checklist, select
the next extraction boundary, and retain the new T5 distribution gate as open.


LIFE-05a speech session extraction qualified, 2026-10-25.
Direct inspection found the working implementation in speech_worker.c and the
previously extracted speech_runtime.c (original gateway native Pocket/Kokoro ABI,
voice mapping and PCM generation). Extracted the worker's existing package
verification, library-graph check, options/voice selection and native open/close
into private speech_session.c/h. The original worker now consumes this helper;
its request parser, guardian ownership, PCM packets, provisional streaming,
terminal publication and public speech adapter are unchanged. The helper retains
one loaded runtime across sequential requests and owns a duplicated bound root
fd plus copied host path/pin. Package checks precede/follow generation; successful
worker finish also rechecks after native destruction as the original worker did.
No second speech engine, installer, model or public lifecycle implementation.

Focused fixture checks run in a separate child process: repeated Caro/Stuart
requests after deliberately changing the native ABI probe prove no reopen;
closing the caller's fd preserves the session's bound root; early cancellation
and package mutation expose no successful result/PCM. Failed generations require
caller cleanup before worker reuse. This private helper alone does not establish
public residency, readiness or process ownership.

Final macOS normal/SAN worker354/358, independent Debian ARM64 Linux355/357;
public speech175/175 on both platforms. Poll-related worker counts vary.
Existing runtime324/324, loader2713/2713 and package compatibility/publication
checks pass both platforms. Final qualified handles69131/62358, regressions
12075/64376 and actual11876 are terminal0; no live qualification handles remain.
Earlier session-mac/final logs are development evidence, superseded by qualified
logs after retaining the post-destruction verification boundary.

Actual public Pocket --stream passes265 checks normal and265 sanitizer on macOS,
using an isolated copy of retained local artifacts, all checked against the
existing catalog manifest before copy. Manifest SHA256:
49a3fc30b37676239559a633d9860f129e6201f6fe3475cfe3a5a7f5bca27173
Package: /private/tmp/inference-life05-pocket-vlslz1c_
Caro78720/Stuart90240 samples retain the original exact PCM hashes, ranged reads,
streaming terminal verification and replay. C host/worker code is instrumented;
external Sherpa/ORT libraries remain release builds. This is existing public
one-shot execution via the extracted helper, not actual resident-worker proof.
No actual Linux or Kokoro claim is added. Model/library/config bytes unchanged.

Next LIFE-05b: reuse this session inside the existing guarded speech worker for
sequential requests, keeping the established bounded/provisional PCM packet
format and final hash checks. Reuse engine_process ownership/duplex controls and
existing package/root/workspace descriptors; preserve cancellation/deadline
invalidation and confirmed-reap lease retention. Qualify repeated native/actual
requests under one loaded worker before wiring identity-only public registry
start/status/stop and successful synthesis readiness. Reuse activation/store
semantics for last-good ownership and existing speech package identity; do not
replace speech.c/artifact stores, the native runtime, installer or gateway paths.
Inference area3 remains active, areas4–7/all C01–C14/Agent/App/pinned assembly/
legacy retirement remain open. The purpose remains splitting existing code into
independent repos, with only necessary ownership/API boundary changes.

Evidence: docs/qualification/life05a-speech-session.


LIFE-05b private resident speech worker qualified, 2026-10-25.
Reuses the existing speech session, native Pocket/Kokoro runtime, guarded duplex
engine_process ownership, request decoder, provisional/terminal PCM headers,
PCM packet validator and streaming hash checks. New hidden resident dispatch
reads bounded length-prefixed copies of the existing text/voice JSON requests.
The same native session serves sequential requests; each provisional cumulative
header precedes its newly emitted PCM suffix, and the final header adds no PCM.
The host reconstructs the original regular-file PCM packet and runs the existing
progress/result validators. Existing one-shot dispatch remains available.

The nonblocking pipe transfer/SIGPIPE handling is extracted from summary_process
into controlled_pipe.h and shared with speech, preserving summary errors and
control behavior. Resident host requests retain a60s ceiling plus caller control,
16KiB request/16MiB packet/45s audio bounds and bounded chunks. No public paths,
commands, voices or model rules were added. Guardian retains bound package and
residency and handles lifetime, stderr bounds and process-group/owner cleanup.
Active control/sink/protocol failure clears the result and stops/reaps the worker;
shutdown-pending keeps its process/endpoints for retry. Preflight cancellation
sends no request and leaves the worker available. A caller must serialize use.
Startup itself is not readiness; public activation qualification remains open.

Final native macOS worker493/495 and independent Debian ARM64 Linux493/495
normal/SAN pass (poll counts vary). Added tests prove one native load through
three requests, same guardian, original decoded hashes, final-only generation,
preflight versus active cancellation,200ms stalled-read deadline, malformed
native audio, crash/restart, changed package refusal, request EOF and SIGKILL
owner cleanup, released leases and unrelated-process survival.

Actual retained Pocket normal/SAN passes135 checks each on macOS: original
one-shot Caro/Stuart/cancellation plus both voices on one resident guardian,
provisional streaming, original PCM hashes, cancellation after two chunks with
worker invalidation/reap, and successful restarted Stuart generation. Caro78720
samples/22 chunks; Stuart90240/25. Existing package manifest SHA256 remains
49a3fc30b37676239559a633d9860f129e6201f6fe3475cfe3a5a7f5bca27173;
isolated retained package /private/tmp/inference-life05-pocket-vlslz1c_. Native C
host/worker are instrumented; external Sherpa/ORT libraries are release builds.
This adds no actual Linux or Kokoro model execution claim.

Existing normal/SAN summary owned transport passes macOS274/354 and Linux267/272;
original summary-process scalar/batch/reuse/crash/recovery tests and public
speech175/175 pass both platforms. App gateway build, original native summarizer
supervisor, isolated installer and3 package/PDFium tests pass. Existing stage
manifests now include the extracted controlled_pipe.h so owner source builds
remain self-contained. Model, generation, installer and activation algorithms
were reused; no legacy paths were retired or remote catalog assets changed.

Final native19996/52645, regression93114/49614, actual53142, App82266 and
installer70797 are terminal0; no live qualification handles remain. Development
compile errors in life05b-build.log/native-mac-r1.log are excluded; final gates
retain the strict compiler flags/assertions. Linux export bytes match final source
except a subsequent API ownership comment, also copied and recorded as final
bytes. Source/evidence hashes and compact terminal PASS logs are retained in
components/samosa-inference/docs/qualification/life05b-speech-resident.

Next LIFE-05c: integrate this owner into the existing speech.c/public registry
through a trusted lifecycle callback, reusing package verification, admission,
activation revision/last-good semantics and identity-only lifecycle controls.
Require actual bounded successful synthesis before ready, retain exact
owner/provider operation receipts and provisional audio publication semantics,
and qualify failed replacement, mutation/crash/owner death, cancellation/deadline,
stop/restart, exact replay and cleanup. Do not treat a library bundle as an
executable engine or replace its loader/installer; adapt the existing activation
launch/qualification boundary for this verified multi-file native package.
New T5 production distribution gate remains open. Inference area3 and areas4–7,
all C01–C14/Agent/App/pinned assembly/legacy retirement remain incomplete.
The full goal remains active: split existing implementation and tests into
independent repos, adding only necessary API/ownership boundary changes.


LIFE-05c public resident speech lifecycle qualified, 2026-10-25.
Existing speech.c now delegates execution to a trusted resident callback while
retaining normalization, original JSON request serialization, AudioStore
publication, provisional PCM callback semantics and durable operation receipts.
SpeechHost reuses existing activation admission/revision/last-good and guardian
ownership, speech package/loader verification and the LIFE-05b resident worker.
The transport borrows one authoritative activation EngineProcess record; no live
process descriptors are copied between owners. Warm request failure reaps through
that record while activation retains its lease/store fence until rollback.

Private speech entries optionally select resident-speech-v1 and declare separately
qualified worker_sha256/worker_size/manifest_size. The actual registry executable
and package manifest are admitted as pinned inputs; the closed speech bundle
retains its existing per-file/loader/model/reference verification. A trusted
model_argument lets the adapter record its actual package-directory argv while
model_path remains the manifest. The explicit pinned-host-adapter-v1 activation
policy requires engine==self plus a qualifying launch callback: it records the
already trusted application's runtime separately from external model-engine
system-runtime-v1. Unsafe loader environments still refuse. External engine
policy is unchanged; a test refuses applying this allowance to a different engine.
The first model-engine policy rejected host curl/sqlite/ASan dependencies; that
failed diagnostic is excluded, not used as acceptance or an external-engine waiver.

Discovery remains configuration-only. Public identity-only start/status/stop use
the existing provider lifetime gate, while operation controls stay concurrent.
Readiness requires complete bundle verification and actual bounded successful
synthesis through the existing PCM/hash validator before activation commit, then
live guardian/activation/artifact/package rechecks. Same healthy start is
idempotent. Failed qualification preserves the exact last-good tuple. Confirmed
cleanup releases owned state/manifest/package/residency; shutdown pending retains
ownership for retry. Existing one-shot entries remain available.

Final macOS and independent Debian ARM64 Linux normal/SAN public native gates
pass8 scenarios each: lazy discovery/strict controls, actual warm qualification,
both voices and original ranges/hash on one loaded worker/revision, exact
stopped-worker and missing-package replay, owner refusal, failed native ABI and
synthesis rollback, active cancellation/deadline with busy lifecycle exclusion,
package mutation and crash/restart, EOF/SIGKILL owner cleanup and unrelated survivor.
Actual macOS retained Pocket normal/SAN passes5 scenarios each, including
original Caro/Stuart PCM hashes on one worker, exact replay/offline retained reads,
observed-running operation cancellation and deadline refusal, explicit restart
and EOF cleanup. Public actual cancellation is not asserted to occur after PCM;
private actual PCM interruption/reap was separately qualified in LIFE-05b.
The harness polls the normal pre-registration unknown_operation race rather than
assuming a reserved control request runs after generation registration.

Actual manifest SHA256 is unchanged:
49a3fc30b37676239559a633d9860f129e6201f6fe3475cfe3a5a7f5bca27173
Package: /private/tmp/inference-life05-pocket-vlslz1c_. C hosts/workers are
instrumented in SAN; external Sherpa/ORT remain release builds. No actual Linux
or Kokoro model execution is added. Existing native activation final113/121
macOS and115/171 Linux, text260/260 (Linux261/261), embedding332/332, T5 lifecycle
11/11, public speech175/175 and nine-provider459/459 pass normal/SAN on both
platforms. Resident worker/old one-shot/PCM integrity/owner cleanup regressions
also pass. Variable activation/worker counts include polling and executable size.

Final native52168/23080, actual65811, text/embedding/T5 regressions55155/26548,
activation26802/61254 and final-default21959 are terminal0. No live gates remain.
Initial host-policy and actual polling failures are excluded in life05c native/
actual logs; no production deadlines/compiler flags/PCM assertions were relaxed.
Final source/export bytes match. Compact logs, source SHA256 and derived reports
are in components/samosa-inference/docs/qualification/life05c-speech-registry.
Reports preserve generation receipts; decoded audio-range hashes/lengths replace
large base64 ranges and original report locations remain recorded.

Next LIFE-05d: qualify the existing published Kokoro package/profile through this
same resident owner and original voice/PCM quality harness, without changing its
runtime, voices, loader or installer. Then audit/extract remaining transcription,
image/video and decision lifecycle ownership from the monolith/component before
closing area3. Persistent registry-session incremental PCM delivery remains an
explicit area5 integration gate; existing one-shot --stream remains qualified and
unchanged, while --session currently returns complete receipts. Do not mistake
correlated replies for incremental generation/events. New T5 production distribution
remains open. Inference area3/areas4–7, all C01–C14/Agent/App/pinned assembly/legacy
retirement are incomplete. Full goal stays active; continue splitting existing
code and tests into independent repos with necessary boundary changes only.


## Remaining engine ownership audit — 2026-10-25

Inspected the owner sources and original monolith implementations before
selecting the next extraction. The missing common lifecycle is in host adapters;
several engines already retain their model and serve repeated requests.

| Provider | Existing implementation to retain | Remaining boundary |
| --- | --- | --- |
| Text decisions | `engines/decision/main.cpp` loads NativeText once and invokes `serve_requests`; `transport.h` defines repeated bounded 1MiB BE frames. `tests/opendecision_worker_reference.py` already verifies ten actual cases on one worker. `src/decision_assets.c` pins/retains the four closed engine/runtime/model/tokenizer files. | `decision.c:run` currently passes one framed regular-file request and clean EOF, then decodes/validates a completed worker. Reuse asset ownership, guardian duplex, controlled_pipe and typed Rust result validation for persistent activation. This is next LIFE-06a. |
| Image decisions | The same worker loads NativeImage once and serves typed requests, with existing pixels/preprocessing and pinned SigLIP model. | Its initial image is selected by trusted launch argv and captured in the backend. Repeated different images need an explicit bounded input boundary around existing preprocessing, without replacing model inference or quietly reloading every image. Remains open after text lifecycle. |
| VisionPsy | `engines/visionpsy/samosa_visionpsy.cpp` already has a JSON-lines command loop and load/unload; existing App supervisor/vision bridge and image preparation are extracted. | Registry image adapter currently uses one-shot guarded execution. Connect the existing resident command protocol and native loaded-model state to activation/readiness, preserving prepared pixel identity. |
| Molmo2 video | `engines/molmo2/samosa_molmo2.cpp` already loops over framed commands with explicit hello/load/unload/generate behavior; the original App uses that retained model owner. | Reuse this loop, temporal input/package verification and prepared frames through the guardian. Do not replace the native model or expose engine-selected decoding paths. |
| Whisper transcription | `src/whisper.c` already verifies the pinned executable/model and owns a bounded canonical WAV/SRT workspace under the guardian. The existing `tools/build_whisper_runtime.sh` builds the pinned whisper.cpp source and CLI. | Current invocation is per audio input. Inspect/reuse upstream and original host lifetime APIs before choosing a loaded-worker boundary. No persistent loaded residency or public lifecycle is yet claimed. |

LIFE-05d separately qualifies Kokoro on the existing speech owner using the
unchanged published manifest/profile and original quality assertions. This audit
does not close any remaining provider or numbered area. Persistent registry PCM
event delivery remains area5; common resource scheduling/admission remains area4.


LIFE-05d Kokoro on existing public speech owner qualified, 2026-10-25.
No production code/model/runtime/installer changed. The existing lifecycle
harness now accepts the original manifest-bound voice quality profile and runs
all6 Kokoro voices through the same loaded worker and activation revision.
The original public real-model harness's fresh-generation assertions were
extracted verbatim into tests/speech_quality.py and reused by both harnesses.
The published profile's1% sample-count tolerance is unchanged; actual generated
PCM is validated against its own exact hash/ranges, and retained replay is exact.
Reference hashes for variable fresh PCM remain diagnostics, as before.

Retained local runtime/model/voice/eSpeak/notice files were matched against every
published catalog manifest entry before copying into an isolated read-only QA
package.375 files including manifest; no conversion, new download or catalog
publication. Package /private/tmp/inference-life05d-kokoro-u75dyiqk;
manifest SHA256:
ebaa30beadfcc040df55621155c8cb56e839ca7ee9ce54f0ce929d7d08afbe76
Original quality profile and all model/library/data pins are unchanged.

Final actual macOS ARM64 normal/SAN public resident gates pass5 scenarios each:
synthesis-qualified start/idempotent readiness, all6 voices with original bounds
and exact range/hash verification on one worker/revision, exact stopped-worker/
missing-package replay and owner refusal, observed-running cancellation/deadline
refusal with explicit reactivation, EOF cleanup. Final profile and executable
pins are checked before/after execution. Earlier local actual passes remain
corroborating but final reports include the explicit profile-pin fence.

The original one-shot public --stream gate also passes583 checks normal and583
SAN across all6 voices, preserving byte-exact provisional stream versus retained
ranges and exact replay. Existing Pocket snapshot/isolation regressions pass265
checks per mode with original Caro/Stuart PCM hashes. The release QA snapshot
helper now includes the shared speech_quality.py with its own recorded hash;
an isolated renamed harness plus pinned helper executes actual Pocket without
repository imports. This is QA tooling reuse, not a new installer or another
published-URL qualification claim.

Native public speech lifecycle remains8/8 scenarios normal/SAN on macOS and
independent Debian ARM64 Linux. Only QA profile/pin/report handling changed after
those native gates; no production bytes changed. Final source/export bytes match.
Actual model scope remains macOS: C host/worker instrumented in SAN, external
Sherpa/ORT release builds. No Linux model execution, intelligibility/human quality
score, full area3 closure or persistent-session PCM-event completion is claimed.
Final actual14985, legacy/original52848, native30757/65616 and snapshot99157 are
terminal0; no live qualification handles remain. Compact logs, derived reports,
voice metrics, source/export/profile pins and original report locations are in
components/samosa-inference/docs/qualification/life05d-kokoro-resident.

Remaining engine audit found existing persistent NativeText decision framing,
VisionPsy load/unload JSON lines and Molmo2 framed command loops. Preserve these
implementations; missing ownership is in registry adapters. Image decisions bind
pixels at startup, and Whisper still invokes its CLI per canonical audio input;
their boundaries need explicit inspection before claiming resident reuse.

Next LIFE-06a: integrate text decisions around its existing persistent
engines/decision/main.cpp serve_requests loop. Reuse DecisionAssets open/recheck/
close and closed native loader graph, existing1MiB BE framing, guardian duplex,
controlled_pipe I/O, typed Rust result validation/normalization, operation
receipts and last-good activation. Require a bounded successful native decision
from the retained reference corpus before readiness. Do not apply the trusted
host dependency allowance to an external ORT engine, replace the engine/model,
or reimplement typed decisions. Preserve the existing image path while its
changing-image resident boundary remains open. Then continue the remaining
modality lifecycle audit/extraction against the full area3 checklist.
Inference area3/areas4–7, new T5 production distribution, all C01–C14/Agent/App/
pinned assembly/legacy retirement remain open. Full goal stays active. The
purpose remains separating existing code/tests into independent repos, with only
necessary ownership and API boundary changes.


### LIFE-06a — public persistent text decisions, 2026-10-25

Qualified: existing persistent decision engine/typed algorithms, DecisionAssets
loader verification and guardian transport now back registry activation/readiness.
The public adapter preserves normalization, result validation/provenance and
receipts. Last-good stores all four pins/profile as a recoverable hash-bound
bundle snapshot, verified before launch. Native macOS/Linux normal/SAN8/8
scenarios and actual macOS5/5 plus all ten original reference cases pass; maximum
error0.000583839146 within unchanged0.005. Existing provider/image/transport
regressions pass. No model/engine implementation was replaced or published.

Authoritative commands, scope, diagnostics, hashes, terminal handles and actual
asset/reference paths: [LIFE-06a evidence](qualification/life06a-decision-registry/README.md).
No live gates remain. Next LIFE-06b: reuse NativeImage/image_pixels/image_backend
for changing prepared images on one loaded context, preserving existing limits
and legacy image behavior; then registry lifecycle/actual SigLIP qualification.
Remaining image/VisionPsy/Molmo2/Whisper lifecycle, areas4–7, new T5 distribution,
persistent session PCM events and all assembled milestones remain open.


### LIFE-06b — changing prepared image decisions, 2026-10-25

The existing NativeImage/preprocessing/typed executor now serves changing frames
on one loaded context through a bounded explicit envelope. Same DecisionHost,
asset verifier, activation snapshot and registry controls; public DTO/provenance/
receipts and original image/text modes are preserved. Independent image/typed1MiB
limits and512-axis bounds remain. Native macOS/Linux normal/SAN codec21/21 and
image/text lifecycle8/8 pass. Actual macOS image20-case and text10-case original
corpora pass resident and legacy modes normal/SAN, within unchanged0.005 bounds.
The old image engine fails new-mode readiness without state/lease leakage.

Evidence/commands/provenance/terminal handles: [LIFE-06b evidence](qualification/life06b-image-registry/README.md).
New engine3054cf6a1beead8b75afa911ac6467a9c82e0b8f6bc8908931c0b2a3069c9458
is a local candidate; production distribution admission remains open alongside
T5. No model/runtime algorithms were replaced or legacy paths retired.

Next LIFE-07a reuses specialist_supervisor.c/SamosaMmSession and existing
VisionPsy/Molmo2 load/unload loops, with original verification, P6/white-alpha
preparation, reply/usage checks and guardian workspace/Metal-cache behavior.
Require actual native generation before public readiness. VisionPsy/Molmo2/
Whisper lifecycle, full area3/areas4–7, events, Agent/App/assembly/retirement remain
open; the full repo-split goal stays active. No live qualification handles remain.
