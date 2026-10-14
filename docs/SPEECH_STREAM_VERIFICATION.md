# Public incremental speech checkpoint

The public registry CLI now supports `--stream` for existing `speech_generate`
requests. Native PCM callbacks publish provisional prefix metadata in the
guardian's bounded result descriptor. The owning process verifies prefix hashes
and stable metadata before invoking a trusted sink. Complete audio publication
still requires whole-packet integrity, successful engine termination and the
existing complete-package checks. Nonstreaming request/receipt schemas and
operation identity stay compatible.

Local macOS normal and ASan/UBSan fixture gates pass:

- Public speech144 checks per mode: ordered PCM/offset/hash alignment, exact
  normal/stream receipt parity, terminal-only replay, actual live callback before
  completion, owner cancellation, closed-reader cleanup, undrained consumer
  deadline, persisted typed failure, and a surviving new generation.
- Owned worker351 normal/355 sanitizer checks: native ABI/result integrity,
  provisional PCM cannot decode as completed output, live sink refusal retains
  its error and reaps before return, ownership/residency and prior crash/death
  controls. Polling counts may vary.

Logs: `/private/tmp/samosa-speech-stream-final-tests.log` (worker and initial public
gates), `/private/tmp/samosa-speech-stream-public-final.log` (final public gates),
and `/private/tmp/samosa-speech-stream-registry-regressions.log` (combined native
registry/CLI normal and sanitizer regressions).
Actual-model gates use `tests/test_speech_public_real.py <providers CLI> <package>
<manifest pin> [profile JSON] --stream`. They check each stream against its own
retained/ranged PCM and whole-receipt hash, plus exact replay without generation.
Pocket Caro/Stuart passed263 checks per mode with unchanged pinned sample counts
and checksums. All six Kokoro voices passed577 checks in each mode, using the
existing1% sample-count tolerance for its variable fresh generations. Logs are
`/private/tmp/samosa-speech-stream-{pocket,kokoro}-{normal,sanitize}.log`.
Native external model engines/libraries are not sanitizer-built; adapter,
guardian and worker clients are. Actual-model Linux execution remains open.

Development failures were corrected: the terminal receipt's trailing newline
initially split its NDJSON envelope; the envelope now trims only trailing JSON
whitespace. Darwin's closed write-pipe probe required `POLLOUT`; a native local
probe established that before the disconnect cleanup gate passed. Neither fix
relaxes receipt integrity or operation validation.

The transport's provisional/failure semantics, packet/event/copy bounds and
two-second partial-frame/terminal delivery grace are documented in
[speech-output-v1.md](../contracts/speech-output-v1.md). Full provider-integration
acceptance, incremental text generation, authenticated ingress, global scheduling
and final release/legacy retirement remain open.
