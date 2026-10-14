# Per-file remote cache evidence

The native remote cache adapter supplies verified descriptors to the existing
assembly source interface. The remote artifact harness now requests two distinct
indexed identities through different trusted URLs. One URL requires its configured
bearer credential; the other returns different pinned content. Checks verify both
held descriptors, reject substituted size and incomplete mapping counts with
untouched output, and read a verified cache hit with the download configuration
removed. A fresh acquisition preserves a request deadline error and resumes after
cancellation.

Local macOS arm64 results:

- Remote artifact suite: 188 checks, normal and ASan/UBSan.
- Existing production TLS CLI: verified HTTPS, untrusted certificate refusal and
  offline exact retry.
- Credential CLI: 42 checks, including private single-link credential input and
  absence of secret output.
- Static analysis of the new adapter: no diagnostics.

Log: `/private/tmp/samosa-package-remote-cache-final.log`. The mapping harness uses
its existing test-only loopback HTTP allowance; production TLS evidence comes
from the existing HTTPS CLI gate. These combined checks do not prove a complete
remote speech-package installation or actual model readiness. Shared reservations,
coordinator recovery and public catalog installation remain open.

Independent CI passed preceding direct-cache revision `cbdbc96` in run
38591525467 and local-acquisition revision `082fbdc` in run 38592032764, each on
macOS arm64 and Linux x86_64. This remote adapter revision needs its own CI result.
