# Whisper transcription contract v1 — integration in progress

The optional samosa-inference-whisper executable adapts the monolith's existing
whisper.cpp CLI runner. It implements public Inference JSON and uses the existing
FIFO admission, durable operation, scoped cancellation and deadline machinery.
It does not implement another speech engine or call Sources private code/state.

Host arguments are --whisper-cli, --whisper-model, --whisper-cli-sha256,
--whisper-model-sha256, --admission-state and optional --owner/--provider-id,
--queue-capacity/--admission-wait-ms. Executable/model paths must be absolute,
regular, owner-held, single-link files without group/other write permission;
symlinks refuse. SHA256 values are explicit lowercase hashes. Executable<=64MiB,
model<=512MiB. Model bytes are SHA-verified and inherited read-only through fd5;
the retained model is verified again before publishing a transcript. Descriptor
metadata describes configuration, not live readiness or model accuracy.

describe reports transcription, operations, cancellation and request_budgets,
English language and a30000ms input maximum. This adapter supports transcribe,
operation_get and operation_cancel. Text generation remains in existing text
adapters; no provider is silently substituted.

transcribe requires contract_version=1, provider_id and audio_wav_base64.
Optional operation_id, parent_operation_id and timeout_ms follow the existing
operations contract; parents require an operation ID. Unknown fields refuse;
public callers cannot supply filesystem paths, engine arguments, models or hashes.
Input must be canonical44-byte-header RIFF/WAVE PCM16 little-endian mono16000Hz,
with nonempty whole-millisecond samples and duration<=30000ms. The maximum WAV is
960044 bytes; base64 must be canonical, including padding/unused bits. This
matches Sources prepare_audio output. Requests<=2MiB only for this executable;
existing text executable requests retain their1MiB maximum.

The default budget is60s; timeout_ms1..60000 may reduce it. One deadline spans
admission, artifact observations, engine execution and final result validation.
Artifact SHA verification uses the existing bounded verifier; filesystem hashing
and process cleanup are not hard real-time guarantees. One active job and the
existing0..8 FIFO waiters remain bounded. Exact completed retries replay offline
before filesystem/engine access; normalized identity binds audio, provider,
configured engine/model SHA, parent and requested budget. Histories keep existing
128-operation/8MiB result bounds and16MiB operation database limit.

Success is kind=derived, artifact_type=transcript, language=en, input_sha256,
duration_ms, text and segments. Each segment has start_ms/end_ms/text in
input_audio_milliseconds; endpoints must lie within the prepared input, and
starts must be monotonic. Source timeline offsets belong to the Sources client.
Coverage stays partial/speech_recognition_uncertain. Engine/model hashes remain
in provenance. Recognized text is not original literal source evidence.

The runner preserves existing CLI model/audio/language/SRT flags but omits -nt
for timestamped output. The real installed engine demonstrated that -nt disables
timing and emits a padded30s cue for a short clip; the adapter refuses such
out-of-input coordinates rather than presenting them as measured source times.
SRT is strict UTF-8, at most16KiB/128 cues; empty, malformed or out-of-range output
refuses invalid_provider_response. Whisper timestamp tokens use20ms steps. An end
beyond the input duration is accepted only when it equals the first20ms boundary
at or after that duration, and is clipped to the input end. Starts outside the
input, off-grid overruns and larger overruns still refuse. Provenance records
timestamp_policy=whisper_20ms_end_clipped_to_input. This addresses a measured
Linux result ending a2672ms clip at2680ms without accepting arbitrary late cues.
Engine logs<=256KiB; child-created individual
files<=64MiB, allowing the existing Metal compiler's temporary library. The parent
monitors retained SRT file size during execution and validates it afterward.

The existing guardian owns the engine process group and admission lease. For this
adapter it also retains the private workspace directory descriptor and removes
input.wav/result.srt after engine reaping, including requester SIGKILL. The parent
retains the output descriptor to read qualified bytes after unlink. Cleanup is
bound to that directory inode and fixed owned filenames. Foreign files/processes
are not selected by pattern. Failure/cancel/deadline publishes no transcript.

make test-whisper/sanitize-whisper uses a clearly labeled fixture CLI/model to
exercise full30s input, artifact/input/refusal controls, offline replay, timeout,
cancel and real requester SIGKILL cleanup. The native test-whisper-real tool separately takes
actual Inference/Sources executables, an installed Whisper engine/model and an
English WAV fixture. Optional trailing media-worker and encoded-audio arguments
qualify compressed capture against an independently decoded WAV reference. The
test exercises actual Sources capture and transport-absent offline cache reuse;
its result must not be conflated with fixture engine QA. Sources preserves the
recognized timing policy when mapping cues to its source timeline.
Chutni transcript publication, broader audio/language qualification and vision
adapters remain required. Neither Inference nor Sources is released by this slice.
