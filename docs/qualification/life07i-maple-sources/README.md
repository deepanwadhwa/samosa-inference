# LIFE-07i: extract the existing Maple implementation and native tests

Qualified 2026-11-21 on this macOS26.5.1 ARM64 host. Baselines: Chat fd82700,
Inference cbc165f. Extract/reuse the working implementation; no model algorithm,
cache policy, model format, source loader or HTTP protocol was redesigned.
No Python, model download/conversion, Docker/VM or Linux work ran.

All10 src/maple implementation/header files and maple_pack.cpp now belong to
Inference. Their source is byte-identical to the parent baseline. The20 native
C++ tests/diagnostics also move here; only ../src/maple owner includes change.
The shared cache implementation was already extracted; Maple now uses the owner
header too, removing the last parent cache-header copy. Shared headers retain
the previously qualified named Unicode-buffer portability adaptation in JSON.

Chat delegates existing builds/tests through the owner, retaining native
compiler options, frameworks, executable names, matching MLX source/build/kernel
inputs and original real-model memory guards. The exported relative
MAPLE_MODEL_DIR input is resolved from the parent before the native test changes
to its owner fixture directory; both the default and relative-input cases pass.
No production launch/configuration code or catalogue entry changed.

After native source/behavior verification,32 parent source/test/header copies
(7115 lines) were retired. Original reference data used by unported legacy
drivers remains in Chat. Ignored locally generated data is preserved too.
The independent owner includes the same19 frozen fixture files, including6
formerly local component vectors, so native tests need no Python generator or
hidden parent test data. Those are existing synthetic/reference inputs, not a
new checkpoint or changed expected results.

## Evidence

Every final qualified command returned0 under the PATH-based no-Python guard.
The guard also detects suppressed PATH interpreter attempts; it is not an
absolute-path interpreter sandbox. Source/recipe review found no Python call in
the moved native profile. Python mentions in old scratch-test comments are not
invocations; project-wide legacy tooling cleanup remains open.

| Gate | Evidence |
| --- | --- |
| Original engine/packer, parsing/store/views/cache/Metal, tensor/model and tiny-model tests before extraction | baseline-macos.log |
| Delegated original parent commands and numerical assertions | delegated-macos.log and retired-builds-macos.log; both pass, including after parent source removal |
| Owner supported native build set | owner-final-macos.log;18 C++ tests/diagnostics compile |
| Original native behavior | owner-final-macos.log;9 programs execute: OpenAI parsing, exact store/index/slabs, lease/projection views, rotating-cache boundaries, Metal smoke, tensor components, model sequence/decode, tiny checkpoint/tokenizer generation and attention parity |
| ASan/UBSan owner checks | owner-final-macos.log; message parsing, expert store/cache and MLX lease/projection views pass; owner C/C++ code is instrumented, external retained MLX libraries are not |
| Fresh source/data subset outside both repositories | standalone-macos.log; engine/packer,18 supported builds, same9 native runs and3 sanitizer runs pass with explicit third-party MLX inputs only |
| Default and exported relative model input | retired-builds-macos.log and relative-model-input-macos.log; original tiny-model fixture, not actual checkpoint qualification |
| Assembled Gateway quarantine and runtime configuration | gateway-macos.log; original incomplete-streaming rejection/complete-artifact availability and runtime-settings checks pass |
| Exact package hashes/sizes/version | package-macos.log |
| Clean native install/upgrade, no optional model fetch and unchanged legacy model | install-macos.log |
| Original implementation/test/fixture comparison | source-audit.txt, copied-files.json, audit-extraction.mjs and source-audit-result.txt;51 pins cover all moved inputs and the existing owner cache header; original assertions preserved |

Native tensor assertions retain their original tolerances and produce the same
reference results. The full model-fixture sequence and two decode comparisons
remain within the original numerical thresholds. They are synthetic/reference
checks; they are not evidence of full real-model quality, long-lived memory,
actual application readiness or current real-checkpoint token parity.

## Inherited scratch diagnostics

An audit compiling all20 files found two pre-existing failures. Original parent
test_maple_gate_c_debug.cpp references removed word_embeddings_/attention-mask
helpers; test_maple_raw_parity.cpp calls an obsolete string-taking MapleModel
constructor. Both failures reproduce against the unchanged parent baseline:
baseline-legacy-debug-macos.log and baseline-legacy-raw-parity-macos.log.
Initial owner attempts are owner-macos.log and all-test-build-audit-macos.log.

Neither diagnostic belonged to the original supported native Make targets.
Their source and assertions remain preserved, and explicit build targets still
reproduce the failures. maple-test-builds covers the18 compilable programs;
the two inherited scratch programs are listed in MAPLE_LEGACY_DIAGNOSTICS.
No model implementation was changed to revive those private debugging APIs.
The old tokenizer scratch program's printed100/100 counter is also not an
accuracy gate: it does not compare expected token IDs. It is compiled only.

Inherited source whitespace and exact raw compiler/diff logs are retained for
source fidelity. Existing MLX C++20-extension/deployment-target warnings are
also present in the original C++17 profile. This26.5.1 qualification does not
establish support on older macOS or another OS.

## Reproduction

From Chat, with the existing native MLX build (no rebuild required):

```sh
./tools/without_python.sh make samosa-maple maple-pack maple-mlx-smoke test-maple-openai-messages test-maple-expert-store test-maple-expert-views test-maple-cache-boundary test-maple-native test-maple test_attn_parity
./tools/without_python.sh make test-maple-streaming-quarantine test-runtime-settings
./tools/without_python.sh sh tests/test_package_pdfium.sh
./tools/without_python.sh sh tests/test_runtime_only_release.sh
./tools/without_python.sh node components/samosa-inference/docs/qualification/life07i-maple-sources/audit-extraction.mjs
```

From this repository, or the source/data subset recorded in standalone-source.txt:

```sh
make maple-engine maple-pack maple-test-builds test-maple-core test-maple-tensors sanitize-maple-core MLX_DIR=/absolute/mlx-checkout MLX_BUILD_DIR=/absolute/mlx-build
```

source-files.sha256 pins maintained source/test/assembly inputs; the independent
source subset contains only the owner Makefile/license, required native/shared
engines, moved test sources and frozen data. native-dependencies.sha256 pins
the retained third-party native build/kernel and baseline engine. No parent
product implementation is compiled by that isolated build. Larger independent
release/registry qualification remains open.

## Remaining work

LIFE-07j moves remaining primary-backend launch/group/reap/probe ownership,
preserving existing Qwen/Maple/llama flags, loopback/environment/log behavior,
PID markers, persisted selection and runtime settings. Reuse the existing
managed owner where contracts match; retain model selection as caller policy.
Guarded legacy Whisper and shared turn/admission coordination follow.

No actual Maple checkpoint remains installed after the earlier authorized model
cleanup. This extraction did not download one or rerun real-checkpoint parity,
packing, production lifecycle or catalogue qualification. Existing safety and
quarantine behavior remains unchanged. Historical Maple results are not recast
as a current area3 or release pass.

Full area3, all later Inference areas, project-wide Python removal (including
old Maple drivers/generators), Agent/App, pinned assembly and qualified monolith
retirement remain open. Inkling-2-Mini remains a post-refactor catalogue follow-up.
