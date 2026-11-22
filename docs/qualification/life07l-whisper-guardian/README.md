# LIFE-07l: reuse the existing guardian for the legacy Whisper CLI

Qualified on this Mac, 2026-11-21. Baselines: Chat77e9127, Inference1194d31.
Preserve working implementations; no Python, Docker/VM, Linux work, model
conversion or model download. Full Inference/common lifecycle/refactor remain open.

## Preserved implementation and required ownership adaptation

whisper_process.c uses the existing EngineProcess group/lifeline/workspace owner.
The CLI remains on-demand, English/-nt, text or SRT, with original return codes,
trim/UTF-8 validation and16-MiB nonempty output. It accepts the same long-window
inputs; no30-second input restriction, arbitrary lifetime, log or file-size cap
was introduced. stdout/stderr still go to /dev/null. A cap on the engine's file
writes would change oversized-output refusal into process failure; the original
post-completion read/validation boundary is preserved.

The existing guardian's new command compatibility profile reports the actual
engine PID. It holds the unreaped leader/group and private result workspace
until the host reads/validates output and withdraws cancellation/tracking, then
acknowledges reap/cleanup. Existing bounded and primary-server profiles keep
their contracts. Its trusted JSON allowance is64KiB only for this profile, to
preserve worst-case quoting of original native Mac path arguments. Relative
trusted engine/model/audio/temp paths and literal shell/control characters are
preserved. The existing primary self-executable discovery helper is moved,
unchanged, into the shared owner rather than copied again.

Existing acquire/start/reaped/release/cancel callbacks remain explicit host
coordination. started publishes the actual worker. reaped now withdraws PID/
tracking while identity is still reserved, before acknowledgement/reap; Gateway
clears voice_transcription_pid under its existing mutex there. release runs only
after complete group/workspace cleanup. Cancellation discards an interrupted
result and finishes a TERM-resistant actual CLI; borrowed state/operation
reservation survives a stalled guardian until cleanup can finish. Owner death
closes the existing lifeline and kills/reaps the owned group and workspace.
No second guardian, transcription server, model or scheduler was implemented.

Original run() borrows model/audio and never removes them. A reproduced actual
Gateway crash exposed a separate ownership gap: its prepared recording remained
after worker/result cleanup. run_owned() explicitly transfers only caller-created,
regular, singly linked temporary audio into the existing private workspace as
input.wav. It makes the transferred file0600, preserving readable decoder output
rather than rejecting the existing video fixture. Busy/preflight failures leave
caller cleanup intact; after transfer, normal return/cancel/owner death clean the
input and results together. Handoff uses the workspace filesystem. Preparation
before successful handoff remains the caller's responsibility, including its
existing cleanup paths; pre-admission crash recovery is not claimed closed.

Gateway chooses owned handoff for microphone recordings and prepared decoded/
PCM windows. Single-window durable WAV attachments keep borrowed ownership.
Original model/fingerprint, audio decoding/windowing, evidence/cache, selection,
error mapping and HTTP behavior are otherwise unchanged. Full-source audit
checks the complete Gateway against its baseline plus these exact boundary edits.

## Native evidence

Final passes ran under tools/without_python.sh and exited0 on macOS26.5.1 ARM64,
Apple clang21.0.0/Xcode26.5. The guard detects PATH interpreter attempts, including
suppressed calls; it is not an absolute-path sandbox. No Python was invoked.

