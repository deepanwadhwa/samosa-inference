# OpenDecision package descriptor v1 — integration in progress

The bounded `decision-package.json` declaration binds the existing native worker
profile to an already pinned `PackageIndex`. Recipe `opendecision-package-v1`
uses the existing catalog installer, compatibility dispatch and public setup.
Controlled installation is qualified; actual installed-model and production
catalog qualification remain in `OPENDECISION_PACKAGE_INTEGRATION.md`.

```json
{"format":"samosa.inference.decision-package.v1",
 "profile":"image","protocol":"opendecision-framed-v1","assets":"assets",
 "runtime_revision":"327d6911fc7f31e779d2fa673e83ba1f3410b468"}
```

All five fields are required, with no extra or duplicate keys. The body is at
most4096 bytes. `profile` is `text` or `image`; protocol and asset directory are
the fixed strings shown. `runtime_revision` is a40-character lowercase hex source
revision, which must later agree with independent host qualification policy.
The declaration does not contain commands, URLs, credentials or loader settings.
It cannot qualify its own runtime or model by naming a revision.

The index must include this descriptor as metadata and exactly four files in
the `assets/` subtree:

| Relative name | Index role |
| --- | --- |
| `assets/decision-engine` | engine |
| `assets/libonnxruntime.1.27.0.dylib` on macOS, or `assets/libonnxruntime.so.1.27.0` on Linux | library |
| `assets/model.onnx` | model |
| `assets/tokenizer.json` | data |

Asset names reuse the existing execution verifier's host names. Other index
entries may only be metadata outside that closed subtree, such as provenance
and license records. Reordered index entries resolve to the same asset identities;
missing assets, wrong roles, duplicate names, extra execution/data payloads and
extra files in `assets/` refuse. Descriptor output is published only on success.

This parser resolves declarations; the existing index checksum, whole-package
tree, named-root binding, independent host pins, text/image file-size caps,
platform/minimum OS and native loader/asset checks remain required before
compatible publication. Actual installed inference and loaded readiness remain
separate qualification gates. No successful descriptor parse reports readiness.

Trusted setup configuration supplies all six options together:
`--decision-profile text|image`, `--decision-runtime-revision`,
`--decision-engine-sha256`, `--decision-runtime-sha256`,
`--decision-model-sha256` and `--decision-tokenizer-sha256`.
These are host flags, never public JSON or manifest-selected policy. Missing
policy refuses before acquisition or reservation. Partial, malformed or duplicate
flags refuse before setup work. Discovery's supported flag reports admission
capability; installation continues to report readiness `unqualified`.

The verifier compares profile, revision and all four asset pins with independent
host policy, reuses whole-package tree/native graph verification and existing
text/image flat asset limits, and binds the named asset directory to the held
package root. A final whole-tree check and named-root check precede successful
output. Molmo2 shares its existing directory-binding helper through the common
native package module; its admission behavior is preserved.
