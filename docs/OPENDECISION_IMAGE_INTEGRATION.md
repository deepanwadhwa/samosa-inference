# OpenDecision image integration — qualified

Assignment OD-I01 follows C02/C03/C13 and the seven-area Inference plan.
The public text prerequisite is qualified at `1cfeb05`; its hosted macOS/Linux
CI run `38946753940` completed successfully. Image decisions are now qualified under the expanded area1 audit. This work belongs to the independent Inference
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

## Shared asset profile and framed worker

The existing asset verifier now has explicit text/image profiles. Text remains
limited to 1 GiB for its model; image admits at most 2 GiB for the pinned float32
graph. Engine/runtime/tokenizer caps and every held-file, closed-tree, owner,
mutation, cancellation and loader-graph check remain shared. Unknown profiles
refuse admission. Sparse boundary tests exercise both caps without allocating
model storage. `sm_decision_result_validate_profile` similarly shares the strict
Rust result schema and request-consistency checks; image relation metadata must
be `siglip_image_text`, text must be `explicit_opposite`, and image batch
relations are refused. The original C ABI remains a text-profile wrapper.

The existing native worker accepts an optional prepared raw frame through
`--image-rgba ABSOLUTE_PATH --width N --height N`. It checks a regular owned
file, exact packed RGBA size and 1..512 dimensions, prepares pixels once, and
serves the same repeated bounded length-prefixed requests. Text launches retain
their original arguments and computation. Neither mode imports Python or
decodes an encoded image file.

`tests/test_decision_image_worker.py` passes all eleven pinned actual-model cases
through normal and ASan/UBSan worker builds, covering repeated frames, clean EOF,
invalid dimensions and truncated raw inputs. Maximum typed numeric error remains
`0.00000314415`. Logs: `/private/tmp/samosa-image-worker-real.log` and
`/private/tmp/samosa-image-worker-real-sanitize.log`. The typed actual-model
harness also passes all results through the new strict image-profile validator.
Rust's eight tests and normal/SAN asset-profile checks pass locally; independent
Linux checks cover the same Rust/schema and asset logic, plus worker compilation.
Final asset checks pass 104 assertions per normal/SAN configuration on both
platforms. The combined worker also passes an original actual-model text request
with the original text assets (`/private/tmp/samosa-image-worker-text-regression.log`).

The same converted graph now sits in a flat static execution root at
`/private/tmp/samosa-decision-image-onnx-v1`; no second weight copy was made.
The export provenance moved beside it to
`/private/tmp/samosa-decision-image-onnx-v1.provenance.json` so the execution root
has exactly the four required assets. A small engine/runtime/tokenizer copy
completes the layout. Inventory `/private/tmp/samosa-decision-image-flat-v1.json`
pins those bytes. The actual-asset gate passes image-profile admission and held
recheck for the 1.5 GB graph, including the existing closed native loader graph.
This proves static acceptance, not installed-package or public readiness.

Hosted CI passed `f603265`, `d7d7a5f` and `8c375dc`. Worker/profile commit
`327d691` has a subsequent hosted run; current status must be checked there.

## Public provider integration

The existing registry/configuration supports `image_decisions` and `decide_image`
alongside `text_decisions` and all original modalities. Only the explicit image
kind admits64 context positions; other provider limits are preserved. The
existing decision dispatcher handles both profiles, reusing the strict Rust
request decoder/normalizer, operation store, scheduling lease, cancellation,
deadline and framed-file guardian. It rejects image batch relations before
launch and validates all image results with the shared profile-aware validator.

Prepared RGBA base64 decoding was extracted directly from the existing VisionPsy
adapter into `rgba_input.h`, with unchanged canonical padding and exact-size
checks. No new encoded-image decoder is added. The parent writes a bounded
owned `input.rgba`; the existing guardian now cleans that fixed filename along
with request/result frames, including on requester death. Image identity binds
RGBA bytes and dimensions; the consumer's nested state never substitutes for
that identity. Public pins, retry, result hashes and preprocessing metadata are
specified in [the shared decision contract](../contracts/decision-v1.md).

The existing public control fixture accepts image-mode launch arguments, and
the same normal/SAN control runner now covers both modalities. Image checks add
full512x512 input, canonical base64 refusal, malformed dimensions, dimension-only
and pixel-only retry conflicts, unavailable batch relations and refusal of text
relation metadata. Existing NUL/order, offline replay, owner isolation, worker
death, deadline, cancellation, mutation and requester-death cleanup all remain.
No fixture score is treated as model quality evidence.

The combined registry test now configures text and image decisions together and
checks discovery, operation separation,64-position metadata, public path hiding,
foreign-platform refusal and inherited deadlines. Existing VisionPsy and native
registry CLI tests cover the shared decoder and generic dispatcher changes.

