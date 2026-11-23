# Existing-code extraction

Current user constraints, 2026-11-20: extract/reuse the working monolith code;
no Python anywhere in project runtime/tooling/tests/CI; qualify natively on this
Mac first. Do not start Docker/VMs. Linux verification is deferred and does not
block current work. See Chat's docs/ARCHITECTURE_REBUILD_PRIORITIES.md.

LIFE-08g, 2026-11-22. A native regression reproduced an overlapping guardian
launch keeping another operation's control pipe open. On macOS each guardian
now inherits only the descriptors mapped for that launch. A second regression
reproduced a worker starting after the owner had closed its control pipe.
The guardian now checks the pipe before admission and cleans the retained
workspace when the owner has already gone. Native and sanitizer runs pass
592 engine-process checks.

Recovery after guardian loss with an already-started worker and cleanup
before workspace handoff remain open. A standalone Mac reproduction stopped
Whisper's owner in its cancellation callback immediately after it opened its
new result directory. Killing the owner at that point left the empty result
directory behind and preserved the borrowed WAV. The runner had yet to
publish a worker PID.

Previous LIFE-08f, 2026-11-22. The Inference visual lifecycle qualified
stock VisionPsy and Molmo. The current Mac release uses VisionPsy as its
visual specialist. Activation verifies and probes the
original workers before admitting requests. Retained receipts allow a
lower-resolution retry to reuse verification while model inputs keep their
verified identity. Shared recovery after guardian loss or interruption
before admission remains open.
[Visual lifecycle and release inputs](DEFAULT_VISUAL_LIFECYCLE.md).

Previous LIFE-07s, 2026-11-22. Gateway startup, selection, rollback and visual reclaim
use the Inference primary owner for pinned deployments. Native Qwen passes
Gateway startup and the separate visual reservation reclaim test on this Mac.
Resume rechecks retained identity and qualifies the fresh worker at the same
revision. Visual startup refuses pending primary cleanup. Ordinary installs
still need pins from trusted installation metadata before default cutover.
Actual Maple and llama, end-to-end actual visual reclaim, long-window Whisper
and recovery remain open.
[Native evidence and deployment contract](qualification/life07s-primary-reclaim/README.md).

LIFE-07r, 2026-11-22. The Inference primary C adapter now uses the original
backend owner, JSON transport, loader inspector and Activation transaction to
qualify a pinned engine and model before reporting readiness. The retained
native Qwen model passed on this Mac. Maple and llama passed protocol fixtures.
Next connect the adapter to Gateway selection, rollback and visual pause/resume.
Reuse verified inputs on an intentional resume. Actual Maple and llama,
long-window Whisper, default visual controls and crash recovery remain open.
[Native evidence and remaining scope](qualification/life07r-primary-readiness/README.md).

LIFE-07q checkpoint, 2026-11-22: the original primary owner now exposes typed
start/physical-status/stop and can use Activation's one canonical process record.
A real native reproduction found lost-guardian cleanup releasing the marker fence
while a resistant descendant remained alive. The owner now retains its host lease
and record on pending/unknown cleanup, withdraws readiness and refuses replacement.
Native/independent normal/SAN163 checks exercise the Activation transaction,
failed-qualification last-good preservation, same process identity and pending/
unknown ownership. All original primary676 checks pass in normal/SAN; Gateway
stop/SIGKILL, original budget/selection, exact package/native install/upgrade/boot
and retained Qwen compatibility pass. Evidence and limits:
[boundary, commands and provenance](qualification/life07q-primary-lifecycle/README.md).
This does not establish full primary capability readiness or general recovery
after terminating an unknown owner. Next connect these proven owners with the
existing JSON transport and Activation verification for original model bundles,
native dependencies and actual successful generation; preserve caller selection/
settings/rollback. Long-window/default visual controls, generic unknown-group/
pre-admission recovery, area3, later Inference areas, Python removal, Agent/App,
pinned assembly and qualified retirement remain required. Sources stays closed;
Inkling stays deferred until the entire refactor finishes.

LIFE-07p extracted the original primary JSON transport; see
[its evidence](qualification/life07p-backend-json/README.md).
LIFE-07o qualified the separate structured on-demand Whisper lifecycle; see
[its evidence](qualification/life07o-whisper-lifecycle/README.md).
It does not replace the original long-window txt/SRT contract.

Historical LIFE-07n checkpoint, 2026-11-21: the original shared text/visual generation gate,
visual selection/reclaim/pause/resume/60-second wait and lower-side/owned-page
retry now belong to Inference turn_control.c/public API. Gateway retains trusted
host/selection/evidence adapters. A reproduced direct-visual close reset race is
fixed by withdrawing only its own generating contribution. Native normal/SAN,
fresh31-file standalone builds, original visual/voice/selection/LAN/App workflows,
all15 ported prompt-budget scenarios, exact packaging/native install and retained
actual-model parity qualify the recorded scope. Original2048 heading quality
remains unqualified; no model algorithm was changed. The stale raw-PID budget
fixture is repaired through backend_process, preserving its domain assertions;
its Python driver/caller are retired for Node. Evidence and limits: Inference
qualification/life07n-turn-control/README.md. No Python/model fetch/Docker/VM/Linux
work. Common controls/Activation/observed identity/capability readiness, stale
rejection/last-good, guardian-death/pre-admission recovery and full area3 remain
open. This preserves exclusion; global reservations/fairness remain area4.
Later Inference areas, Python removal, Agent/App, pinned assembly and qualified
retirement remain. Sources stays closed; Inkling stays after the full refactor.

