# Nonresident lifecycle boundary

LIFE-07o, native Mac first. This extends the existing Activation and Whisper
adapter; it does not introduce a resident transcription server.

An opt-in `on-demand-whisper-v1` registry profile uses the existing public
provider_start/provider_lifecycle_status/provider_stop controls. Start verifies
the pinned engine/model and a trusted, pinned canonical speech WAV, executes
the existing transcription adapter, and checks expected text. Only successful
qualification commits Activation's next last-good revision. Configuration alone,
saved metadata, silence, or an available executable cannot establish readiness.

Idle status means qualified for on-demand execution with unchanged retained
inputs and state identity. It reports execution_mode=on_demand and resident=false;
there is no live Whisper engine or loaded model between requests. Status checks
identity, not an invented persistent process. Stop withdraws qualification and
releases retained descriptors/fences after users join. Restart requalifies;
activation_get continues to report persisted metadata as live_readiness=unobserved.

Each transcription retains its original request guardian, private workspace,
admission lease, input/UTF-8/SRT limits, cancellation, operation and replay rules.
Managed synchronous calls retain their scope through a suspended guardian:
a request deadline does not release lifecycle ownership before physical cleanup.
Stop remains busy until that cleanup finishes. A lost/killed guardian invalidates
qualification and keeps Activation's lifecycle fence; stop/start return a typed
error rather than claiming cleanup. No public force-reset is supplied. Recovery
of an unknown orphan group and recovery after terminating that host remain open.
The guardian retains the request admission lease through cleanup after host
death. A replacement host must qualify through that same admission namespace;
it cannot launch a second request while the old guardian still owns the lease.
Runtime failures invalidate qualification; ordinary request cancellation or
queue refusal does not imply a changed engine. Invalid input does not launch it.

Qualification WAV path/hash/size and expected text are private host configuration,
never public request fields. They are included in committed qualification identity
and rechecked while active. Engine/model permissions retain the existing Whisper
rule: owned regular singly linked files, no group/other write; read access need
not be private. Activation state/fences remain private. Existing resident modes
retain their stricter input and live-process rules.

This profile preserves the existing structured Whisper adapter's30-second
canonical-WAV/timed-SRT contract. It does not replace, truncate or qualify the
separate legacy long-window txt/SRT CLI. Its common-control integration, default
primary/visual controls, generic guardian-death policy, pre-admission recovery
and full area3 remain required. Global residency/fairness remains area4.

Native qualification exposed an inherited structured-worker defect: the generic
64MiB process-wide file cap killed the installed Metal Whisper engine with
SIGXFSZ while its OS shader cache was223739904bytes. The managed adapter opts
into the same bound-workspace guardian without that global cap. Its original
16KiB SRT validation/watch, audio bounds,60-second qualification deadline and
262144-byte log limit remain. Other capped and explicitly sized guardian modes
are unchanged; this does not constrain Apple's shared cache to transcript bytes.
