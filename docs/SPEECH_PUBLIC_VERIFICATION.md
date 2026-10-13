# Public speech and retained audio checkpoint

The provider registry now exposes native speech output alongside the existing six
modalities. It reuses the extracted native runtime, closed package/loader verifier,
owned guardian, FIFO admission, operation replay/cancellation and request budgets.
See [public contract](../contracts/speech-output-v1.md).

Audio is not placed inside the operation JSON result. `audio_store.c` publishes
verified worker PCM into a bounded transactional SQLite store and returns an
owner/provider-scoped handle. Reads validate the complete checksum before returning
at most32KiB of PCM. Deletion frees storage without rewriting operation history.
Kill/cancel tests observe unpublished transactions from another reader, then prove
rollback and successful later publication. Both artifact count and byte quotas
refuse excess work and permit capacity reuse after release.

Local macOS ARM64 gates:

| Gate | Evidence |
| --- | --- |
| `make test-audio-store sanitize-audio-store` |406 checks each: scoped handles, idempotency conflicts, range bounds, corrupt bytes, cancellation, killed-writer rollback,128-artifact/64MiB quotas and release/reuse |
| `make test-speech-public sanitize-speech-public` |91 checks each: separate native CLI/guardian/worker processes, exact fixture PCM, public ranges, foreign owner/provider refusal, operation status/cancellation/deadline, fresh worker recovery, offline receipt/audio replay and deletion behavior |
| Existing regression gates | Registry CLI427/430, linked modalities202/202, worker324/324 and guardian152/152 normal/sanitizer checks |
| Actual public Pocket | `tests/test_speech_public_real.py` passes55 checks with each normal/sanitized registry CLI: Caro78720 and Stuart90240 samples at24kHz, reconstructed range hashes match previous native/guardian receipts, exact replay and completed status |

Actual command arguments: registry executable, closed package
`/private/tmp/samosa-pocket-pinned.0mx98nek`, manifest pin
`4685a88efdcad754bdf09b67b8407b33d22bc9845410bdf59aa0c7aa84848a16`.
The actual phrase is “The meeting starts at nine. Please bring your notes.”
Caro PCM SHA256 is
`5a854797b1ebc66817950c875966dc8eefbd0ae10c143e0964546fbbdd7270c4`;
Stuart is
`0687ebda438c0f27ae2279306a51b8c2ceae8b0c02bdf4880c6a842a6cadb3fc`.
The external Sherpa/ORT package is not sanitizer-built. These checks do not prove
intelligibility or actual Linux/Kokoro model quality.

The sanitizer public test exposed a terminal error pointer into a local process
record: the public failed-voice response could contain an empty error code. The
engine stop API now returns stable closed-protocol error strings. Both public and
legacy guardian/control regressions pass after the fix. Initial new-test compilation
also exposed a shadowed `read` identifier and missing stdio declaration; corrected
before the passing runs.

Independent source archive CI passes all configured gates on macOS14/ARM64 and
Ubuntu24.04/x86-64 at `1e8ffe8e9b33a7f5328123b63f4f2fb0e29fcc24`:
[run38460361119](https://github.com/deepanwadhwa/samosa-inference/actions/runs/38460361119).
Both platforms pass audio-store406 checks in normal/sanitizer builds. Public speech
passes91/89 on Linux and89/91 on macOS; the count varies with operation-status
polling, while all work/control assertions execute. Actual Pocket55-check runs
were also repeated locally with the final published binaries in both build modes.

Existing Kokoro assets were found under `~/.samosa/voice/kokoro`. Its complete
eSpeak data includes `espeak-ng-data/voices/!v` and104 files below it, which the
current package path character rule rejects. A narrowly scoped native data path
rule and negative path tests are needed before qualifying that complete package.
The original assets were inspected only; they were not modified or filtered.

Open gates include common authenticated service, incremental events, persistent
engine lifecycle/readiness, immutable package staging, coordinated orphan retention,
global memory/disk scheduling, actual other-model/platform tests and Chat replacement
integration. Original Chat voice paths remain active. No full numbered area or
release is closed by this checkpoint.
