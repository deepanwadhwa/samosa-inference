# Native OpenDecision text distribution candidate

This uses the existing native OpenDecision worker, original prepared tokenizer,
qualified float32 ONNX graph and ONNX Runtime1.27.0. No model conversion,
Python inference runtime or alternate installer is introduced. The16-file package
has index SHA-256 `3ef083607a32e6dbfe1a5529301f41c9f78abc6fb21b2e1dc3486adec373553c` and914475719
payload bytes. Its closed `assets/` execution directory has exactly the original
four files. Metadata/notices are outside that directory. Preparation copies only
metadata; model, tokenizer, library and worker files remain at retained sources.

Worker source is0a65512, pinned to
`cc2266a25773068d462628de69a53b8e1f8ddd7c3735b5ea0ebab940bef362b4`.
The worker deployment minimum is macOS26.0; ORT's observed minimum is15.5.
The catalog/index enforce26.0. Actual model execution qualification remains
macOS26.5.1 ARM64 only. Linux and older-macOS execution are not claimed.

Both the source model card and source-asset inventory are included unchanged in
content. Model graph/tokenizer conversion is explicitly noted. The upstream
text-model card's training-data licensing notes are preserved without claiming a
commercially filtered checkpoint. Complete Samosa/OpenDecision Apache license,
model license, ORT MIT license and upstream versioned third-party notices are
included. Tokenizer notices cover77 resolved dependencies, including vendored
COPYING/LICENSE/NOTICE files, taken directly from existing crate archives after
verification against the qualified Cargo.lock checksums. The lock and per-file
notice inventory are included. Notice collection uses Python3.11+ as release
QA tooling only; it installs nothing and performs no network/model download.

The exact16-file normal and sanitizer-equipped setup/inference gates pass10 actual
public cases per mode, including retry/routing/refusal and aggregate input limits.
Exact host/harness/log pins are in `qualification.json`. Staging verification and
cold published-URL qualification remain required before this entry closes. Original
five-file runtime/model qualification does not substitute for this final package.
Logs are `/private/tmp/samosa-decision-distribution-text-{normal,sanitize}.log`.
Installation does not imply loaded lifecycle readiness or full area2 completion.

`qa-artifacts.json` pins the original reference report and any synthetic prepared
RGB frames for independent public qualification. They contain no private machine
paths. Reference SHA-256 is `f2c54e4d1be67059c184b9a2326b1b7b96ee7b1de02b6efcaeef5cbf3951dc11`. Supplemental QA
artifacts are separate release assets, not installed model/runtime files. The
shared remote gate can fetch those small pinned fixtures; all actual package and
model acquisition still uses native setup. The model graph is uploaded directly
from its retained file, with no local staging copy. Both converted graphs are
below GitHub's documented2GiB per-asset bound; one combined release deduplicates
common worker/library/notice assets by SHA-256. Proposed URLs do not yet prove
publication. Other distributions and all remaining full goal requirements stay open.
