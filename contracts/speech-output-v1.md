# Speech output v1

Speech uses the same public provider registry as text, embeddings, summaries,
transcription and image/video analysis. Configure a private registry entry with
`kind: "speech_output"`, `provider_id`, `state`, absolute `package` and a pinned
`manifest_sha256`. The package follows `speech-package-v1.md`. Configuration and
owner come from the host; requests cannot select paths, native libraries or owners.
Loading configuration and discovery do not start or qualify an engine.

```
{"contract_version":1,"op":"speech_generate","provider_id":"voice",
 "operation_id":"greeting-1","text":"Good morning.","voice":"caro",
 "timeout_ms":60000}
```

`operation_id` is required. Text is nonempty UTF-8, at most2400 bytes. `voice`
is optional; Pocket accepts `caro`/`stuart` (default Caro), Kokoro accepts
`bella`/`sarah`/`nicole`/`adam`/`michael`/`emma` (default Bella). The native runtime
remains authoritative for voice validation. Unknown fields, null voice and public
paths refuse. `parent_operation_id` uses the existing operation parent contract.
Timeouts are1..60000ms; absent timeout defaults to60 seconds. Host cancellation,
operation cancellation and deadlines apply to admission, generation and publication.

The result contains `operation_id`, `provider_id`, opaque `audio_id`,
`format: "pcm_s16le"`, `channels: 1`, `sample_rate`, `samples`, `bytes` and
`pcm_sha256`. It contains no local paths or raw audio. The guardian owns the worker
process group and retains residency until cleanup. Each request starts and stops
its worker; this adapter does not implement persistent readiness or model reuse.

```
{"contract_version":1,"op":"audio_read","provider_id":"voice",
 "audio_id":"<64 lowercase hex characters>","offset":0,"length":32768}
```

Ranges refer to mono PCM bytes, with even offset and even length, length2..32768,
and exact bounds within the retained audio. The result repeats the audio metadata,
`offset`, `length` and `pcm_base64`. The full PCM hash is checked before exposing
any range. Foreign-owner/provider handles return `unknown_audio`. Audio reads are
bounded retrieval operations, not token streaming or incremental generation.

```
{"contract_version":1,"op":"audio_delete","provider_id":"voice",
 "audio_id":"<64 lowercase hex characters>"}
```

Deletion releases the retained audio and returns `deleted: true`. Unknown or
foreign handles refuse. The existing completed operation receipt remains saved:
replay returns that exact receipt, and subsequent reads return `unknown_audio`.
Deletion never silently regenerates speech. Audio reads/deletes do not accept an
operation ID or request timeout; trusted host controls still apply during reads.

The provider's separate private SQLite audio store is bounded to128 artifacts and
64MiB total PCM, with a68MiB database page ceiling. Rollback journals add transient
space up to the database size; this is not a global disk scheduler. Atomic
transactions publish complete audio only after worker packet and copied digest
verification. Interrupted writers roll back on reopening. No request includes
partial cancelled audio. There is a publication/operation-finish gap: a requester
that dies after audio commit but before the operation receipt commit can leave
bounded unreferenced audio. The interrupted operation is not retried silently.
Coordinated orphan retention and global quotas remain lifecycle/release work.

Existing owner-scoped `operation_get` and `operation_cancel` apply. Replay binds
text, explicit voice, provider, package manifest, parent and explicit timeout.
Saved receipts/audio remain readable without the model package. Package identity
checks observe files before and after execution; immutable installation/staging
against concurrent same-owner writers remains separate work. Host owner scoping
is not yet an authenticated network service.

## Incremental PCM transport

`samosa-inference-providers --config ABSOLUTE_PRIVATE_JSON --stream` accepts the
same `speech_generate` request and owner-bound retry identity. Other valid work
operations refuse `unsupported_stream_operation`. Private configuration-load
errors occur before request ingress and retain their ordinary startup-error DTO.
The C registry interface offers the corresponding trusted `SpeechConfig.pcm_sink`
callback, with the active operation's cancellation/deadline hook for backpressure.
No public request chooses a callback, file or output path.

The CLI emits newline-delimited JSON `speech_pcm` events with increasing
`sequence`, byte `offset`, `bytes`, provider/operation IDs, `pcm_s16le`, one channel,
sample rate and `pcm_base64`. Chunks are even-sized and at most8192 PCM bytes;
encoded events are at most16384 bytes. Each event says `provisional: true`.
Clients must treat the stream as incomplete until a `completed` terminal event
contains the ordinary retained receipt. Final concatenated PCM must match that
receipt's bytes/rate/hash. A `failed` terminal event contains the ordinary typed
operation error; it publishes no completed audio artifact. Connected consumers
receive exactly one terminal event when delivery succeeds.

Completed replay emits one terminal event with `replayed: true` and no regenerated
PCM. Fresh completed work sets `replayed: false`. Partial progress is not durably
replayed; an interrupted operation follows the existing refusal/recovery rules.
The terminal sequence follows the last fully delivered PCM event. A valid
request rejected before generation may produce only a failed terminal event.

Private worker progress uses a provisional header and cumulative prefix hash.
The owning process verifies a stable snapshot plus its newly copied suffix
before delivering chunks. The final packet keeps its existing strict whole-PCM,
metadata and package verification. A snapshot race emits nothing until a stable
one is available; a terminal mismatch fails. Sink refusal stops/reaps the same
guardian and retains residency until cleanup. The largest temporary suffix copy
is bounded by the16MiB worker packet ceiling; this is not global memory admission.

CLI output is nonblocking during event delivery. Controls are checked while
waiting for writability. If cancellation arrives after a frame has started, it
has at most two additional seconds to finish that JSON frame before stopping;
terminal delivery has a separate two-second bound after worker cleanup. An
incomplete delivered frame or broken consumer ends with transport exit2 and no
appended terminal frame; clients must discard an unterminated stream. Disconnect
detection also cancels a worker that stalls before another callback. A connected
consumer that does not drain output cannot extend model work indefinitely past
its deadline. Kernel/filesystem syscall latency retains the existing host limits.
The worker result/operation receipt remain authoritative if terminal delivery
fails after persistence. Native generation callback boundaries and CLI chunk
boundaries need not be identical. This does not provide text-token streaming,
network authentication or persistent model sessions.
