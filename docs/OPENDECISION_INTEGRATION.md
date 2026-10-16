# Existing OpenDecision inference to preserve

The user identified OpenDecision as an existing Samosa dependency and its new
image decisions as required context for the inference extraction. Inspection used
upstream and the clean local checkout at revision
`cbd0b50db42bba6975f342de1789fb3c8f2dc670`:
[OpenDecision](https://github.com/deepanwadhwa/OpenDecision/tree/cbd0b50db42bba6975f342de1789fb3c8f2dc670).

## Existing Chat use

Chat's `tools/samosa_decision.py` imports `OpenDecisionEngine` and explicitly
loads the locally pinned text model
`MoritzLaurer/deberta-v3-large-zeroshot-v2.0`, revision
`cf44676c28ba7312e5c5f8f8d2c22b3e0c9cdae2`. Routing, document scoring and
memory decisions call this wrapper. The reviewed wrapper has no image-engine
integration. Existing application rules and side effects must remain owned by
their calling modules; extracting inference must preserve their model results.

## Image backend and semantics

OpenDecision's [image engine](https://github.com/deepanwadhwa/OpenDecision/blob/cbd0b50db42bba6975f342de1789fb3c8f2dc670/src/opendecision/images.py)
uses `google/siglip2-base-patch16-224` through a complete image/text model and
processor. Its typed decisions are:

| Operation | Existing behavior |
| --- | --- |
| Choice | Softmax over candidate-description matching logits; choose the maximum |
| Noul | Sigmoid of one visual statement, or softmax over explicit true/false descriptions |
| Score | Expected position on the caller's ordered description scale |
| Relation | Independently threshold support and contradiction sigmoid scores into supports, contradicts, unknown or conflicted |

For image Choice and Score, instructions are carried in the API but do not
condition the scoring; candidate descriptions are the model inputs. Scores are
visual matches, not calibrated truth or text entailment. Text processing uses
fixed maximum length 64 with truncation and maximum-length padding. These
details must survive reuse or any native backend implementation; a generated
caption followed by a text decision is a different computation.

The upstream image decoder accepts bounded single-frame JPEG/PNG/WebP bytes,
applies EXIF orientation and converts to RGB. In Samosa, prepared image identity
and preprocessing must be reconciled with Sources rather than silently decoding
the image twice under different policies. Logical text-token accounting does
not account for image bytes or vision memory/compute.

## Relationship to VisionPsy

VisionPsy generates descriptions and answers using a SigLIP2 512-pixel visual
tower, connector and SmolLM2 language model. OpenDecision uses a separate
224-pixel image/text matching checkpoint and typed scoring. VisionPsy's
checkpoint does not substitute for the complete dual encoder and processor.
Both capabilities belong in the inventory; completing VisionPsy installation
does not establish OpenDecision image support.

## Qualification gap

The inference provider registry currently has no OpenDecision decision provider.
This is an uncovered integration requirement, not completed work. Before the
extraction is qualified, preserve the existing text decisions, add the image
scoring capability, independently pin/check its model and processor assets,
exercise typed-result parity against the existing engine, and route callers
through the inference service's ownership and control mechanisms. Application
action rules remain with their consumers.

The local Hugging Face cache contains a SigLIP2 224 snapshot referenced by
`75de2d55ec2d0b4efc50b3e9ad70dba96a7b2fa2`. This is an observed asset location,
not a qualified model pin or permission to trust a mutable cache ref. Full asset
inventory, hashes, package compatibility and actual-model tests remain required.
The existing OpenDecision Python backend is a parity reference; introducing a
new shipped Python dependency has not been established as the module design.

Keep implementation sequential: finish the OpenDecision text provider first,
then its image provider. Qualify their package dependencies and the remaining
production catalogs before declaring model acquisition complete or advancing
to engine lifecycle. Completed Molmo2 and VisionPsy evidence remains below.

## Pinned text reference checkpoint

The Molmo2 installation and affected VisionPsy regressions are now qualified;
OpenDecision is the active prerequisite. `tests/opendecision_text_assets.json`
pins the seven existing Chat checkpoint files, including weights SHA-256
`2031ec34340911b2cecf4f95f5e24db91b2a5d7ea0fa1e704e9cd6d61585e477`.
This is an inventory of the existing local checkpoint, not an installed native
package or production distribution entry.

`tests/opendecision_reference.py` imports a tracked-source export at the revision
above, validates the original checkpoint before and after execution, and records
actual input tensors, logits and typed results. It preserves the existing CPU,
four-thread and batch-eight settings. Nine actual-model cases passed twice with
identical tensors, logits and results: Chat routing, duplicate descriptions,
positive/negative predicates, explicit binary criteria, ordered scoring,
support/contradiction and structured Unicode state. The routing fixture selected
`find`. This reference uses the existing Python environment only for QA; no
Python provider or runtime is added to the module.

The observed environment is Torch 2.14.0, Transformers 5.17.0, Tokenizers 0.23.2
and SentencePiece 0.2.2. The actual model uses float16 weights, two output labels
(`entailment` and `not_entailment`), a 512-position limit and a Hugging Face
Unigram tokenizer backend. The effective tokenizer differs from the cached JSON
in its normalizer, pre-tokenizer and decoder. Its exported initializer has
4242551 bytes and SHA-256
`7caa16d3d730231fc7b9c2db4716e3e38736880bd4f2e016bb37987645e4900a`.
The existing native BPE helper cannot be substituted for this computation.

Text Choice runs two profiles and, on disagreement, a third adjudication;
the Chat routing fixture exercises all three. Preserve its rendering, original
candidate order, collision handling and selection of the returned probability
distribution. Noul uses per-pair entailment probability, or softmax over explicit
true/false entailment logits. Score uses its question-specific hypothesis
template and zero-based expected position. For this two-label checkpoint,
Relation uses independent entailment scores for explicit opposite statements;
it must not pretend to expose a native three-way NLI distribution.

Private reference report SHA-256:
`7afae3530f696e67abffeb41b91620b7af335fc4adf13b0398fa1cccf90f7d33`.
Reproduce with the existing OpenDecision environment:
`python tests/opendecision_reference.py OPENDECISION_REPOSITORY MODEL_DIRECTORY
NEW_REPORT_JSON`. The matching `.tokenizer.json` is written beside the report.
The native model backend, tokenizer parity, typed provider and package gates
remain open. An existing qualified speech runtime contains ONNX Runtime and
exports `OrtGetApiBase`; using it for this model still requires execution proof.


## Native backend checkpoint

The native tensor primitive now executes all 14 extended reference batches in
normal and sanitizer builds using the existing ORT runtime, including eight
512-position inputs in one batch. See [backend evidence](OPENDECISION_BACKEND_VERIFICATION.md)
for artifact pins, numerical tolerance, independent build inputs and platform
limits. This closes the raw model execution prerequisite. Native tokenizer
parity and the typed public provider are separate prerequisites; no public
OpenDecision provider is advertised yet.

The private [native tokenizer](../engines/decision/tokenizer/README.md) now
preserves the effective Hugging Face Unigram initializer through the exact
Tokenizers 0.23.2 Rust implementation. macOS ARM64 passed 1,187 successful exact
three-matrix comparisons plus expected refusals against the original HF wrapper.
Ubuntu 24.04 ARM64 replayed all 1,222 pinned corpus cases with its independently
compiled library. The shared CI entry point builds the locked tokenizer and
tests its C ABI. Typed scoring, provider integration and model packages remain
open; tokenizer parity alone does not preserve complete decision results.

The first [text scoring primitives](OPENDECISION_TEXT_SCORING_VERIFICATION.md)
now preserve predicate, explicit binary, explicit-opposite relation, multi-profile
Choice and ordered Score postprocessing. Nine actual-model captures agree within
`1e-7` probability on macOS and Ubuntu ARM64 in normal and sanitizer builds;
102 branch checks additionally cover ordering, rendering and refusal. Native
model/tokenizer connection and public service integration remain open.
