# LIFE-05b existing resident speech worker

LIFE-05b private resident speech worker qualified, 2026-10-25.
Reuses the existing speech session, native Pocket/Kokoro runtime, guarded duplex
engine_process ownership, request decoder, provisional/terminal PCM headers,
PCM packet validator and streaming hash checks. New hidden resident dispatch
reads bounded length-prefixed copies of the existing text/voice JSON requests.
The same native session serves sequential requests; each provisional cumulative
header precedes its newly emitted PCM suffix, and the final header adds no PCM.
The host reconstructs the original regular-file PCM packet and runs the existing
progress/result validators. Existing one-shot dispatch remains available.

The nonblocking pipe transfer/SIGPIPE handling is extracted from summary_process
into controlled_pipe.h and shared with speech, preserving summary errors and
control behavior. Resident host requests retain a60s ceiling plus caller control,
16KiB request/16MiB packet/45s audio bounds and bounded chunks. No public paths,
commands, voices or model rules were added. Guardian retains bound package and
residency and handles lifetime, stderr bounds and process-group/owner cleanup.
Active control/sink/protocol failure clears the result and stops/reaps the worker;
shutdown-pending keeps its process/endpoints for retry. Preflight cancellation
sends no request and leaves the worker available. A caller must serialize use.
Startup itself is not readiness; public activation qualification remains open.

Final native macOS worker493/495 and independent Debian ARM64 Linux493/495
normal/SAN pass (poll counts vary). Added tests prove one native load through
three requests, same guardian, original decoded hashes, final-only generation,
preflight versus active cancellation,200ms stalled-read deadline, malformed
native audio, crash/restart, changed package refusal, request EOF and SIGKILL
owner cleanup, released leases and unrelated-process survival.

Actual retained Pocket normal/SAN passes135 checks each on macOS: original
one-shot Caro/Stuart/cancellation plus both voices on one resident guardian,
provisional streaming, original PCM hashes, cancellation after two chunks with
worker invalidation/reap, and successful restarted Stuart generation. Caro78720
samples/22 chunks; Stuart90240/25. Existing package manifest SHA256 remains
49a3fc30b37676239559a633d9860f129e6201f6fe3475cfe3a5a7f5bca27173;
isolated retained package /private/tmp/inference-life05-pocket-vlslz1c_. Native C
host/worker are instrumented; external Sherpa/ORT libraries are release builds.
This adds no actual Linux or Kokoro model execution claim.

Existing normal/SAN summary owned transport passes macOS274/354 and Linux267/272;
original summary-process scalar/batch/reuse/crash/recovery tests and public
speech175/175 pass both platforms. App gateway build, original native summarizer
supervisor, isolated installer and3 package/PDFium tests pass. Existing stage
manifests now include the extracted controlled_pipe.h so owner source builds
remain self-contained. Model, generation, installer and activation algorithms
were reused; no legacy paths were retired or remote catalog assets changed.

Final native19996/52645, regression93114/49614, actual53142, App82266 and
installer70797 are terminal0; no live qualification handles remain. Development
compile errors in life05b-build.log/native-mac-r1.log are excluded; final gates
retain the strict compiler flags/assertions. Linux export bytes match final source
except a subsequent API ownership comment, also copied and recorded as final
bytes. Source/evidence hashes and compact terminal PASS logs are retained in
components/samosa-inference/docs/qualification/life05b-speech-resident.

Next LIFE-05c: integrate this owner into the existing speech.c/public registry
through a trusted lifecycle callback, reusing package verification, admission,
activation revision/last-good semantics and identity-only lifecycle controls.
Require actual bounded successful synthesis before ready, retain exact
owner/provider operation receipts and provisional audio publication semantics,
and qualify failed replacement, mutation/crash/owner death, cancellation/deadline,
stop/restart, exact replay and cleanup. Do not treat a library bundle as an
executable engine or replace its loader/installer; adapt the existing activation
launch/qualification boundary for this verified multi-file native package.
New T5 production distribution gate remains open. Inference area3 and areas4–7,
all C01–C14/Agent/App/pinned assembly/legacy retirement remain incomplete.
The full goal remains active: split existing implementation and tests into
independent repos, adding only necessary API/ownership boundary changes.

Commands (component directory):

```
make test-speech-worker sanitize-speech-worker
make test-summary-owned sanitize-summary-owned test-summary-process sanitize-summary-process test-speech-public sanitize-speech-public
make speech-worker build/samosa-inference-speech-worker-sanitize build/test-speech-process-real build/test-speech-process-real-sanitize
build/test-speech-process-real ABSOLUTE_WORKER PACKAGE MANIFEST_PIN
build/test-speech-process-real-sanitize ABSOLUTE_WORKER_SANITIZE PACKAGE MANIFEST_PIN
```

Linux: independent export /private/tmp/inference-life03-linux-siqynlpg,
CC=clang CURL_LIBS=-lcurl in the existing samosa-s17a-linux-qa container.
App directory: make samosa-gateway test-native-summarizer-supervisor;
sh tests/test_gateway_installer.sh; python3 tests/test_package_pdfium.py.
Logs are compact extracts of the retained /private/tmp/inference-life05b-* logs.
