# LIFE-07c: reuse existing visual ownership and readiness

Qualified component slice, 2026-11-20. Supports C03/C04/C11/C13/C14; Mac first, no Python.
Source baseline: Inference 3e9b6f1, Chat b3c79c1. Full repo-split goal stays active.

Existing implementation to reuse:

- `vision.c`: `verify`, `same_receipt`, `model_verify`, white-alpha P6 preparation,
  inspect command and strict identity/token/observation validation in `run`.
- `temporal.c`: `engine_verify`, `write_owned`, prepared-frame request construction,
  strict timestamp/frame/usage validation and existing `temporal_package_verify`.
- `engine_process.c`: workspace/duplex guardian, retained lease/directory/model
  descriptors, parent-death/reap cleanup and existing Metal-cache policy.
- `specialist_supervisor.c`: original line/framed transport, bounded control and
  owned endpoint/retryable close. `summary_host.c`/`activation.c`: existing
  verified launch, actual-request qualification, last-good activation and stop.

## Implemented boundary

`visual_assets.c` contains the existing verification, white-alpha P6 preparation,
inspect/analyze command construction and strict reply validation. One-shot
`vision.c`/`temporal.c` now use these same helpers. Seven complete helper bodies
match baseline 3e9b6f1 after whitespace/local helper renames; see
`source-helper-audit.txt`. Encoders/preparation and response checks were extracted
from the original run bodies, preserving fields, identity, dimensions, tokens,
frame/timestamp limits and observation rules. Existing one-shot quit-response
validation remains in its original execution path.

`visual_host.c` adapts existing Activation, guardian workspace/duplex ownership
and SamosaMmSession. The missing boundary was reusable ownership across requests
with actual-generation readiness; neither legacy one-shot run supplies that
boundary. No new engine/model/transport algorithm was introduced. The bridge
adds verified borrowed pipe endpoints; retained model/workspace descriptors and
the existing Metal-cache file-size policy are preserved.

