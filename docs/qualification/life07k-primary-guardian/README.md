# LIFE-07k: reuse the existing guardian for the primary backend

Qualified natively on this Mac, 2026-11-21. Baselines: Chat eed930b,
Inference20832f5. Preserve working implementations; no Python, model download,
Docker/VM or Linux work. This completes the primary guardian integration slice,
not common lifecycle readiness, Inference area3 or the entire modular refactor.

## Ownership and preserved behavior

backend_process.c now uses the existing EngineProcess guardian, its retained
lease/control-pipe and WNOWAIT/group cleanup. No second guardian or model server
was added. The new internal server profile is required because the original
primary backend has unlimited lifetime, append/fallback logs and an actual
engine PID marker. Existing bounded profiles keep their limits and protocol.

The guardian starts a private same-executable child which executes the original
Qwen/Maple/llama launch body. Exec preserves that child's PID/group. The complete
original environment/log/argv/exec body and llama helper are unchanged. Launch
configuration travels through an unlinked0600 file descriptor, closed before
engine exec; it is not a public/persistent wire format or command-line payload.
The host environment is unchanged. The public owner state retains an opaque
guardian handle; callers must keep its mutex/state alive and serialize full
start/stop calls. Host binaries dispatch the private entry point before normal
configuration. No engine/model algorithms were modified.

The published PID/marker still identifies the launched backend, not its guardian.
On exit, the guardian reserves the unreaped leader until host acknowledgement or
owner death, preventing PID/group reuse during marker cleanup. It retains the
per-marker0600 ownership lock through group cleanup/reap; competing owners cannot
replace it. The lock file stays in place after release to avoid inode races.
Stop retains the original eight-second TERM grace and finishes descendants before
reaping. Host death closes the existing lifeline and triggers the same cleanup.
A stalled guardian keeps its handle/lease and refuses replacement; timeout does
not pretend ownership was released. A failed start without a guardian cannot
remove another owner's marker. No PID adoption/external reaping is supported.

The original bounded HTTP probe remains transport availability. Actual model
identity/generation qualification and common activation controls are still open.
The caller retains selection, settings, fingerprint, rollback and UI policy.
Gateway changes are only early private dispatch and existing run-directory
creation before acquiring ownership. Native source packages/installers compile
this same guardian into the owner archive rather than duplicating it in App.

## Native evidence

All final passing commands below ran under tools/without_python.sh and exited0.
The guard detects PATH interpreter attempts, including suppressed calls; it is
not an absolute-path sandbox. No Python invocation was added or used.

| Gate | Evidence |
| --- | --- |
| Source preservation, complete Gateway comparison | source-audit.txt and audit-extraction.mjs: original full engine launch, llama helper, marker writer and HTTP probe preserved; full Gateway unchanged beyond the two declared boundary edits |
| Final primary native and ASan/UBSan | primary-final-macos.log:676 checks each; all previous398 assertions retained, exact launch/env/flags, fresh/auto/locked runtime settings, literal paths, busy port, immediate exec/engine failure, marker collision/mode, bounded HTTP, original callback locking/upstream interruption, leader/helper cleanup and original8-second stubborn-primary grace |
| New guardian boundary cases | Same final676: host SIGKILL including TERM-resistant primary/helper, unrelated process survives, missing marker, immediate exit followed by host death, PID stays reserved until acknowledgement, ownership exclusion, stopped guardian retains handle/lease through timeout then completes after SIGCONT, append logs beyond1MiB and original stdout/stderr fallback |
| Existing guardian and llama regressions | native-sanitize-macos.log: EngineProcess525 normal/SAN; original llama flags/overrides/projector/literal paths/exec refusal normal/SAN. Its earlier643-check primary run is development evidence; final primary evidence is the separate676-check log |
| Existing activation, Whisper, Vision and summary profiles | compatibility-regressions-macos.log: activation124/132, Whisper508/511, Vision661/671, summary478/478 and protocol checks; normal/SAN. Polling-dependent counts vary; assertions retained. Fixture engines, not model-quality claims |
| Fresh independent source/public archive | standalone-macos.log:676 primary checks normal/SAN and original llama normal/SAN; standalone-source.txt names private subset, standalone-files.sha256 pins exact source bytes matched to owner checkout. No parent implementation/model/SDK dependency |
| Actual assembled Gateway shutdown and killed owner | gateway-stop-macos.log and gateway-sigkill-macos.log: actual compiled Gateway, native fixture engine, stubborn helper and marker all removed after SIGTERM/SIGKILL |
| Original model selection and last-good rollback | selection-macos.log: original orphan/auth/selection/reconnect/concurrent409/timeout/wedged-health/crash/fingerprint/durable-commit failure assertions retained |
| Runtime settings, compaction, backend limits, LAN | settings-macos.log, limits-macos.log, lan-macos.log |
| Native App and llama lifecycle | app-lifecycle-macos.log, llama-lifecycle-macos.log; isolated homes/releases and nondefault test launchd labels19372/19373, verified unused before tests |
| Exact source packaging and native clean install/upgrade | package-macos.log and install-macos.log: guardian source/header included, native staged archive/Gateway compile/smoke, no optional model fetch, legacy model untouched |
| Actual retained Qwen generation | actual-macos.log: same15 prompt/4 generated tokens and "Hello! How can", zero swap growth, native HTTP shutdown, Gateway and owned process stopped; actual-source.txt names retained private state. Uses prior LIFE-07j qualifier and LIFE-07h read-only APFS candidate under existing6144-MiB/zero-swap-growth guard,2 OpenMP threads/context512. This is a short compatibility check, not a performance/long-context/quality or24-hour soak claim |
| Static analysis | static-analysis-macos.log: backend owner no diagnostics; engine first-argv null warning also reproduces against20832f5 (static-analysis-baseline-macos.log). Existing parser validates nonempty array/string arguments before access. No new analysis diagnostic claimed fixed |