Historical LIFE-07m checkpoint, 2026-11-21: default VisionPsy/Molmo launch ownership now
reuses the existing EngineProcess guardian. Original model/protocol implementations,
VisionPsy512–2048/64-KiB/120-second and Molmo image/joint/native-video1-MiB/
600-second profiles, inherited diagnostics and grace/retry behavior are preserved.
Actual PID/group remains reserved until cleanup acknowledgement; resistant helpers,
parent death and pending shutdown are qualified. Explicit handoff owns only prepared
PDF pages; durable image/video/PDF/model files stay borrowed. Existing lower-side
retry gives the page back before unload and retakes it after restart. One reproduced
Mac write-pipe HUP timeout is fixed without changing production deadlines.
Final native/SAN276 and independent source276 pass, original supervisor/launch/
protocol assertions remain, and existing guardian525/primary676/Whisper522 pass.
Both actual compiled Gateway PDF SIGKILL checks clean processes and prepared pages,
while preserving durable inputs and an unrelated peer. Original VisionPsy/Molmo
workflows and exact package/native install/upgrade pass. Actual-model parity and
compatibility results are in Inference qualification/life07m-visual-guardian/README.md.
Original VisionPsy2048 heading quality fails this fixture and stays explicitly
unqualified;1536 heading recognition passes. Do not change model algorithms for
an ownership extraction. No Python, model fetch/conversion, Docker/VM or Linux work.
Next complete common controls/activation/observed identity/capability readiness,
stale rejection/last-good and guardian-death policy, then move the existing shared
turn gate/pause/resume/retry coordination into its owner. Pre-admission crash
recovery stays open. Area3/full refactor remain incomplete. Later Inference areas,
Python removal, Agent/App, pinned assembly and qualified retirement remain;
Sources stays closed absent a demonstrated defect. Inkling stays after refactor.

Previous Native Mac LIFE-07l checkpoint, 2026-11-21: legacy Whisper CLI now reuses existing
EngineProcess group/lifeline/retained-workspace ownership. Original English/-nt,
txt/SRT, long-window inputs,16-MiB UTF-8/result validation and return codes stay.
The command profile reserves actual PID/workspace until output validation and
tracking withdrawal, then finishes reap/cleanup; cancellation handles resistant
helpers and retains busy ownership through a stalled guardian. No new model,
transcription server, guardian or30-second clip restriction was introduced.
An actual killed-Gateway check reproduced a leaked caller-prepared WAV. Explicit
owned handoff now moves only prepared microphone/decoded/PCM windows into the
existing workspace; durable WAV attachments remain borrowed. Readable decoder
output is made0600 during handoff, preserving the original video workflow.
Final native/SAN522 and fresh independent522 pass, with all54 original fixture
assertions retained. Original runner reproduces engine/helper/result leaks on
owner death (survivors explicitly cleaned); fixed native and actual Gateway
SIGKILL checks remove processes/prepared/result workspace. Existing guardian525,
primary676 and activation/Whisper/Vision/summary normal/SAN pass. Full voice and
durable audio including271 seconds/30 windows, video, cache/cancel/restart pass;
actual installed Whisper txt/SRT/expected words and exact package/native install/
upgrade pass. See components/samosa-inference/docs/qualification/
life07l-whisper-guardian/README.md for evidence, source/input hashes and excluded
failures. Entire Gateway is audited against only PID withdrawal and explicit
prepared-input ownership changes; primary owner only moves its self-path helper.
Next integrate default visual helpers using the existing guardian without
shrinking VisionPsy2048/64-KiB/120-second or Molmo image/joint/native-video1-MiB/
600-second profiles, traces/retry/grace. Prove/fix its stubborn-helper path.
Common activation/identity/readiness/stale rejection/last-good, guardian-death and
pre-admission crash recovery/shared coordination remain open; area3/full refactor
are incomplete. Later Inference areas, Python removal, Agent/App, pinned assembly
and qualified retirement remain. Sources stays closed; defer Inkling until after
the full refactor. Native Mac only; no Python/model fetch/Docker/VM/Linux work.

Previous native Mac checkpoint, 2026-11-21: LIFE-07k reuses existing EngineProcess for
primary backend ownership, preserving original Qwen/Maple/llama launch, actual
PID/marker, unlimited lifetime, append/fallback logs and8-second TERM grace.
Retained per-marker lease, WNOWAIT identity reservation, parent-death lifeline and
pending-stop handle retention finish groups/marker cleanup before reap. No new
model/guardian/server implementation. Gateway only adds early private dispatch
and existing runtime directory creation before acquiring ownership.
Final primary676 normal/SAN, fresh standalone676 normal/SAN, existing guardian525
normal/SAN, activation/Whisper/Vision/summary normal/SAN, assembled Gateway stop/
SIGKILL, selection/rollback/settings/LAN/App/llama, exact package/native install/
upgrade and retained actual Qwen4-token generation/zero swap growth pass.
See qualification/life07k-primary-guardian/README.md for evidence and limitations.
Next: guarded legacy Whisper txt/SRT/long-window/16-MiB and default visual wide/
native-video paths, preserving Chat's INFERENCE_GUARDIAN_COMPATIBILITY.md.
Common lifecycle readiness/identity/activation, guardian-death policy and shared
coordination remain open; area3/full goal are incomplete. No Python/model download/
Docker/VM or Linux work ran. Inkling remains after the complete refactor.

Previous native Mac checkpoint, 2026-11-21: LIFE-07j moves existing primary launch/group/
probe/PID-marker/stop into backend_process.c/public API and the owner archive.
Gateway passes original trusted settings/selection and shared-mutex callbacks;
raw PID/group mutations now belong to Inference. Original bodies/flags are
preserved by audit, except a reproduced stubborn-helper leak fixed with the
existing guardian's WNOWAIT pattern. Native normal/SAN398 checks, standalone
archive, full original selection/rollback/settings/LAN/app/llama flows, exact
package/clean install and retained actual Qwen before/after checks pass. Test
Python JSON/time/tier helpers are ported without dropping assertions.
See qualification/life07j-primary-process/README.md for exact scope/results.
Next LIFE-07k reuses existing guardian/host adapters for remaining legacy/default
paths under Chat's docs/INFERENCE_GUARDIAN_COMPATIBILITY.md. Primary unlimited
lifetime/PID/environment, Whisper txt/SRT/long-window/16-MiB and visual wide
profiles must survive. Common generation readiness/identity/parent-death and
shared coordination remain unfinished; area3/full goal stay open. No Python,
new model, Docker/VM or Linux work ran.

Previous native Mac checkpoint, 2026-11-21: LIFE-07i moves the unchanged10-file Maple
implementation, packer and20 original C++ tests/diagnostics here. Shared cache
ownership is consolidated, including its last parent header copy. Original
native flags/frameworks/outputs and assertions are preserved; frozen fixture
inputs are included for standalone testing.18 supported C++ builds,9 native
runs,3 sanitizer checks, fresh independent source/data builds, Gateway quarantine/
runtime settings and package/install/upgrade pass.32 parent source/test/header
copies (7115 lines) are retired. Two scratch compile failures reproduce in the
parent baseline and remain preserved as legacy diagnostics; no algorithms were
changed to revive them. No real Maple checkpoint/catalogue readiness is claimed.
See qualification/life07i-maple-sources/README.md for commands, pins and scope.
Next LIFE-07j moves original primary launch/group/reap/probe ownership, then
guarded legacy Whisper and shared coordination. Area3/full goal remain open;
no Python, new model or Docker work ran.

