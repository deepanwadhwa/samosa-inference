# General text distribution candidate

The seven-file candidate reuses the already-qualified static CPU llama.cpp engine
`958601dbd22ab2f5857e09860247f30c74153cb0f3914b8d62f2bacbba9aab97`
and retained Ornith1.0 9B Q4_K_M GGUF
`5720d1f671b4996481274fffe01868c3c36e87c135cc8538471cc7bd6087b106`.
The existing native GGUF reader reports version3, architecture `qwen35`,
427 tensors,37 keys and10942337 metadata bytes. No new parser, inference runtime,
model download or conversion is used for staging.

`tools/prepare_text_catalog.py` reuses the common release helper and existing
`llama-gguf-package-v1` text descriptor. The candidate index is
`f5a6faa589c9478f29cb5fe343be224152780fc0264a20a3ff5d9a8a0d3a7c22`,
with5645083556 payload bytes. Weights use the original App catalog's immutable
upstream URL/revision directly, rather than this release's assets. The original
source namespace redirects to `ornith-ai`; revision and content pins remain the
same. The pinned original model card explicitly declares MIT and is retained
unchanged. That pinned repository contains no separate LICENSE file despite the
card's link; no upstream copyright notice or license file is invented. The full
MIT native-runtime notice accompanies the package.

The shared HTTPS installer gate now invokes the existing `managed_native.sh`
against the freshly installed engine/model, with the existing registry and
offline adapters. It preserves the complete acquisition/refusal/recovery/removal
checks before actual model execution. Both normal and sanitizer candidate gates pass, including
native token count24, arithmetic answer4, observed active cancellation, survivor,
owner-scoped status, foreign-owner refusal, FIFO EOF shutdown, private credential
cleanup and exact runtime-free offline replay. The seven unique release assets
passed download-back hashes before publication. Cold published-URL qualification
is running; its production catalog entry remains pending. The underlying engine itself is
a release build.

Logs: `/private/tmp/samosa-text-distribution-v1-{normal,sanitize}.log`.
Qualification is bounded arithmetic/control behavior on macOS26.5.1 arm64, not a
general coding benchmark, incremental token streaming, common-registry owned
readiness, KV-session semantics, or another platform's model execution.

2026-10-17 resume audit: the cold published-URL statement above is a historical
checkpoint. Current process inspection finds no matching live qualifier and
`/private/tmp/samosa-text-published-{normal,sanitize}.log` are absent. No cold URL
pass or production text entry is inferred. Recover the exact published index and
asset pins and run/collect its native installation/actual-model gates before
closing area2; preserve the original full acceptance requirements.

## Recovered published candidate and live qualification — 2026-10-17

Published release `inference-text-package-2026-10-17.1` is present in the owner's
GitHub repository with all seven expected assets. The retained original Ornith
GGUF and the pinned released engine/card recreate the candidate through the
existing `tools/prepare_text_catalog.py`, without conversion or source-layout
weight copies. Candidate: `/private/tmp/inference-text-resume-candidate`.
Its index remains exactly
`f5a6faa589c9478f29cb5fe343be224152780fc0264a20a3ff5d9a8a0d3a7c22`;
catalog SHA256 `a2ade600c0cc707e4603896d40b05a904ddb3fec01d9aa26bc7083f2029e9971`,
sources SHA256 `17e4e6f36ee9455d2324c59ea1522a373777f8b2edbedb93ff8b003f63f2cbc3`.

All eight native setup/providers/managed/offline normal/SAN binaries build from
the current tree; log `/private/tmp/inference-text-resume-build.log`.
Current native reservation97/identity61 checks pass in normal and SAN modes;
log `/private/tmp/inference-text-resume-controls.log`.

Normal published-URL qualification is live in exec session46057 (qualifier
PID18061, owned installer PID18062). Original command:

```sh
python3 components/samosa-inference/tests/provider_distribution_remote.py \
  --candidate /private/tmp/inference-text-resume-candidate \
  --setup "$PWD/components/samosa-inference/build/samosa-inference-setup" \
  --public "$PWD/components/samosa-inference/build/samosa-inference-providers" \
  --harness "$PWD/components/samosa-inference/tests/managed_native.sh" \
  --managed "$PWD/components/samosa-inference/build/samosa-inference-managed" \
  --offline "$PWD/components/samosa-inference/build/samosa-inference-llama" \
  --profile text --report /private/tmp/inference-text-resume-normal.json
```

Log: `/private/tmp/inference-text-resume-normal.log`. The fresh owned cache is
growing (observed1.9GiB); no pass is claimed yet. Poll that exact session rather
than restarting on observation timeouts. After terminal success and report
inspection, run the identical command with the four `-sanitize` executables and
`/private/tmp/inference-text-resume-sanitize.json` report, sequentially to avoid
simultaneous large-model residency. Only after both complete may metadata and
receipts enter the production text catalog and the full area2 audit be closed.

Normal result: session46057 terminated0. Report
`/private/tmp/inference-text-resume-normal.json` is `passed` and confirms
`temporary_installation_removed:true`, with exact index/catalog/sources and
setup/provider/managed/offline/harness executable SHA256 pins. The log confirms
cold remote installation/status, actual registry counting/generation, owner
isolation, active cancellation, survivor, EOF shutdown, exact offline replay,
cache-only resume/removal/reinstallation and final cleanup. Actual host:
macOS26.5.1 build25F80 ARM64. No production entry or area2 closure yet: the
sequential sanitizer-host run has now started and remains required.

Revalidated GitHub run39054802760: completed/success at
`b08b4f7d25577199ba07a702e0b401b59fbd81d3`. Current tracked `src` and package
fixture/reservation/identity sources have no diff against that tested revision.
The new extracted host-library sources are separate from installation logic;
their own normal/SAN platform evidence remains in EXTRACTION_STATUS.md.

Sanitizer collection is exec session50277; log
`/private/tmp/inference-text-resume-sanitize.log`, report
`/private/tmp/inference-text-resume-sanitize.json`. It uses the exact normal
command above with `-sanitize` setup/public/managed/offline executables. Poll
this same handle. After a verified pass, retain both reports and the candidate's
metadata-only payload in `catalog/ornith-text-cpu-v1`, audit all original area2
requirements and then advance to area3. The runtime/model remain the original
release binaries; sanitizer qualification instruments the native hosts/controls.

Final result: sanitizer collection50277 terminated0. The report is `passed` with
`temporary_installation_removed:true`; the log confirms cold installation/status,
actual managed/registry counting/generation, active cancellation, survivor,
foreign-owner refusal, EOF shutdown, offline exact replay and cache-only
resume/removal/reinstallation/cleanup. Both reports and exact metadata pins are
retained in `catalog/ornith-text-cpu-v1`. The text distribution is qualified;
the full original area2 audit now closes installation. Area3 is active and all
remaining module requirements remain intact.
