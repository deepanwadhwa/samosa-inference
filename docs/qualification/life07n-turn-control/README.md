# LIFE-07n: extract the existing shared turn coordination

Native Mac work, 2026-11-21. Baselines: Chatabfd79f/Inferencebe6df7c.
Reuse working implementations. No Python, Docker/VM, Linux verification, model
download or conversion. Full common lifecycle/area3 and the refactor remain open.

## Ownership and preserved implementation

turn_control.c/public turn_control.h now own the existing shared text/visual
generation gate, visual availability/preference, small-Mac primary reclaim,
resume wait and one-step lower-side retry. Gateway passes its existing trusted
availability, process, budget, session and trace adapters. Planning, evidence,
selected-model configuration and App presentation remain caller owned. No new
scheduler, engine, guardian, server or model algorithm is introduced.

This is a prerequisite ownership move for the remaining common lifecycle work:
the current managed_host adapter is specifically pinned static-system llama,
while default Qwen/Maple and wide/native-video paths retain different launch,
asset and lifetime contracts. Substituting that adapter would lose behavior.
Their shared gate/pause/resume policy first needs an Inference owner; it must not
remain authoritative in Gateway when the remaining lifecycle controls converge.

The moved bodies retain installed Molmo preference, explicit extended-provider
refusal, VisionPsy fallback, original pressure/required-memory checks and the
18-GiB reclaim threshold. Primary resume keeps its60-second/100-ms wait, including
startup failure and shutdown behavior. The visual lease stays held through
close/resume. Retry retains2048→1536→1024→512 and at most one lower-side attempt,
with prepared-page exclusive give-back before unload and retake after restart.
Original model, framing, traces, error codes, caller deadlines and media limits
stay unchanged. Gate allocation adds only explicit owner lifetime; callers join
users before destroy and never copy live state.

One reproduced defect is fixed. Direct visual close originally released visual
exclusion before atomic_store(generating,0). A new text request admitted between
those steps lost its count, permitting another visual start while text was active.
direct-count-baseline-failure.log reproduces that exact interleaving using the
extracted original logic; the source audit confirms the reset window in the
original Gateway. The owner now tracks the direct flag and changes only its own
contribution under the existing gate mutex. Repeated set/clear is idempotent;
an admitted primary keeps its count. No new queue or fairness policy is claimed.

## Native verification

Runs use Chat's tools/without_python.sh on macOS26.5.1 ARM64/Xcode26.5. That
guard detects PATH interpreter attempts; it is not an absolute-path sandbox.
No Python was invoked.

| Gate | Evidence |
| --- | --- |
| Extraction/source audit | source-audit.txt/audit-extraction.mjs: complete Gateway equals baseline plus owner adapters/runtime/gate accesses; original moved coordination bodies match callback substitution; sole gate behavior fix is the demonstrated direct-count withdrawal; engines/protocols/guardians/backend/Whisper/budget implementations unchanged |
| Final normal/ASan/UBSan owner | native-final-macos.log:12500/12140 checks; counts vary with thread interleaving. Original availability/failure/exclusion, simultaneous text counts, direct reset race/idempotence, exact reclaim threshold, close-before-resume, failed start/pressure/recovery,60-second virtual deadline, shutdown, all lower-side tiers, and native guardian-backed prepared-page retry/Molmo cleanup |
| Concurrent requests | Same suite:8 threads×300 attempts share one gate; accepted text/visual work never overlaps, active counts/reservations return to zero. Concurrent text retains original counting behavior. This is exclusion evidence, not fairness/global admission qualification |
| Independent source build | standalone-final-macos.log:12373/10719 normal/SAN checks and actual-driver builds from31 source/public files outside both checkouts, pinned in standalone-files.sha256; no parent implementation or prebuilt object borrowed |
| Original product visual workflows | vision-gateway-macos.log and molmo-gateway-macos.log: original VisionPsy image/PDF/multi-page/partial/OCR/traces plus Molmo joint/native-video/direct/delegated/selection/streaming/tracking assertions pass. molmo-final-macos.log reruns direct/delegated paths after the counter fix |
| Actual compiled Gateway crash | gateway-vision/gateway-molmo receipt/log filenames below: active PDF requests killed by SIGKILL remove primary/visual/helper/guardian, prepared page and workspace; durable PDF/model and unrelated peer remain. Uses LIFE-07m's unchanged native fixture driver |
| Original settings/text budgeting | root-final-macos.log: fresh Gateway, runtime-settings and all15 prompt-budget scenarios, zero context overflows; all80 facts reach review and derived synthesis, including context4096, reduction, fallback, transient/empty/truncated retries, thinking removal and cancellation |
| Other affected product callers | voice-macos.log, selection-macos.log, lan-macos.log, app-macos.log: original voice/auth/cleanup, selection/rollback/fingerprint refusal, LAN and App lifetime checks pass. These preceded only the direct-flag counter fix, which does not alter their non-direct paths |
| Exact packaging/native install | package-final-macos.log and install-final-macos.log: exact staged owner source/hashes/sizes; native clean install/upgrade/boot, no optional model requests, existing model unchanged |
| Analysis/public header | static-analysis-macos.log and public-header-cpp-macos.log: no owner diagnostics; the public header compiles in C++20 |