Previous native Mac checkpoint, 2026-11-21: LIFE-07h moves the unchanged Qwen native
model/cache/vision/kernels/support implementation and useful original tests here.
Chat delegates CPU/OpenMP/runtime-schedule builds with original flags/outputs;
Maple consumes the owner cache implementation. Source audit, original suites
normal/SAN, independent source builds, actual old/new Mac model CLI comparison,
gateway runtime settings, package and clean-install/upgrade pass.16 parent
source/test/fixture copies (10297 lines) are retired. Serve-test edits are only
owner/include and explicit caller App test asset paths; assertions are preserved.
See qualification/life07h-qwen-sources/README.md for exact commands and limits.
Next LIFE-07i moves original Maple sources/builds/useful tests, then remaining
primary launch/group/reap/probe ownership. Guarded Whisper and shared coordination
follow the remaining-area3 audit. No Python/Docker ran; area3/full goal stay open.

Previous native Mac checkpoint, 2026-11-21: LIFE-07g consolidates visual engine source/
build ownership here. Chat delegates its Mac builds/tests with explicit existing
MLX inputs. Original compiler/link/output/media behavior is retained; original
VisionPsy tests and Molmo packer move unchanged. All13 engine files,4 tests and
packer compare identical before18 parent copies (3884 lines) are retired.
Fresh standalone native builds, original component normal/SAN and real heading
gates, newly built actual VisionPsy/Molmo owner normal/SAN, gateway workflows
and package/clean-install/upgrade checks pass. See
qualification/life07g-visual-builds/README.md for exact scope and excluded attempts.
Next: remaining primary-backend source/build/launch ownership, then guarded
Whisper execution and shared coordination per Chat's remaining-area3 audit.
Area3 and later areas remain open. No Python or Docker ran.

Previous native Mac checkpoint, 2026-11-21: LIFE-07f moves the existing visual memory/
thermal probes, adaptive budget, reserve and bounded reclaim wait into
vision_budget.c/public API. Gateway retains thin stop/trace and manifest adapters;
its pause/resume/retry policy is unchanged. Source audit,25 native normal/SAN
boundary cases, live Mac probes, independent owner build, existing Molmo/
VisionPsy workflows and package/install checks pass. No Python or Docker ran.
See qualification/life07f-visual-budget/README.md and Chat's
docs/INFERENCE_AREA3_REMAINING_AUDIT.md for exact requirements still open.
Next LIFE-07g delegates existing Mac visual engine builds/tests to the already
extracted owner source, retiring proven parent duplicates after integration.
All13 engine source/header files and3 support headers compare identical; JSON
has the documented portability adaptation. Then move remaining primary backend,
Whisper guarded execution and shared coordination per the audit. This does not
close area3, global scheduling, later areas or the complete repository split.

Previous native Mac checkpoint, 2026-11-21: LIFE-07e extracts the original shared Whisper
runner into whisper_process.c/public header and the existing owner archive.
Gateway calls preserve selected model/fingerprint and explicit original busy/
PID/cancel coordination. Original txt/SRT argv, long-window behavior, UTF-8/
16-MiB output, return codes, wait/reap and result cleanup are preserved by source
audit and native normal/SAN tests. Existing verified short-clip Whisper, fresh
standalone owner, gateway voice/durable-audio, packaging/install and actual
retained Mac model checks pass. Embedded Python in the two workflow drivers was
ported to Node/shell with assertions retained. Actual SRT syntax/words are
qualified, not timing accuracy. Exact evidence and limits:
qualification/life07e-whisper-runner/README.md.
Next LIFE-07f extracts existing Mac visual resource/thermal budget and bounded
Molmo memory reclaim policy, then audits remaining area3 ownership. Shared
admission and legacy/verified guardian consolidation remain open; this does not
close area3, later areas, whole-project Python removal or the repository split.

Previous native Mac checkpoint, 2026-11-21: LIFE-07d moves the original gateway visual
launch profiles/close bodies into the existing specialist owner/public archive.
Thin gateway calls preserve configured paths, availability, trace events and
original limits. A reproduced inactive-session close bug now releases retained
worker/lease ownership after a failed exchange. Normal/SAN launch, protocol,
supervisor/guardian and fresh standalone gates pass. Gateway summary/runtime,
Molmo, full VisionPsy workflows and clean package/install checks pass natively.
VisionPsy's Python fixture/assertions were ported to Node/shell; an obsolete image
text-only expectation was reproduced against the unchanged gateway, then split
into preserved image-floor and document-text-only checks. Product policy is
unchanged. See qualification/life07d-visual-launch/README.md for exact evidence.
Next LIFE-07e extracts/reuses the shared Whisper runner with original text/SRT,
microphone/long-window semantics, model identity, scoped cancellation and cleanup.
Full area3/later areas, Python migration/CI, Agent/App/assembly/retirement remain
open. The following checkpoints retain prior evidence.

Previous native Mac checkpoint, 2026-11-20: LIFE-07c qualifies the component's optional
resident VisionPsy/Molmo2 owners. Shared `visual_assets.c` reuses existing
verification, P6 preparation, commands and strict reply validation; one-shot
adapters use those helpers too. `visual_host.c` adapts existing Activation,
guardian workspace/duplex ownership and SamosaMmSession, with bounded native
generation required before ready. No engine/model algorithm changed.
Normal/SAN native owner/public-registry checks, isolated retained actual Mac
VisionPsy heading/Molmo2 changing-input runs, fresh source-subset owner builds
and affected original native regressions pass. No Python or Docker was invoked.
Evidence, commands, exact source hashes and limitations:
`qualification/life07c-visual-residency/README.md`.
Chat's docs/EXTRACTION_GATEWAY_LIFECYCLE_AUDIT.md now records original
gateway-owned visual/Whisper entry points, tests and concrete profile differences.
Next LIFE-07d mechanically extracts original visual launch-profile/close glue
through the existing public supervisor/trace boundary, then the shared Whisper
runner. Preserve larger-image/long-video/long-audio behavior and App policy;
component residency alone does not retire gateway-owned lifecycle.
Full area3, areas4–7, remaining Python-tool migration, Agent/App, assembly and
legacy retirement remain open. Older next-actions below are historical.

