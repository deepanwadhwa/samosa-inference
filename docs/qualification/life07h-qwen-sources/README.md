# LIFE-07h: extract the existing Qwen native engine

Qualified 2026-11-21 on macOS26.5.1 ARM64. Baselines: Chat4cb5c9c,
Inferenceecf729e. Extract/reuse the working model, cache and kernels; no Python,
new algorithms, conversion/download, Docker/VM or Linux qualification.

The original qwen36b.c, expert_cache.c, vision.c and private support headers
now belong to Inference/engines. All11 copied engine/support files match the
baseline byte-for-byte; source-audit.txt records their hashes/sizes. Existing
Inherited trailing spaces in qwen36b.c/vision.c/vision.h are deliberately retained
to keep that exact source comparison; raw command/diff evidence also stays exact.
owner tokenizer/image headers match the parent; engines/json.h retains only
the previously documented named Unicode-buffer portability adaptation.
engines/CHAT_NOTICE preserves the complete baseline attribution notice, and
the repository retains the Apache license.

Chat delegates standard CPU, OpenMP and separate experimental runtime-schedule
builds to owner targets with the original compiler/link options and filenames.
No default algorithm, cache policy, model format, runtime flag or environment
behavior changes. Maple compiles the owner cache source with its existing flags.
The parent cache header remains temporarily for Maple and is identical to the
owner header; its removal belongs with Maple extraction. Gateway still uses its
parent HTTP header; the engine's copied header is identical.

The five original native suites and original legacy base64 image smoke test
move here. Tests retain every original assertion. Serve-test changes are only
the owner include and explicit caller App asset paths; the original model-context
fixture also moves here. Whole-file comparison proves those exact substitutions
(serve-test-paths.diff). The legacy decoder changes only its include path.
The optional serve test takes app.html/logo as declared test data, not hidden
parent implementation dependencies. Native Qwen builds need no App checkout.

After source and behavior verification,16 parent source/test/fixture copies
(10297 lines) were retired; see retirement.txt. Packaging now sources the exact
owner files under the same staged engine filenames. The manifest/source test
checks these bytes. The code-read tool fixture and active source/attribution
references were updated to the moved source path. Historical records remain
historical.

## Evidence

Every command below finished0 under tools/without_python.sh. It refuses and
records PATH interpreter attempts, including suppressed calls; it is not an
absolute-path interpreter sandbox. Moved source and build recipes contain no
Python invocation.

| Gate | Evidence |
| --- | --- |
| Original OpenMP build, five native suites and legacy decoder before extraction | baseline-omp-macos.log, baseline-components-macos.log, baseline-vision-decoder-macos.log |
| Delegated CPU/OpenMP/runtime-schedule builds, original suites | delegated-builds-macos.log; retired-components-final-macos.log also passes after parent removal |
| Original suites with ASan/UBSan | sanitize-components-macos.log; standalone-vision-final-macos.log additionally covers legacy decoder normal/SAN |
| Fresh source subset outside both repositories, CPU/OpenMP/runtime-schedule, original native tests normal/SAN | standalone-builds-macos.log, standalone-vision-final-macos.log, standalone-final-macos.log; standalone-source.txt identifies the private source subset and explicit copied App test data |
| Actual installed-model clone, original vs newly built owner OpenMP CLI | baseline-actual-macos.log, owner-actual-macos.log, actual-comparison.txt |
| Remaining Maple consumer | maple-consumer-macos.log; unchanged Maple engine and packer build, expert-store exact-slab/index tests pass using the owner cache implementation |
| Gateway runtime configuration and delegated original tests | gateway-runtime-macos.log |
| Exact owner package sources, sizes/hashes and baked version after retirement | package-after-retirement-macos.log |
| Clean native install/upgrade, no optional model downloads and untouched legacy model | install-after-retirement-macos.log |
| Removed paths no longer required by supported native recipes | retired-paths-make-dry-run.log; Makefile/tools/dist/tests/workflow implementation-reference scan found no removed native build/include dependency |

Cache tests retain16807 exhaustive traces and50000 random transitions; kernel
tests retain grouped Q4/mixed Q8-down checks. Serve tests retain context limits,
session sealing/compaction, resident eviction, scheduler, stream metadata and
real local HTTP/static/cancel/shutdown checks. They use tiny native fixtures;
they are not full-model quality evidence.

The actual CLI pair uses the same private APFS model clones, two OpenMP threads,
512-token configured context and original four-token direct-mode request.
Both emit `Hello! How can`,15 prompt/4 generated tokens, the same default16-slot
2.07-GB cache policy and zero swap growth. Both terminate0 under the existing
6144-MiB process-tree RSS guard. Native engine peak RSS is3.60/3.41GB; sampled
guard peaks are3263/3380MiB. Timing/RSS differences are observations, not a
performance claim. This short compatibility check does not qualify general
quality, long-context behavior or common lifecycle readiness.

actual-source.txt records the private model/runtime directory. Installed model
and user conversation/activation files were not modified. actual-artifacts.sha256
pins all six retained model files and both binaries using streaming hashes;
source-files.sha256 pins maintained implementation/test/assembly inputs.
The actual engine processes are ordinary native OpenMP builds, not sanitizer
engines. Existing MLX26.5.1-vs26.0 linker warnings in the Maple build do not
qualify older macOS targets.

## Reproduction

From Chat, with existing native libomp available:

```sh
./tools/without_python.sh make samosa-engine omp omp-sched-runtime test-qwen-engine-components test-runtime-settings
./tools/without_python.sh make samosa-maple maple-pack test-maple-expert-store
./tools/without_python.sh sh tests/test_package_pdfium.sh
./tools/without_python.sh sh tests/test_runtime_only_release.sh
```

From Inference or a separate source subset with only its Makefile, license,
Qwen/support engines, moved tests/fixture and explicit caller App test assets:

```sh
make qwen-engine qwen-omp qwen-omp-sched-runtime
make test-qwen-engine-core sanitize-qwen-engine-core
make test-qwen-engine-serve sanitize-qwen-engine-serve QWEN_TEST_ASSETS=/absolute/app-assets
```

The retained actual check (run sequentially, under the existing guard) is:

```sh
SNAP="$candidate/model" TOKENIZER="$candidate/model/tokenizer_qwen36.json" SAMOSA_CHATS_DIR="$candidate/chats" OMP_NUM_THREADS=2 SAMOSA_CONTEXT_TOKENS=512 MAX_FOOTPRINT_MB=6144 MAX_SWAP_DELTA_MB=0 sh tools/run_memory_guarded.sh "$candidate/qwen36b" --chat 'Say hello.' --tokens 4 --no-thinking
```

## Remaining scope

Next: move the original Maple sources/builds/useful tests, then remaining primary
backend launch/group/reap/probe and public lifecycle wiring. Gateway still owns
backend selection/PID and persists original settings. This is source ownership,
not closure of the primary backend or any full area3 requirement. The inherited
Metal experiment targets already lack tools/metal_spike.m and src/metal_expert.m;
they were not repaired or executed. The old Docker recipe remains deferred and
unqualified, including its legacy Python path. No Docker process was started.

Full independent source/release qualification exceeds this native source subset.
Whisper guardian consolidation, shared turn/admission coordination, all later
Inference areas, whole-project Python removal, Agent/App, pinned assembly and
qualified monolith retirement remain open. Inkling-2-Mini remains a post-refactor
catalogue follow-up.
