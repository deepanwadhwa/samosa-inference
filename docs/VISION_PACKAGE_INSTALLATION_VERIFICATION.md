# VisionPsy installation verification

The `visionpsy-package-v1` catalog recipe applies host-qualified engine, config,
tokenizer, weights and Metal-kernel pins before publication. The existing public
VisionPsy bridge is reused. Component 2 remains open: the independent engine build
and its installed-model qualification, Molmo2 packages and production catalogs
still require completion.

## Controlled gates

`make test-vision-package sanitize-vision-package test-setup sanitize-setup`
passed on the development macOS arm64 host. The native verifier passed 657 checks
per mode, covering independent policy pins, strict descriptors, fixed model and
kernel paths, role conflicts, descriptor truncations, closed native dependencies
and caller controls. Synthetic payloads exercise admission, not model quality.

Setup tests cover discovery with missing policy, incomplete host flags, public
policy injection refusal, untrusted TLS, verified publication, wrong engine and
each model/kernel pin preserving the last completed installation, offline
status/resume, complete/incomplete removal and cache-only reinstall. Kernels must
be colocated with the root-level executable and are bounded to 256 MiB.

## Actual existing engine package

A normal setup/provider/harness run passed verified HTTPS acquisition and the
complete installation/removal/reinstallation gate, then 15882 actual heading
recognition checks against fresh installed inodes. The observed text contained
`hello`. The harness checked input identity, derived observation provenance,
uncertainty and durable replay. This is bounded fixture recognition, not a claim
of general visual accuracy or lifecycle readiness.

The seven-file payload totals 1199704650 bytes. Its index SHA-256 is
`f125118d199eaa0897db5eb52990e3e55183a193c9b7abec7bcd823a98e4a9ba`.
The actual run used macOS 26.5.1 arm64; the existing executable's deployment
metadata is 26.0. No other platform or deployment-version execution is proven.

| Artifact | SHA-256 |
| --- | --- |
| Existing native engine | `f1f4e3c54fd0f5ad99ffd517b3baffb24bdcfa95268c6fe73eedc096edf7a8e4` |
| Config | `f25b85a4aef0df1867e0221310b86d87058de9ea6fadc50e993a2de5b08851a8` |
| Tokenizer | `9da3f64c3f7b940520c48fc81dc203149080934d4c7f52201883d24d0cd390a8` |
| Weights | `0cc93c1027a58aec26c4350115b5eca02a836aba06277fe700de854ff078572f` |
| Colocated MLX kernels | `846add228d562bb224b11f18c4655b75a05f3336e543654ce49943577e26cd5a` |
| Rendered heading PPM | `1c4497cbf9e9b55b61d2f2103814adfb37d64d16083945f4b7f880f46e762c43` |

`tests/vision_package_candidate.py` copies separately pinned source assets into
an owned package and records observed minimum OS metadata. It does not grant
qualification. `tests/llama_setup_https.py` accepts the resulting root/index/pin,
five pin arguments, and `--vision-cli`, `--vision-test`, `--image` to run the full
gate. The harness snapshots its setup executable so concurrent development builds
cannot change the executable under test between operations.

The rendered PPM comes from Chat's existing `tests/fixtures/documents/hello.pdf`
using `pdftoppm -f 1 -singlefile -scale-to 512`; this rendering produced 396x512
pixels. Source engine/model/kernel files remain unchanged. No runtime Python
dependency or production distribution catalog is introduced.

An earlier exploratory run was invalidated by rebuilding the setup executable
between calls and is excluded from passing evidence. The executable snapshot and
mandatory kernel policy were in effect for the passing run. Actual sanitizer
qualification of this engine pair has not yet been performed.
