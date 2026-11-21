# LIFE-07a specialist control — partial transport checkpoint

2026-10-25. This qualifies the control extension to the **existing** extracted
SamosaMmSession transport. It does not qualify public resident visual inference.
VisionPsy/Molmo2 engines and their inference/command loops were not changed.

The additive `samosa_mm_session_request_controlled` accepts a monotonic absolute
deadline and a caller check returning a stable error string. Both existing JSON
line and BE32 framed protocols use the same original framing code, with control
checks during reads and writes (poll interval at most10ms). The original request
API delegates without caller control. Writes reuse the existing summary
transport's thread-local SIGPIPE helper; no process-wide signal change.

An interrupted/failed exchange invalidates the session to prevent a late or
partial response from being consumed by another request. The original supervisor
keeps the specialist lease until close kills/reaps its child. A preflight
cancellation before writing preserves the usable session. Caller control does
not signal or release another request's process/lease.

Meaningful tests cover both protocols: original echo/reservation/close/cancel,
preflight expiration followed by successful request on the same session,
caller cancellation/deadline while blocked reading, cancellation while writing
512KiB to a helper that stops reading after handshake, stale-session rejection,
lease retention until reap, and a closed input pipe under default SIGPIPE policy.
Shared summary-process regressions also run. Tests are fixture transport evidence;
no real visual-model generation or GPU/workspace ownership claim is made here.

Commands (terminal exit0 required; logs retained):

```
make -C components/samosa-inference test-specialist-supervisor sanitize-specialist-supervisor test-summary-process
docker exec samosa-s17a-linux-qa make -C /host-tmp/inference-life03-linux-siqynlpg CC=clang CXX=clang++ CURL_LIBS=-lcurl test-specialist-supervisor sanitize-specialist-supervisor test-summary-process
```

Linux uses the existing incremental owner export. `source-sha256.txt` records
byte-identical changed sources/headers/tests/Makefile checked against that export;
this is not a new standalone release archive qualification. macOS and Linux both
use strict warnings and ASan/UBSan specialist builds. One preliminary macOS run
failed the initial helper-start assertion; a direct run and subsequent normal/SAN
runs passed. No root cause was established; preserve this observation rather than
claiming every exploratory invocation passed. Final logs are the current source.

Next LIFE-07a work remains: adapt the same supervisor to borrowed guardian-owned
endpoints with retryable close; combine the existing guardian workspace/duplex
ownership while preserving Metal cache behavior and bounded protocol/log output.
Then reuse original visual package verification, RGBA-to-white PPM preparation,
response parser and actual generation readiness in the registry. A successful
ping/hello is only a transport handshake. Full area3 and areas4–7, Agent/App,
C01–C14 assembly and retirement remain open. Keep the full repo-split goal active.
