# LIFE-07m: existing guardian ownership for default visual specialists

Native Mac qualification, 2026-11-21. Baselines: Chat7f47e25 and
Inferencef06d5c1. Extract/reuse existing working code; no Python, Docker/VM,
Linux verification, model download or model conversion. Area3 and the full
refactor remain open.

## Preserved behavior and boundary adaptation

The default specialist_supervisor launcher now reuses the existing EngineProcess
guardian. Its original fork/exec/group/reap branch is retired. No second guardian,
engine, visual server, decoder or scheduler is introduced. specialist_protocol.c
and the model implementations are unchanged. Original trusted absolute engine/
model paths, literal argv, availability checks, inherited stderr, trace events,
framing and caller error/cancellation semantics remain.

VisionPsy retains sides512–2048,64-KiB frames including the line newline,
20-second readiness,120-second requests and2-second shutdown grace. Molmo retains
image/joint-image/native-video requests,1-MiB framed payloads,90-second readiness,
600-second requests and3-second shutdown grace. Both retain quit250ms, then
their grace, TERM750ms and KILL. No new24-hour lifetime, stderr/file cap,512-side
restriction or prepared16-frame replacement is imposed. The existing modern
bounded/external-lifecycle profiles retain their contracts.

The compatibility profile retains duplex pipe endpoints, inherited stderr and a
private0700 workspace through the existing descriptor/lifeline plumbing. Actual
engine PID/group remains published; the guardian holds the unreaped leader until
host acknowledgement, finishes resistant descendants and removes its workspace.
Cancellation signaling and PID withdrawal share the supervisor mutex. Pending
shutdown retains the handle/reservation and refuses replacement. Typed close
exposes pending/error; the original void default close waits for confirmed cleanup.
Unexpected guardian death remains a fail-closed, unfinished policy requirement.
FD4 here retains diagnostics; it is not proof of global residency admission.

Only caller-prepared PDF pages transfer into that workspace as input.ppm. Durable
image/video/PDF/model paths stay borrowed. The serialized page loop reuses that
name without a new page limit. For the original VisionPsy lower-side retry,
exclusive give-back restores caller ownership before unload; the restarted
session retakes the same bytes. Capacity/type/link checks precede handoff; a
readable prepared page becomes0600. Normal return, cancel and owner death clean
the transferred page. Preparation and the retry give-back interval remain caller
owned; pre-admission crash recovery is still open.

One demonstrated Mac transport defect is fixed: poll on a closed write pipe
can report POLLHUP without POLLERR/POLLOUT. The original helper waited until its
full request deadline. The sole framing change treats write-HUP as failure.
The isolated original runner takes1000ms with the test's shortened deadline;
the fixed runner takes0ms. Production request deadlines are unchanged.

## Evidence

Commands run under Chat's tools/without_python.sh on macOS26.5.1 ARM64,
Xcode26.5/Apple clang21. The guard records PATH interpreter attempts; it is not
an absolute-path sandbox. No Python was invoked.

| Gate | Evidence and scope |
| --- | --- |
| Original implementation/assertions | source-audit.txt and audit-extraction.mjs compare the complete Gateway against only explicit PDF ownership/give-back/retake edits; original transport bodies except write-HUP, external lifecycle branch, specialist_protocol.c and original tests/assertions are preserved; engines unchanged |
| Final default ownership normal/ASan/UBSan | visual-final-macos.log:276 checks each, both original launch profiles,64-KiB/1-MiB exact boundaries and overflow refusal, original grace/TERM/KILL, actual PID/group, resistant helpers, busy/pending ownership, exec/crash/reuse, stderr beyond1MiB, cache beyond64MiB, closed stderr, owned-page/refused-link/collision/give-back/restart and killed parent cleanup |
| Original tests and external owned profile | visual-final-macos.log: original supervisor/launch/protocol normal/SAN including25 protocol exchanges/refusals; existing externally owned specialist normal/SAN retains its own bounded workspace/cache/deadline semantics |
| Independent source build | standalone-final-macos.log: same276 normal/SAN, original supervisor/launch/protocol and actual-driver builds outside both repositories; standalone-files.sha256 pins all27 source/public files, matching owner bytes; no parent implementation or prebuilt objects borrowed |
| Original descendant defect | legacy-final-helper-leak-macos.log and legacy-final-confirmation.txt: f06d5c1 raw launcher leaks TERM-resistant helpers after leader quit for both original profiles; expected exit1, all observed fixture survivors explicitly removed |
| Original Mac write-HUP defect | legacy-write-hup-macos.log expected failure versus fixed-write-hup-macos.log; closed writer elapsed1000ms versus0ms, fixture survivors cleaned |
| Actual compiled Gateway PDF owner death | gateway-vision-receipt.json and gateway-molmo-receipt.json, corresponding logs and qualify-gateway.mjs: SIGKILL with an active request; primary/visual/helper/guardian, prepared page and workspace gone; durable PDF/model and unrelated peer survive |
| Original assembled workflows | vision-gateway-macos.log and molmo-gateway-macos.log: original image/PDF/multi-page/text-only/OCR/partial recovery/traces and image/joint/native-video/direct/delegated/selection/streaming/tracking assertions pass unchanged |
| Shared ownership regressions | native-serial-final-macos.log: EngineProcess525, primary676 and legacy Whisper522 checks each normal/SAN; this predates only the final parent workspace-name identity check, which the final visual suites exercise |
| Common bounded-profile regressions | compatibility-macos.log: activation124/132, structured Whisper508/511, Vision661/671, summary478/478 plus framing, normal/SAN; fixture engines, not quality claims |
| Actual retained engines, before/after | actual-parity.txt and actual-baseline/owner/owner-sanitize JSON/logs: all six observations match exactly; peak RSS3376/3373/3406MiB, zero swap growth each; only new runs exercise prepared-page ownership |
| Packaging and native installation | package-macos.log and install-macos.log: exact owner sources/hashes/sizes, native clean install/upgrade and boot; no optional model request, existing model unchanged |
| Static analysis | static-analysis-macos.log: no specialist owner diagnostics |

