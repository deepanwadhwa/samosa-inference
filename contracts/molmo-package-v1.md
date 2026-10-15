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
not model validity or inference quality.

## Native admission

`MolmoPackagePolicy` requires three independent host-qualified SHA-256 pins:
engine, kernels and native model manifest. `molmo_package_verify` first binds the
held package directory to its absolute pathname, hashes the bounded descriptor
around parsing, resolves its index entries and compares the host pins. A runtime
pin mismatch reports `unqualified_molmo_package_runtime`; a model manifest pin
mismatch reports `unqualified_molmo_package_model`.

The gate applies shared native package verification, then opens the fixed model
directory without following symlinks. It reuses `temporal_package_verify` with
the independent manifest pin, binds that receipt to the generic index, rechecks
the held model files and revalidates the closed native package. Root pathname
binding and caller controls are checked again before success. Its descriptor
output is unchanged on failure. This establishes static admission; it performs
no MLX model load or inference and does not report readiness.

The existing temporal manifest parser now owns an 8192-token JSON budget in its
translation unit, matching its established provider builds. Package setup builds
must not accidentally inherit a smaller unrelated transport-parser budget.

`make test-molmo-package sanitize-molmo-package` passed 72 controlled refusal
checks per mode, including invalid policy/root, independent pin mismatches,
missing kernel, foreign symlink, cancellation, unchanged failure output and
refusal of a hash-consistent but invalid native model manifest. The temporal
input regression gates also passed normally and with sanitizers. Actual native
package success is tested separately with `test-molmo-package --real ROOT INDEX
INDEX_PIN ENGINE_PIN KERNEL_PIN MANIFEST_PIN`.

## Catalog setup

The catalog recipe `molmo2-mlx-package-v1` requires three host-only flags:

```
--molmo-engine-sha256 ENGINE_PIN --molmo-kernels-sha256 KERNEL_PIN
--molmo-manifest-sha256 MANIFEST_PIN
```

All three are required together. Missing policy makes discovery report the
recipe unsupported and refuses installation before acquisition/state creation.
Malformed or incomplete flags exit with code 2. Public request bodies cannot
provide policy. Both publication and completed read-only status apply the native
gate. Removal uses the cached pinned index and owned inventory without runtime
policy or credentials, including after a compatibility failure.

The portable setup fixture intentionally has an invalid native model manifest.
It must refuse publication even though all bytes match their hashes. Normal and
sanitizer setup tests cover policy/TLS refusal, failed compatibility leaving no
completion, `incomplete_unverified` status, offline failed resume preserving
copied inodes, and repeated owned incomplete removal. A synthetic fixture is not
allowed to stand in for a valid Molmo checkpoint. Full actual-package HTTPS
installation and installed inference are separate qualification gates.