Previous native Mac checkpoint, 2026-11-20: LIFE-07b moves the original Molmo2 analyze and
VisionPsy inspect request/response functions into `src/specialist_protocol.c` and
its public header. Chat delegates through explicit trace callbacks. Original
function bodies, framing, prompts, numeric limits, Unicode/error behavior and
engine algorithms are preserved. Native public codec normal/SAN, fresh independent
archive build/regressions, Chat summary/runtime/Molmo regression, shell manifest,
clean/atomic/PATH installation and guarded native HTTP/runtime-only upgrade gates
pass. Retired the parent Python packager/selection test in favor of shell.
See `qualification/life07b-specialist-protocol/README.md` for exact evidence and
the earlier `qualification/life07a-specialist-owned/README.md` for owned close.
Historical next LIFE-07c reuses existing vision/temporal verification, image/frame preparation,
guardian workspace/duplex ownership and real generation before public residency
readiness. Do not rebuild the visual system or its model loops.
Full visual lifecycle, area 3 and the wider split remain open. Older checkpoint
next-actions below are historical where superseded by this entry.

Checkpoint 2026-10-25: LIFE-07a is in progress. The existing SamosaMmSession
now has additive request cancellation/deadline control for both original protocols,
with interrupted streams invalidated and leases retained until existing close/reap.
See `qualification/life07a-specialist-control/README.md` for transport evidence.
Next: borrowed guardian endpoints/retryable close and workspace/duplex ownership;
then original visual preparation/verification and real generation readiness.
This does not close visual lifecycle, area3 or the full repo-split goal.

Current checkpoint, 2026-10-25: LIFE-06b changing-image decisions qualifies on
one loaded NativeImage using existing preprocessing/typed algorithms and the
shared owner. Native macOS/Linux normal/SAN codec/control and actual macOS20
image/10 text cases pass in resident and legacy modes. Existing limits preserved.
New decision engine is a local candidate; production distribution gate remains
open. See qualification/life06b-image-registry/README.md and latest continuation.
Next LIFE-07a reuses SamosaMmSession plus VisionPsy/Molmo2 existing command loops,
verification/preparation and guardian workspace/Metal-cache ownership. Other
modality lifecycle, full areas3–7 and assembled work remain open; Sources is complete.

The current task is separating the existing Samosa implementation into its owners.
Model packaging work is supporting evidence, not a prerequisite for moving code.
Before adding behavior, inspect both the monolith and this repository and move or
wire the existing implementation and tests.

| Existing code moved | Inference owner | App integration |
| --- | --- | --- |
| `src/samosa_multimodal.c` and its header | `src/specialist_supervisor.c`, `include/samosa_inference/specialist_supervisor.h` | App links the owner-built static library through its Makefile; old header is a public-interface adapter |
| Original specialist helper and supervisor tests | `tests/fake_multimodal_helper.c`, `tests/test_multimodal_supervisor.c` | Existing App test target delegates to Inference; gateway tests retain their fixture interface |
| Gateway `molmo2_session_analyze_request` and `visionpsy_session_inspect` | `src/specialist_protocol.c`, public `specialist_protocol.h`, existing owned engine JSON parser | Gateway keeps thin calls and explicit trace callbacks; native release assembly includes the owner object/header/parser |
| `src/samosa_summarizer.cpp` and real-model protocol test | Existing `engines/t5_summary.cpp`, `tests/test_native_summarizer.c`, owner `t5-engine`/`test-t5-engine-real` targets | Chat delegates its existing build/test commands and copies the owner-built executable; release source staging consumes the owner copy |

The implementation, header and tests match the original bytes except for the
published include path. No supervisor algorithm or protocol was rewritten.
Independent normal/SAN tests pass on macOS; the App gateway builds against the
archive, its Molmo gateway integration passes, and existing summarizer supervisor
and runtime-settings tests pass. Linux independent verification is recorded with
the extraction commit when complete.

Remaining work includes extracting the gateway-owned text/summarizer/speech
lifecycle and admission/session behavior and retiring duplicate engine build paths.
Use existing implementations and their tests, keeping App product policy outside
Inference. Existing lifecycle primitives in this repository are also reused.

## T5 build ownership extraction — 2026-10-17

Assignment EX-T5-01 supports C03/C13/C14: the existing Chat summarizer build
consumes the identical engine from its Inference owner rather than compiling a
second source copy. Inference owns engine code and its existing native real-model
protocol test. Chat retains product policy and its supervisor integration test.
No engine algorithm, model, protocol or runtime configuration changes.

The development `t5-engine` target preserves the original macOS dynamic Prism
link flags with explicit dependency paths. The independent pinned static release
builder remains `tools/build_t5_runtime.sh`; this development target does not
claim Linux compatibility or replace that release profile. The moved test can
also exercise a separately built engine using explicit engine/model paths.

Acceptance: exact source/test byte comparison before removal of old copies;
`make test-native-summarizer-real SAMOSA_SUMMARIZER_TEST_MODEL="$PWD/build/samosa-text-summarization-Q8_0.gguf"`
passes actual native T5 inference through Chat's delegated command. The unchanged
test compiles with Inference's strict warning flags. Source release staging and
NOTICE now refer to the owner source. `make test-native-summarizer-supervisor`
passes the existing gateway integration regression. Both repository whitespace
checks pass. Release staging's removed-supervisor dependency is resolved by
EX-REL-01 below.
No full numbered area is closed by this extraction.

Next: inspect existing gateway
summary session ownership and the extracted specialist supervisor before moving
the remaining lifecycle implementation. Keep full area2 installation acceptance
open and distinguish this ownership move from area3 readiness qualification.

## Extracted supervisor release integration — EX-REL-01

Supports C03/C13/C14: preserve clean Chat installation after the specialist
supervisor ownership move. Stage the owner source, public header and license in
an explicit Inference subtree, build its static archive, and link gateway/jobsd
against that archive. Do not restore the deleted monolith implementation.
Existing installer and package-manifest acceptance scenarios must pass. This is
transitional product assembly, not full Inference release or lifecycle closure.

