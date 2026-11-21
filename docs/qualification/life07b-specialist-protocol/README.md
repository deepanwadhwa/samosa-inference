# LIFE-07b: extract the existing visual protocol operations

Assignment opened 2026-11-20; C03/C11/C13/C14. Native Mac only, Python-free.

Move Chat's existing `molmo2_session_analyze_request`,
`visionpsy_session_inspect` and their private encoding/numeric helpers into
`src/specialist_protocol.c`, published through
`include/samosa_inference/specialist_protocol.h`. The source baseline is Chat
commit 789a425. Reuse this component's already copied `engines/json.h` parser
instead of importing parent/private state or designing a new codec.

Preserve original framing, JSON fields/default prompts, Unicode replacement,
image/frame limits, numeric validation, error translation and trace events.
Replace the `Gateway *` dependency with explicit optional event/payload callbacks.
Chat keeps thin calls plus its routing/resource/product policy. Add the owner
object to its existing static archive and include the same files in native shell
release assembly and staged installation. Do not modify engine algorithms.

Acceptance: native public-owner tests with exact expected request bytes and
controlled independent worker replies, normal/SAN; existing supervisor/ownership
regressions; unchanged Chat Molmo gateway assertions and summary/runtime tests;
shell package manifest checks and a clean installed gateway/jobsd build on this
Mac. Public provider readiness/actual resident generation remain separate work.

## Native Mac qualification — 2026-11-20

Both original function bodies match Chat 789a425 after only public function/type
names, explicit trace callbacks and the equivalent local monotonic-clock helper
are substituted. `extraction-audit.txt` records that comparison. Encoding helpers
were copied from the gateway; the existing owned engine JSON parser is reused.
The gateway retains its small bounded-number helper because document selection
and product input validation also call it. An initial compile exposed those
remaining calls; retaining the helper fixed that extraction mistake.

Host: macOS 26.5.1 ARM64, Apple clang 21.0.0. All commands below terminated 0.

| Check | Command/scope | Evidence |
| --- | --- | --- |
| Public owner protocol normal/SAN | `make test-specialist-protocol sanitize-specialist-protocol`; 25 independent worker exchanges/refusals per build, exact expected wire bytes/defaults, Unicode, limits, failures and trace callbacks | `native-macos.log` |
| Fresh standalone source subset | Same protocol normal/SAN plus inherited supervisor normal/SAN, owned guardian normal/SAN and summary-process regression | `independent-macos.log`, `independent-source.txt` |
| Existing product behavior | From Chat: `make test-native-summarizer-supervisor test-runtime-settings test-molmo2-gateway` | `app-macos.log` |
| Source staging/version | From Chat: `sh tools/without_python.sh sh tests/test_package_pdfium.sh` | `package-selection-macos.log` |
| Clean installed gateway/jobsd/archive | From Chat: `sh tests/test_gateway_installer.sh` | `installer-macos.log` |
| Installation recovery | From Chat: `sh tests/test_atomic_install.sh`; intentional corrupted payload refused without replacing live release | `atomic-macos.log` |
| Shell/profile integration | From Chat: `sh tests/test_install_path.sh`; all six existing cases, including repeat installation | `path-macos.log` |
| Native HTTP fixture | From Chat: `sh tools/without_python.sh make test-fake-download-server` | `download-fixture-macos.log` |
| Real control-plane install/upgrade smoke | From Chat: `sh tools/without_python.sh sh tests/test_runtime_only_release.sh`; optional chat-model downloads absent, legacy model bytes/inode/mtime preserved | `runtime-only-macos.log` |

The independent subset includes only the component Makefile, native source/public
headers, the existing engine parser and selected native fixture/test files. It
builds a new static archive without parent or sibling sources. Its directory is
recorded in `independent-source.txt`. Exact owner and product source hashes are
in `source-sha256.txt` and `app-source-sha256.txt`.

EX-REL-02 replaces the former Python packager/selection-test with shell and
preserves their useful checks. Its first HTTP-fixture regression still executed
a second overlooked Python byte generator; that exploratory invocation is
excluded from Python-free evidence. Both byte generators are now shell/awk,
the native fixture and runtime-only test pass with PATH-based Python calls
refused/recorded, and source review finds no absolute interpreter calls in those
entry points. Other legacy Python tests/CI have not been migrated by this slice.

No model algorithms, wire formats, original operation deadlines or legacy product
assertions were changed. This qualifies extraction and native Mac assembly for
these protocols; it does not close public visual resident readiness, area 3,
complete Python removal, Inference release or the full refactor.

Next LIFE-07c: reuse the existing `vision.c` model verification/preparation and
`temporal.c` package/prepared-frame validation with the existing guardian workspace
and duplex owner, then qualify real generation before public resident readiness.
Inspect `run`/`model_verify`, `temporal_package` and the already qualified summary
host launch/activation adapter before adapting their ownership. Keep original
VisionPsy/Molmo2 model loops and App routing/resource policy intact.
