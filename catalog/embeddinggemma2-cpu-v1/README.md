# EmbeddingGemma 2 CPU distribution

This reuses the existing pinned static CPU llama-server and retained Q8_0 text
model through the original GGUF installer, managed owner/activation controls and
public embedding registry. There is no new inference implementation, conversion
or downloader. The ten-file index is
`fb11fc6c5885830282cc8e0d03d12dfbf7ceae0202bd4fb97c369d1af382a3ea`,
with325842726 payload bytes. The model remains at its immutable upstream URL;
neither model nor engine bytes are stored in this source repository or copied
into preparation output.

The engine's observed deployment minimum is macOS26.0. Catalog and index declare
26.0, so an older host refuses before acquisition. Actual execution qualification
is macOS26.5.1 ARM64 only. No Linux or older-macOS execution is claimed. Only text
embeddings are exposed; media/projector capabilities of the broader source model
are outside this package's public operations.

The pinned [converted model card](https://huggingface.co/ggml-org/embeddinggemma-2-GGUF/blob/bfcd298762cc34d0357ece5ebdd31791a3a374d8/README.md)
and [Google source-model card](https://huggingface.co/google/embeddinggemma-2/blob/914f7f89142e33e77833254d9c9b90c3cef7303b/README.md)
both declare Apache-2.0. The converter's `.src_sha` binds that original revision.
Both complete cards, Apache model license and MIT native runtime notice are
included with independent expected hashes. The immutable model endpoint reports
309855456 bytes and SHA-256
`2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135`,
matching the retained actual model.

Normal and sanitizer-equipped hosts pass the shared per-file HTTPS setup gate,
policy/TLS refusal, publication, last-good preservation, cache-only reuse/removal
and actual managed/registry inference from fresh installed files. The original
`embedding_real.py` checks finite768-dimensional L2-normalized vectors,4/4
paraphrase rankings, exact input/runtime/model provenance, long-input refusal,
single/stream parity, replay after shutdown without credentials and clean engine
shutdown. Exact host/harness/log hashes are in `qualification.json`; logs remain
at `/private/tmp/samosa-embedding-distribution-{normal,sanitize}.log`.

The [package prerelease](https://github.com/deepanwadhwa/samosa-inference/releases/tag/inference-embedding-package-2026-10-17.1)
contains ten non-model runtime/metadata/index assets, all downloaded back and
checked by name, size and SHA-256 before publication. The model stays upstream.
Both normal and sanitizer-equipped remote gates pass cold acquisition from the
real URLs, exact installed file verification, actual vector/managed/registry
checks, cache-only reuse/removal and exclusive workspace cleanup. The installed
recipe is compared with the actual managed harness configuration. Exact
configuration/executable pins are in `remote-normal.json` and
`remote-sanitize.json`; logs remain at
`/private/tmp/samosa-embedding-published-{normal,sanitize}.log`.

The shared setup fixture suite passes normal/SAN on macOS ARM64 and Ubuntu ARM64;
logs are `/private/tmp/samosa-embedding-setup-regression-{macos,linux}.log`. Those
are controls/platform checks, not Linux embedding model execution. The minimum-OS
preparation extension reproduces all published Whisper/T5 bytes unchanged.
Installation is not loaded readiness. Full area2 and other distributions remain
open. App-owned provisioning must copy catalog/source configuration into private
files before native setup. Preparation retains about84KiB of metadata, with no
retained new model or engine copy.
