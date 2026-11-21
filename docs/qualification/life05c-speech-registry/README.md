# LIFE-05c public existing speech owner

LIFE-05c public resident speech lifecycle qualified, 2026-10-25.
Existing speech.c now delegates execution to a trusted resident callback while
retaining normalization, original JSON request serialization, AudioStore
publication, provisional PCM callback semantics and durable operation receipts.
SpeechHost reuses existing activation admission/revision/last-good and guardian
ownership, speech package/loader verification and the LIFE-05b resident worker.
The transport borrows one authoritative activation EngineProcess record; no live
process descriptors are copied between owners. Warm request failure reaps through
that record while activation retains its lease/store fence until rollback.

Private speech entries optionally select resident-speech-v1 and declare separately
qualified worker_sha256/worker_size/manifest_size. The actual registry executable
and package manifest are admitted as pinned inputs; the closed speech bundle
retains its existing per-file/loader/model/reference verification. A trusted
model_argument lets the adapter record its actual package-directory argv while
model_path remains the manifest. The explicit pinned-host-adapter-v1 activation
policy requires engine==self plus a qualifying launch callback: it records the
already trusted application's runtime separately from external model-engine
system-runtime-v1. Unsafe loader environments still refuse. External engine
policy is unchanged; a test refuses applying this allowance to a different engine.
The first model-engine policy rejected host curl/sqlite/ASan dependencies; that
failed diagnostic is excluded, not used as acceptance or an external-engine waiver.

Discovery remains configuration-only. Public identity-only start/status/stop use
the existing provider lifetime gate, while operation controls stay concurrent.
Readiness requires complete bundle verification and actual bounded successful
synthesis through the existing PCM/hash validator before activation commit, then
live guardian/activation/artifact/package rechecks. Same healthy start is
idempotent. Failed qualification preserves the exact last-good tuple. Confirmed
cleanup releases owned state/manifest/package/residency; shutdown pending retains
ownership for retry. Existing one-shot entries remain available.

Final macOS and independent Debian ARM64 Linux normal/SAN public native gates
pass8 scenarios each: lazy discovery/strict controls, actual warm qualification,
both voices and original ranges/hash on one loaded worker/revision, exact
stopped-worker and missing-package replay, owner refusal, failed native ABI and
synthesis rollback, active cancellation/deadline with busy lifecycle exclusion,
package mutation and crash/restart, EOF/SIGKILL owner cleanup and unrelated survivor.
Actual macOS retained Pocket normal/SAN passes5 scenarios each, including
original Caro/Stuart PCM hashes on one worker, exact replay/offline retained reads,
observed-running operation cancellation and deadline refusal, explicit restart
and EOF cleanup. Public actual cancellation is not asserted to occur after PCM;
private actual PCM interruption/reap was separately qualified in LIFE-05b.
The harness polls the normal pre-registration unknown_operation race rather than
assuming a reserved control request runs after generation registration.

Actual manifest SHA256 is unchanged:
49a3fc30b37676239559a633d9860f129e6201f6fe3475cfe3a5a7f5bca27173
Package: /private/tmp/inference-life05-pocket-vlslz1c_. C hosts/workers are
instrumented in SAN; external Sherpa/ORT remain release builds. No actual Linux
or Kokoro model execution is added. Existing native activation final113/121
macOS and115/171 Linux, text260/260 (Linux261/261), embedding332/332, T5 lifecycle
11/11, public speech175/175 and nine-provider459/459 pass normal/SAN on both
platforms. Resident worker/old one-shot/PCM integrity/owner cleanup regressions
also pass. Variable activation/worker counts include polling and executable size.

Final native52168/23080, actual65811, text/embedding/T5 regressions55155/26548,
activation26802/61254 and final-default21959 are terminal0. No live gates remain.
Initial host-policy and actual polling failures are excluded in life05c native/
actual logs; no production deadlines/compiler flags/PCM assertions were relaxed.
Final source/export bytes match. Compact logs, source SHA256 and derived reports
are in components/samosa-inference/docs/qualification/life05c-speech-registry.
Reports preserve generation receipts; decoded audio-range hashes/lengths replace
large base64 ranges and original report locations remain recorded.

Next LIFE-05d: qualify the existing published Kokoro package/profile through this
same resident owner and original voice/PCM quality harness, without changing its
runtime, voices, loader or installer. Then audit/extract remaining transcription,
image/video and decision lifecycle ownership from the monolith/component before
closing area3. Persistent registry-session incremental PCM delivery remains an
explicit area5 integration gate; existing one-shot --stream remains qualified and
unchanged, while --session currently returns complete receipts. Do not mistake
correlated replies for incremental generation/events. New T5 production distribution
remains open. Inference area3/areas4–7, all C01–C14/Agent/App/pinned assembly/legacy
retirement are incomplete. Full goal stays active; continue splitting existing
code and tests into independent repos with necessary boundary changes only.

Commands (component directory):

```
make test-registry-speech-lifecycle sanitize-registry-speech-lifecycle
python3 tests/speech_registry_lifecycle.py build/samosa-inference-providers --package PACKAGE --manifest-pin PIN --output NEW_QA_DIRECTORY
# Repeat with build/samosa-inference-providers-sanitize.
make test-activation sanitize-activation test-speech-worker sanitize-speech-worker test-speech-public sanitize-speech-public test-registry-modalities sanitize-registry-modalities
make test-registry-lifecycle sanitize-registry-lifecycle test-registry-embedding-lifecycle sanitize-registry-embedding-lifecycle test-registry-summary-lifecycle sanitize-registry-summary-lifecycle
```

Linux uses independent /private/tmp/inference-life03-linux-siqynlpg source export,
CC=clang CURL_LIBS=-lcurl in existing samosa-s17a-linux-qa container. Compact
PASS logs derive from /private/tmp/inference-life05c-*.log. Native library fixture
adds only controlled failed-calibration and stalled-generation markers; model
and runtime implementations remain the original code.
