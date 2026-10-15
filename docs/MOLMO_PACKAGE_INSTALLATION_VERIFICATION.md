# Molmo2 installation verification

The static native compatibility gate reuses the existing temporal model verifier
and binds its receipt to the shared installation index. Public catalog/setup
dispatch, installed video inference and an independent engine build remain open.
This document records evidence for component 2 without claiming that the full
installation slice or later engine lifecycle work is complete.

## Controlled checks

The descriptor/index binding gate passed 302 checks in both normal and
ASan/UBSan modes. The native compatibility gate passed 72 controlled refusal
checks per mode: independent policy, root identity, pin mismatch, missing kernel,
foreign symlink, cancellation, unchanged failure output and invalid native model
profile despite consistent file hashes. Existing temporal-input regression gates
also passed in both modes after making the native manifest parser's 8192-token
budget independent of unrelated build flags.

## Actual model package

`tests/molmo_package_candidate.py` copied the separately pinned existing native
engine, colocated MLX kernels and all files declared by the existing model
manifest into a private candidate. It verifies the originals before and after
copying and verifies destination hashes. No original Chat asset is changed.

The candidate has 29 files totaling 3553773738 bytes. The model manifest binds 24
files under the fixed model directory. The package passed the normal native
compatibility gate (11 harness checks), including the shared closed namespace,
actual host executable/library inspection, native model pre/post verification
and exact generic-index/native-manifest binding. The sanitizer-equipped actual
package check is still pending.

| Artifact | SHA-256 |
| --- | --- |
| Complete package index | `92ad14eb6ca92d2540a920fa8a88392b1bc3419d6188be00eb4572fbf3be51f3` |
| Existing native engine | `c8aa53e06d30305f3077e260f5fbda258cd911941b109dfe18d286fc96d08a05` |
| Colocated MLX kernels | `846add228d562bb224b11f18c4655b75a05f3336e543654ce49943577e26cd5a` |
| Native model manifest | `a07230dd952e97969921223328e7812020bb58711728450cdfb2205e3a15b770` |

Run `test-molmo-package --real ROOT INDEX INDEX_PIN ENGINE_PIN KERNEL_PIN
MANIFEST_PIN` for actual static admission. The candidate reports observed engine
minimum OS 26.0; testing occurred on macOS 26.5.1 arm64. This establishes static
compatibility on that host, not execution on macOS 26.0 or another platform,
loaded model readiness or generated-video quality. Python is used only by the QA
packager; this native gate adds no Python runtime dependency.