Development failures are retained and excluded from passing evidence:
selection-initial-directory-order-failure.log caught run-directory creation after
launch; selection-initial-llama-fields-failure.log caught requiring Qwen's fields
for llama. Corrected caller setup/validation now preserve both original branches.
The initial final/standalone fallback test read63 of the last64 bytes, excluding
the expected final newline; the corrected runner reads all64 and preserves the
stderr assertion. Those initial-log-tail-harness-failure logs are not passes.
Earlier LIFE-07j failed-harness fixture groups were also found and removed after
verifying executable, live leader/group, parent and private log ownership; see
earlier-fixture-cleanup.txt. Current fixtures explicitly prove the leader gone
rather than accepting ECHILD alone after the guardian makes it a grandchild.

## Reproduce and continue

From Inference, use the parent's interpreter guard (or equivalent interpreter-free
standalone invocation):

```sh
../../tools/without_python.sh make test-backend-process sanitize-backend-process test-engine-process sanitize-engine-process test-llama-launch sanitize-llama-launch
../../tools/without_python.sh make test-activation sanitize-activation test-whisper sanitize-whisper test-vision sanitize-vision test-summary sanitize-summary
../../tools/without_python.sh node docs/qualification/life07k-primary-guardian/audit-extraction.mjs
```

From Chat, build samosa-gateway/test_fake_openai_backend/test-runtime-settings,
then use the existing tests/test_model_selection.sh, test_settings_compact_proxy.sh,
test_backend_limits_match.sh, test_lan_access.sh, test_samosa_app_lifecycle.sh,
test_samosa_llama_lifecycle.sh, test_package_pdfium.sh and test_runtime_only_release.sh
under the guard. Native fixture CLI accepts ABSOLUTE_NATIVE_FIXTURE followed by
--gateway or --gateway-crash and ABSOLUTE_GATEWAY. The actual model check reuses
../life07j-primary-process/qualify-qwen.mjs with explicit retained native paths.
Source/runtime artifacts are pinned in source-files.sha256/actual-artifacts.sha256.

Continue existing guardian reuse for legacy Whisper txt/SRT/long-window/16-MiB
and default visual wide/native-video profiles per Chat's
INFERENCE_GUARDIAN_COMPATIBILITY.md; do not substitute narrower modern profiles.
The specialist stubborn-helper cleanup still needs its own dynamic proof/fix.
Common lifecycle controls/observed identity/generation/stale-readiness/last-good
qualification, guardian-death failure policy, shared turn/pause/resume/retry and
full supported-provider qualification remain open. This does not qualify every
model or prove safety against killing the guardian itself. Global fairness and
reservations are separate area4 work. Later Inference areas, project-wide Python
removal, Agent/App, pinned assembly and qualified monolith retirement remain.
Inkling-2-Mini is deferred until after the refactor; Sources stays closed absent
a demonstrated defect.
