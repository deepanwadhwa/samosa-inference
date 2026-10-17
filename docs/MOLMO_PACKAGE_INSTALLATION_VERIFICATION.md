# Molmo2 installation verification

The static native compatibility gate reuses the existing temporal model verifier
and binds its receipt to the shared installation index. Catalog/setup dispatch
is connected. The corrected independent engine now passes full HTTPS
installation and installed video inference in normal and sanitizer-equipped
host builds on macOS 26.5.1 arm64. Combined public-registry execution also passes
in both modes. Earlier static checkpoints and failed runs remain below as
historical evidence. This closes this Molmo2 installation/execution slice, not
all of component 2, wider video quality, other platforms or engine lifecycle.

## Controlled checks

The descriptor/index binding gate passed 302 checks in both normal and
ASan/UBSan modes. The native compatibility gate passed 72 controlled refusal
checks per mode: independent policy, root identity, pin mismatch, missing kernel,
foreign symlink, cancellation, unchanged failure output and invalid native model
profile despite consistent file hashes. Existing temporal-input regression gates
also passed in both modes after making the native manifest parser's 8192-token
budget independent of unrelated build flags.

## Actual model package

`tests/molmo_package_candidate.py` copied the separately pinned existing native
engine, colocated MLX kernels and all files declared by the existing model
manifest into a private candidate. It verifies the originals before and after
copying and verifies destination hashes. No original Chat asset is changed.

The candidate has 29 files totaling 3553773738 bytes. The model manifest binds 24
files under the fixed model directory. The package passed the normal native
compatibility gate (11 harness checks), including the shared closed namespace,
actual host executable/library inspection, native model pre/post verification
and exact generic-index/native-manifest binding. The sanitizer-equipped actual
package check also passed (11 checks). These are static verification tests;
neither run loads the model or generates an observation.

| Artifact | SHA-256 |
| --- | --- |
| Complete package index | `92ad14eb6ca92d2540a920fa8a88392b1bc3419d6188be00eb4572fbf3be51f3` |
| Existing native engine | `c8aa53e06d30305f3077e260f5fbda258cd911941b109dfe18d286fc96d08a05` |
| Colocated MLX kernels | `846add228d562bb224b11f18c4655b75a05f3336e543654ce49943577e26cd5a` |
| Native model manifest | `a07230dd952e97969921223328e7812020bb58711728450cdfb2205e3a15b770` |

Run `test-molmo-package --real ROOT INDEX INDEX_PIN ENGINE_PIN KERNEL_PIN
MANIFEST_PIN` for actual static admission. The candidate reports observed engine
minimum OS 26.0; testing occurred on macOS 26.5.1 arm64. This establishes static
compatibility on that host, not execution on macOS 26.0 or another platform,
loaded model readiness or generated-video quality. Python is used only by the QA
packager; this native gate adds no Python runtime dependency.

## Independent engine build

The ten files in `engines/molmo2/` are byte-for-byte copies of Chat's current
native model, processor, contract, media helpers and framed engine. The copy
retains the existing prepared-frame protocol needed by the temporal adapter.
The source entry point has SHA-256
`f1081b4e8aac0e780b985e5c7351a8ea17f2f833e14660dee8552998500fd4a7`.
Chat's working tree, including its uncommitted prepared-frame changes, was not
modified. Shared engine support headers matched their original bytes at
extraction; the JSON header subsequently received the GCC portability fix below.

