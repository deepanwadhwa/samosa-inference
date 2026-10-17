# Native Kokoro speech distribution

The existing Sherpa speech implementation, native manifest, closed-tree verifier
and native installer are reused. No model conversion, speech implementation or
second installer is introduced. The unused C++ API and unversioned ORT library
are omitted. Pocket additionally omits unrelated sample WAVs while preserving its
two configured voice references. Kokoro retains the original eSpeak data tree.

The 375-file index is
`612248e08181e1e7c77c7471cc9622f95b313e840be9871533deaadfb12ff7c7`,
with 190948102 payload bytes. Only metadata is stored here; libraries, model
weights, voice data and language data are absent. The producer reads retained
sources in place. Upload uses temporary symlink aliases and streams originals
without weight copies or hardlinks.

The [speech prerelease](https://github.com/deepanwadhwa/samosa-inference/releases/tag/inference-speech-packages-2026-10-17.1)
contains the pinned object-code assets and corresponding sources. Original
model LICENSE/README and full dependency notices accompany the native manifest.
The Pocket README caution and bundled CC BY4.0 license are both retained;
configured Pocket voice-zero samples are CC0. Sherpa includes GPL3.0 eSpeak
components. The [source publication audit](../../docs/SPEECH_SOURCE_PUBLICATION_AUDIT.md)
records the matching runtime version, inspected source graph, full notices,
13-source archive bundle and separate complete ORT source archive. Both source
assets are available publicly alongside object-code downloads.

Exact-package normal/sanitizer HTTPS setup and independently cold native
installation from published URLs pass. Each mode runs 583 existing public
speech checks, including actual synthesis, streamed PCM, retained audio ranges
and replay. All installed hashes/sizes, native cache-only resume/removal/reinstall
and temporary cleanup pass. Pocket preserves its original exact voice PCM hashes;
Kokoro preserves its original variable-PCM voice metrics and tolerance. The
underlying Sherpa/ORT libraries are release builds, not sanitizer-built.

Execution qualification is macOS26.5.1 arm64 only. The15.5 observed library
minimum is not an older-version execution claim. Installation does not grant
loaded readiness. App provisioning must copy catalog/source configuration into
owned private files; checkout permissions are not native trusted configuration.
Full component2 and the final seven-component release remain incomplete.
