# LIFE-07e: extract the working shared Whisper runner

Opened 2026-11-21. Baselines: Chat f2414d7, Inference ad3a1e1.
Native Mac; no Python or Docker. Full refactor remains unfinished.

Move gateway whisper_transcribe_path's existing temporary-output, fork/argv,
wait/reap, bounded file read, UTF-8/trim and cleanup implementation into Inference
with explicit engine/model/temporary directory and synchronous coordination
callbacks. Preserve txt/SRT, English/-nt,16-MiB output cap, return codes and long
audio behavior. Gateway selection/fingerprint and its shared voice/audio busy/
cancel state remain thin explicit callbacks until shared admission is extracted.

The existing verified structured whisper.c path already owns its guardian and
short prepared-clip validation. Its30-second input, strict segment DTO and PID
control differ from the original text/long-window caller. Do not force the
gateway through that narrower boundary or introduce another server/engine. Keep
that verified implementation intact and record its remaining consolidation with
the extracted legacy runner as open, rather than replacing working semantics.

Reuse the original bounded-file/trim/UTF-8 helpers byte-for-byte inside the owner;
the gateway still uses its generic helpers elsewhere. New code is only boundary
configuration/callback wiring, native acceptance fixtures and required removal
of Python-backed test/packaging dependencies. Public API paths are trusted host
configuration, not public request-selectable commands or files.

Acceptance: native normal/SAN txt/SRT, literal argv, busy/preserve-cancel,
failed/invalid/missing/oversized output, cancellation/reap/cleanup and long input;
existing verified Whisper normal/SAN regression; original gateway voice and
durable audio-window behavior after porting their Python fixtures/assertions;
independent owner archive build and clean native packaging/installation. Record
actual evidence and excluded failures below; planned commands are not passes.

## Qualified extraction — 2026-11-21

`src/whisper_process.c` and public `whisper_process.h` now own the original
temporary-output, argv/fork/wait/reap, read/trim/UTF-8 and unlink implementation.
The existing owner archive includes it. Gateway selects the same model and
fingerprint, then calls this API through explicit acquire/start/reap/release/
cancel hooks retaining its original shared voice/audio coordination. All seven
return values and existing caller error mapping are preserved. No engine/model,
language, timestamp flag, clip/window limit or cache identity changed.

`audit-extraction.mjs` compares with recoverable Chat f2414d7 and proves six
helper bodies, selected-model fingerprint, runner core and original coordination
blocks are preserved after explicit configuration/callback substitutions.
See source-audit.txt. Generic gateway helpers still serve other workflows; this
owner's copies keep its native archive independent of Gateway/private headers.

Every final gate below returned0 on macOS26.5.1 ARM64 with Xcode26.5 C/C++ and
system SQLite. Test ports use already available Node23.10.0; none introduces a
shipped interpreter dependency. No Python or Docker/VM/Linux work was invoked.

| Gate | Result/evidence |
| --- | --- |
| Extracted runner, normal/SAN | Original txt/SRT argv, quoted literal paths,271-second input, valid Unicode, exact16-MiB acceptance/oversize refusal, busy/preserve-cancel, preflight/publish/active cancellation, unrelated-peer survival, failure/reap/result cleanup; native-macos.log |
| Existing verified Whisper public adapter, normal/SAN |508/511 checks, including original30-second and guardian controls/parent-death cleanup; native-macos.log |
| Fresh independent owner source/archive, normal/SAN | Same extracted-runner gates and real-driver builds pass outside parent/module; standalone-final-macos.log |
| Gateway/summary/runtime build/regressions | Pass; chat-build-macos.log |
| Original localhost voice behavior | Model/voice selection, authentication, PCM validation, exact microphone response and cleanup pass; voice-macos.log |
| Original durable audio integration | Whole existing script passes, including failure/resume/offline cache, native AAC/MP3/window decoding, transcript cancellation/restart, qualified video speech, exact overlapping-tail coverage and271-second/30-window balanced retrieval; audio-macos.log |
| Actual retained Whisper, normal/SAN owner | Generated spoken words recognized in txt and SRT; output files removed; actual-macos.log and actual-sanitize-macos.log |
| Release staging and clean installation | Exact owner source/manifest/version and installed gateway pass; package-macos.log and installer-macos.log |

