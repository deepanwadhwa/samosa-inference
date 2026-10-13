# Owned speech worker checkpoint

This checkpoint moves the extracted native Pocket/Kokoro runtime into the existing
owned engine guardian. It is a private adapter, not yet public speech routing.
The original Chat voice path remains in use until its replacement qualifies.

The host supplies a pinned closed package, a residency lease and private workspace
descriptors. Requests contain only text and optional voice. The worker checks the
package and native loader graph before opening Sherpa, generates bounded PCM16,
then rechecks the package before writing terminal metadata. Package checks observe
contents and identity; they do not provide an immutable snapshot against concurrent
same-owner writers.

The private `SMPCM001` packet contains an 80-byte little-endian header followed by
mono PCM16: byte count at offset8, sample rate at16, sample count at24, callback
count at32 and raw SHA256 at40. Offsets20 and72 are reserved zero fields. The
header stays zero until generation succeeds. The decoder checks exact lengths,
rate, duration, callback count, digest and descriptor identity, and exposes no
receipt on failure. The existing runtime enforces the tighter text-dependent
duration bound; the decoder's outer bound is45 seconds.

The new opt-in binary guardian profile imposes a16MiB kernel file-size limit,
disables core dumps and maps SIGXFSZ to `protocol_limit`. Existing JSON protocol
limits remain unchanged. Cancellation and deadlines stop and reap the process
group before returning; requester death also releases residency and removes the
workspace. The caller retains the `EngineProcess` record if shutdown remains
pending and must retry stopping it. This is ownership and lifecycle enforcement,
not an OS sandbox for arbitrary native code.

Controlled gates on macOS ARM64:

| Command | Result | Evidence |
| --- | --- | --- |
| `make test-speech-worker sanitize-speech-worker` |324 checks each | Native ABI fixture, callback/final-only output, corrupt metadata/PCM, malformed requests, cancellation, deadlines, crash and requester-death cleanup |
| `make test-engine-process sanitize-engine-process` |152 checks each | Existing ownership/group/foreign-survivor checks plus binary success and kernel-enforced overflow refusal |

The worker fixture is compiled without sanitizers, as an external native package.
The module, worker, guardian and test harness are sanitizer-built. Separate native
runtime tests also exercise a sanitizer-built ABI fixture.

Actual-model harnesses are `build/test-speech-process-real` and its `-sanitize`
variant. They take the absolute worker executable, package path and manifest pin.
The local Pocket package is `/private/tmp/samosa-pocket-pinned.0mx98nek`, pinned by
`4685a88efdcad754bdf09b67b8407b33d22bc9845410bdf59aa0c7aa84848a16`.
The phrase is “The meeting starts at nine. Please bring your notes.” Expected
24kHz PCM hashes are:

- Caro,78720 samples: `5a854797b1ebc66817950c875966dc8eefbd0ae10c143e0964546fbbdd7270c4`.
- Stuart,90240 samples: `0687ebda438c0f27ae2279306a51b8c2ceae8b0c02bdf4880c6a842a6cadb3fc`.

The harness runs Caro, cancels another request after partial PCM, then starts a
fresh Stuart worker. Success requires exact hashes, zero cancelled receipt,
terminal retained process records and removed workspaces. External Sherpa/ORT
libraries are not sanitizer-built. This does not establish intelligibility,
actual Linux/Kokoro qualification or persistent engine reuse.

Both actual-model harnesses pass39 checks on macOS ARM64. Regression gates pass
registry CLI427/430, summary478/478 and native speech runtime324/324 checks in
normal/sanitizer builds after the guardian change.

Independent archive CI also passes all configured normal/sanitizer gates on
macOS14/ARM64 and Ubuntu24.04/x86-64 at source
`a7b8c8046c18e0124c317ab104e049ec0e654a0f`:
[run38458189005](https://github.com/deepanwadhwa/samosa-inference/actions/runs/38458189005).
Both job logs confirm owned speech324 and guardian152 checks in each build mode.
CI uses controlled native fixtures; the actual Pocket evidence above is local.

Public owner isolation, admission, audio artifact publication, range reads,
streaming, readiness/session recovery and replacement integration remain open.
No numbered area or final release is closed by this checkpoint.