Result, 2026-10-17: `sh tests/test_gateway_installer.sh` passes clean staged
installation, gateway/jobsd executable checks and the new owner archive/header
assertions. Log: `/private/tmp/inference-extraction-installer.log`.
`python3 tests/test_package_pdfium.py` passes all three existing scenarios plus
exact owner source/header/license and T5 source staging checks; log:
`/private/tmp/inference-extraction-package.log`.
`make -C components/samosa-inference test-specialist-supervisor sanitize-specialist-supervisor`
passes both normal and ASan/UBSan modes; log:
`/private/tmp/inference-extraction-supervisor.log`. Shell syntax and both
repository whitespace checks pass. This is macOS verification; Linux installer
acceptance and full independent release gates remain open.

Summary lifecycle reuse inspection: `summarizer_start_locked`,
`summarizer_stop_locked`, `native_summarize_once` and
`native_summarize_batch_now` still own the resident T5 pipes/process in Gateway.
The existing regression checks repeated requests, resident PID reuse, map/reduce
and eight concurrent callers. Extract that existing process/transport state into
Inference with explicit configuration; retain Chat's enrichment/activity policy.
The general `fd_write_all` also serves unrelated gateway persistence and must
remain available there. Compare the existing summary protocol/guardian APIs
before adding any duplicate framing or lifecycle machinery.

## Resident summary process extraction — EX-T5-02

Deliver C03/C13 ownership migration: move existing start/stop, descriptor cleanup
and resident PID/pipe state from Gateway into an Inference-owned native library.
Inputs are explicit engine/model/log/GPU configuration; callers serialize access
through the process mutex. Reuse the existing fork/exec/reap implementation.
Keep enrichment, batching policy, framing and activity reporting unchanged in
Chat for this move; existing public summary protocol/guardian stays intact.
Acceptance: existing resident/repeated/map-reduce/concurrent summary regression,
owner standalone build and sanitizer checks, gateway build and release staging.
This does not close area3 or authorize a replacement lifecycle design.

Result, 2026-10-17: implemented `src/summary_process.c` and public
`include/samosa_inference/summary_process.h` in the existing owner library.
Gateway embeds this owned process state and delegates start/stop; GPU selection
is resolved by the host before calling the owner. The existing fake summarizer
also moved byte-for-byte into Inference; Chat's fixture target delegates to it.
Framing, batch queue and summary policy remain unchanged in Gateway.

Passing evidence:
- `make test-native-summarizer-supervisor`: existing repeated/resident PID,
  map/reduce and eight-caller concurrency regression;
  `/private/tmp/inference-summary-process-gateway.log`.
- `make -C components/samosa-inference test-summary-process sanitize-summary-process`:
  direct process reuse, descriptor EOF isolation, SIGKILL/restart, stop/reap and
  invalid-engine refusal; `/private/tmp/inference-summary-process-owner.log`.
- The same owner tests pass from a separate source subset without the parent
  checkout on macOS; `/private/tmp/inference-summary-process-independent.log`.
- Existing Linux QA container reused with a fresh source-only subset:
  `make -C /host-tmp/samosa-summary-process-linux-r68nrns0 test-summary-process sanitize-summary-process test-specialist-supervisor sanitize-specialist-supervisor`;
  all four gates pass, `/private/tmp/inference-summary-process-linux.log`.
- `make test-runtime-settings samosa-gateway` passes;
  `/private/tmp/inference-summary-process-regression.log`.
- `sh tests/test_gateway_installer.sh` passes clean installed gateway/jobsd and
  owner archive integration; `/private/tmp/inference-summary-process-installer.log`.
- `python3 tests/test_package_pdfium.py` passes all three scenarios including
  exact staged owner source/header/license hashes; shell syntax and whitespace
  checks pass.

The first development archive recipe included only its first object, causing a
link failure. It now archives both objects and depends on its Makefile; the
passing results above follow that correction. No engine/model implementation
was rewritten. No actual-model readiness, global admission or full area3 closure
is claimed from these fixtures; Linux clean product installer remains separate.

Next: move the existing scalar/batch summary pipe transport into the owner,
reusing `summary_protocol` where compatible and preserving the current batch
semantics and Chat activity policy. Then continue the remaining text/speech
ownership moves and the original seven-area acceptance, with area2 closure still
requiring its complete current installation audit.

## Resident summary transport extraction — EX-T5-03

Move existing `native_summarize_once` and `native_summarize_batch_now` pipe
transport into the Inference process owner, including framing, reply bounds,
punctuation normalization, warm/cold deadlines and failure cleanup. Preserve
Chat's batch queue, activity callbacks and map/reduce policy. The existing public
summary codec validates UTF-8/NUL and limits replies to8192 bytes; the inherited
batch transport admits65536-byte replies and independent chunks. Replacing it
with that codec would change behavior, so retain the working transport here.
Acceptance uses existing gateway concurrency/map-reduce tests plus owner-level
scalar/batch/error recovery, independent normal/SAN macOS/Linux and clean release
installation. No new engine or scheduler is introduced.

Result, 2026-10-17: scalar/batch IPC now lives in `src/summary_process.c`
behind public `samosa_summary_process_once`/`samosa_summary_process_batch`.
Gateway retains thin configuration/operation calls; its batch queue, map/reduce
and activity callbacks remain intact. The source prefix buffer uses bounded
malloc/memcpy instead of the Gateway TextBuffer utility, with identical request
bytes and60000-byte input limit. Existing wire flags, scalar8192/batch65536 reply
limits,15s/60s/180s deadlines, normalization and failure cleanup are preserved.
The unrelated gateway persistence writer remains local to Chat.

Passing evidence:
- macOS owner normal/SAN scalar/batch and lifecycle/error recovery:
  `/private/tmp/inference-summary-transport-owner.log`.
- Independent source-only native Linux normal/SAN:
  `docker exec samosa-s17a-linux-qa make -C /host-tmp/samosa-summary-transport-linux-1_u5pksh test-summary-process sanitize-summary-process`;
  `/private/tmp/inference-summary-transport-linux.log`.
- Existing Gateway resident/concurrent/map-reduce regression, runtime settings
  and actual gateway build: `make test-native-summarizer-supervisor test-runtime-settings samosa-gateway`;
  `/private/tmp/inference-summary-transport-gateway.log`.
