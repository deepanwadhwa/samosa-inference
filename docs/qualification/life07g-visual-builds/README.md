# LIFE-07g: consolidate visual engine source and build ownership

Qualified 2026-11-21 on macOS26.5.1 ARM64. Baselines: Chat d236548,
Inference056578e. Extract the working implementation; no model redesign,
Python, MLX rebuild, model download, Docker/VM or Linux work.

Chat's native VisionPsy/Molmo2 builds now delegate to this repository's existing
engines. The original compiler options, executable/object names, optional JACCL
link and Mac media frameworks are retained. The explicit development dependency
is the existing matching MLX source/build pair; the independent pinned release
builders are unchanged. Component targets can build outside Chat with those
declared third-party inputs. They do not read parent implementation files.

The original VisionPsy component and real-checkpoint tests and molmo2_pack.cpp
now live here byte-for-byte. The two original Molmo component tests already
matched this repository. No assertions were removed. Chat retains useful build
commands, its PDF renderer and original assembled workflows. Packaging obtains
VisionPsy from the owner while retaining its existing staged filenames; the
source/manifest test now checks that exact owner source.

After verification,18 byte-identical parent files (3884 lines) were retired:
all13 visual engine source/headers, the4 component/real test copies and the
packer copy. source-audit.txt records SHA-256, sizes and each comparison before
deletion. Parent shared support headers remain required by other engines.
engines/json.h retains only the previously documented named Unicode-buffer
portability adaptation; see json-portability.diff. No new engine algorithm or
second implementation was introduced.

## Evidence

All final commands returned0 under the PATH-based no-Python guard. The guard
also detects suppressed PATH interpreter attempts; it is not an absolute-path
interpreter sandbox. The recipes and moved native code were reviewed too.

| Gate | Evidence |
| --- | --- |
| Delegated native builds, packer/model compile and original component tests | delegated-builds-macos.log |
| Fresh source subset outside both repositories, both engines/packer/model compile, original Molmo tests normal and ASan/UBSan, original real-test compilation | standalone-builds-macos.log; standalone-source.txt records the local subset |
| Original parent real-checkpoint test before replacement | baseline-vision-direct-macos.log; original heading/processor/1536-tier/RSS assertions pass |
| Delegated original real-checkpoint gate, including native PDF rendering | delegated-vision-real-macos.log; heading/processor/1536-tier/RSS assertions pass |
| Rebuilt VisionPsy behind existing owner, normal and ASan/UBSan owner harnesses | actual-vision-host-final-macos.log and actual-vision-host-sanitize-macos.log;15868 checks each, heading and changed corner both yield helloworld, same guardian/revision, cleanup |
| Rebuilt Molmo2 behind existing owner, normal and ASan/UBSan owner harnesses | actual-temporal-host-macos.log and actual-temporal-host-sanitize-macos.log;319 checks each, red/blue prepared frames change observations on one guardian/revision, cleanup |
| Original gateway Molmo and full VisionPsy workflows | gateway-macos.log; component/UI/authenticated trace/image-vs-text/multi-page/partial-recovery assertions pass |
| Exact staged sources, hashes/sizes and version | package-macos.log and package-after-retirement-macos.log |
| Clean native runtime installation and upgrade, no model download, original legacy model unchanged | install-macos.log |
| Retired paths no longer required by native build recipes | retired-paths-make-dry-run.log; read-only Makefile/tools/dist/tests/workflow reference scan found no remaining removed implementation references |

Actual runtime binaries are private APFS clones of the newly compiled owner
binaries and their matching existing MLX kernel library; see actual-runtime.txt
and native-dependencies.sha256. Models/heading input reuse the private retained
candidate recorded in ../life07c-visual-residency/actual-source.txt and its
artifact hashes. Installed models and user activation/conversations were not
changed. The actual engine processes are native release-style builds; only the
named owner harnesses and portable Molmo component tests are sanitizer builds.

The initial baseline real target failed before model execution because the local
build/samosa-extract could not find its PDFium dylib. Its original compiled test
then passed directly with the retained rendered fixture. A private clone of the
installed native PDFium library restored build/libpdfium.dylib; the delegated
full PDF/render/real gate subsequently passed. See baseline-vision-real-macos.log
for the excluded failed attempt. An initial owner invocation used a relative
argv[0], which its existing harness rejects; actual-vision-host-macos.log is that
excluded attempt and actual-vision-host-final-macos.log is the corrected run.
Existing MLX26.5.1 vs26.0 linker warnings do not qualify older macOS versions.

## Reproduction

From Chat, with the existing native MLX build and retained models available:

```sh
./tools/without_python.sh make samosa-visionpsy samosa-molmo2 molmo2-pack molmo2-model-compile test-visionpsy-components test-molmo2-contract test-molmo2-processor
./tools/without_python.sh make test-visionpsy-real VISIONPSY_MODEL_DIR=/absolute/retained/vision-model
./tools/without_python.sh make test-molmo2-gateway test-visionpsy
./tools/without_python.sh sh tests/test_package_pdfium.sh
./tools/without_python.sh sh tests/test_runtime_only_release.sh
```

From a separate copy of this repository's Makefile, engines, moved tests/tool
and license (no Chat implementation files), supply native third-party paths:

```sh
make vision-engine molmo2-engine molmo2-pack molmo2-model-compile test-molmo-engine-components sanitize-molmo-engine-components vision-engine-real-check MLX_DIR=/absolute/mlx-checkout MLX_BUILD_DIR=/absolute/mlx-build
```

Run actual GPU checks sequentially, using absolute harness/engine paths and
the paired kernel library as described in engines/README.md. The existing
visual-owner harness command is `test-visual-host METADATA ENGINE MODEL vision
HEADING.ppm` or `test-visual-host METADATA ENGINE MODEL temporal`; substitute
test-visual-host-sanitize for the sanitizer owner. Exact local inputs are
recorded above. Full source/release qualification remains larger than this
fresh native source subset.

## Remaining scope

This finishes visual engine source/build consolidation, not Inference area3.
Primary Qwen/Maple engine source/build and launch ownership, guarded legacy
Whisper execution, shared turn/admission coordination and complete supported
lifecycle qualification remain open. No fresh full native-video/media or
multi-gigabyte packing conversion qualification is claimed: those algorithms
are unchanged; existing gateway workflow and actual prepared-frame behavior
are the execution evidence here. Independent release builders were not rerun.
All later Inference areas, project-wide Python removal, Agent/App, pinned
assembly and qualified monolith retirement remain required. Inkling-2-Mini
stays a post-refactor catalogue follow-up.
