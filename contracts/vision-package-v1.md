# VisionPsy package v1

`vision_package_verify` provides static admission for the existing VisionPsy
bridge. A host-owned `VisionPackagePolicy` independently pins the executable,
config, tokenizer and weights with lowercase SHA-256 values. A catalog/index
cannot qualify its own model or engine by merely declaring matching metadata.

The package contains exactly one engine and one model-role file. Its fixed
model layout matches the public bridge:

| Path | Index role | Maximum bytes |
| --- | --- | --- |
| `model/config.json` | metadata | 1048576 |
| `model/tokenizer.json` | data | 8388608 |
| `model/model.safetensors` | model | 8589934592 |
| `vision-package.json` | metadata | 4096 |

The descriptor contains exactly four fields:

```json
{"format":"samosa.inference.vision-package.v1","profile":"visionpsy-v1","engine":"engine","model_directory":"model"}
```

`engine` must name the single indexed engine. The profile and model directory
are fixed. Extra fields, role conflicts, missing required files and malformed
descriptors refuse admission. Additional pinned assets may exist, subject to
the shared closed package namespace and native library graph verification.

The verifier hashes the descriptor around parsing and applies the common native
package verifier, including full payload hashes, actual host architecture,
minimum OS and dependency closure. Reads and hashes observe caller controls.
Failure leaves the output receipt unchanged.

The exact independently qualified model pins establish the supported model
bytes; this function does not parse safetensors, load MLX, run inference or
report engine readiness. The synthetic tests exercise admission and hostile
metadata, not model quality.

`make test-vision-package sanitize-vision-package` passed 643 checks in each mode
on the development macOS arm64 host. Public setup dispatch, actual installed
model qualification and production catalog publication remain subsequent work
within component 2. This primitive alone does not advertise an installable recipe.