- Actual original T5 corpus through the owner transport, normal/SAN:
  `make -C components/samosa-inference test-summary-transport-real sanitize-summary-transport-real T5_TEST_ENGINE="$PWD/build/samosa-summarizer" T5_TEST_MODEL="$PWD/build/samosa-text-summarization-Q8_0.gguf"`;
  `/private/tmp/inference-summary-transport-real.log`. Scalar facts and two
  four-subject/number batches pass with the same resident PID and no cross-subject
  contamination. Engine SHA256
  `514e5c8512d44ecc3da246039d30fcd7a8218e0be6f363364a53e18c212df397`,
  model SHA256 `30c73e0b2ebd4f65538d87f5157aa7193261e8ff362e441df86c6941f3e9265c`.
  Actual model execution is macOS ARM64; the native engine is not sanitizer-built.
- Updated clean installer passes: `sh tests/test_gateway_installer.sh`,
  `/private/tmp/inference-summary-transport-installer.log`.
- All three `python3 tests/test_package_pdfium.py` scenarios, both repository
  whitespace checks and installer shell syntax pass.

No numbered area closes from this transport extraction. Next inspect the
remaining gateway summary batching queue against Inference's existing admission
and opening-summary implementation, then extract reusable ownership without
moving App activity policy or introducing a second scheduler. Text/speech
ownership and the full current area2 installation audit remain required.

## Existing summary batch coalescer ownership — EX-T5-04

Move the existing4ms/64-frame summary coalescer from Gateway to Inference, keyed
by its public resident process handle and concise mode. Preserve caller-owned
arrays, reply order, one coordinator and the existing wait/dispatch algorithm.
Expose activity notifications through an explicit callback; App still decides
how those events affect its UI/jobs. This moves an existing model-specific batch
optimization, not a second global resource-admission implementation. Inference's
existing durable admission remains the authority for the later common scheduling
integration; this extraction does not claim bounded queue/fairness acceptance.
Existing opening-summary code uses single-operation guardian execution and has
no cross-caller batching implementation to reuse in place of this coalescer.
Acceptance: existing eight-caller Gateway regression plus direct concurrent owner
requests with distinct reply identity, normal/SAN and independent Linux coverage.

Implemented in `src/summary_process.c` with public
`samosa_summary_process_coalesce`. Gateway now only passes its process/config,
request arrays and App activity callback. The original global coordinator and
coalescing algorithm moved; no alternate admission engine was created. The new
public boundary refuses counts outside1..64 before enqueue, avoiding an
unserviceable request; valid existing callers retain identical behavior.

Passing evidence so far, 2026-10-17:
- Owner normal/SAN direct tests, including eight simultaneous callers across
  two process handles and both concise modes, exact distinct replies and one
  queued/running notification per caller:
  `/private/tmp/inference-summary-coalescer-owner.log`.
- Original Gateway resident/map-reduce/eight-caller regression:
  `/private/tmp/inference-summary-coalescer-gateway.log`.
- Independent Linux source subset normal/SAN:
  `docker exec samosa-s17a-linux-qa make -C /host-tmp/samosa-summary-coalescer-linux-hpr_krdw test-summary-process sanitize-summary-process`;
  `/private/tmp/inference-summary-coalescer-linux.log`.
- Actual T5 scalar, direct batch and coalesced batch preserve the original
  four-subject/number isolation corpus, same resident PID, normal/SAN host:
  `/private/tmp/inference-summary-coalescer-real.log`. Engine/model SHA256 pins
  are unchanged from EX-T5-03. Actual model execution remains macOS ARM64,
  external engine not sanitizer-instrumented.
- All three package manifest/staged-source scenarios pass:
  `python3 tests/test_package_pdfium.py`.

Clean installer collection76250 is terminal0 with `gateway installer: PASS`;
log `/private/tmp/inference-summary-coalescer-installer.log`. Both repository
whitespace checks pass. No numbered acceptance area is closed.

Next ownership audit: Gateway `backend_child_running`, `backend_probe`,
`backend_stop` and `backend_start` still own text process PID/PGID, health probing
and provider-specific launch arguments (around8040..8275). Preserve its stale-port
and process-group safety scenarios while comparing existing `managed_provider`,
`activation` and guardian implementations. App selection, upstream socket and
multimodal cancellation policy must stay outside the extracted text process
owner. Reuse existing managed lifecycle wherever it already satisfies the
behavior instead of adding a parallel backend supervisor.

## Native llama launch extraction — EX-TEXT-01

Move the existing llama-server argument/override construction from
`backend_start` into an owner-built launch adapter with explicit host inputs.
Keep its process group, port refusal and App selection/socket policy at the
current integration boundary until the existing managed guardian is connected.
Reuse the exact native launch flags and environment override interpretation;
do not add a second process supervisor. Qwen's private chat-directory dependency
is not transplanted into Inference; its removal belongs to session integration.
Acceptance: native launch argv scenarios (defaults, overrides, CPU-only,
projector), existing runtime/gateway checks and clean installer. Actual text
model qualification remains the existing registry/managed-provider gate.

Implemented `src/llama_launch.c` with public host-only
`include/samosa_inference/llama_launch.h`, linked into the existing owner archive.
Gateway now passes resolved host configuration to `samosa_llama_launch` in its
already tracked child. The inherited execv flags and atoi override behavior are
unchanged; paths remain literal argv entries. Release staging includes the owner
source/header and compiles the archive before linking Gateway/jobsd.

Passing evidence, 2026-10-17:
- `make -C components/samosa-inference test-llama-launch sanitize-llama-launch`:
  exact exec argv, defaults, override/fallback, CPU-only, optional projector,
  paths containing spaces and missing-executable errno;
  `/private/tmp/inference-llama-launch-owner.log`.
- Independent Linux source subset normal/SAN:
  `docker exec samosa-s17a-linux-qa make -C /host-tmp/samosa-llama-launch-linux-8ne005h2 test-llama-launch sanitize-llama-launch`;
  `/private/tmp/inference-llama-launch-linux.log`.
- `make test-runtime-settings samosa-gateway` passes;
  `/private/tmp/inference-llama-launch-gateway.log`.
