# LIFE-05a extracted speech session

LIFE-05a speech session extraction qualified, 2026-10-25.
Direct inspection found the working implementation in speech_worker.c and the
previously extracted speech_runtime.c (original gateway native Pocket/Kokoro ABI,
voice mapping and PCM generation). Extracted the worker's existing package
verification, library-graph check, options/voice selection and native open/close
into private speech_session.c/h. The original worker now consumes this helper;
its request parser, guardian ownership, PCM packets, provisional streaming,
terminal publication and public speech adapter are unchanged. The helper retains
one loaded runtime across sequential requests and owns a duplicated bound root
fd plus copied host path/pin. Package checks precede/follow generation; successful
worker finish also rechecks after native destruction as the original worker did.
No second speech engine, installer, model or public lifecycle implementation.

Focused fixture checks run in a separate child process: repeated Caro/Stuart
requests after deliberately changing the native ABI probe prove no reopen;
closing the caller's fd preserves the session's bound root; early cancellation
and package mutation expose no successful result/PCM. Failed generations require
caller cleanup before worker reuse. This private helper alone does not establish
public residency, readiness or process ownership.

Final macOS normal/SAN worker354/358, independent Debian ARM64 Linux355/357;
public speech175/175 on both platforms. Poll-related worker counts vary.
Existing runtime324/324, loader2713/2713 and package compatibility/publication
checks pass both platforms. Final qualified handles69131/62358, regressions
12075/64376 and actual11876 are terminal0; no live qualification handles remain.
Earlier session-mac/final logs are development evidence, superseded by qualified
logs after retaining the post-destruction verification boundary.

Actual public Pocket --stream passes265 checks normal and265 sanitizer on macOS,
using an isolated copy of retained local artifacts, all checked against the
existing catalog manifest before copy. Manifest SHA256:
49a3fc30b37676239559a633d9860f129e6201f6fe3475cfe3a5a7f5bca27173
Package: /private/tmp/inference-life05-pocket-vlslz1c_
Caro78720/Stuart90240 samples retain the original exact PCM hashes, ranged reads,
streaming terminal verification and replay. C host/worker code is instrumented;
external Sherpa/ORT libraries remain release builds. This is existing public
one-shot execution via the extracted helper, not actual resident-worker proof.
No actual Linux or Kokoro claim is added. Model/library/config bytes unchanged.

Next LIFE-05b: reuse this session inside the existing guarded speech worker for
sequential requests, keeping the established bounded/provisional PCM packet
format and final hash checks. Reuse engine_process ownership/duplex controls and
existing package/root/workspace descriptors; preserve cancellation/deadline
invalidation and confirmed-reap lease retention. Qualify repeated native/actual
requests under one loaded worker before wiring identity-only public registry
start/status/stop and successful synthesis readiness. Reuse activation/store
semantics for last-good ownership and existing speech package identity; do not
replace speech.c/artifact stores, the native runtime, installer or gateway paths.
Inference area3 remains active, areas4–7/all C01–C14/Agent/App/pinned assembly/
legacy retirement remain open. The purpose remains splitting existing code into
independent repos, with only necessary ownership/API boundary changes.

Commands (component directory):

```
make test-speech-worker sanitize-speech-worker test-speech-public sanitize-speech-public
make test-speech-runtime sanitize-speech-runtime test-speech-loader sanitize-speech-loader test-package-compatibility sanitize-package-compatibility
python3 tests/test_speech_public_real.py build/samosa-inference-providers PACKAGE PIN --stream
python3 tests/test_speech_public_real.py build/samosa-inference-providers-sanitize PACKAGE PIN --stream
```

Linux runs use the independent source export /private/tmp/inference-life03-linux-siqynlpg,
CC=clang CURL_LIBS=-lcurl in the existing samosa-s17a-linux-qa container.
Compact logs retain PASS lines from /private/tmp/inference-life05-session-*.log.
