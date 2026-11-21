# LIFE-06b changing-image registry ownership

Qualified2026-10-25. The engine change is a bounded input boundary around the
existing NativeImage, image_pixels, image_backend and typed executor. Inference
algorithms, tokenizer, model, preprocessing and public request/result/receipt
formats are unchanged. This does not close Inference area3 or the full rebuild.

The explicit --resident-images-v1 mode loads the existing NativeImage once and
serves sequential prepared frames. A24-byte header contains SMIMG001 and four BE
uint32 fields: width, height, exact RGBA bytes, typed JSON bytes. RGBA and JSON
retain independent1MiB limits; each axis stays1..512. The original reply format
and whitelist of semantic errors are shared by both engine loops. Legacy text
and fixed-image argv modes remain supported. No image decoder/path is exposed
through the new envelope; Sources still owns decoding.

DecisionHost reuses its verified four-file bundle, complete last-good pin/profile
snapshot, guardian duplex ownership, admission, scoped controls, typed Rust result
validation and unchanged durable operations. Private image entries opt into
resident-image-decision-v1; image model limit stays2GiB and text stays1GiB.
Existing parsed RGBA/identity/provenance pass through a trusted image callback.
Interrupted/malformed frames invalidate and reap the same worker; complete typed
context refusal leaves framing reusable. The existing one-shot adapter is intact.

Native normal/SAN on macOS and independent Debian12 ARM64 Linux pass:

- New codec21 checks: repeated changing frames, UTF-8/binary bytes, fragmented
  headers/body, simultaneous maximum image and typed buffers, invalid bounds/
  byte counts/version, truncated image/request, semantic refusal recovery,
  oversized reply, exception sanitization, broken reader and clean EOF.
- Original text codec28 checks unchanged.
- Image lifecycle8 scenarios and text lifecycle8 scenarios each: actual native
  warm request before readiness, one PID/revision, distinct prepared image hashes/
  dimensions, typed refusal/recovery, exact offline replay, owner isolation,
  failed warm/last-good, active/queued controls and healthy peer, mutation/crash,
  EOF/SIGKILL cleanup, lease release and unrelated-process survival.

Actual macOS ARM64 source-built candidate, normal/SAN C hosts:

- Image resident5 scenarios each, all20 original SigLIP reference cases including
  changing images, resize/aspect/alpha, Unicode, capacity and full-frame cases.
  Maximum difference5.958962719887495e-05 within the unchanged0.005 tolerance.
- Text resident5 scenarios each, all10 original DeBERTa cases; maximum difference
  0.0005838391459607184 within the same original0.005 tolerance.
- Both owners also prove exact stopped/missing-assets replay, actual engine
  SIGKILL readiness refusal, explicit stop/requalification/inference recovery and
  EOF release. There is no per-image reload during the reference corpus.
- Original one-shot public reference/routing/refusal/replay gates pass all20 image
  and10 text cases in normal/SAN against this same engine candidate.
- The old published engine refuses the resident flag, leaves ready false, commits
  no last-good row and releases residency. It remains valid for legacy execution.

Native C hosts/guardians and C++ codec fixture are instrumented in sanitizer
runs; the source-built actual worker, ORT and Rust tokenizer are release builds.
Actual model execution is macOS26.5.1 ARM64 only. Linux proves native host/control/
codec behavior, not actual model execution. Existing text/image public controls,
text/embedding/T5/speech lifecycle and nine-provider regressions pass normal/SAN
on both platforms. Assertions/deadlines/limits were not reduced.

Candidate engine SHA256:
3054cf6a1beead8b75afa911ac6467a9c82e0b8f6bc8908931c0b2a3069c9458
The original pinned ORT SDK headers/LICENSE were read from source revision
8f0278c77bf44b0cc83c098c6c722b92a36ac4b5 and checked against ort_sdk_pins.json
before build and after qualification. SDK/runtime/model/tokenizer pins remain
unchanged. The existing Makefile worker target compiled the existing native
sources plus only this framing/mode change. The existing native installer acquired
the unchanged published image package and original reference/fixtures. No model
conversion, replacement inference backend or installer was introduced.

The candidate has NOT been admitted/published as a production distribution.
Its new engine pin needs distribution qualification before production release;
published legacy catalogs are unchanged. Source/SDK/model/package provenance is
retained in candidate-provenance.json, original-image-package.json and
ort-sdk-pins.json. Matching workspace/export source hashes and compact terminal
logs/raw reports are retained here. Native fixtures are model-free.

Final native97349/11947, actual79556, old-engine64096 and regressions79381/4082
are terminal0. Build5915, SDK76218, original package67740 and candidate90717 are
terminal0. No live qualification handles remain. Full logs and source-build command
are /private/tmp/inference-life06b-*.log; installer wrapper is
/private/tmp/inference-life06b-install.py. All declared limits and algorithms were
preserved. No legacy path was retired; all full-goal assembled milestones stay open.

Commands (component directory):

```
make test-decision-transport sanitize-decision-transport
make test-registry-image-decision-lifecycle sanitize-registry-image-decision-lifecycle test-registry-decision-lifecycle sanitize-registry-decision-lifecycle
make build/samosa-decision-engine DECISION_ORT_INCLUDE=PINNED_SDK DECISION_ORT_LIBRARY=PINNED_ORT
python3 tests/decision_registry_lifecycle.py build/samosa-inference-providers --image --assets CANDIDATE_ASSETS --engine-pin QUALIFIED_CANDIDATE_PIN --reference PINNED_REFERENCE --reference-pin REFERENCE_PIN --host-policy catalog/opendecision-image-cpu-v1/host-policy.json --output NEW_QA_DIRECTORY
# Repeat with sanitized CLI; text uses text policy/reference and omits --image.
# Original test_decision_public.py --image consumes a separately pinned inventory.
```

Linux uses the existing independent /private/tmp/inference-life03-linux-siqynlpg
export in samosa-s17a-linux-qa, CC=clang CXX=clang++ CURL_LIBS=-lcurl. Both source
trees match the listed final files; no parent source dependency is introduced.

Next LIFE-07a: reuse the already extracted specialist_supervisor.c /
SamosaMmSession line/frame transport and existing VisionPsy/Molmo2 command loops.
App gateway's visionpsy_session_start/inspect already consume that API; do not
replace it with a new inference backend or duplicate protocol implementation.
Inspect/extract vision.c's existing verification, P6 preparation and reply checks
for common loaded ownership. Preserve VisionPsy's white alpha compositing (it is
different from SigLIP's alpha discard), original prepared hashes, usage/limits and
actual generation qualification. Reuse VisionPackagePolicy and kernels verification.

The persistent visual path must retain guardian-owned request-workspace cleanup
and the already qualified Metal-cache behavior: the existing bounded GPU bridge
permits OS-managed shader-cache growth without removing protocol/log limits.
Audit the workspace/duplex ownership boundary before integrating registry lifetime;
startup ping alone cannot qualify readiness. Then continue Molmo2/Whisper and
remaining area3 gates, areas4–7, pending new-runtime distributions, persistent
session PCM events, Agent/App, pinned assembly and legacy retirement.