- `sh tests/test_model_selection.sh`: orphan-listener refusal, authentication,
  immediate crash rollback, swapped-model refusal and registry-commit rollback
  all pass; `/private/tmp/inference-llama-launch-selection.log`.
- `sh tests/test_samosa_llama_lifecycle.sh` passes original App-owned llama
  launch/shutdown scenario; `/private/tmp/inference-llama-launch-lifecycle.log`.
- `sh tests/test_gateway_installer.sh` passes clean staged assembly;
  `/private/tmp/inference-llama-launch-installer.log`.
- All three `python3 tests/test_package_pdfium.py` scenarios, installer shell
  syntax and both repository whitespace checks pass.

These tests do not claim an actual-model or full numbered-area closure. Existing
managed provider/activation/guardian implementations remain the target for shared
process/readiness ownership; this adapter adds no fork/lease/readiness engine.

Next: resolve the current area2 general-text distribution acceptance before
declaring installation complete. The parent text record says cold URL gates were
running, but current process inspection finds no matching live qualifier and the
expected text-published log paths are absent. Reconstruct its exact native
qualification from retained package/index/source pins and published assets;
do not treat historical "running" prose as a verified wait or a pass. Then
connect the already-existing managed owner to the extracted launch boundary,
preserving selection rollback and owned-process tests above.

## General text cold distribution acceptance — INSTALL-TEXT-02

Recover the published `inference-text-package-2026-10-17.1` metadata with existing
`prepare_text_catalog.py`; retain the original index/model/runtime pins. Use the
retained Ornith GGUF for metadata validation without conversion or source-layout
weight copies. Run existing `provider_distribution_remote.py --profile text`
through native setup/managed/registry/offline binaries in fresh exclusive caches,
normal then SAN. Preserve its original acquisition, actual-model, cache-only
retry/removal/reinstallation and cleanup checks. Only after both pass may this
distribution enter the production catalog; full area2 also needs its final audit.

Recovered candidate matches the original index pin exactly. Current native
builds and reservation/identity normal/SAN controls pass. Normal cold gate is
live session46057; exact paths/command and sequential SAN next action are in
`TEXT_PACKAGE_INSTALLATION_VERIFICATION.md`. No distribution or area closure is
claimed while it is running.

Normal cold collection46057 has now terminated0 with actual-model, full offline
reuse/removal/reinstallation and cleanup pass. Exact receipt:
`/private/tmp/inference-text-resume-normal.json`. Sequential SAN collection50277
is live; poll it before catalog admission or area2 closure. Detailed next action
and source/CI binding audit are in TEXT_PACKAGE_INSTALLATION_VERIFICATION.md.

INSTALL-TEXT-02 is complete: SAN collection50277 terminated0; both text receipts,
exact metadata and qualification pins are retained in `catalog/ornith-text-cpu-v1`.
The final audit verifies all ten catalog/source/index/receipt bindings and107
metadata payload files, with the unchanged installation-source binding to passing
cross-platform CI. Area2 is COMPLETE; area3 ACTIVE. Read
LIFECYCLE_REUSE_INVENTORY.md for LIFE-01: reuse `llama_generate` in the existing
managed text qualification callback, then qualify failure/rollback and actual
readiness without replacing activation/guardian machinery. All remaining area3
and areas4–7 requirements are intact. No product pin or retirement is claimed.

LIFE-01 continuation: managed text startup now requires bounded actual generation
through the existing `llama_generate` transport; token/vector qualification is
retained in shared production `llama_probe`. Native adapter477/477 and CLI41/41
pass macOS/Linux normal/SAN, existing activation rollback gates pass, and actual
pinned Ornith managed/registry normal/SAN passes with the new startup behavior.
See LIFECYCLE_READINESS_VERIFICATION.md for exact files, limits, commands and logs.
Next finish its integrated negative startup/persistence case before marking
LIFE-01 qualified. Reuse the native HTTP fixture and real activation/guardian;
common all-provider lifecycle and every remaining area3–7 requirement stay open.

LIFE-01 integrated negative gate is now qualified: real managed start/activation/
guardian tests pass152/152 normal/SAN on macOS and154/154 in independent Linux
sources. Generation HTTP error, empty/zero/invalid usage, active cancellation and
deadline all preserve exact last-good state and reap failed candidates; an
unrelated process survives and a subsequent healthy replacement succeeds.
The original HTTP fixture is shared, and existing native adapter477/477 checks
still pass both platforms. Exact commands, paths, instrumentation and logs are
in LIFECYCLE_READINESS_VERIFICATION.md. No production code changed this turn.

Next LIFE-02: extract existing trusted config/setup/key/cleanup ownership from
`src/managed_main.c` for reuse by common registry lifecycle. Reuse current
managed provider/activation/guardian; preserve CLI and actual-model behavior.
Area3 remains ACTIVE,4–7 pending, full goal and retirement open.

LIFE-02 scope: extract the existing managed CLI's private configuration loading,
credential creation/removal, launch construction and provider ownership into
`managed_host.{c,h}`. The CLI keeps signals, request transport and output. The
host API returns typed errors and owns the configuration backing provider
pointers, so registry lifecycle can reuse it without parsing CLI state or
duplicating an installer/supervisor. Preserve existing config/CLI and actual text
tests, and verify both platforms normally and with sanitizers. Other modalities
and full area3 lifecycle remain required after this extraction.

LIFE-02 is qualified: existing managed setup/config/key/cleanup moved into
`managed_host.{c,h}`; CLI delegates while retaining signals and transport.
Existing CLI41/41 checks pass macOS/Linux; actual pinned text normal/SAN and
embedding cold normal/SAN pass through the extracted owner, including full
offline recovery/cleanup. See MANAGED_HOST_EXTRACTION.md and retained compact
logs/receipts/source hashes in docs/qualification/life02-managed-host.
Next LIFE-03 connects this owner to common registry lifecycle with trusted
configuration and unchanged public provider/owner identities. Preserve every
remaining modality and original area3 requirement; areas4–7 remain pending.

LIFE-03 prerequisite: registry configuration loading currently promises no engine
startup. Split the already extracted managed host's configuration-only load from
activation, retaining `managed_host_start` as its existing composed entry point.
This lets registry discovery validate/reuse configured managed identities without
launching models or copying a parser. Preserve text/embedding configuration,
credentials, controls and existing CLI behavior. Then connect actual registry
lifecycle operations; the prerequisite alone does not close LIFE-03 or area3.

