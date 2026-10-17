# Whisper base.en CPU distribution

This entry passed exact-package and published-URL qualification. Its runtime
assets are in the [package prerelease](https://github.com/deepanwadhwa/samosa-inference/releases/tag/inference-packages-2026-10-17.1).
It does not qualify the full Inference module or declare a loaded provider ready.

The six-file package has index SHA-256
`abcfffb782ee315d2cf2d5a7074f727dde53a8da031bf0e70937a48dfe10e06d`
and150372786 payload bytes. It reuses the separately built static CPU engine and
original base.en-f16 model already qualified by the independent Inference module.
Deployment metadata says macOS14.0; actual execution evidence is macOS26.5.1
ARM64 only. Linux model execution is not qualified.

- `catalog.json` pins the complete index, recipe and declared platform.
- `sources.json` binds upstream and published release URLs to that index.
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
identity. The remaining full area2 gates and other provider distributions remain
open.

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

`tests/whisper_distribution_remote.py` additionally passes normal and sanitizer
host runs against the published GitHub and immutable Hugging Face URLs. It starts
with an empty private cache, verifies every installed file's size/hash and single
link, runs2441 native transcription checks, deletes source configuration, then
resumes/removes/reinstalls from verified cache and removes the final payload.
Both exclusive workspaces are removed. Exact executable/configuration pins are
in `remote-normal.json` and `remote-sanitize.json`; logs remain at
`/private/tmp/samosa-whisper-published-{normal,sanitize}.log`. The runtime assets
were also downloaded from the staged release API and checked by size/SHA-256
before publication. No model weight was uploaded to GitHub.
