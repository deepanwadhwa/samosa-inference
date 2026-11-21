# LIFE-07j: extract existing primary-backend launch and cleanup

Qualified on this Mac, 2026-11-21. Baselines: Chat7dc5a37, Inference2afb6c5.
Reuse the working Qwen/Maple bodies and existing llama launch helper. No model
algorithm, scheduler, server protocol or new model was introduced. No Python,
Docker/VM or Linux work ran.

backend_process.c/public backend_process.h and the existing owner archive now
own primary fork/exec/environment/group, bounded health probe, child observation,
PID publication/marker and stop/reap. Gateway contains explicit trusted launch
inputs and thin callbacks for original upstream detachment/shutdown and shared
specialist cancellation. The same borrowed Gateway mutex preserves coordination.
PID reads now use the public owner API; Gateway no longer mutates raw child PID/
group fields. Original model selection/availability, settings and rollback policy
stay in the caller. Virtual Molmo behavior remains in the existing caller path.

The original loopback/LAN hygiene, literal argv/model/tokenizer/chats paths,
Qwen CPU/context locks and auto settings/compaction, Maple model-dir, llama
aliases/projector/sizing/overrides, log append/permissions, atomic marker writer,
80x100-ms stop budget and1-second socket deadlines are retained. Llama's trusted
selection inputs are resolved before fork for the owner boundary; no values are
re-derived by a new runtime policy.

## Reproduced defect and bounded fix

The original stop loop returned as soon as waitpid reaped the leader, leaving
a SIGTERM-resistant helper alive. The original compiled Gateway reproduces it:
legacy-gateway-reproduction-confirmed-macos.log fails its cleaned assertion,
after explicitly killing the leftover fixture helper. The mechanically extracted
original loop reproduces it too (legacy-unit-reproduction-macos.log).

The owner reuses EngineProcess's WNOWAIT pattern: observe the leader without
reaping, finish the owned group, then reap through EINTR. Holding the child
prevents group identity reuse during cleanup. Unexpected child exit uses the
same cleanup before dropping ownership; missing child ownership is not adopted.
The same actual-Gateway fixture now passes, including the stubborn helper:
fixed-gateway-reproduction-macos.log. No model inference behavior changed.

audit-extraction.mjs reconstructs the original bodies from recoverable commits,
applies only cleanup-fix.diff in a private temporary directory and compares the
complete result. It also checks original availability/runtime persistence/
effective settings/jobs stop/llama selection and the original selection-driver
assertions. source-audit.txt records the pass. This distinguishes explicit
boundary substitutions and the reproduced fix from unrelated implementation.

## Evidence

All final qualified commands below finished0 under tools/without_python.sh.
The guard refuses and records PATH interpreter attempts, including suppressed
calls; it is not an absolute-path interpreter sandbox. Source review found no
Python invocation in the new C owner or affected native workflows.

| Gate | Evidence |
| --- | --- |
| Original selection workflow before extraction, all assertions preserved after replacing JSON/time helper calls with Node | baseline-selection-macos.log |
| Owner native and ASan/UBSan | native-sanitize-macos.log;398 checks each, Qwen/Maple/llama launch/health/group/env, manual/auto/locked settings, literal paths, marker collision/permissions, busy port, exec/child failure, socket deadline, atomic callbacks/upstream shutdown, idempotent stop, leader/helper crashes and stubborn-primary8-second force stop |
| Existing llama launch normal/SAN | native-sanitize-macos.log; original flags/overrides/projector/literal paths and exec refusal preserved |
| Fresh source/public archive outside both checkouts | standalone-macos.log; same normal/SAN owner and llama checks using only copied owner declarations/source/tests, no parent implementation/model/SDK dependency |
| Actual original vs fixed compiled Gateway helper cleanup | legacy-gateway-reproduction-confirmed-macos.log (expected original failure) and fixed-gateway-reproduction-macos.log (pass) |
| Original selection/rollback workflow after extraction | selection-final-macos.log; orphan listener refusal, auth, readiness/reconnect/concurrent409, timeout/wedged health, immediate crash, fingerprint and durable commit failure/last-good rollback all pass |
| Original HTTP runtime settings/compaction and limits comparison | settings-macos.log and limits-macos.log |
| Original LAN isolation/auth | lan-macos.log; shared authenticated Gateway retains loopback-only raw backend |
| Native App/Qwen and llama shutdown/lifecycle | app-lifecycle-macos.log and llama-lifecycle-macos.log; isolated private release/home and nondefault test launchd labels |
| Exact source/manifest staging | package-macos.log; includes new owner source/header |
| Clean native install/upgrade | install-macos.log; staged owner compiled into archive, no optional model fetch, original legacy model unchanged |
| Retained actual Qwen through original vs extracted launch path | actual-baseline-macos.log and actual-owner-macos.log, qualify-qwen.mjs; same15 prompt/4 generated tokens and Hello! How can, native HTTP model shutdown, guard exit0/zero swap growth, Gateway/backend stopped |

