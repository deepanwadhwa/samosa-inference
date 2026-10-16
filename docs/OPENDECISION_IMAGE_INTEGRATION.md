# OpenDecision image prerequisite — active

Assignment OD-I01 follows C02/C03/C13 and the seven-area Inference plan.
The public text prerequisite is qualified at `1cfeb05`; its hosted macOS/Linux
CI run `38946753940` completed successfully. Image decisions remain
required before area1 can close. This work belongs to the independent Inference
repository; Sources owns image decoding/prepared pixels, Agent owns application
policy, and App assembles pinned public dependencies.

## Reuse audit and observable deliverable

The existing `ImageDecisionEngine` at OpenDecision revision
`cbd0b50db42bba6975f342de1789fb3c8f2dc670` already specifies the computation.
Preserve its typed Choice, Score, Noul and Relation results over actual
`google/siglip2-base-patch16-224` matching logits. Do not substitute generated
captions, text entailment or VisionPsy's separate512-pixel tower.

Reuse the already qualified native ORT runtime/backend patterns, locked Rust
Tokenizers implementation, typed result validators, public registry, operation
store, admission, asset verification and guardian. Inspect the existing Molmo2
bilinear and VisionPsy preprocessing before choosing native pixel code. Their
current algorithms must not be silently treated as PIL resize parity: the
pinned image processor specifies PIL bilinear (`resample:2`) with224x224 output,
rescaling by1/255 and normalization mean/std0.5. Text is truncated/padded to64.
Sources-prepared pixels and their identity cross the public boundary; a new
private image-file decoder is excluded.

First qualify a pinned original-engine reference with observed processor input
tensors and logits, reusing the existing text reference inventory/finite helpers
and immutable tracked-source export. The six existing cached assets are now
inventoried in `tests/opendecision_image_assets.json`. No model was downloaded or
copied, and inventory alone does not qualify a model/runtime package.

## Captured reference and tokenizer evidence

The original CPU engine passed all eleven cases below using the existing cached
weights. The report is `/private/tmp/samosa-decision-image-reference-v1/reference.json`,
SHA256 `228d574b40c0a5634bd5b937f9e5f637deea749718bc81674f1a2be36ed3c3a5`.
It pins the actual model inputs, raw matching logits and complete typed results.
Pixel blobs include both a patterned non-square downsample and a small upscale.
The temporary report directory contains approximately 15 MB of reference data
and the effective tokenizer, with no copied model weights.

The effective tokenizer is 13,718,435 bytes, within the existing native 16 MiB
bound, SHA256 `9846872af4d6df792f3931d68b65394370948d3cf80fa8974743a006facd351e`.
The existing Rust tokenizer reproduced input IDs, attention masks and token type
IDs for all eleven cases on macOS and independently built Linux ARM64. The
corpus SHA256 is `86bdc30bd2e0bd0f8b3851c6c535433c4fecbaf0f09845ceb2ed4106ee3593bf`.
Only the generic QA replay gained an optional initializer pin; the native
tokenizer implementation and bounds remain unchanged.

These captures also preserve a significant semantic distinction: red-versus-blue
choice confidence is approximately 0.997, while the independent red predicate
sigmoid is approximately 0.243. The relation therefore returns `unknown` at a
0.5 threshold. Native qualification must preserve those results rather than
treating relative choice confidence as predicate calibration.

## Native tensor execution

`tests/opendecision_image_export.py` reuses the text exporter digest/inventory
helpers and verifies exact original-model logits for every captured tensor
before exporting. The sole converted artifact is outside the checkout at
`/private/tmp/samosa-decision-image-onnx-v1/model.onnx`: 1,501,166,742 bytes,
SHA256 `dd3301b3310117bc09f6e8e1e10af9bab98fdc38d6c41e907ba148d1938fb73b`.
ONNX 1.18.0 full model validation passes. Provenance remains explicitly a
candidate, not a distributable or installed model package.

The private `OrtImage` primitive adds the fixed SigLIP signature: int64
`input_ids[candidates,64]`, float32 `pixel_values[1,3,224,224]` and float32
`logits_per_image[1,candidates]`. It shares runtime API initialization, CPU
threading and graph options with the existing NLI primitive through
`ort_runtime.h`. It validates bounds, vocabulary, normalized finite pixels,
output signature and finite logits, preserves caller tensors and leaves the
loaded session intact after a failed replacement.

Normal and ASan/UBSan native builds each pass all eleven captured tensor cases
on macOS ARM64 with ORT 1.27.0. Maximum absolute logit error is
`0.0000181198`, below the predeclared `0.005` gate. Refusals preserve outputs;
the existing model continues inference after failed replacement. The adapter
and harness are instrumented; the prebuilt ORT library is not. Logs are
`/private/tmp/samosa-decision-image-native-v1.log` and
`/private/tmp/samosa-decision-image-native-sanitize-v1.log`.

