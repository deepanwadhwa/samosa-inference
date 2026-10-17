# T5 installation qualification

The existing `llama-gguf-package-v1` installer now accepts a distinct host policy
kind `summary`, restricted to GGUF architecture `t5`. The pinned six-field
descriptor must agree with the separate host revision, engine hash, kind and
architecture. Text/embedding policies cannot qualify a summary descriptor.
The shared full-tree, native executable/dependency graph, platform, quota,
publication, offline status and removal gates remain mandatory. Installation
continues to report readiness `unqualified`.

The native descriptor gate passes 474 checks normally and under ASan/UBSan,
including unsupported/unterminated kinds and non-T5 summary refusal. Combined
native package gates pass 87 checks in both builds. The portable HTTPS setup gate
now exercises both embedding and T5 summary fixtures, including absent policy,
public policy injection, TLS refusal, last-good preservation, offline status and
resume, complete/incomplete removal and cache-only reinstall. Existing speech
installation gates also pass. Logs:

- `/private/tmp/samosa-t5-static-final.log`
- `/private/tmp/samosa-t5-setup-final.log`

## Actual CPU package

The engine implementation is copied unchanged from Chat into
`engines/t5_summary.cpp` (source SHA256
`4d5f82a431c23364431c0bddb90c91b12f0b7d1cc14db5c7113cda6651375f21`). It is built from this
repository against the existing pinned Prism source using
`tools/build_t5_runtime.sh`. The static CPU profile has no non-system loader
dependencies; it links to the macOS Accelerate framework, libc++ and libSystem.
Its observed deployment target is 14.0. The successful standalone build log is
`/private/tmp/samosa-t5-cpu-build-final.log`.

Qualified local candidate inputs:

- Runtime revision: `9fcaed763ccda38ea81068ad9d7f991aaddca451`.
- Engine: `f3748ba69924338abcabbcffea32e6f567c73b00c0c1f9d38def7a984e708ecf`
  (3,993,536 bytes).
- Model: `30c73e0b2ebd4f65538d87f5157aa7193261e8ff362e441df86c6941f3e9265c`.
- Complete index: `a31462a26b12c91546e34f0d8b88dc82f40d4968e0f0a02a22831f4226be29b5`.
- Four files, 69,359,361 bytes, including descriptor and source/build provenance.
- Local candidate record: `/private/tmp/samosa-t5-cpu-candidate.json`.

The normal actual-model gate passes cold HTTPS installation, static publication,
offline status/resume, complete/incomplete removal, cache-only reinstall, then
actual public summary generation from the fresh installed engine/model inodes.
The existing summary harness passes 1,347 checks, including library/March scalar
quality, scalar long-input refusal, opening-summary map/reduce coverage,
input/model/engine provenance and replay. Log:
`/private/tmp/samosa-t5-cpu-installed-quality.log`.

The same complete installed-package gate also passes with ASan/UBSan setup,
provider CLI and summary harness, again with 1,347 actual-model checks. Log:
`/private/tmp/samosa-t5-cpu-installed-quality-sanitize.log`. The separately built
CPU engine is a release binary, not sanitizer-instrumented. Actual execution was
on macOS 26.5.1 arm64; the 14.0 deployment target is build metadata, not evidence
of execution on a macOS 14 host.

## Rejected shared-runtime candidate

The existing shared runtime was first repackaged in owned copies with local
dependency paths. Static graph/HTTPS installation passed, but first execution
from fresh installed copies failed. A temporary diagnostic build (not committed)
observed `SIGXFSZ` (signal 25) under the existing process file cap. Direct/warm
retries passed, so those retries were not accepted as cold-install qualification.
The shared profile includes Metal even for the public bridge's CPU-mode request.
The CPU-only package avoids those optional backend initialization effects while
retaining the existing controls and unchanged summarizer implementation.

Failure logs: `/private/tmp/samosa-t5-installed-quality.log` and
`/private/tmp/samosa-t5-installed-debug-details.log`. The latter records the actual
signal; it does not identify the exact temporary file that exceeded the cap.
Original Chat/runtime/model assets were not edited.

No actual Linux, macOS 14 execution, other architecture, loaded readiness or
production distribution catalog is established by this local qualification.
Installation remains the active component; lifecycle work is still pending.

## Published distribution

The [nine-file distribution](../catalog/t5-summary-cpu-v1) adds complete model,
sidecar and dependency notices around the same independently qualified CPU
engine/model. Exact-package per-file HTTPS setup and actual summaries pass in
both host modes. The shared published-provider gate then independently passes
cold acquisition from the actual release URLs, exact installed hashes/sizes,
1347 summary checks per mode, cache-only resume/removal/reinstall and cleanup.
Both remote receipts retain exact configuration and host-executable pins.
The source repository contains metadata only; release upload streams the retained
converted model directly from its existing file. No additional conversion or
retained weight copy is created. Actual execution remains macOS26.5.1 ARM64 only;
this closes T5's distribution slice, not full area2 or lifecycle readiness.