SAN instruments the C owner/harness; the actual external installed engine is not
sanitizer-built. Actual recognition uses an isolated native Mac-generated
"The quick brown fox jumps over the lazy dog" recording, the existing installed
Whisper CLI and base.en model. No download/conversion/new model is involved.
`actual-artifacts.sha256` pins those inputs; actual-source.txt locates the local
audio. The gate checks expected words and SRT syntax, not timestamp accuracy or
general model quality. The retained CLI's original `-nt` SRT reports a30-second
end for this short recording; that inherited output is preserved, not qualified
as accurate timing by this extraction. The verified structured short-clip
adapter's separate timing rules and original regressions remain unchanged.

## Reproduction

From Inference:

```sh
../../tools/without_python.sh make test-whisper-process sanitize-whisper-process test-whisper sanitize-whisper
../../tools/without_python.sh node docs/qualification/life07e-whisper-runner/audit-extraction.mjs
../../tools/without_python.sh make build/test-whisper-process-real build/test-whisper-process-real-sanitize
actual=$(cat docs/qualification/life07e-whisper-runner/actual-source.txt)
../../tools/without_python.sh "$PWD/build/test-whisper-process-real" /Users/deepanwadhwa/.samosa/voice/runtime/whisper-cli /Users/deepanwadhwa/.samosa/voice/stt-whisper-base-en/ggml-base.en.bin "$actual/speech.wav"
../../tools/without_python.sh "$PWD/build/test-whisper-process-real-sanitize" /Users/deepanwadhwa/.samosa/voice/runtime/whisper-cli /Users/deepanwadhwa/.samosa/voice/stt-whisper-base-en/ggml-base.en.bin "$actual/speech.wav"
```

The actual recording was prepared without an interpreter:

```sh
/usr/bin/say -o "$actual/speech.aiff" 'The quick brown fox jumps over the lazy dog.'
/usr/bin/afconvert "$actual/speech.aiff" "$actual/speech.wav" -f WAVE -d LEI16@16000 -c 1
```

Standalone: copy Makefile, src/, include/, engines/json.h and the two
test_whisper_process*.c drivers into an empty directory outside both repositories.
Run the native/SAN runner targets and the two real-driver build targets there.
The normal binary supplies the native fixture engine to the SAN harness. No
parent source or prebuilt object/library is borrowed. The new source/header are
also included in existing release source staging, manifest checks and installer
archive compilation. Complete component release/default CI remain separate gates.

From Chat:

```sh
./tools/without_python.sh make samosa-gateway test-native-summarizer-supervisor test-runtime-settings
./tools/without_python.sh sh tests/test_voice_gateway.sh
./tools/without_python.sh sh tests/test_audio_attachments.sh
./tools/without_python.sh sh tests/test_package_pdfium.sh
./tools/without_python.sh sh tests/test_gateway_installer.sh
```

## Test migration and remaining limits

Voice and audio drivers now call Node fs/JSON/WAV fixtures and native shell
tools instead of embedded Python. All29 audio blocks map to their replacements
in audio-port-map.txt; the original shell assertions and workflow sequencing
remain. The synthetic MP3 was moved from inline base64 to a1.4-KiB fixture with
identical decoded bytes (fixture-audit.txt). No assertion was removed or relaxed.
The voice model/WAV fixtures preserve their exact sizes/sample pattern, and the
same response/selection assertions are retained. Existing Mac codec tests still
use afconvert and the original native decoder; no ffmpeg/runtime dependency was
added. Remaining Python tooling/CI elsewhere is still open.

The first voice port transformation stopped before writing, and the guard refused
the original driver's interpreter call; voice-guard-refusal.log is excluded.
No Python interpreter executed. The first actual C driver missed the Mac feature
declaration for mkdtemp; it was corrected and both final builds/runs pass.
actual-fixture-declaration-failure.log is excluded. The guard detects PATH-based
calls, not absolute interpreter paths; changed source/recipes were reviewed too.

The legacy runner preserves its original fork/wait/cancel/output behavior. This
does not establish guardian owner-death cleanup or verified artifact pins for
that legacy profile. Its consolidation with existing guarded execution and the
remaining shared voice/model admission are open. Product busy/cancel selection
is explicitly retained through callbacks; no second scheduler was introduced.
The four pre-existing dirty Chutni files remain unchanged and match Chat's saved
dependency hashes. Clean assembled dependency/release qualification remains open.

Next LIFE-07f extracts the existing Mac visual memory/thermal budget and bounded
Molmo reclaim wait with explicit telemetry/control/trace inputs, preserving its
thresholds/retry policy. Then audit remaining area3 ownership against its complete
checklist and consolidate existing guardian/admission paths without narrowing
legacy workloads. Full areas3–7, Python removal/CI, Agent/App/assembly/retirement
and deferred Inkling-2-Mini remain unfinished.
