# LIFE-05d retained Kokoro residency

LIFE-05d Kokoro on existing public speech owner qualified, 2026-10-25.
No production code/model/runtime/installer changed. The existing lifecycle
harness now accepts the original manifest-bound voice quality profile and runs
all6 Kokoro voices through the same loaded worker and activation revision.
The original public real-model harness's fresh-generation assertions were
extracted verbatim into tests/speech_quality.py and reused by both harnesses.
The published profile's1% sample-count tolerance is unchanged; actual generated
PCM is validated against its own exact hash/ranges, and retained replay is exact.
Reference hashes for variable fresh PCM remain diagnostics, as before.

Retained local runtime/model/voice/eSpeak/notice files were matched against every
published catalog manifest entry before copying into an isolated read-only QA
package.375 files including manifest; no conversion, new download or catalog
publication. Package /private/tmp/inference-life05d-kokoro-u75dyiqk;
manifest SHA256:
ebaa30beadfcc040df55621155c8cb56e839ca7ee9ce54f0ce929d7d08afbe76
Original quality profile and all model/library/data pins are unchanged.

Final actual macOS ARM64 normal/SAN public resident gates pass5 scenarios each:
synthesis-qualified start/idempotent readiness, all6 voices with original bounds
and exact range/hash verification on one worker/revision, exact stopped-worker/
missing-package replay and owner refusal, observed-running cancellation/deadline
refusal with explicit reactivation, EOF cleanup. Final profile and executable
pins are checked before/after execution. Earlier local actual passes remain
corroborating but final reports include the explicit profile-pin fence.

The original one-shot public --stream gate also passes583 checks normal and583
SAN across all6 voices, preserving byte-exact provisional stream versus retained
ranges and exact replay. Existing Pocket snapshot/isolation regressions pass265
checks per mode with original Caro/Stuart PCM hashes. The release QA snapshot
helper now includes the shared speech_quality.py with its own recorded hash;
an isolated renamed harness plus pinned helper executes actual Pocket without
repository imports. This is QA tooling reuse, not a new installer or another
published-URL qualification claim.

Native public speech lifecycle remains8/8 scenarios normal/SAN on macOS and
independent Debian ARM64 Linux. Only QA profile/pin/report handling changed after
those native gates; no production bytes changed. Final source/export bytes match.
Actual model scope remains macOS: C host/worker instrumented in SAN, external
Sherpa/ORT release builds. No Linux model execution, intelligibility/human quality
score, full area3 closure or persistent-session PCM-event completion is claimed.
Final actual14985, legacy/original52848, native30757/65616 and snapshot99157 are
terminal0; no live qualification handles remain. Compact logs, derived reports,
voice metrics, source/export/profile pins and original report locations are in
components/samosa-inference/docs/qualification/life05d-kokoro-resident.

Remaining engine audit found existing persistent NativeText decision framing,
VisionPsy load/unload JSON lines and Molmo2 framed command loops. Preserve these
implementations; missing ownership is in registry adapters. Image decisions bind
pixels at startup, and Whisper still invokes its CLI per canonical audio input;
their boundaries need explicit inspection before claiming resident reuse.

Next LIFE-06a: integrate text decisions around its existing persistent
engines/decision/main.cpp serve_requests loop. Reuse DecisionAssets open/recheck/
close and closed native loader graph, existing1MiB BE framing, guardian duplex,
controlled_pipe I/O, typed Rust result validation/normalization, operation
receipts and last-good activation. Require a bounded successful native decision
from the retained reference corpus before readiness. Do not apply the trusted
host dependency allowance to an external ORT engine, replace the engine/model,
or reimplement typed decisions. Preserve the existing image path while its
changing-image resident boundary remains open. Then continue the remaining
modality lifecycle audit/extraction against the full area3 checklist.
Inference area3/areas4–7, new T5 production distribution, all C01–C14/Agent/App/
pinned assembly/legacy retirement remain open. Full goal stays active. The
purpose remains separating existing code/tests into independent repos, with only
necessary ownership and API boundary changes.

Commands (component directory):

```
python3 tests/speech_registry_lifecycle.py build/samosa-inference-providers --package PACKAGE --manifest-pin PIN --quality-profile catalog/kokoro-speech-cpu-v1/quality-profile.json --output NEW_QA_DIRECTORY
# Repeat with build/samosa-inference-providers-sanitize.
python3 tests/test_speech_public_real.py build/samosa-inference-providers PACKAGE PIN catalog/kokoro-speech-cpu-v1/quality-profile.json --stream
# Repeat with sanitized CLI; Pocket omits the Kokoro profile argument.
make test-registry-speech-lifecycle sanitize-registry-speech-lifecycle
```

Linux native controls use existing independent source export
/private/tmp/inference-life03-linux-siqynlpg, CC=clang CURL_LIBS=-lcurl and the
samosa-s17a-linux-qa container. Native fixture execution does not load Kokoro.

Actual resident voice observations (fresh PCM may vary within the unchanged profile):

| Mode | Voice | Observed samples | Original profile samples | Observed PCM SHA256 |
| --- | --- | --- | --- | --- |
| normal | bella | 55696 | 55591 | fa3b7829bd3d457e15943df6243b5823c24f21068f0d1f7e49a5af9d3ea5d7e2 |
| normal | sarah | 59136 | 59112 | bf687e288a7ae84c9024b65b5703863af26a35d45c1b4ecdb32a851b6c06be70 |
| normal | nicole | 67084 | 67188 | cd74ff3853ffae3126e07e15ef8c900fff0ccc4d6ebd587546e302baf46b4abd |
| normal | adam | 56681 | 56676 | 49954481b43056b34027846cbb9d350e0222e4df1b9b2eb994563f875eb0ba58 |
| normal | michael | 60137 | 59983 | 7e59f2c0c0c03ad3bcbb5442341540057dc6649cc8e17f679739799e2955b1aa |
| normal | emma | 60177 | 60158 | 19e0603475a4e676abe5611fb7bc650a700eb16c893720803bc78ffef60e2985 |
| sanitize | bella | 55594 | 55591 | a5e171d04d1619f3cf1d334ea369194fa29a2e47a9b99eb953f5549fbff621cc |
| sanitize | sarah | 59117 | 59112 | 58fbbda936b6d25c12efd9b872f4066a6f4879398b8653e3711705e9dc475423 |
| sanitize | nicole | 67187 | 67188 | 49ccc4f931e3058c42048291208a921cbd09c182f9f362fc2f553489f9d82355 |
| sanitize | adam | 56656 | 56676 | f93b84be787f4a09b2dd7fa7618ddb5aadd5545d03e54475cab1d9e1141d3ad7 |
| sanitize | michael | 59975 | 59983 | 6e13e9f189bcad8eff9122de8f06be0314b2ea62de8d1ea4e33b6dd6a48a3bad |
| sanitize | emma | 60072 | 60158 | a006a896ec6bcc1323d5eabe1604cecee6465664b4ac9dc436faacf90cffcc7a |
