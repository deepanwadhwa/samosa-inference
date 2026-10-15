# Molmo2 installation package v1

The descriptor and model binding primitives bridge the shared installation index
to the existing native temporal model manifest. They do not replace the model
verifier or qualify installation on their own.

The root metadata file `molmo-package.json` is bounded to 4096 bytes. Its strict
four-field descriptor is:

```json
{"format":"samosa.inference.molmo-package.v1","profile":"molmo2-4b-mlx-q4-v1","engine":"samosa-molmo2","model_directory":"model"}
```

The engine must be the single indexed executable and reside at package root,
beside `mlx.metallib` (data role, at most 256 MiB). The model directory is fixed
to `model`; its native `manifest.json` has metadata role and is at most 1 MiB.
Model files are flat relative to that directory, matching the existing temporal
verifier's supported manifest layout. At least one model-role file is required.
Model-role files outside this directory refuse descriptor resolution. Additional
engine libraries and metadata remain subject to the common closed package checks.

`molmo_package_descriptor_parse` resolves engine, kernel and model-manifest index
positions. It validates strict format/profile/layout, index file names, pins,
roles and bounded sizes, and leaves its output unchanged on failure. It does
not hash any payload or inspect a native executable.

After `temporal_package_verify` succeeds on the bound model directory,
`molmo_package_bind_model` compares its receipt with the generic package index.
Every declared model file must occur exactly once with matching bytes and hash.
Safetensors files use model role, `tokenizer.json` uses data role, and remaining
native manifest files use metadata role. The native manifest itself is accounted
for separately. Missing, duplicate, substituted or additional files in the model
namespace refuse binding. The native verifier retains its existing upstream,
processor, quantization and fixed config/tokenizer pins.

`make test-molmo-package-descriptor sanitize-molmo-package-descriptor` passed 302
checks per mode on the development macOS arm64 host: descriptor truncations,
unknown fields, role/size/layout conflicts, output preservation and exact
manifest/index mapping. Synthetic metadata establishes these composition rules,
not model validity or inference quality. Host runtime/kernel/manifest policy,
native file verification and setup dispatch remain required before the recipe
can be advertised as installable.