`tools/build_mlx_runtime.sh` now holds the common pinned source/dependency build
previously embedded in the VisionPsy script. Each engine wrapper invokes it
independently; building Molmo2 does not require building VisionPsy. It verifies
the same clean MLX/fmt revisions and json/metal-cpp archive checksums, disables
FetchContent network access, builds in exclusive private output and copies
dependency licenses. The compiled kernel fallback remains disabled. Build
inputs and commands are in [engine documentation](../engines/README.md#molmo2).

A fresh `build_molmo_runtime.sh` run completed MLX compilation and native Molmo2
linking on macOS 26.5.1 arm64. The engine SHA-256 is
`4fe26c8d7626078ece6e599bfe85d9cb3926f1c1693d9f9bf93553ad473fe809`.
Its colocated kernels match the kernel pin above. Inspection found only system
libraries/frameworks; minimum OS metadata is 26.5.1, matching the explicit host
deployment profile. No earlier macOS or Linux model execution is established.
Actual installed inference of this new engine is still required; the earlier
static package evidence uses the separately identified existing engine.

The extracted sources passed C++ syntax checking. The existing native contract
and processor tests were copied unchanged and passed in normal and ASan/UBSan
modes through `make test-molmo-engine-components sanitize-molmo-engine-components`.
They exercise tensor mapping/shape rules, prepared image/video patches,
timestamps, frame limits and context/output bounds without loading a model.
The shared build helper and both engine wrappers also refused a corrupt
dependency archive before creating an output directory.
The refactored VisionPsy link step also passed using this freshly built shared
MLX output; that link check does not replace its separately pinned actual-model
qualification evidence.

## C++ compiler portability

Ubuntu CI for `e5e6a37` rejected two C-style temporary-array expressions in the
reused JSON header when compiling the newly carried native tests with GCC.
The Unicode decoder now uses named local character arrays for the same four
hex digits and terminator. The engine/model sources remain unchanged.
Strict C++20 compilation with pedantic errors and Unicode key, BMP character,
surrogate-pair and escaped-newline checks passed locally; the checks also passed
with ASan/UBSan. Existing native contract/processor tests passed in both modes.
Commit `7a19b62` subsequently passed hosted macOS and Ubuntu CI. The full
shared native suite also passed locally on macOS 26.5.1 arm64 and Ubuntu 24.04
arm64, including GCC 13 C++ component checks and normal/sanitizer modes.
Hosted Ubuntu additionally covers its configured x86_64 runner.

Before the portability change, the independent-engine candidate also passed
normal and sanitizer-equipped static admission (11 checks each): 29 files,
3553773740 bytes, index
`86ba886ffd61815eecb1a6e848475cba46ce2054916891c88bd4fb2d54f06b72`.
This records compatibility for the `4fe26c8d...` engine, not installed inference.
Relinking the corrected source into a separate output produced engine SHA-256
`9a6a718e31d73c006be15f9266996286214f7f492c380d1b09097006d61b87af`.
The prior pinned executable was preserved. The corrected binary requires its
own package qualification; previous pins must not be silently reused.

## Installed execution failure

The existing-engine candidate completed verified HTTPS acquisition, native
compatibility admission and publication in the installation harness. Installed
actual video inference then failed with `provider_process_failed`; the actual
sanitizer execution gate was not reached. This is a failed qualification, not
an installation/readiness success.

A separate diagnostic guardian, retaining the production limits, observed the
engine terminate with macOS signal 25 (`SIGXFSZ`). The legacy workspace guardian
sets `RLIMIT_FSIZE` to 67108864 bytes for every regular file written by the
engine, including runtime temporary files. Identifying the offending write and
qualifying a correction remain required. Diagnostic executables are temporary
QA tools and are not package artifacts or evidence of successful inference.
The independently built corrected engine also still needs its own complete
installed execution qualification.

A subsequent diagnostic run of the existing engine passed the public two-frame
actual-model harness (524671 checks) under the unchanged file cap, without an
oversize write reported by the temporary tracer. A minimal Metal library-load
probe also succeeded with that cap. These results do not reproduce or explain
the failed installed run, and neither qualifies the independent engine package.
The required next evidence is complete installed execution using the production
provider, followed by resolving any remaining cold-start failure.

## Corrected independent package

The separately packaged `9a6a718e...` engine passed normal and
sanitizer-equipped actual static admission (11 checks each). Its 29-file index
is `4e2431487e934bd9c124cff3e3c018b335011fd18472ad9463e62f3dbaa0b520`;
the manifest and colocated kernel pins are unchanged. At this static checkpoint, installed execution remained a separate required
check; its completed qualification is recorded below.

The shared local/hosted check entry point introduced in `55ba1b0` passed its
complete local macOS arm64 and Ubuntu arm64 runs before push, then passed both
hosted matrix jobs in run `38802304062`. These controlled checks compile the
actual-model harnesses; actual model execution requires the additional pinned
artifacts described above.

## Independent installed failure and identified write

The corrected independent engine (`9a6a718e...`, index `4e243148...`) also
completed the full normal HTTPS installation, offline/last-good and removal/
reinstall sequence, then failed actual installed temporal inference with
`provider_process_failed`. The sanitizer execution loop did not start after
this failure. Successful static compatibility does not establish execution.

The QA guardian with unchanged limits and a smoke-tested temporary write tracer
reproduced `SIGXFSZ` and captured the triggering operation: `ftruncate` of the
macOS shared Metal cache `com.apple.metal/32024/libraries.data` to 211812352
bytes. The file was already 211746816 bytes, above the guardian's 67108864-byte
limit, before the attempted growth. The checked original and independent builds
use the same pinned MLX revision and identical kernel bytes. This is not evidence
of an engine/kernel mismatch.

The required correction is to bound inference output independently from Metal's
shared cache files; a larger blanket limit would remain sensitive to unrelated
cache growth. Production correction, controlled output/cleanup regression tests,
and complete installed normal/sanitizer execution were required next. No OS cache was
manually cleared or edited.

## Guardian output capture correction

Image and temporal adapters now use a dedicated stdout pipe. The existing
process guardian copies at most 32768 bytes into the retained private result
file and reports `protocol_limit` on overflow; adapters preserve their public
`invalid_provider_response` refusal. Stderr has its existing separate log cap.
The bridge still execs the native engine, preserving direct-child reaping,
process-group ownership, deadlines, cancellation and requester-death cleanup.
The captured mode only accepts the private bounded bridge command and closes
the result descriptor across engine exec. Core dumps are disabled. Legacy
workspace, summary and sized-protocol file limits remain unchanged. The GPU
mode permits normal OS-managed Metal cache writes independently of output.

Normal and ASan/UBSan controlled image tests passed 661/671 checks, temporal
checks passed 1052/1058 checks, and direct guardian tests passed 187 checks per
mode. Both adapter fixtures grow an existing sparse 128 MiB cache file by 64 KiB
while producing a valid response. Existing oversized-result, model mutation,
cancellation, deadline and requester-death cleanup cases remain covered.
Direct guardian tests also check exact result-file bounds during a 128 KiB
stdout flood, draining buffered output after child exit, and refusal of an
arbitrary command attempting captured mode. Temporal checks use the original
pinned model metadata with explicitly synthetic weights and engine; these are
control tests, not model execution qualification. The subsequent full local CI
and actual installed independent-engine results follow.

## Completed independent installed qualification

Source `c26bee7` passed the full shared local CI script in clean macOS arm64 and
Ubuntu 24.04 arm64 exports before push. macOS used Apple Clang 21; Ubuntu used
Clang 18 and GCC 13 for its C++ component checks. Both runs include normal and
ASan/UBSan checks. Hosted runner qualification remains a separate exact-commit
gate, including its Ubuntu x86_64 architecture.

The corrected independent engine package has 29 files totaling 3553773748 bytes,
index `4e2431487e934bd9c124cff3e3c018b335011fd18472ad9463e62f3dbaa0b520`,
engine `9a6a718e31d73c006be15f9266996286214f7f492c380d1b09097006d61b87af`,
and the kernel/manifest pins recorded above. Both normal and sanitizer-equipped
setup/provider/harness runs passed full HTTPS acquisition, policy/TLS refusals,
publication, last-good preservation, offline status/resume, completed and
incomplete removal, repeated removal and cache-only reinstall. Each then passed
524671 actual public temporal harness checks using freshly installed inodes.
The real observation described the red-to-blue order; pixel/PPM/timestamp and
engine/model provenance, foreign-owner refusal and exact runtime-free replay
were checked. The two-frame fixture is not a general video-quality benchmark.

Both combined-provider runs additionally passed 524778 checks through the
public registry using this same independent engine/model package. The accompanying
fake text provider supplies discovery evidence only. Private receipt directories:

- Normal: `/private/tmp/samosa-molmo-captured-registry-7c942eb6149b47bfa84309bfb201e2ae`
- Sanitizer: `/private/tmp/samosa-molmo-captured-registry-sanitize-f7de86f7e01c4cec8ca947580d671bc4`

Installation command: `python3 tests/llama_setup_https.py SETUP ROOT INDEX
INDEX_PIN --engine-pin ENGINE_PIN --kernels-pin KERNEL_PIN --manifest-pin
MANIFEST_PIN --temporal-cli TEMPORAL_CLI --temporal-test ACTUAL_TEMPORAL_HARNESS`.
For sanitizer execution, use each host executable's `-sanitize` build. Registry
command: `SAMOSA_TEST_USE_REGISTRY=1 ACTUAL_TEMPORAL_HARNESS PROVIDERS_CLI
ENGINE MODEL_DIRECTORY NEW_PRIVATE_DIRECTORY`.

The engine and MLX themselves are the pinned release builds, without sanitizer
instrumentation; host setup/provider/guardian and harness checks are instrumented
in sanitizer mode. Observed engine minimum OS and actual execution host are
macOS 26.5.1 arm64. No earlier macOS or Linux model execution is established.
Original Chat assets were preserved; temporary installed copies were removed by
the successful harness. Production catalog/distribution and the OpenDecision
provider/package prerequisite remain open before component 2 can close.

## Production distribution candidate

`tools/prepare_molmo_catalog.py` reads the retained native manifest, all existing
small converted shards, corrected independent engine and colocated kernels in
place. It reuses common release metadata preparation without copying or
repacking weights. Full original upstream card, standard Apache2.0 terms,
conversion provenance, native dependency licenses and stb_image notice accompany
the existing model/engine descriptor.

The37-file candidate has3553820519 payload bytes and index
`ea12e2c91353f0098e846bace435d69830a53e85b8c5f7596f1ca9abfeefff6a`.
The engine/kernel/native-manifest pins above are unchanged. Exact installed
normal/SAN full HTTPS gates pass, with524671 existing actual temporal checks per
mode, including foreign-owner refusal and exact runtime-free replay. These are
bounded two-frame ordering checks, not a general video benchmark. Logs:
`/private/tmp/samosa-molmo-distribution-v1-{normal,sanitize}.log`.

All38 unique release assets passed streaming download-back hashes before
publication. Existing small shards fit the current uploader; no new splitting,
archive unpacker, downloader or inference implementation is introduced. The
prerelease is published; cold published-URL/common-registry qualification and the
production catalog entry remain pending. Actual scope is macOS26.5.1 arm64 only.