The receipt filenames are gateway-vision-receipt.json and
gateway-molmo-receipt.json. Native Gateway checks use a private fake model/helper,
not real inference quality. They do not touch the user's normal Samosa home.

Actual model parity passes are recorded in actual-parity.txt.
The old launcher baseline uses the retained native engines/kernels/models and
the same two VisionPsy tiers plus Molmo red/blue, joint2-image and native1-second
2-frame video requests. Original1536 heading recognition passes. Original2048
heading quality fails with both prompts attempted: see the two baseline-failure
logs. Final2048 assertion therefore proves execution/output parity only; its
semantic quality remains explicitly unqualified. Joint count and video metadata
checks do not establish general reasoning, tracking or timestamp accuracy.
The old baseline does not exercise the newly added prepared-page ownership API,
even though its earlier unconditional PASS label mentions prepared cleanup.
Only new native/owner/Gateway runs establish that cleanup.

Development evidence is excluded from final passes. Initial/workspace/native
logs precede later fixes. excluded-overlap.txt identifies an invalid overlapping
shared-build log retained privately; serial/final runs replace it. The Molmo
Gateway fixture initially used computed dummy-file fingerprints that do not
match the original fixed admission metadata; it was corrected to the original
test fixture pins without changing product admission. visual-relative-final-
macos.log is a rejected extra fixture: it requested relative/symlinked paths,
which the original public preflight rejects. That trial and its unnecessary
validation change were reverted; no relative-path support was added.

## Reproduce and continue

From Inference:

```sh
../../tools/without_python.sh make test-specialist-guardian sanitize-specialist-guardian
../../tools/without_python.sh make test-specialist-supervisor sanitize-specialist-supervisor test-specialist-launch sanitize-specialist-launch test-specialist-protocol sanitize-specialist-protocol test-specialist-owned sanitize-specialist-owned
../../tools/without_python.sh node docs/qualification/life07m-visual-guardian/audit-extraction.mjs
```

Build test-specialist-guardian-real and its -sanitize target, then run each with
absolute VisionPsy executable/model, Molmo executable/model, image-fixture directory
and video path. Set SAMOSA_VISUAL_ACTUAL_RECEIPT to an absolute output file.
Use Chat's existing run_memory_guarded.sh with MAX_FOOTPRINT_MB6144 and
MAX_SWAP_DELTA_MB0. Models/engines themselves are not sanitizer builds. Private
paths/pins are in actual-artifacts.sha256 and the retained source locator files.
From Chat, qualify-gateway.mjs takes absolute Gateway/helper/fake-backend paths,
vision or molmo, and an absolute receipt path. Existing workflow/package scripts
are named in the evidence table; original VisionPsy E2E also requires explicit
MLX_DIR/MLX_BUILD_DIR for the existing native build.

Next complete common lifecycle controls/activation, observed identity and actual
capability readiness, stale rejection/last-good and guardian-death policy across
default paths. Move the existing shared turn gate/pause/resume/retry coordination
into its owner without redesigning it. Pre-admission crash recovery remains open.
Global reservation/fairness is separate area4 work. Other Inference areas,
project-wide Python removal, independent Agent/App, pinned assembly and qualified
legacy retirement remain. Sources stays closed absent a demonstrated defect.
Inkling-2-Mini remains after the entire refactor; keep the broad goal active.
