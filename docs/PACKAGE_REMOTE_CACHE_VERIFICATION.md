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

## Complete HTTPS package assembly

The native compatibility harness now has an optional production HTTPS gate. It
exports the qualified native-format speech fixture, verifies its native manifest
pin, builds a pinned generic index, and starts a loopback TLS server with a freshly
created certificate and ephemeral port. The native client refuses an untrusted
certificate, then uses the trusted CA and per-file source mapping to acquire all
11 indexed files, assemble through verified cache descriptors, inspect the native
manifest/host/library graph, and publish/revalidate completion. A second cold
destination installs entirely from retained verified cache after URL configuration
is removed. The server verifies response hashes and observes exactly one GET per
unique payload identity; cache/offline assembly adds no download requests.

Normal and ASan/UBSan runs passed locally on macOS arm64. Core local compatibility
fixture gates also passed after the harness changes. Final HTTPS log:
`/private/tmp/samosa-complete-remote-package-final.log`. The ephemeral server and
fixture directories are cleaned up on success or failure. There is no test-only
HTTP allowance in this complete package gate; TLS uses the production downloader.

`test-package-remote` and `sanitize-package-remote` now join the independent
native CI targets. This qualifies the connected fixture path, not actual model
weights, a production catalog entry, shared destination reservations, transaction
recovery, loading/readiness or the public installation service. Those remain open.
