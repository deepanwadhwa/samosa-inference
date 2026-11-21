# LIFE-06a existing persistent text decision ownership

Qualified2026-10-25. Inference area3 remains active; image/other-modality
lifecycle, areas4–7 and the full repo split/assembly/retirement are incomplete.

Existing code retained:

- `engines/decision/main.cpp`/serve_requests, NativeText, ORT/model/tokenizer and
  all typed decision algorithms are unchanged. No new engine build or conversion.
- `decision_assets` supplies its original closed four-file tree, pinned held
  descriptors, native library graph and rechecks. `engine_process` owns duplex
  pipes, bounded stderr, lifetime, group/parent-death cleanup and residency.
- `decision.c` retains normalization, public provenance, image execution and
  durable operation receipts. Its text callback changes execution ownership.
  The original Rust typed result validator is shared with readiness/transport;
  the original actual reference comparator is factored into a reusable function
  in the same QA script, preserving all0.005 checks.

`DecisionHost` binds the existing asset verifier and persistent worker to public
identity-only start/status/stop, the registry request lifetime gate and existing
admission. Private text entries opt into resident-text-decision-v1 and declare
engine/model sizes alongside the original four pins. Image entries cannot select
this text profile; one-shot entries stay available. A real bounded successful
native choice is required before ready/activation commit. Healthy start is
idempotent; status checks live ownership and all pinned assets.

Activation adds a trusted closed-runtime verifier that binds the retained
engine/model files to admitted descriptors before launch. Its explicit
closed-native-bundle-v1 policy keeps external system-only and trusted-host
policies unchanged. Last-good stores a recoverable JSON profile/four-pin snapshot
plus its SHA256; JSON/hash correspondence is checked before state acquisition.
Failed graph/qualification preserves the exact prior tuple. During warm failure,
reap does not release activation's lease/store fence prematurely. Confirmed
shutdown releases assets/pipes/state/residency; pending shutdown retains ownership.

Transport uses the original1MiB BE framing and controlled_pipe. Active partial
failure, cancellation, deadline and invalid typed output invalidate/reap the
worker. A complete typed context_limit answer preserves the healthy loaded
worker for a later request. Existing60s default/120s public maximum remains.
Queued deadline does not stop the active worker; another provider remains healthy.

Final native normal/SAN gates pass8 scenarios on macOS and8 on Debian12 ARM64
Linux, including no-state discovery, strict controls, native warm qualification,
same worker/revision, typed refusal recovery, exact stopped/missing-assets replay,
owner refusal, failed warm outputs/last-good, active/queued controls, busy lifecycle,
non-model mutation, crash/restart, EOF/SIGKILL owner cleanup and unrelated survivor.
Final generic activation tests include graph refusal and mismatched bundle JSON/
digest refusal with last-good preserved. Counts vary with binary size/polling.

Final actual macOS ARM64 normal/SAN each pass5 lifecycle scenarios and the
unchanged ten-case reference corpus on one loaded worker/revision. Maximum
numeric difference in both is0.0005838391459607184 versus the existing0.005 bound.
Actual engine SIGKILL refuses stale readiness, explicit stop/requalification and
inference recover, stopped/missing-assets replay is exact and EOF releases
residency. Native C host/guardian are instrumented; external published engine,
ORT and Rust tokenizer remain release builds. No Linux actual-model claim follows.
Original one-shot text reference/routing/refusal/replay also passes all10 cases in
normal/SAN, with the same maximum difference. Existing text/embedding/T5/speech/
nine-provider and text/image decision controls pass both platforms. Original
asset gates104/104 pass both; transport fragmentation/recovery28/28 passes macOS.
T5/speech were also rechecked after the final bundle-snapshot change.

The existing native installer acquired the unchanged published decision package
using a private copy of its catalog/source configuration and original host policy.
No remote publication or release change. Initial direct use of the public-mode
catalog refused invalid_catalog_file; copying it privately follows the established
QA runner. GCC emitted maybe-uninitialized in unchanged text_evidence.cpp; final
Linux uses CC=clang CXX=clang++ with the same strict warnings, without engine edits
or relaxed flags. Those diagnostics and the earlier identity-only state experiment
are excluded; final bundle reports use the complete recoverable snapshot.

Final native74631/6582, actual12751 and activation44729 are terminal0. Earlier
legacy/regression14259/57977/46830 and assets/transport65866 are terminal0.
No live qualification handles remain. Source/export SHA256s match. Raw JSON
reports and compact terminal PASS logs are retained here; full logs remain at
/private/tmp/inference-life06a-*.log. Product code and the supported model corpus
were preserved; no legacy retirement or full numbered-area closure follows.

Actual package: /private/tmp/inference-life06a-package-4au43gvm/package/payload/assets
Reference: /private/tmp/inference-life06a-package-4au43gvm/reference.json
Reference SHA256: f2c54e4d1be67059c184b9a2326b1b7b96ee7b1de02b6efcaeef5cbf3951dc11
Catalog index SHA256:3ef083607a32e6dbfe1a5529301f41c9f78abc6fb21b2e1dc3486adec373553c
All four unchanged engine/runtime/model/tokenizer pins: model-and-package.json.
Installer invocation retained at /private/tmp/inference-life06a-install.py.

Commands (component directory):

```
make test-registry-decision-lifecycle sanitize-registry-decision-lifecycle test-activation sanitize-activation
python3 tests/decision_registry_lifecycle.py build/samosa-inference-providers --assets ASSETS --reference REFERENCE --reference-pin PIN --host-policy catalog/opendecision-text-cpu-v1/host-policy.json --output NEW_QA_DIRECTORY
# Repeat with build/samosa-inference-providers-sanitize.
make test-decision-public sanitize-decision-public test-decision-assets sanitize-decision-assets test-decision-transport sanitize-decision-transport
make test-registry-lifecycle sanitize-registry-lifecycle test-registry-embedding-lifecycle sanitize-registry-embedding-lifecycle test-registry-summary-lifecycle sanitize-registry-summary-lifecycle test-registry-speech-lifecycle sanitize-registry-speech-lifecycle test-registry-modalities sanitize-registry-modalities
```

Linux uses the independent /private/tmp/inference-life03-linux-siqynlpg export in
the existing samosa-s17a-linux-qa container, CC=clang CXX=clang++ CURL_LIBS=-lcurl.

Next LIFE-06b: reuse NativeImage, image_pixels and image_backend to accept changing
prepared images on the same loaded model. The existing worker binds pixels from
startup argv, so repeatedly starting it per image does not establish residency.
Add only the necessary bounded, explicit input boundary around the existing
preprocessing/inference. Preserve512-axis/1MiB RGBA and1MiB typed request limits;
do not shrink them to squeeze both into the old1MiB text frame. Keep legacy image
argv/framing behavior and text engine behavior. Qualify different images, typed
refusals/controls and original actual SigLIP reference cases before extending the
same registry owner/activation. Any new engine candidate needs its own qualified
pin/distribution gate. VisionPsy/Molmo2 existing load/unload loops and Whisper
lifetimes remain subsequent extraction work; do not replace those implementations.
