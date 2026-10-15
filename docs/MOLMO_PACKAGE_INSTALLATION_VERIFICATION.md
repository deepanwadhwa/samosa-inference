# Molmo2 installation verification

The static native compatibility gate reuses the existing temporal model verifier
and binds its receipt to the shared installation index. Catalog/setup dispatch
is connected and an independent engine build passes. Actual HTTPS installation
and installed video inference qualification remain open.
This document records evidence for component 2 without claiming that the full
installation slice or later engine lifecycle work is complete.

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
