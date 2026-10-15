# VisionPsy package v1

`vision_package_verify` provides static admission for the existing VisionPsy
bridge. A host-owned `VisionPackagePolicy` independently pins the executable,
config, tokenizer, weights and Metal kernels with lowercase SHA-256 values. A catalog/index
cannot qualify its own model or engine by merely declaring matching metadata.

The package contains exactly one engine and one model-role file. Its fixed
model layout matches the public bridge:

| Path | Index role | Maximum bytes |
| --- | --- | --- |
| `model/config.json` | metadata | 1048576 |
| `model/tokenizer.json` | data | 8388608 |
| `model/model.safetensors` | model | 8589934592 |
| `vision-package.json` | metadata | 4096 |
| `mlx.metallib` | data | 268435456 |

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

The pinned MLX runtime searches for `mlx.metallib` beside its executable before
trying fallback locations. The engine must therefore be packaged at the root
beside this file. Kernel bytes require a separate host pin, since successful
native library inspection alone does not establish this runtime dependency.

Setup admits the catalog recipe `visionpsy-package-v1` with five host-only flags:

```
--vision-engine-sha256 ENGINE_PIN --vision-config-sha256 CONFIG_PIN
--vision-tokenizer-sha256 TOKENIZER_PIN --vision-weights-sha256 WEIGHTS_PIN
--vision-kernels-sha256 KERNELS_PIN
```

All five are required together. Missing policy makes package catalog discovery
report the recipe unsupported and refuses installation before acquisition or
assembly state creation. Public JSON cannot inject these pins. Both assembly
publication and read-only status use the policy; removal remains independent of
credentials and runtime policy. A changed policy cannot overwrite a completed
installation. Engine pin mismatch reports `unqualified_vision_package_runtime`;
any model constituent mismatch reports `unqualified_vision_package_model`.

`make test-vision-package sanitize-vision-package` passed 672 checks in each mode
on the development macOS arm64 host. The fixture exporter also drives the shared
HTTPS setup harness from `make test-setup sanitize-setup`, covering acquisition,
publication, status/resume, last-good preservation, complete/incomplete removal
and cache-only reinstall. Actual-model and distribution qualification are
separate requirements; passing the synthetic setup fixture does not prove them.
