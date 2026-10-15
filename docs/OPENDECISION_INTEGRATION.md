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

Keep the current installation work sequential: finish the in-flight VisionPsy
qualification, then resolve this newly identified provider/package dependency
before declaring model acquisition complete or advancing to engine lifecycle.