The actual checks use the same read-only private APFS model/runtime candidate
recorded in ../life07h-qwen-sources/actual-source.txt and pinned by that phase's
actual-artifacts.sha256. No installed models or normal user conversations/
activation were changed. The test-only shell wrapper forwards original argv to
that native engine under the existing6144-MiB/zero-swap-growth guard. It is not
a new production worker or model conversion. Inputs are2 OpenMP threads,
512-token context, four-token direct-mode generation. This is a short native
compatibility check, not general quality, long-context or production-profile
qualification. Native engines are not sanitizer builds.

actual-baseline-source.txt and actual-owner-source.txt record private state/
response/guard logs. Both model and Gateway are stopped. The actual test shuts
the model through its native HTTP shutdown so the guard can publish its footer,
then stops Gateway. Live SIGTERM/group cleanup is covered separately by native
fixtures and app tests. No performance comparison is claimed.

Excluded attempts: early Gateway fixture setups lacked the
marker parent directory; a test initially queried an already-exited child's
process group instead of checking live groups after readiness; the first actual
runner expected a guard footer after SIGTERM, whose trap exits before reporting.
The model response and stop succeeded in that attempt, but the runner failed;
actual-initial-runner-macos.log is not passing evidence. Final runners preserve
the cleanup/readiness/actual-generation assertions and correct those harness
assumptions.

## Reproduction and limits

From Inference (parent guard shown explicitly):

```sh
../../tools/without_python.sh make test-backend-process sanitize-backend-process test-llama-launch sanitize-llama-launch
```

From Chat:

```sh
./tools/without_python.sh make samosa-gateway test_fake_openai_backend test-runtime-settings
./tools/without_python.sh node components/samosa-inference/docs/qualification/life07j-primary-process/audit-extraction.mjs
./tools/without_python.sh sh tests/test_model_selection.sh
./tools/without_python.sh sh tests/test_settings_compact_proxy.sh
./tools/without_python.sh sh tests/test_backend_limits_match.sh
./tools/without_python.sh sh tests/test_lan_access.sh
./tools/without_python.sh sh tests/test_samosa_app_lifecycle.sh
./tools/without_python.sh sh tests/test_samosa_llama_lifecycle.sh
./tools/without_python.sh sh tests/test_package_pdfium.sh
./tools/without_python.sh sh tests/test_runtime_only_release.sh
```

The optional native fixture actual-Gateway test is `test-backend-process
ABSOLUTE_NATIVE_FIXTURE --gateway ABSOLUTE_GATEWAY`; source-only profile and
private runtime inputs are recorded in standalone-source.txt and the actual
source files above. Source/runtime inputs are pinned by source-files.sha256
and actual-artifacts.sha256. Original test scalar booleans/clock/JSON checks and
tier comparison are ported to Node/shell without removing assertions.

This is the existing legacy process owner. HTTP health remains transport
availability; host serialization remains required. It does not supply guardian
parent-death ownership, artifact mutation controls or common actual-generation
readiness for every default path. No broader area3 closure is claimed. Next
reuse existing guardians/host adapters under Chat's
docs/INFERENCE_GUARDIAN_COMPATIBILITY.md, preserving primary long lifetime/PID/
environment and Whisper/visual long-window/protocol contracts.

All later Inference areas, whole-project Python removal, Agent/App, pinned
assembly and qualified monolith retirement remain open. Inkling-2-Mini remains
a post-refactor catalogue follow-up.