| Gate | Evidence |
| --- | --- |
| Original source/assertions preserved | source-audit.txt/audit-extraction.mjs: six helper bodies, complete original trim/UTF-8/16-MiB/error mapping, original txt/SRT CLI argv, all54 original fixture assertions and actual-recognition assertions. Primary unchanged except exact self-helper move; complete Gateway differs only in tracking withdrawal and explicit prepared-input handoff |
| Final public runner normal and ASan/UBSan | owned-primary-final-macos.log:522 checks each. Original long/text/SRT/Unicode/16-MiB/oversize/missing/failed/busy/preserve-cancel/publish/active cancellation assertions retained. Added held PID, resistant helpers, foreign-peer survival, owner death while active/after result, stopped guardian, relative/escaped paths beyond16KiB JSON, discarded verbose logs, owned-input success/failure/busy/cancel/death and readable-file privatization |
| Fresh independent source/archive normal/SAN | standalone-final-macos.log: same522 each and actual-driver builds, from source outside both checkouts. standalone-source.txt names subset; standalone-files.sha256 matches all23 source/public files to owner bytes. No parent implementation or prebuilt object borrowed |
| Reproduced original process leak | legacy-final-owner-death-macos.log and legacy-final-confirmation.txt: same native CLI fixture with1194d31's original runner, expected exit1, engine/helper/result survive killed host. Harness explicitly removes its surviving fixture PIDs/result before failing. legacy-source.txt locates retained C/binary. SAMOSA_WHISPER_LEGACY suppresses dispatch and supplies a link-only owned-entry shim; --owner-death-only calls borrowed run() only |
| Fixed native process owner | owner-death-final-macos.log and final522 suites: engine/helper/guardian/result gone, unrelated peer and borrowed audio/model retained |
| Actual compiled Gateway killed during CLI | gateway-owned-macos.log, gateway-owned-receipt.json and qualify-gateway.mjs: SIGKILL Gateway with active TERM-resistant native CLI/helper; engine/helper/guardian and prepared input/result workspace gone, source upload/model retained, voice/tmp empty |
| Original full voice and durable audio | voice-final-macos.log and audio-final-macos.log: auth/model selection/PCM/exact response, failure/resume/offline cache, native AAC/MP3/window decode, cancellation/restart, qualified video speech, exact overlapping tail,271-second/30-window balanced retrieval. Existing scripts/assertions were not changed |
| Existing guardian and primary regressions | native-regressions-macos.log: EngineProcess525 and primary676 normal/SAN. Its pre-owned377-check Whisper run is development evidence; final runner proof is522 |
| Existing activation and structured modality profiles | compatibility-regressions-macos.log: activation124/132, structured Whisper508/511, Vision661/671, summary478/478 plus framing, normal/SAN. Poll-dependent counts are profile evidence; fixture engines, not quality claims |
| Retained actual Whisper, normal/SAN owner | actual-final-macos.log, actual-final-sanitize-macos.log: same native Mac-generated quick-brown-fox recording, installed CLI/base.en, expected fox/dog words in text/SRT and output cleanup. Engines/models are not sanitizer builds. Actual paths/hashes are recorded, no fetch/conversion |
| Exact source packaging/native clean install and upgrade | package-final-macos.log, install-final-macos.log: staged source/manifest matches, owner/Gateway compile and boot, no optional model request, original legacy model unchanged |
| Native source analysis | static-analysis-macos.log: no Whisper owner diagnostics |

Actual recognition is a bounded compatibility check, not a general quality or
timestamp-accuracy claim. The retained CLI's original -nt SRT emits a30-second
end for the short recording; this behavior is preserved. The separate structured
Whisper adapter retains its original strict30-second/timing qualification.

Development artifacts are not passing evidence: initial-cleanup-observation-race.log
checked output disappearance immediately after engine reap, before the guardian
finished cleanup; final harness waits for the guardian too. The initial legacy
exit checker used zsh's reserved status variable; final checker uses a task-specific
name and verifies expected1. gateway-initial-owned-input-failure.log/receipt show
worker/result cleanup succeeding while the caller's prepared WAV leaked; that
reproduced gap is fixed by explicit handoff. audio-initial-owned-handoff-failure.log
caught the original video fixture's readable temporary output being rejected;
privatization now preserves that workflow without changing its assertions.
Earlier pre-owned runner/standalone/actual/voice/audio/build logs are scoped
development evidence, superseded by the named final gates above. A private
traced audio run diagnosed the video handoff and is not a published credential log.

## Reproduce and continue

From Inference, use the parent's interpreter guard (or equivalent interpreter-free
standalone invocation):

