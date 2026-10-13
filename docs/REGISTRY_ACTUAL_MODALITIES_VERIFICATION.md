# Actual Whisper and Vision through the common registry

The existing actual-model harnesses now support `SAMOSA_TEST_USE_REGISTRY=1`.
They construct a private host configuration containing the actual modality and a
separate explicit fake-text provider, verify discovery and routing, then retain
all original quality, provenance and replay assertions. Default standalone modes
remain available. The fake provider does not supply actual-model evidence.

Whisper's Sources transport uses its existing explicit argument file with
`["--config", "<private registry config>"]`. Sources selects the provider already
configured in its host contract; neither source data nor public requests choose
an executable, owner or state path. Full-duration tests use this same transport.
No product adapter or Sources validator was relaxed.

Local macOS ARM64 results in normal/sanitizer builds:

| Gate | Checks in each mode | Preserved assertions |
| --- | --- | --- |
| Actual VisionPsy |15918 | Existing heading fixture resized to395x512 RGBA, native observation `helloworld`, exact prepared input hash, pinned engine/model provenance, uncertainty and exact retained replay |
| Actual Whisper/Sources WAV |2489 |2672ms prepared audio, exact PCM hash, expected coriander/sample transcript, ordered bounded timed segments, uncertainty and offline capture/operation replay |
| WAV durable SDK publication |2673 | Actual model capture, durable audio queue, public Chutni publication/readback and exact embedded Sources provenance |
| MP3 durable SDK publication |2673 | Existing qualified media worker decodes the compressed input; independent reference PCM hash, native transcript, SDK readback and exact provenance/replay |
|66-second WAV |2682 | Three overlapping30/30/8-second windows, independently computed PCM hashes, initial/middle/late speech, source-time bounds, native capture, SDK publication and origin/model-free retained Sources readback |

Commands use `build/test-vision-real`/`-sanitize` with arguments: absolute registry
CLI, existing `build/samosa-visionpsy` engine, `~/.samosa/models/visionpsy`, and
`/private/tmp/samosa-area4-vision-hello.ppm`. Whisper harness arguments are registry
CLI, Sources CLI, installed `~/.samosa/voice/runtime/whisper-cli`,
`~/.samosa/voice/stt-whisper-base-en/ggml-base.en.bin`, and reference WAV.
Short WAV is `/private/tmp/samosa-whisper-real-native-bISwK3/files/speech.wav`;
full WAV is `/private/tmp/samosa-whisper-full-duration.wav`.

Both harnesses use `SAMOSA_TEST_USE_REGISTRY=1`. SDK modes additionally set
`SAMOSA_TEST_CHUTNI_LIBRARY=/private/tmp/samosa-chutni-artifact-qa/libchutni.dylib`
and `SAMOSA_TEST_CHUTNI_WORKER` to the corresponding freshly built Sources worker.
Full-duration mode sets `SAMOSA_TEST_FULL_DURATION=1`. Sources executables were
built from the independent Sources submodule with `make all
build/samosa-sources-inference-sanitize chutni-worker
build/samosa-sources-chutni-worker-sanitize`.

Compressed-audio mode appends the qualified media-worker executable and encoded
input to the Whisper arguments, with decoded reference WAV in the fifth argument.
The files are `/private/tmp/samosa-whisper-mp3-reference-mac.wav` and
`/private/tmp/samosa-whisper-speech.mp3`; media workers are the normal/sanitizer
executables under `/private/tmp/samosa-source-final-native/models-mac/build`.

The first SDK readback invocation selected the older Chat `build/chutni` library
and correctly refused `chutni_response_invalid`. Sources' declared dependency
contract requires the newer artifact-content range extension; the older reply
lacks required range metadata. Selecting the already qualified artifact-read SDK
fixes the invocation. The original response validation and all quality assertions
remain unchanged. The earlier fake routing assertion also expected a generation
quality field from a token-count result; it now checks the existing fake tokenizer.

Exact asset hashes accompany this record in `REGISTRY_ACTUAL_ASSETS.json`.
These are observations of explicit installed/local assets, not production model
catalog entries or independent native engine builds. External Whisper/Vision/SDK
and codec libraries are not sanitizer-built. Sanitizer harnesses, the common
Inference registry and Sources adapters are sanitizer-built.

Regression gates pass Whisper508/511, Vision634/644 and registry CLI427/430 in
normal/sanitizer builds. Hosted CI also compiles the optional actual-model harnesses
on both native platforms; actual execution requires explicitly supplied artifacts.

Independent archive CI passes all configured gates on macOS14/ARM64 and
Ubuntu24.04/x86-64 at `6c5c3e1855ff56cd950c7c6f1cba6c03c8b15c96`:
[run38464409173](https://github.com/deepanwadhwa/samosa-inference/actions/runs/38464409173).
Both logs confirm normal/sanitizer builds of all four optional actual-model
harnesses. Controlled Whisper passes508/514 on macOS and511/589 on Linux;
controlled Vision passes634/644 and639/769 respectively. Counts vary with polling
and platform-specific native checks; both complete gate commands pass.
This CI evidence does not substitute for actual-model execution on Linux.

Actual Linux modality execution, other configured models, generic vision quality,
persistent readiness, global residency/scheduling, public incremental events,
authenticated service and legacy Chat retirement remain open. This checkpoint
does not close a numbered area or final release.