Crash receipt filenames are gateway-vision-receipt.json and
gateway-molmo-receipt.json. These and the ordinary Gateway workflows use private
fake engines/models, not real inference quality or the user's Samosa home.

Final actual-model passes are recorded in actual-parity.txt: all six observations
match the original launcher exactly, with3374/3404MiB peak RSS and zero swap growth. The real driver
reuses LIFE-07m's six original requests and output assertions through the new
turn owner. It tests original1536 heading recognition,2048 execution/output
parity, changed red/blue inputs, joint2-image count, native1-second/2-frame video,
owned prepared-page cleanup, actual PID close and gate release. It explicitly
uses unknown resource telemetry in this compatibility driver; deterministic
threshold/reclaim tests and product workflows qualify that policy separately.
The6144-MiB/zero-swap-growth process-tree guard remains enabled. Engines/models
are retained native artifacts, not sanitizer builds. Original2048 semantic
heading quality already failed LIFE-07m's baseline and stays unqualified.
Joint/video metadata assertions do not establish general reasoning or accuracy.

## Preserve and repair the original budget test

The original tests/test_prompt_budget.c no longer compiled at Chatabfd79f because
it directly assigned the retired Gateway.backend_pid. legacy-budget-build-failure.log
reproduces that failure without invoking its Python driver. The repaired fixture
dispatches/reuses backend_process instead of adopting a synthetic raw PID, and
uses the new shared gate API. It retains the complete C evidence generation,
budget inputs, cancellation and domain assertions byte-for-byte except gate access.

tests/test_prompt_budget.mjs ports the original75-line Python HTTP driver and all
15 scenarios. Native owner launch happens before the private Node HTTP fixture
listens, preserving occupied-port refusal. The owned idle helper provides fixture
liveness; Node supplies synthetic token/model responses. This is a budget test,
not proof of actual model readiness or endpoint identity. The old Python driver
and its Make caller are retired. Source audit verifies the complete C domain body
and original scenario list. Exact original assertions remain for zero overflows,
every fact/review, derived-note reduction, thinking removal, four failed/truncated
attempts, pre/mid cancellation and oversized-question refusal.

Development failures are excluded from passing gates. native-initial-macos.log
caught a missing Darwin declaration in the new test fixture, fixed by the same
feature macro used in other native fixtures. prompt-budget-macos.log and
prompt-budget-final-macos.log caught incomplete private owner-fixture launch
configuration; Maple-shaped idle execution and the required log field fix the
harness without changing product code. Earlier native/standalone/actual-owner
logs precede the final direct-count fix. Earlier package/install logs qualify
the initial move; final logs recheck the exact final owner sources.

## Reproduce and continue

From Inference:

```sh
../../tools/without_python.sh make test-turn-control sanitize-turn-control
../../tools/without_python.sh make build/test-turn-control-real build/test-turn-control-real-sanitize
../../tools/without_python.sh node docs/qualification/life07n-turn-control/audit-extraction.mjs
```

The independent subset named in standalone-source.txt builds those same targets.
Run either actual driver with absolute VisionPsy executable/model, Molmo
executable/model, media-directory and video paths from LIFE-07m's retained
artifact pins. Set SAMOSA_VISUAL_ACTUAL_RECEIPT to an absolute output file;
run under tools/run_memory_guarded.sh with MAX_FOOTPRINT_MB6144 and
MAX_SWAP_DELTA_MB0. Final actual binaries and inputs have separate artifact pins.
From Chat, use guarded make samosa-gateway test-runtime-settings test-prompt-budget
and the existing workflow/package scripts named above. VisionPsy E2E requires
explicit native MLX_DIR/MLX_BUILD_DIR. Never invoke broad legacy Make aggregates
that still contain Python.

Next common controls must reuse existing Activation and these extracted owners
while preserving default Qwen/Maple/llama, nonresident Whisper txt/SRT/long-window
and wide/native-video visual contracts. Observed identity, actual capability
readiness, stale rejection/last-good, guardian-death policy and pre-admission
crash recovery remain open. Gate ownership alone does not finish area3 or the
separate area4 global residency/reservations/fairness requirements. Other
Inference areas, project-wide Python removal, independent Agent/App, pinned
assembly and qualified monolith retirement remain. Sources stays closed absent
a demonstrated defect. Inkling-2-Mini stays after the complete refactor.
