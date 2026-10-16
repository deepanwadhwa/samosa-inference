# Decision execution asset binding

`src/decision_assets.{h,c}` supplies the asset checks required before the public
decision adapter can launch its worker. It reuses controlled artifact hashing,
the host native-library parser and the closed loader graph checker. It does not
install models, publish catalogs, start an engine or claim model readiness.

The private execution root contains exactly four regular files:

- `decision-engine`
- `libonnxruntime.1.27.0.dylib` on macOS, or `libonnxruntime.so.1.27.0` on Linux
- `model.onnx`
- `tokenizer.json`

The caller provides their four SHA-256 pins in that order. The gate holds the
root and file descriptors and records inode, ownership, permissions, size and
timestamps. The root must be private and owned; files must be single-link,
owned and non-writable by group/others; the engine must be executable. Symlinks,
extra entries, missing files and pin mismatches are refused. Byte ceilings are
64 MiB for engine/runtime, 1 GiB for the model and 16 MiB for the tokenizer.

The engine must explicitly depend on the indexed ORT runtime. Both native
images must satisfy the host architecture/minimum-OS policy and resolve direct
non-system loader dependencies within this root. OS runtime libraries remain
the existing declared trust boundary. The gate does not claim arbitrary native
code cannot open other files dynamically.

Rechecking compares held files with their named entries and original receipts,
rehashes them under request controls and verifies the closed namespace again.
Cancellation is checked during file reads and graph/namespace inspection.
Failed opens preserve the caller's output and close temporary descriptors.
Closing a successful handle is repeatable. Directory enumeration opens an
independent file description so repeated checks cannot silently inherit EOF.

Worker builds now use `@loader_path` on macOS and `$ORIGIN` on Linux for the ORT
search path. Place the pinned runtime beside the executable before launching.
This removes the earlier worker prototype's absolute build-machine search path.
`DECISION_ORT_RPATH` can select a QA override, but the asset gate still refuses
an unqualified search profile.

## Evidence and remaining integration

Normal and ASan/UBSan checks passed 94 assertions on macOS ARM64 / Apple Clang 21
and Ubuntu 24.04 ARM64 / GCC 13.3.0. These synthetic native-image fixtures reuse
the existing loader fixtures and prove refusal, not model behavior. GCC also
exposed an existing misleading one-line loop in the shared graph helper; its
formatting was corrected without changing behavior. The existing library
runtime suite passed all 4,431 checks in normal and sanitizer builds on macOS.

The actual macOS worker, existing ORT runtime, converted DeBERTa model and
effective tokenizer passed the static asset gate from a distinct private root.
The worker linked with `@loader_path` then passed all ten actual-model cases,
request-error recovery and clean EOF from that root. Maximum numeric result
error remained approximately `0.000583839146`. Its engine SHA-256 is
`fa5736d57166220d36c481e99615f84c8dd06764eae2f6679887123a38724877`;
runtime/model/tokenizer pins match the previous native qualification.

`make test-decision-assets sanitize-decision-assets` belongs to the shared
local/hosted gates. The normal target also compiles the explicit real-asset
harness: `build/test-decision-assets-real ROOT ENGINE_SHA RUNTIME_SHA MODEL_SHA
TOKENIZER_SHA`. The new binding/search-path changes require full shared CI
before pushing. The public adapter must still hold these assets through owned
execution, recheck before publishing, and use common operation/admission controls.