`tests/test_decision_public.py --image` reuses the text actual-model public gate.
The pinned flat asset inventory is SHA256
`7672b919504d0994431402890a7bcc51e06f7fef5122c36e0f9ec338da973733`.
All eleven original image cases pass through fresh public processes with complete
typed result comparison, retained replay, operation status and changed timeout/
pixel retry conflict. Maximum result error is `0.00000314414501`. Normal log:
`/private/tmp/samosa-image-public-real.log`. The ASan/UBSan public adapter also
passes every case at the same maximum error, logged in
`/private/tmp/samosa-image-public-real-sanitize.log`; its external qualified worker
and prebuilt ORT are not instrumented by that public-adapter gate. Private worker
sanitizer qualification is recorded above.

Normal/SAN control gates pass on macOS and independent Ubuntu ARM64. The final
paired registry checks pass438 assertions per configuration on both platforms,
and guardian checks pass234. Minimal registry, native registry CLI and VisionPsy
normal/SAN regressions pass on both hosts. Full-frame controls ensure the public
envelope admits the base64 representation of512x512 RGBA while retaining the
1MiB nested decision-request/worker-frame limit. Decision-enabled single-modality
builds use the existing2MiB image envelope budget; combined temporal builds keep
their existing24MiB envelope budget. No weight download or duplication was used
for these checks. Hosted CI for the final public integration remains required.
The original ten-case text public actual-model suite also passes against the
updated public binary, including its full8x512 capacity case, maximum typed
error `0.000583839146`. Log:
`/private/tmp/samosa-image-public-text-regression.log`.

Executed public binary SHA256 pins are
`fa9b089dbc5e2aca169b72f5c1a8dbf794d39611ee1581897ba178c66d19a8e8`
(normal) and
`a1f641acd89e58e7563d59ffd1a206d8717dc6c959b2ce5ec1ed402449267010`
(ASan/UBSan). They are also retained in
`/private/tmp/samosa-image-public-executed-binaries.json`. The final header change
adds the decision-only2MiB envelope branch; combined-provider binaries already
use the unchanged24MiB temporal branch. Its effective decision-only macro was
independently checked with the compiler preprocessor.

Actual image-model quality is qualified on macOS ARM64 only so far. Linux
control/build tests do not establish Linux actual-model execution. Package
installation, persistent readiness/residency, global scheduling and final release
retain their independent downstream acceptance requirements. Area1 closure
requires its complete integration audit; this record does not close all seven
Inference areas.

## Expanded boundary qualification

The final requirement audit identified that the original eleven cases did not
exercise the public128-candidate limit or cross the native8-item batch boundary.
`opendecision_image_reference.py --extended` captures twenty actual original-engine
cases in `/private/tmp/samosa-decision-image-reference-v2/reference.json`, SHA256
`ca1a8222ba9eb10b33648f5f1cca18b1aa61e0d493daf006dca530603fdc2ee6`.
The original eleven captured cases and model/version/initializer identities are
unchanged. Nine additions cover name fallback,9- and128-candidate Choice,
128-level Score, the three non-unknown relation outcomes,512x512 and1x512 frames.
Both capacity Choices select the final candidate, exercising a winner beyond
the first native batch. Score preserves a complete128-position distribution;
it does not assume all probability lies at the final level.

The twenty-case private native pipeline passes normally and under ASan/UBSan,
maximum typed error `0.0000595896`. Its expanded sanitizer harness additionally
checks exact processor tensor bytes for every frame, including both new pixel
bounds. Logs: `/private/tmp/samosa-image-native-extended.log` and
`/private/tmp/samosa-image-native-extended-sanitize.log`.

The nine additions pass through normal and sanitized public adapters, maximum
typed error `0.0000595896272`, with complete result comparison, retry/status and
pixel identity conflicts. The existing eleven-case public proof remains valid:
the first eleven extended captures are identical, and the executed normal/SAN
public binary hashes above are unchanged. Thus the public evidence covers all
twenty cases, rather than treating a selected subset as the full corpus.
Logs: `/private/tmp/samosa-image-public-extended.log` and
`/private/tmp/samosa-image-public-extended-sanitize.log`. Reproduce additions with
`test_decision_public.py --image` and repeated `--case NAME` selections; omitting
selectors runs the complete pinned report and unknown/duplicate selectors refuse.

The existing tokenizer corpus/replay now splits original HF matrices into the
native8-row batches. All51 complete matrices match exactly on macOS and Linux.
Extended corpus SHA256:
`23c700545b49e83afc824fedc70c82836f881f737a76a88c670e7afabab3f248`.
The model-free processor gate now checks twelve reference-pinned RGB/RGBA
tensors, including512x512 and1x512, normally and under sanitizers on both hosts.
The combined registry additionally configures all nine provider kinds in one
instance; its normal/SAN gates pass459 checks on both platforms. Logs:
`/private/tmp/samosa-image-pixels-extended-linux.log`,
`/private/tmp/samosa-image-nine-provider-registry.log` and
`/private/tmp/samosa-image-nine-provider-registry-linux.log`.

No production computation changed for these additional cases, and no weights
were downloaded, copied or re-exported. The extra18MiB reference directory holds
only tokenizer and captured inputs/results. Full exported-source checks passed on macOS/Linux, and hosted run38973353427
passed at46612a4. The expanded provider audit therefore closes area1, while
installation and all area2–7 requirements remain open.

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
