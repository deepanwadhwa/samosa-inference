# Whisper package v1

The catalog recipe `whisper-ggml-package-v1` installs a host-qualified native
Whisper engine and its exact model. The indexed metadata file
`whisper-package.json` is at most 4096 bytes and contains exactly these fields:

```json
{"format":"samosa.inference.whisper-package.v1","runtime_revision":"f049fff95a089aa9969deb009cdd4892b3e74916","profile":"base.en-f16","engine":"whisper-cli","model":"ggml-base.en.bin"}
```

The index must identify exactly one engine and one model, at the descriptor's
canonical relative paths. The only supported profile is `base.en-f16`. Its model
uses legacy GGML, not GGUF. Verification checks the magic and eleven little-endian
header values against this profile and hashes the entire model around inspection.
The format follows the [pinned upstream loader](https://github.com/ggml-org/whisper.cpp/blob/f049fff95a089aa9969deb009cdd4892b3e74916/src/whisper.cpp).

The embedding host supplies `WhisperPackagePolicy`: a 40-character lowercase
revision, profile, and independently qualified 64-character lowercase SHA-256
pins for both engine and model. Setup exposes these host-only arguments:

```
--whisper-runtime-revision REVISION --whisper-profile base.en-f16
--whisper-engine-sha256 ENGINE_PIN --whisper-model-sha256 MODEL_PIN
```

All four are required together. Missing policy makes catalog discovery report the
recipe unsupported; invalid policy is refused before acquisition. Public JSON
cannot supply policy or credentials. An engine/revision/profile mismatch reports
`unqualified_whisper_package_runtime`; a model pin mismatch reports
`unqualified_whisper_package_model`.

Compatibility also requires the shared closed native package checks: pinned
files, executable permissions, host architecture, observed minimum OS and native
library graph. Assembly applies this gate before publication; status revalidates
it without acquisition. Cancellation and deadlines apply during bounded reads
and hashing. Failed verification leaves the output descriptor unchanged and
preserves the last completed installation.

This is static installation admission. Actual transcription qualifies the pinned
pair separately; installation status does not imply a loaded or ready engine.
Removal follows the existing owned package inventory protocol and needs neither
credentials nor runtime policy. See the [qualification evidence](../docs/WHISPER_PACKAGE_INSTALLATION_VERIFICATION.md).
