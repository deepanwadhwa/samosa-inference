# Native speech loader compatibility checkpoint

2026-10-13, macOS ARM64. The private native image parser/closed-package graph
checker advances speech integration; public routing, owned guardian execution,
actual Kokoro/Linux model qualification and the seven-area release remain open.

Commands `make test-library-runtime sanitize-library-runtime` pass 4,360 checks
each. The parser handles thin 64-bit little-endian Mach-O dylibs and ELF shared
images with ARM64/x86-64 policies. It checks CPU, type, bounded load/program/
dynamic tables, trusted loader search profile, Mach-O minimum macOS version and
ELF GLIBC version requirements. Specific corrupt tables, excessive lengths,
foreign rpaths, injected dyld environment, ELF audit/filter/text relocation tags,
malformed version requirements, older hosts and unknown required Mach-O commands
refuse. A 1,024-case byte-mutation and 384-case truncation corpus runs under
sanitizers; this is controlled robustness coverage, not a general binary verifier.

`make test-speech-loader sanitize-speech-loader` passes 2,713 checks each, using
explicit private policies for Mach-O/ELF on both architectures. The host factory
chooses the actual compiled architecture and observed macOS/glibc version.
The graph checks every pinned library/dependency, even unreferenced aliases;
resolves package edges to same-directory pinned native entries; compares Mach-O
compatibility requirements; refuses missing/transitive foreign edges, path
traversal, wrong architecture, metadata masquerading as a dependency, old hosts,
ELF package shadows of system names and unsafe loader environments. Package
identity/content rechecks surround inspection; cancellation/deadlines propagate.
The existing engine runtime gate still passes 87 checks. Native speech gates
now pass 324 checks normally/sanitized, including explicit incompatible Sherpa
reported ABI refusal before model initialization.

Actual `test-speech-real --package` and its sanitized build passed the loader
checks before opening the observed 18-file Pocket package, then passed both
voices, partial-output cancellation, same-runtime recovery and post-use package
recheck. The package pin/model identity and PCM hashes are unchanged from
[the prior package gate](SPEECH_PACKAGE_VERIFICATION.md). All four native libraries
(C API, C++ API and both ORT filenames) are inspected. External Sherpa/ORT code
is not sanitizer-built. `SherpaOnnxGetVersionStr` is now required to report
exactly `1.13.4`, matching the locally owned struct ABI.

Production profile: macOS supports only a package-local `@loader_path` rpath and
same-directory `@loader_path/NAME` or `@rpath/NAME` dependencies; OS absolute
`/usr/lib/` and `/System/Library/` paths are trusted. ELF permits one `$ORIGIN`
rpath/runpath and same-directory dependency names, plus the existing engine OS
library allowlist. Other search layouts fail explicitly instead of searching
ambient paths. Mach-O install names cannot claim arbitrary absolute OS identities;
ELF sonames cannot claim trusted OS names. Fat images, alternate platforms,
32-bit/big-endian images, arm64e and non-glibc Linux hosts are unsupported here.

This establishes the declared native graph for this loader profile. It does not
constrain arbitrary native code dynamically opening files, prevent concurrent
package writers between observations, prove C++ symbol-version availability on
all Linux hosts, or remove already-loaded-library/interposition risks in a reused
host process. Dedicated guardian execution, package staging and actual load/model
qualification remain required before public activation. The observed actual
macOS package proves one host works; synthetic ELF parsing does not prove an
actual Linux model works.

The independent repository now has a pinned-checkout CI workflow which exports
its own `git archive` and runs native normal/sanitizer, combined registry and
controlled adapter gates on macOS/Linux. Actual-model and temporal package gates
remain separate. The local Docker daemon still returns HTTP500; no local Linux
pass is claimed. Hosted CI results will be recorded from their actual run state.

The complete indexed source was independently exported to
`/private/tmp/samosa-inference-speech-loader-independent.jier4t4g`. Image parsing
(4,360), speech graph (2,713), speech runtime (324) and original engine loader
(87) each pass normal and sanitizer gates there. Both actual pinned-package
speech runs also pass after rebuilding all source in that export. No parent
source path or prebuilt inference executable was used.

The first independent CI run (38453311974) failed rather than silently skipping
platform checks: macOS14/Xcode15.4 did not expose per-thread locale declarations
from `locale.h`, and Ubuntu24.04/x86-64 executable publication only had an ARM64
branch. The follow-up includes `xlocale.h` on macOS in the existing text/embedding
adapters, supports native x86-64 executable publication, and extends generic
system-loader parsing to both architectures with the matching ELF interpreter.
Wrong-architecture publication still refuses without changing permissions.
Local native install normal/sanitizer passes 267/269 checks; engine-loader
normal/sanitizer passes 99. Existing registry and controlled native text/acquisition
CLI gates pass after the portability changes. Hosted results remain pending.

CI run 38453770595 passes every configured macOS14 job step, including the clean
export, core/speech normal/sanitizer, combined registry/CLI normal/sanitizer and
controlled text/acquisition/summary/Whisper/vision gates. Linux progresses beyond
publication/guardian/runtime checks but exposes catalog host detection limited
to ARM64. The next fix extends catalog detection, setup fixtures and managed
provider/session identity to the real compiled x86-64 platform. Local catalog
normal/sanitizer passes 951 checks, setup passes 40 and managed CLI passes 41.
No production x86-64 model recipe or actual-model readiness is inferred.

## Passing independent macOS/Linux CI

[Run 38454301972](https://github.com/deepanwadhwa/samosa-inference/actions/runs/38454301972)
is terminal `success` at source commit
`9df7ae0f3d29668812c9b9795c3e04b79900865a`. Both jobs succeed after exporting
`git archive HEAD` outside checkout:

- macOS14 ARM64: Darwin23.6.0, Apple clang15.0.0 / Xcode15.4 SDK.
- Ubuntu24.04 x86-64: Linux6.17.0-1022-azure, clang18.1.3.

All configured normal/sanitizer commands pass: `test sanitize`,
`test-registry-modalities sanitize-registry-modalities`,
`test-registry-cli sanitize-registry-cli`, `test-native sanitize-native`,
`test-summary sanitize-summary`, `test-whisper sanitize-whisper`,
`test-vision sanitize-vision`. This covers core sessions/operations/admission,
artifact acquisition/install, guardian/activation/managed transport, catalog,
combined routing and existing controlled modality adapters. Speech primitive
checks remain 324; package 2,330; image parser 4,360; graph 2,713; engine loader99;
catalog951; combined registry202 on both jobs, normally and sanitized.

The complete job logs were inspected, not inferred from a check badge. Native
registry CLI checks are427/430 on macOS and430/508 on Linux. Additional loop/read
assertions can vary with instrumented binary size and process timing; both run
the same contract/control cases. Actual-model qualification is still separate.
The hosted native C-ABI speech fixture does not qualify actual Linux Pocket,
Kokoro, temporal packages, streaming public ingress or final release/retirement.
Later documentation-only receipts preserve this exact tested code revision.
