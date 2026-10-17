# Lean speech distribution candidates

The metadata-only producer `tools/prepare_speech_catalog.py` reuses the existing
Sherpa speech native manifest, native package installer and public speech tests.
It reads retained model/runtime files in place. It does not download, convert or
copy weights into release staging. The shared producer accepts a library-only
runtime without inventing an executable engine.

The original pinned manifests are Pocket
`4685a88efdcad754bdf09b67b8407b33d22bc9845410bdf59aa0c7aa84848a16`
and Kokoro
`8d3ce27844a7147201dbb0520d7407d861de52b9c729e7140175484a7a848aa7`.
Candidates omit the unused C++ API and unversioned ORT libraries. Pocket also
omits unreferenced sample WAVs, while preserving its two configured voice files.
Kokoro retains the existing eSpeak data inventory. Original model licenses and
README files remain unchanged; the Pocket README caution and bundled CC BY 4.0
license are both preserved. Added runtime license notices and pinned source
references are included in the closed native manifest.

| Candidate | Files | Installed payload bytes | Index SHA-256 |
| --- | ---: | ---: | --- |
| Pocket | 25 | 232329759 | `172350c46c8496a9d813ade4bd5ab1507b73a037398f1e9e8143aec881d1ed12` |
| Kokoro | 373 | 190560479 | `4f1913d67295dd2e8864cf5c65fdd8827a39928e7a55d0c3cc94d21bee17eb5d` |

On macOS 26.5.1 arm64, the existing shared HTTPS setup gate now accepts the
existing `sherpa-speech-package-v1` recipe. It checks separate catalog/payload
credentials, TLS refusal, source configuration validation, verified installation,
status/resume, completed and incomplete removal, and cache-only reinstallation.
Speech uses the existing native recipe qualification rather than llama runtime
policy flags. The test then calls `test_speech_public_real.py --stream` against
the exact installed closed tree, using the original Kokoro quality profile with
only its manifest identity updated.

Pocket passed normal and sanitizer gates, including 265 public synthesis,
streaming, audio range and replay checks in each mode. Both configured voices
match their existing exact PCM hashes. Kokoro passed normal and sanitizer gates with 583 checks each, preserving all
existing voice metrics and the original variable-PCM tolerance.

Logs are `/private/tmp/samosa-{pocket,kokoro}-distribution-{normal,sanitize}.log`.
Candidate metadata directories occupy less than 1 MiB each; native test cache and
installed payloads are temporary and removed by the shared test's finally block.

These are local candidate results, not published-URL qualification or Linux model
execution. Speech publication, complete corresponding-source packaging review,
and published-URL tests remain open. Component 2 is not complete.

## Complete notice candidates and remote QA tooling

The shared `provider_distribution_remote.py` gate now accepts `speech-pocket`
and `speech-kokoro`, calls the existing public speech harness with `--stream`,
and retains native cold installation, per-file hashes, cache-only reuse and
cleanup. Speech has a library-only runtime, so no fake engine executable or
llama policy is introduced. Kokoro requires a manifest-bound quality profile;
the gate snapshots that profile before execution. Actual speech published-URL
qualification remains pending. A real published T5 regression run passes 1347
summary checks and the existing cache/removal gates after this extension.

`collect_speech_source_notices.py` reuses the release helper's hashing and metadata
serialization. It reads retained, separately pinned source archives without
extracting them or downloading dependencies. Its input inventory is
[`speech-source-inventory-v1.json`](../catalog/candidates/speech-source-inventory-v1.json).
The collector records every LICENSE/COPYING/NOTICE file and builds a source-only
bundle containing original archives and build directions. All 13 enclosed archive
hashes were independently read back from the generated bundle and matched.

- Combined notices: 378609 bytes, SHA-256
  `70d02ac824f60eabf1e59ee738779fd011fc2f07e10e9373a90226f5f57c7e63`.
- Notice inventory SHA-256:
  `f3f3cd93fa6cdbebb4e0a8228247276c970d22d6b3618c6aafc113afe1e824d5`.
- Source bundle: 54620401 bytes, SHA-256
  `42b582ec9ec9812567a2888909cd0cbd13f251f431fb7d6d4a655f2bee4a4656`.

The archives and bundle remain outside the repository. The bundle does not
establish byte-identical reproduction or automatically certify dependency
completeness; final source/build closure remains a publication gate.

Revised candidates add these notices inside the native closed tree:

| Candidate | Files | Payload bytes | Index SHA-256 |
| --- | ---: | ---: | --- |
| Pocket v2 | 27 | 232717382 | `022eb7848df4d8c8a00f7eeb4ba4214f186296914246a94811e021a60586a3aa` |
| Kokoro v2 | 375 | 190948102 | `612248e08181e1e7c77c7471cc9622f95b313e840be9871533deaadfb12ff7c7` |

Both exact v2 candidates pass normal and sanitizer native HTTPS setup and
actual public synthesis/streaming gates: 265 Pocket checks and 583 Kokoro checks
per mode. Cache-only reinstall/removal and temporary cleanup also pass. Logs:
`/private/tmp/samosa-{pocket,kokoro}-distribution-v2-{normal,sanitize}.log`.
No speech release has been published and full component 2 remains open.
