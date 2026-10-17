# Whisper base.en CPU distribution candidate

This entry is prepared for exact-package qualification and distribution. It is
not an installed provider or a readiness declaration. The proposed release URL
is not evidence that its assets have been published.

The six-file package has index SHA-256
`abcfffb782ee315d2cf2d5a7074f727dde53a8da031bf0e70937a48dfe10e06d`
and150372786 payload bytes. It reuses the separately built static CPU engine and
original base.en-f16 model already qualified by the independent Inference module.
Deployment metadata says macOS14.0; actual execution evidence is macOS26.5.1
ARM64 only. Linux model execution is not qualified.

- `catalog.json` pins the complete index, recipe and declared platform.
- `sources.json` binds upstream and proposed release URLs to that index.
- `host-policy.json` records independent runtime/model pins and redirect hosts.
- `payload/` contains only the descriptor, provenance and license notices.
- `index.json` also names the engine and model; those binaries are deliberately
  absent from the source repository. The model remains at its pinned upstream URL.

`tools/prepare_whisper_catalog.py` verifies the exact engine, model and license
hashes before creating a new private output directory. It writes metadata,
local QA source mappings and a non-model release asset list, without copying
weights or the engine. The local mappings/asset list contain private machine paths
and are excluded from this source-controlled candidate.

The immutable model URL is derived from the original App catalog's Whisper
base.en entry, with the moving revision replaced by upstream commit
`5359861c739e955e79d9a303bcbc70fb988958b1`. On2026-10-17 its HEAD response reports
147964211 bytes and the same pinned SHA-256 as the retained model. The
[model repository](https://huggingface.co/ggerganov/whisper.cpp/blob/5359861c739e955e79d9a303bcbc70fb988958b1/README.md)
declares MIT licensing. Full notices from
[whisper.cpp](https://github.com/ggml-org/whisper.cpp/blob/f049fff95a089aa9969deb009cdd4892b3e74916/LICENSE)
and [OpenAI Whisper](https://github.com/openai/whisper/blob/86098128c0b4f24f0e2aa2994de830614b474227/LICENSE)
are included and hash-pinned in provenance.

App provisioning must copy the catalog and source configuration into its owned
private configuration with mode0600; the native setup loader refuses ordinary
world-readable checkout configuration. Public requests select only the provider
identity. Publication, cold installation from the actual upstream/release URLs,
and the remaining area2 gates still need their own recorded evidence.

The exact six-file package passed the shared per-file HTTPS setup gate and2441
actual Sources-to-Whisper checks in each of normal and sanitizer-equipped modes.
Both runs cover TLS/policy refusal, publication, last-good preservation, offline
status/resume, completed/incomplete removal, cache-only reinstall and fresh
installed transcription. Logs are
`/private/tmp/samosa-whisper-distribution-normal-v2.log` and
`/private/tmp/samosa-whisper-distribution-sanitize-v2.log`. The initial normal
attempt used the unrelated JFK recording with a harness expecting the existing
coriander fixture; its transcript assertion failed. The corrected runs use the
original input SHA-256
`927b006bef03f0c5b2430f5ddfa20629ea8fcabf1f0b422403f5143159e8eeb5`.
All temporary installation cache/payload directories were removed. Preparation
retains about40KiB of metadata and no new weight/engine copy.