Optional private profiles are `resident-vision-v1`/`resident-temporal-v1` with
engine size and config.json/manifest.json anchor size, plus all existing pins.
Loading is lazy. Start verifies complete artifacts/system runtime, exchanges the
existing ping/hello, then performs an actual bounded warm generation before
committing last-good activation/readiness. The bundle snapshot retains complete
model pins. Each request rechecks complete model/executable bytes, shares the
existing command/result helpers and preserves identity/usage/provenance/replay.
See [profile contract](../../../contracts/provider-registry-v1.md#resident-visual-ownership-native-mac).

Requests serialize on the owned transport. Queued cancellation leaves the active
request alive; interrupted exchanges invalidate/reap. Stop/owner death remove the
workspace and release the existing lease. An unconfirmed shutdown keeps ownership
for retry. No gateway launch/routing/resource policy was switched in this slice.

## Terminal native Mac evidence

Host: macOS26.5.1 (25F80), ARM64, Xcode26.5.0 C compiler, system SQLite.
Every final command below returned0. SAN means ASan+UBSan on the C owner/adapter;
external actual model engines are retained native binaries, not instrumented.
No Python, Docker, VM or Linux qualification was invoked for this assignment.

| Gate | Result | Evidence |
| --- | --- | --- |
| Native owner + public registry, normal/SAN | Both visual modes pass readiness, changing inputs on one guardian, failed-ready last-good retention, queued cancellation with surviving active request, deadline invalidation, mutation/unsafe-directory refusal, owner death/unrelated peer survival/reacquisition, stop and active session EOF cleanup | `host-registry-final-macos.log` |
| Actual retained VisionPsy, normal/SAN | Actual warm generation; original rendered document heading and changed-corner input both yield `helloworld`, same guardian/revision, stop/cleanup | `actual-vision-final-macos.log`, `actual-vision-final-sanitize-macos.log` |
| Actual retained Molmo2, normal/SAN | Actual warm generation; red/blue prepared frames change observations on one guardian/revision, stop/cleanup | `actual-temporal-final-macos.log`, `actual-temporal-final-sanitize-macos.log` |
| Original VisionPsy public real heading gate | 15882 checks, original provenance/uncertainty/durable replay assertions retained | `actual-heading-legacy-macos.log` |
| Original one-shot vision, normal/SAN | 661/671 checks | `regressions-macos.log` |
| Original temporal, normal/SAN | 1052/1058 checks, including16 prepared frames, controls/cleanup and mutation refusal | `regressions-macos.log` |
| Activation, normal/SAN | 124/132 checks | `regressions-macos.log` |
| Extracted specialist protocol, normal/SAN | 25 exchanges/refusals each | `regressions-macos.log` |
| Native registry modalities + CLI, normal/SAN | 459/459 modality and584/587 CLI checks | `regressions-macos.log` |
| Fresh native owner source subset, normal/SAN | Compiled outside parent/module checkout; both visual modes pass without sibling source or prebuilt owner objects | `standalone-final-macos.log`, `standalone-source.txt` |
| Chat gateway build + original summary/runtime assertions | Pass | `chat-integration-macos.log` |

Check totals include fixture hash-read assertions and are not scenario counts.
Public registry fixture gates use native C workers, not model-quality substitutes.
The standalone subset proves this owner boundary only; complete Inference source
release and Python-free aggregate test/CI qualification remain open.

## Reproduction

From the Inference repository (parent guard runner shown explicitly):

```sh
metadata=/Users/deepanwadhwa/.samosa/models/molmo2-4b-mlx-q4-v1
../../tools/without_python.sh make test-visual-host sanitize-visual-host TEMPORAL_METADATA="$metadata"
../../tools/without_python.sh make test-vision sanitize-vision test-temporal sanitize-temporal test-activation sanitize-activation test-specialist-protocol sanitize-specialist-protocol test-registry-modalities sanitize-registry-modalities test-registry-cli sanitize-registry-cli TEMPORAL_METADATA="$metadata"
```

The guard refuses/records PATH-based Python calls; it is not an absolute-path
interpreter sandbox. Source/recipe review found no interpreter/parent dependency
in the new C owner, helpers or native fixture. Existing Rust tokenizer archive
was reused for full registry builds. Linker warnings about its26.5 objects vs
26.0 deployment target do not establish support below this26.5.1 host.

Actual candidates are APFS `cp -c` private clones of retained native assets;
installed model files and normal user activation/conversation state were not
modified. `actual-source.txt` identifies that local candidate. Vision uses the
installed paired engine/metallib; temporal uses the retained prepared-frame-capable
build engine and complete installed model package. `actual-artifacts.sha256`
pins external inputs; the Molmo manifest pins every package member. `hello.ppm`
was rendered by the native PDFium helper from the existing `hello.pdf` fixture.
No new model, conversion, download or Python framework was introduced.

```sh
candidate=$(cat docs/qualification/life07c-visual-residency/actual-source.txt)
../../tools/without_python.sh "$PWD/build/test-visual-host" "$metadata" "$candidate/installed-runtime/samosa-visionpsy" "$candidate/vision-model" vision "$candidate/hello.ppm"
../../tools/without_python.sh "$PWD/build/test-visual-host-sanitize" "$metadata" "$candidate/installed-runtime/samosa-visionpsy" "$candidate/vision-model" vision "$candidate/hello.ppm"
../../tools/without_python.sh "$PWD/build/test-visual-host" "$metadata" "$candidate/installed-runtime/samosa-molmo2" "$candidate/molmo-model" temporal
../../tools/without_python.sh "$PWD/build/test-visual-host-sanitize" "$metadata" "$candidate/installed-runtime/samosa-molmo2" "$candidate/molmo-model" temporal
../../tools/without_python.sh "$PWD/build/test-vision-real" "$PWD/build/samosa-inference-vision" "$candidate/installed-runtime/samosa-visionpsy" "$candidate/vision-model" "$candidate/hello.ppm"
```

All final actual commands use the guard. Run GPU cases sequentially.

Standalone procedure: create an empty directory outside both checkouts, copy
only Makefile, src/, include/, test_visual_host.c, vision_fixture.h and
whisper_fixture.h. Build `build/test-visual-host build/test-visual-host-sanitize`
there. Run each absolute binary with metadata; for SAN set
`SAMOSA_VISUAL_NATIVE_FIXTURE` to the fresh normal binary. Leave
`SAMOSA_VISUAL_REGISTRY` unset, so this subset does not borrow a parent CLI/Rust
archive. Full public integration is the separate registry gate above.

From Chat, the affected integration check was:
`./tools/without_python.sh make samosa-gateway test-native-summarizer-supervisor test-runtime-settings`.

## Development failures and limits

Earlier logs are retained as development history, not passing evidence. Initial
attempts omitted temporal metadata, supplied incompatible activation hooks, used
a nonprivate fixture manifest or tried an ASAN-instrumented fake engine under the
unchanged system-runtime policy. Corrected configuration/fixtures; SAN owners now
use a native fixture worker. A mistyped guard path failed before running tests.

The expanded owner-death fixture initially opened SQLite after a post-thread
fork; this Mac crashed in SQLite/system logging before activation. The fixture
now execs a fresh owner before touching state. `fixture-fork-failure.log` records
that excluded attempt; final owner and standalone gates use the corrected path.
Production guardian launch already uses exec. Existing checks were retained.

A solid-color VisionPsy exploratory case returned empty/gibberish text both
through the new owner and when calling the retained engine directly. It is not
accepted capability evidence. The existing useful document-heading gate remains
unchanged and passes; empty response rejection was not relaxed. Earlier debug
builds/raw responses were excluded and final binaries rebuilt from repository
sources. This is bounded corpus recognition, not a general quality claim.

Read-only gateway audit is in Chat's docs/EXTRACTION_GATEWAY_LIFECYCLE_AUDIT.md.
Next LIFE-07d mechanically extracts original visual launch-profile/close glue
through the existing public supervisor/trace boundary, then the shared Whisper
runner. Preserve original input limits, launch/warm/stop/admission policy and
recorded tests. Gateway lifecycle retirement, complete Whisper qualification, full area3,
areas4–7, Python tooling/CI migration, Agent/App/assembly/retirement remain open.
Inkling-2-Mini remains deferred until the refactor is complete.
