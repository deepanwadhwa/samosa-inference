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

The registry profile preserves the structured adapter's 30-second
canonical-WAV/timed-SRT contract. The separate long-window owner below
preserves the file-based CLI contract.

Native qualification exposed an inherited structured-worker defect: the generic
64MiB process-wide file cap killed the installed Metal Whisper engine with
SIGXFSZ while its OS shader cache was223739904bytes. The managed adapter opts
into the same bound-workspace guardian without that global cap. Its original
16KiB SRT validation/watch, audio bounds,60-second qualification deadline and
262144-byte log limit remain. Other capped and explicitly sized guardian modes
are unchanged; this does not constrain Apple's shared cache to transcript bytes.

## Long-window owner

LIFE-08c extracts the shared Whisper qualification into `whisper_activation.c`.
Both adapters use its pinned engine, model and speech sample checks, expected
phrase check, retained identity and Activation transaction. The file-based
`samosa_whisper_lifecycle_*` controls use the original `whisper_process.c`
runner for the short startup probe and for user work.

The real probe has a 60-second process lifetime and a bounded canonical WAV.
It runs in the existing private command workspace, with the guardian holding
the lifecycle lease until reaping and cleanup finish. The regular file-based
request keeps its original audio length, text and SRT options and 16 MiB
output bound. Stop cancels and joins active work before releasing the lease.
The owner reports unknown cleanup if the guardian dies after its terminal
notice and before physical cleanup. A live host retains the fence in that
case. Shared recovery of unknown orphan groups across host exits remains open.

LIFE-08h registers new long-window workspaces under an owner-controlled
temporary directory. A short root lock serializes registration and recovery.
Each workspace keeps its own lock through preparation and guardian handoff.
The guardian atomically records admission before spawning the CLI. On the
next request, Whisper reclaims only unlocked preparations and empty
reservations in its namespace. It preserves borrowed audio and workspaces
marked admitted, including those awaiting confirmation after guardian loss.

Native Base and Tiny English qualified from the existing 2.533-second
`quick brown fox` WAV. For both models, text and SRT runs through the new
owner kept the final phrase in a 33.482-second recording. The fixture also
verifies stop during a run, restart, stale qualification input and unchanged last-good state after a
successful CLI call returns the wrong expected phrase. The generic guardian
fixture covers expiry at a shorter deadline, lease retention through the
terminal notice and a guardian killed before the cleanup acknowledgement.

Build `build/test-whisper-lifecycle-actual` and its `-sanitize` counterpart.
Pass the absolute native CLI, catalog English model, short probe WAV and the
33.482-second WAV paths in that order. Tiny English has 77,704,715 bytes and
catalog SHA-256 `921e4cf8686fdd993dcd081a5da5b6c365bfde1162e72b08d75ac75289920b1f`.
Use the Base English and short probe hashes from
the existing [short probe evidence](qualification/life07o-whisper-lifecycle/actual-inputs.sha256)
and [long-window evidence](qualification/life07l-whisper-guardian/README.md#life-08a-actual-audio-beyond-one-30-second-window).

LIFE-08d connects these controls to normal Mac Gateway preparation, voice
status and model selection. The original GPU-capable runtime bootstrap is
owned by Inference. It records the local executable's digest and byte size.
For legacy runtimes it can record that receipt once and leaves matching
installations in place. Activation verifies the native loader, receipt,
catalog weights and packaged speech sample before reporting readiness.
The checked runner still serves explicit unpinned CLI overrides and stays
available to legacy callers.
[Packaged speech provenance](../assets/whisper/README.md).
