# OpenDecision native tensor backend

The private `engines/decision/ort_nli` primitive executes the existing Chat
DeBERTa checkpoint through the ONNX Runtime already used by the qualified Pocket
speech runtime. It is not yet a public decision provider. Native tokenization,
question/candidate rendering, typed decision aggregation, common controls and
package admission remain required before that provider can close.

## Source and artifacts

The original seven checkpoint files remain pinned by
`tests/opendecision_text_assets.json`. `tests/opendecision_export.py` requires a
separately pinned reference report and matching dependency versions. It checks
all 13 original post-tokenization batches against the direct float16 PyTorch
model exactly before exporting, validates the ONNX graph, and rechecks original
checkpoint bytes. Its explicit legacy export uses dynamic batch/sequence axes
and opset 17. Exporting is a QA/build activity, not a shipped Python backend.
ONNX 1.18.0 and Protobuf 5.29.5 were installed in a private target directory;
the existing OpenDecision environment was preserved.

| Artifact | Bytes | SHA-256 |
| --- | ---: | --- |
| ONNX float16 model | 872090282 | `15f64c4741ace31bb4130c241bdfdcd48b1d76910210d96f4ee86dacddd54398` |
| Effective tokenizer | 4242551 | `7caa16d3d730231fc7b9c2db4716e3e38736880bd4f2e016bb37987645e4900a` |
| Existing ORT runtime | 28025584 | `5b4c4df69e6d1bc10eb0d08da525b11faf8365a0a78cc597a5cc57926330a9ca` |

The model was exported against reference report
`7afae3530f696e67abffeb41b91620b7af335fc4adf13b0398fa1cccf90f7d33`.
The extended reference adds four long explicit-opposite relations, producing a
full eight-row, 512-position batch. Its other nine cases repeat the original
reference exactly. The extended report SHA-256 is
`f2c54e4d1be67059c184b9a2326b1b7b96ee7b1de02b6efcaeef5cbf3951dc11`.

`ort_sdk_pins.json` pins the five public API headers and MIT license from
[ONNX Runtime source](https://github.com/microsoft/onnxruntime/tree/8f0278c77bf44b0cc83c098c6c722b92a36ac4b5),
API version 27. The existing runtime reports version 1.27.0. A native qualification
copy contains only this byte-pinned runtime and the declared module sources;
it has no dependency on Chat's private implementation or a Python model process.

## Native execution

The primitive validates dynamic int64 `input_ids`/`attention_mask` signatures and
float16 `[batch,2]` outputs. It bounds batches to eight and sequences to 512,
checks vocabulary/mask values, rejects empty attention and non-finite outputs,
and publishes the output vector only on success. Runtime-owned input copies
preserve caller tensors. Failed model replacement preserves the old session.
Calls on an instance are serialized by its owning engine. CPU execution uses
four intra-op threads, one inter-op thread and basic graph optimization.
File identity, operation ownership and admission remain the provider's duties.

Normal and ASan/UBSan builds each passed 10753 checks over 14 captured batches,
including the full 8-by-512 boundary, invalid inputs, output preservation and a
failed replacement followed by successful inference. The maximum absolute logit
error against the original float16 model was 0.005859375, below the pre-set 0.01
tolerance. This is numerical tensor parity, not bitwise equality or qualification
of the future typed decision algorithm. ORT itself remains a prebuilt release;
the local C++ wrapper, public inline API and test harness are instrumented.

`tests/opendecision_native_backend.py` reproduced both passes in an exclusive
source/SDK/runtime snapshot, refusing loader overrides and checking input pins
before and after qualification. Source hashes, compiler and dependency pins are
retained in `qualification.json`. The local artifact is
`/private/tmp/samosa-decision-native-backend-qualified-v1`.

Reproduce with `python3 tests/opendecision_native_backend.py SDK_DIRECTORY
RUNTIME_LIBRARY MODEL_ONNX REFERENCE_JSON NEW_OUTPUT_DIRECTORY --runtime-pin
RUNTIME_SHA --model-pin MODEL_SHA --reference-pin REFERENCE_SHA`. The SDK directory
contains the six pinned files, including `LICENSE`; the library filename must
match its loader identity. All inputs are explicit local dependencies.

Actual execution is qualified on macOS 26.5.1 arm64 with Apple Clang 21. Strict
C++20 syntax checks also passed on Ubuntu 24.04 arm64 using GCC 13.3.0 against
the same pinned headers. Linux model execution and a distributable decision
engine/package are not established by that compiler check. The next prerequisite
is exact native tokenization against the captured backend, followed by typed
scoring and public provider integration. Image decisions follow the text provider.
