# Whisper installation verification

The native Whisper recipe now participates in catalog discovery, verified HTTPS
acquisition, compatibility admission, publication, offline status/resume, owned
removal and cache-only reinstall. This completes the Whisper installation slice
of component 2; visual packages and production catalog qualification remain.

## Portable gates

`make test-whisper-package sanitize-whisper-package` passed 693 checks in each
mode. Coverage includes independent engine/model policy, all profile header
fields, every short header, descriptor truncations, unknown fields, role
conflicts, closed native graph/tree checks and cancellation. Synthetic model
headers test admission rules, not inference quality.

`make test-setup sanitize-setup` passed the Whisper fixture and existing speech,
embedding and T5 setup gates. The HTTPS harness checks missing/incomplete host
policy, public policy injection refusal, untrusted TLS, complete downloads,
last-good preservation under wrong revision/model policy, offline status/resume,
completed and incomplete removal, repeated removal and cache-only reinstall.

## Independent engine build

`tools/build_whisper_runtime.sh` verifies the source archive SHA-256 before
extracting into a new private output directory. The source is whisper.cpp revision
`f049fff95a089aa9969deb009cdd4892b3e74916`, reused from Chat's existing source pin.
Archive URL:
<https://api.github.com/repos/ggml-org/whisper.cpp/tarball/f049fff95a089aa9969deb009cdd4892b3e74916>.
Archive SHA-256:
`d8cd961352377b1cc612224016a9ebdfe0ae508dc2b2f9ef514b341d672e3fdc`.

The build uses static CPU libraries, disables Metal, BLAS backend plugins,
OpenMP, CoreML, OpenVINO, dynamic backend loading and native CPU targeting, and
targets macOS 14.0. The observed executable links only system libraries and the
OS Accelerate framework. The script copies the upstream license into its output.
Python and compilers are qualification/build tools, not installed runtime
dependencies. The Linux build branch remains unqualified.

The original GPU-capable runtime failed fresh installed transcription with
SIGXFSZ during embedded Metal initialization under the existing guardian file
limit. That candidate was rejected. The CPU build passed from fresh installed
inodes; the limit was preserved. Original Chat runtime/model files were unchanged.

## Actual installed model

Both normal and ASan/UBSan-equipped setup/provider/harness runs passed the full
HTTPS installation/removal/reinstallation gate and 2441 actual Sources-to-Whisper
checks per run on macOS 26.5.1 arm64. Sources and the release Whisper executable
were not sanitizer instrumented. The observed 14.0 deployment metadata is not
evidence of execution on macOS 14 or other platforms.

| Artifact | SHA-256 |
| --- | --- |
| CPU engine, 2405000 bytes | `aa28f124f97b13c9315734db912844d6c9b7489aefdf1ad49ea0888ce9da27d5` |
| base.en-f16 model, 147964211 bytes | `a03779c86df3323075f5e796cb2ce5029f00ec8869eee3fdfb897afe36c6d002` |
| Four-file package index, 150369787 payload bytes | `7436006d4a5bcd169ee95cfc4b4ff5a55919a1678a222e65a52c2ca58217dfb5` |
| Original WAV | `927b006bef03f0c5b2430f5ddfa20629ea8fcabf1f0b422403f5143159e8eeb5` |
| Sources-prepared WAV | `61224cc2db0e1b3b7f69344a2aa79b7a7b167fdab0b10bdc995e52c22f6abe88` |

`tests/whisper_package_candidate.py` creates the private candidate from separately
supplied engine/model pins and records the observed deployment target. The shared
`tests/llama_setup_https.py` runner accepts its root/index/pin with `--revision`,
`--profile`, `--engine-pin`, `--model-pin`, and actual-quality arguments
`--whisper-cli`, `--whisper-test`, `--sources-cli`, `--audio`.

The actual result was “The coriander sample contains 7319.” The harness checked
exact prepared-input identity, 2672 ms input duration, timed transcript coverage,
uncertainty, owner operations and offline replay/cache behavior. The candidate is
qualification evidence, not a published production artifact catalog or a claim
of lifecycle readiness.

## Published distribution

The [six-file distribution](../catalog/whisper-base-en-cpu-v1) adds pinned MIT
notices and distribution provenance around the same independently qualified CPU
engine/model. Both normal and sanitizer-equipped hosts pass the exact six-file
setup/actual-model gate, then independently pass cold installation from the
published GitHub runtime and immutable Hugging Face model URLs. Each actual
inference run again passes2441 checks; cache-only resume/removal/reinstallation
and exclusive workspace cleanup also pass. The entry retains exact indexes,
runtime/model/configuration/executable pins and remote qualification receipts.
This closes Whisper's distribution slice, not all of area2 or lifecycle readiness.