## LIFE-03 registry ownership continuation — 2026-10-24

Existing managed_host now separates configuration-only load, activation and
deactivation retaining parsed metadata. Configuration load computes the same
managed identity without engine/state work; legacy start remains composed.
Registry host accepts trusted managed_config_file text/embedding entries only
when existing ID/owner/context/admission/profile bindings match. Public
provider_start/provider_lifecycle_status/provider_stop use existing activation,
actual qualification, status and guardian. Bounded --session transport reuses
managed_stream with concurrent operation controls and per-provider lifetime
exclusion. Discovery advertises owned lifecycle without engine work.

Expanded native fixtures qualify real child startup, same-port restart, exact
last-good preservation, blocked-generation cancellation, busy replacement,
artifact mutation, engine crash/refusal/recovery, EOF and SIGKILL owner cleanup,
and unrelated-process survival. The confirmed-terminal cleanup defect and its
fix are documented in REGISTRY_LIFECYCLE.md. Other modalities still require
their original lifecycle acceptance; this does not close area3.

Final macOS lifecycle normal/SAN260/260 and independent Linux261/261 pass.
Existing managed host71/71 and CLI41/41, nine-provider regression459/459, original
readiness and registry CLI regression gates pass on both platforms. Final
native34631/44775 and actual-model16749 are terminal0; no live qualification
handles remain. Actual retained Ornith normal/SAN six-scenario registry lifecycle
passes after the terminal cleanup fix. Commands, original development failures,
platform/instrumentation scope and evidence pins are in REGISTRY_LIFECYCLE.md;
compact logs/actual receipts/source hashes are retained in
docs/qualification/life03-registry-lifecycle. This qualifies the managed text
slice, not embedding/common all-provider lifecycle or full area3.

LIFE-03E embedding continuation is now qualified: existing lifecycle harness
reused for both modes; native malformed-vector startup/last-good, exact stopped
replay, cancellation/deadline/survivor, mutation/crash/restart and EOF/SIGKILL
owner gates pass macOS normal/SAN332/332 and independent Linux333/333. Text
lifecycle260/260 and261/261 plus native adapter477/477 regressions pass.
Actual pinned EmbeddingGemma2 owned-registry normal/SAN pass the complete
original768-dimensional/four-paraphrase corpus, context refusal, exact replay,
single/stream parity and shutdown, plus no-state discovery, start/readiness,
engine-stopped replay and actual restart. The original external-registry harness
also passes its default mode. No production code changed in LIFE-03E.

Final native1146/58343, actual82367/20128 and regression24331/71030 are terminal0;
no live gates remain. Source/export bytes match. Compact logs, actual artifacts
and source/evidence hashes: docs/qualification/life03e-registry-embedding.
Exact commands/development exclusions/scope are in REGISTRY_LIFECYCLE.md.
Actual model scope remains macOS; Linux is native control/fixture coverage.

LIFE-04a guardian duplex transport is qualified. Existing engine_process owns
borrowed read-only input/write-only output FIFOs, separate bounded stderr,
bound model/residency, group/parent-death cleanup and all former modes. Expanded
native tests prove repeated binary frames, EOF, output backpressure, crash,
pending shutdown/lease retention, lifetime/log limits and unrelated survivors.
The stopped-parent-death gate found a real orphan-group SIGHUP cleanup gap.
Guardian now ignores HUP so its lifeline can finish cleanup; workers still reset
HUP to default. The independent diagnostic caught that signal before cleanup;
it is retained as excluded diagnostic evidence, not acceptance.

Final normal/SAN macOS525/525 and independent Linux527/525 pass (poll counts
vary). Original activation, managed-readiness, public-summary and summary-protocol
regressions pass on both platforms. Final88336/45684 and regression79377/86226
are terminal0; no live qualification handles remain. Exact commands, excluded
development failures, exports and instrumentation are in REGISTRY_LIFECYCLE.md.
Compact logs/source hashes: docs/qualification/life04a-resident-duplex. No actual
resident T5 or full numbered-area closure follows from this transport result.

LIFE-04b controlled transport integration is qualified. Existing scalar/batch/
coalesced framing now accepts externally owned guardian endpoints/lifecycle,
retryable typed close and bounded cancellation/deadlines across queue/mutex
waits and nonblocking writes/reads. Distinct control scopes cannot coalesce;
queued cancellation/deadline does not stop an active peer, while in-flight frame
interruption invalidates that worker. A completed queue head hands off instead
of draining unrelated work beyond its own deadline. Default App launcher and
wire/prefix/limits/batching remain intact; no T5 engine was rewritten.

Final normal/SAN macOS owned transport280/356 and independent Linux267/272 pass
(counters include timed control observations), including same-guardian scalar/
batch/coalescing, malformed output/recovery, blocked I/O controls, queued and
mutex isolation, pending cleanup/retry, caller death and unrelated survivor.
Original transport/legacy tests pass both platforms. Actual pinned T5 original
four-subject/number corpus passes normal/SAN through one guardian with cleanup.
Actual model scope is macOS; C host/transport/guardian are instrumented, external
engine/fixtures retain release builds. Test-host binding is not production
activation/pin/readiness qualification.

App gateway build, original native summary supervisor, isolated installer and
three package/PDFium tests pass. Both stage manifests now include existing
private json.h for the reused UTF-8 validator. Source/export bytes match; final
97811/74849, actual/legacy17757/44803, App77402 and supervisor71189 are terminal0.
No live qualification handles remain. Source/evidence/model hashes and compact
logs: docs/qualification/life04b-summary-owned. Exact commands/exclusions/scope
are in REGISTRY_LIFECYCLE.md. Both whitespace checks pass.

Next LIFE-04c: use existing activation verified admission/revision/last-good
ownership with a trusted duplex launch hook; integrate the pinned resident T5
owner into public summary.c normalization/opening/map-reduce/provenance/receipts
and registry lifecycle. Require an actual bounded warm summary before ready.
Preserve provider/owner/operation IDs, uncertain coverage, pins/controls/replay,
then qualify public start/status/stop, failed replacement, artifact/owner death
and cleanup. Do not replace the model, installer, activation store, guardian or
working summary algorithms. Other-provider residency/full area3 and areas4–7,
all C01–C14/App/assembly/retirement remain open. Full goal stays active.


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