Reproduce with `make build/test-decision-image-ort-real` (or its `-sanitize`
variant), supplying existing `DECISION_ORT_INCLUDE`, `DECISION_ORT_LIBRARY` and
`DECISION_ORT_RPATH`, then invoke the resulting harness with model path, report
path, model SHA256, report SHA256 and tolerance `0.005`. These explicit targets
download nothing. No image model execution on Linux is claimed by this proof.
Strict GCC 13 C++20 compilation checks pass in the independent Ubuntu 24.04
ARM64 container for both model primitives and the image harness. The existing
actual NLI regression also passes all fourteen captured batches after the shared
runtime extraction, with unchanged maximum logit error `0.005859375`.

## Prepared pixel qualification

The existing Molmo2 half-pixel bilinear sampler lacks PIL's downsampling
antialias kernel and quantized intermediate RGB pass. It cannot reproduce the
patterned non-square reference. The small private `image_pixels` adapter supplies
that missing processor step without adding image decoding or changing Molmo2.
It admits bounded packed RGB/RGBA (maximum 512 per axis), discards alpha exactly
as the original RGB conversion, performs separable bilinear antialias resizing
with byte-rounded intermediate pixels, then emits planar float32 normalization.
Failure preserves the caller's previous tensor.

The actual-model harness now prepares the original RGB pixels and verifies exact
float tensor equality against every captured processor input. It separately
verifies RGBA with varying alpha. All eleven model cases continue to pass in
normal and ASan/UBSan builds, maximum logit error `0.0000181198`. Logs:
`/private/tmp/samosa-decision-image-pixels-native-v1.log` and
`/private/tmp/samosa-decision-image-pixels-native-sanitize-v1.log`.

`test-decision-image-pixels` and `sanitize-decision-image-pixels` add eight
reference-pinned tensor hashes and invalid-input/output-preservation checks to
standard model-free CI. Expected hashes come from the pinned original report,
not the native algorithm. This gate runs without weights, ORT, Pillow or Python
model dependencies. Native macOS and Linux ARM64 tests pass in both
configurations; Linux checks use the same independent source container.

## Typed native image decisions

`NativeImage` reuses the locked Rust tokenizer and the text bridge's JSON quoting
and tensor decoder (now shared in `token_json.h`). It submits bounded batches of
eight descriptions at 64 positions to `OrtImage`, then preserves order while
collecting logits. Full candidate distributions span all admitted descriptions;
batch boundaries do not independently normalize scores.

The existing `execute_request` decoder and serializer now accept an internal
`DecisionBackend`; the original `PairInference` entry remains the text adapter.
Image matching supplies only its modality-specific computation: candidate-wise
softmax, single-statement sigmoid, binary softmax, expected ordinal score and
independent relation sigmoids. Entropy confidence is shared with existing text
scoring in `evidence_math.h`. Image choice/score instructions remain unused;
duplicate descriptions retain first-winner ties. Image relations report
`siglip_image_text`; the unimplemented image batch-relations method is refused.

`test_decision_image_native_real.cpp` verifies pinned weights, initializer,
reference and RGB blobs, runs native preprocessing, tokenization, model execution
and shared typed request/result serialization, and compares complete ordered
results against the original engine. All eleven cases pass with maximum typed
numeric error `0.00000314415` (gate `0.005`). Failed model/tokenizer replacements
leave the prior engine usable and malformed requests preserve prior output.
The normal/SAN targets are `build/test-decision-image-native-real` and its
`-sanitize` variant, using the same explicit local SDK/runtime Make variables.
Arguments are model, tokenizer, report, then their three SHA256 pins. No models
are downloaded or copied by these targets.

Typed actual-model logs are `/private/tmp/samosa-image-typed-native.log` and
`/private/tmp/samosa-image-typed-native-sanitize.log`. The original text scoring
and shared request checks pass normal/SAN on macOS and independent Linux ARM64.
Standard model-free CI also compiles the image bridge without linking ORT.
Actual typed image execution on Linux is not established by these checks.

Public image registry/configuration, asset profile admission and worker transport
remain unfinished. This private proof does not close area1 or any downstream
Inference area.

## Acceptance and scope

OD-I01 captures all original methods, candidate order/collisions, instructions
being unused for Choice/Score, explicit true/false scoring, relation threshold
and backend metadata, ordered scales, Unicode/64-token limits, non-square
resize and low-resolution inputs. Actual red/blue matching has predeclared
simple color expectations; broader image accuracy is not inferred.

The next native stage must match processor/token tensors, logits and complete
typed results against those fixed captures, preserving original method semantics.
Then adapt the existing public controls and qualify model ownership, pins,
refusals, cancellation/deadlines, retry and independent builds. Native package
catalog/install acceptance remains area2. Persistent engine readiness/residency,
global scheduling, sessions/streaming, service and complete release retain their
original downstream requirements.

No production runtime may depend on Python. Developer-only reference/export
harnesses use the existing environment under the architecture plan's explicit
runtime clarification. All emitted QA files go to a single temporary report
root; no extra model weight copies are created merely for test layouts.

Existing legacy image call sites: none in `tools/samosa_decision.py`; original
text callers remain until later replacement integration qualifies. This stage
adds the user-required existing OpenDecision image computation; it does not
move application action rules into Inference.
