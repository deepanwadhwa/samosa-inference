# Speech receipt package provenance

Fresh successful speech generation now returns the verified package's manifest
pin and domain-separated identity, exact text hash and requested voice. The
observation is persisted with the existing operation receipt only after successful
worker completion and complete package/output validation. It does not claim an
immutable snapshot or future engine readiness. Descriptors expose declared pins
while remaining configured/unqualified. Audio ranges preserve original PCM
metadata and never attribute older audio to a newly configured package.

Local macOS normal and ASan/UBSan public tests passed183 checks per mode with
`SAMOSA_TEST_LEGACY_SPEECH_CLI` set to an actual saved pre-change executable.
They verify exact provenance, null requested voice for package defaults, declared
versus verified metadata, byte-identical old receipt replay, terminal-only old
stream replay, and old audio retrieval under a different missing package/pin.
Existing owner/range, foreign-platform replay, cancellation, deadline,
disconnect and backpressure/survivor checks remain intact.

Snapshot: `/private/tmp/samosa-speech-legacy-vmamrdwj`, SHA-256
`3e3a098ce1c9f2895b605aa523cb6da3c72701ba5f3fa5a8cf9204f0b6d6b485`,
copied from the locally built86ea184 client before changing receipt generation.
The old executable creates its own original receipt in the same private store;
the test does not forge a historical SQLite record. No storage/replay schema
or normalized request identity changes.

Commands: `SAMOSA_TEST_LEGACY_SPEECH_CLI=<snapshot> make test-speech-public
sanitize-speech-public`. Final log:
`/private/tmp/samosa-speech-provenance-qualified-tests.log`, also including309
combined-registry checks per mode. Earlier compatibility development output is
retained in `/private/tmp/samosa-speech-provenance-final-tests.log`.
Actual-model streamed provenance gates use the existing
`tests/test_speech_public_real.py` command with `--stream`:

- Pocket Caro/Stuart:265 checks per mode, unchanged deterministic PCM hashes,
  per-voice input/package/requested-voice provenance, retained ranges and replay.
- Kokoro Bella:94 checks per mode, existing1% sample-count tolerance for fresh
  variability, exact own-stream/range checksum and saved receipt replay.

Kokoro's focused profile is an unchanged copy of the already qualified
`tests/profiles/kokoro-macos-arm64-v1.json` with its voice list restricted to Bella,
stored at `/private/tmp/samosa-speech-provenance-kokoro-profile.json`. This new
receipt-only change does not modify model voice selection or PCM generation;
all six voices' prior actual streaming evidence remains separately recorded.
Logs: `/private/tmp/samosa-speech-provenance-{pocket,kokoro}-{normal,sanitize}.log`.
Model/runtime libraries are not sanitizer-built. Actual-model Linux support,
persistent readiness, installation/staging and the full release remain open.