```sh
../../tools/without_python.sh make test-whisper-process sanitize-whisper-process
../../tools/without_python.sh make test-engine-process sanitize-engine-process test-backend-process sanitize-backend-process
../../tools/without_python.sh make test-activation sanitize-activation test-whisper sanitize-whisper test-vision sanitize-vision test-summary sanitize-summary
../../tools/without_python.sh node docs/qualification/life07l-whisper-guardian/audit-extraction.mjs
```

Build test-whisper-process-real and test-whisper-process-real-sanitize; run each
with absolute installed CLI, model and retained speech.wav paths recorded in
actual-source.txt/actual-artifacts.sha256. Standalone subset builds the same
runner and real-driver targets. Source files are pinned in source-files.sha256.
The native fixture accepts ABSOLUTE_NATIVE_FIXTURE --owner-death-only for the
isolated crash reproduction. From Chat, qualify-gateway.mjs accepts absolute
Gateway/native fixture/receipt paths; run it under the guard from Chat's root.
Use existing tests/test_voice_gateway.sh, test_audio_attachments.sh,
test_package_pdfium.sh and test_runtime_only_release.sh under the guard.

Next reuse the existing guardian for default visual helpers without shrinking
VisionPsy2048/64-KiB/120-second or Molmo image/joint/native-video1-MiB/600-second
profiles, trace/retry and existing grace behavior. Reproduce/fix the specialist
stubborn-helper leak on its own supported path. Common activation/observed
identity/readiness/stale rejection/last-good, guardian-death policy, pre-admission
crash recovery and shared turn/pause/resume/retry remain open. Global reservation/
fairness is separate area4 work. Later Inference areas, project-wide Python
removal, independent Agent/App, pinned assembly and qualified legacy retirement
remain. Sources stays closed absent a demonstrated defect. Inkling-2-Mini stays
after the full refactor. Do not mark area3 or the broad goal complete for this slice.

## LIFE-08a actual audio beyond one 30-second window

The existing real runner passed with a 33.482-second mono PCM16 recording
assembled from two synthetic speech clips and one second of silence. The
distinct final phrase starts at 31.090 seconds. Plain text contains it, and
the original Whisper SRT output includes it in the segment starting at
30 seconds. Both normal and ASan/UBSan drivers exercised the same retained
Base English native CLI and model. Both removed their result workspace.
The actual runs peaked at 356 MB and 375 MB process-tree RSS with zero swap
growth. The normal and sanitizer synthetic process fixtures also each passed
their existing 522 assertions, including a 271-second input.

The actual input SHA-256 is
`010926ca7582e7345a90ed3f24864b2d4e3b70b26cdd2fa15b707b79ca15089f`.
The original public lifecycle work must preserve the long-window txt/SRT path
and its timing semantics while adding observed readiness and identity.

## LIFE-08b input identity through transcript publication

Gateway now passes the observed engine and model identity to the existing
Inference runner for voice recordings and attachment windows. The runner uses
the same file-version comparison as Activation. Changes to either input reject
the transcript with `SAMOSA_WHISPER_NOT_READY`. A stale request refused before
launch leaves the caller's prepared WAV in place. A change or cancellation
observed during execution or final cleanup discards the result before returning
it to Gateway. The existing CLI arguments and transcript cache key are retained.

Normal and ASan/UBSan runner fixtures passed 572 assertions each. They cover
replacement with the same size and modification time, mutation during cleanup,
cancellation after the engine returns, and a successful retry. An independent
Inference checkout passed the runner and Activation tests. Actual Base English
passed both checked txt and SRT runs with the same 33.482-second recording,
including the final phrase. Peak RSS was 344 MB and 374 MB with zero swap
growth. Gateway also transcribed that recording through its authenticated
voice endpoint and cleared the temporary workspace.

Run `test-whisper-process sanitize-whisper-process test-activation
sanitize-activation` in the standalone Inference Makefile under the native
guard. Build `build/test-whisper-process-real` and its `-sanitize` variant for
the real run. Both accept absolute engine, model and WAV paths and an optional
lowercase phrase to check. For the retained recording, use `silver compass`.

The legacy status endpoint still reports installed input presence. The common
long-window start and stop controls, a bounded speech probe for verified
readiness and shared recovery remain open.
