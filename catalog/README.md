# Qualified provider distributions

Ten catalog entries pass cold installation from published URLs, actual-model
inference, cache-only reuse/removal and cleanup in normal and sanitizer-equipped
macOS ARM64 hosts:

- [Whisper base.en CPU](whisper-base-en-cpu-v1):2441 actual transcription checks per mode.
- [Native T5 summary CPU](t5-summary-cpu-v1):1347 actual summary checks per mode.
- [EmbeddingGemma 2 CPU](embeddinggemma2-cpu-v1):768-dimensional normalized vectors,
  4/4 paraphrase rankings, context refusal, managed/registry parity and shutdown.
- [VisionPsy native](visionpsy-native-v1):15882 bounded heading-recognition checks per mode.
- [Molmo2 native](molmo2-native-v1):524778 bounded common-registry video checks per mode.
- [Pocket speech CPU](pocket-speech-cpu-v1):265 actual synthesis/PCM streaming checks per mode.
- [Kokoro speech CPU](kokoro-speech-cpu-v1):583 actual synthesis/PCM streaming checks per mode.
- [OpenDecision text CPU](opendecision-text-cpu-v1):10 actual public cases per mode.
- [OpenDecision image CPU](opendecision-image-cpu-v1):20 actual public cases per mode.
- [Ornith text CPU](ornith-text-cpu-v1): native counting/generation, active
  cancellation, owner isolation, shutdown and exact offline replay.

These distributions complete the declared component2 package set. Engine
lifecycle/readiness and the final module release retain their separate gates.

Each entry contains a complete pinned index, host-provisioned catalog/source
configuration, independent runtime/model policy, notices/provenance and exact
qualification receipts. Public requests select identities; App-owned provisioning
installs private configuration. The catalogs do not install themselves or grant
loaded readiness. The existing native setup/installer owns acquisition and
compatibility; no second installer is part of this directory.
