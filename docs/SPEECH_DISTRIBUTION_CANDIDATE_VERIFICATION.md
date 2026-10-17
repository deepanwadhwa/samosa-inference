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
