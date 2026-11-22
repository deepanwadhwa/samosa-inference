# LIFE-07p — extract the existing primary JSON request transport

Native Mac, 2026-11-22. Baselines: Chat `93e1608`, Inference `6cfb9fe`.
This is extraction progress toward the unchanged full repository-split goal.
Inference area 3 remains open.

## Moved implementation and boundary

`src/backend_json.c` and public `backend_json.h` now own Gateway's original
`backend_json_with_timeout` transport. Gateway's old network/request loop is
removed and replaced by a thin borrowed-input/trace adapter. The owner archive
build and staged native installer compile this same source. This gives primary
capability qualification the existing request implementation to reuse; it does
not add another HTTP client, select a model or rebuild generation.

The full old function and its socket, clock, timeout and byte-buffer helpers are
preserved after explicit owner-input substitutions. Public configuration borrows
the original port, timeout, turn gate, mutex and upstream cancellation slot;
selected-provider probing and developer tracing remain trusted host adapters.
Callbacks are not public request fields. Model/prompt/planning/evidence policy,
selection/settings/rollback and App presentation remain with their existing owner.
Other App users retain their existing generic socket/buffer helpers.

The original `POST /v1/chat/completions` wire bytes, raw body result, trace names,
4MiB body plus64KiB header ceiling,64KiB read chunks, deadline and cancellation
scope are retained. So are existing quirks: the last chunk may cross the loop's
response threshold; a receive timeout can leave a partial body; status/header
recognition is the old string matching. This is not a new strict HTTP parser or
a promise that a returned body is valid model output. Callers preserve the
original SIGPIPE policy (Gateway ignores it). Typed readiness still needs response
validation and current owned identity; transport success alone does not prove it.

`audit-reuse.mjs`, run from Chat, reconstructs the complete old Gateway outside
only the wrapper/include and compares every byte. It also restores/compares the
complete old transport and helper bodies. Model engines, process/guardian/turn
owners and existing process tests are unchanged. Three JSON validators in the
original web shell test are ported to Node so its full original assertions can
run without Python; the audit proves no other script changes.

## Native evidence

| Evidence | Observed scope |
| --- | --- |
| `native-final-macos.log` | Exit 0,195 normal /195 ASan+UBSan checks. Real socket peers verify original request bytes, split headers/UTF-8,503/empty/missing-header behavior, partial response, total deadline, response threshold, trace/slot/gate cleanup and concurrent cancellation. The preceding `native-macos.log` predates only the optional actual-client test mode. |
| `standalone-macos.log` | Exit 0,195 normal /195 sanitizer checks. Fresh directory with26 source/header/build/license files; original module archive builds from source with no parent implementation, prebuilt objects, Rust, Python or network dependency. Exact matching source manifests and local root are recorded alongside. |
| `prompt-budget-macos.log` | Exit 0, all15 original scenarios: fact/evidence/reduction/UTF-8/thinking/failure/truncation/cancellation/oversized-question assertions retained; zero overflows in every case. |
| `web-macos.log` | Exit 0, original compiled Gateway web suite, including internal model calls, all assertions preserved after three JSON-validator ports. Stubbed search/curl; no external provider/model-quality claim. |
| `selection-macos.log` | Exit 0, original readiness/selection/rollback/fingerprint/commit-failure cases. This preserves old selection behavior; it does not turn old health checks into full common readiness. |
| `source-audit.txt` | Exact original transport/helpers and full Gateway reconstruction pass; original web assertions and other Inference implementations preserved. |
| `static-analysis-macos.log` | Native analyzer exit 0 with no diagnostics for the moved transport. |
| `package-macos.log` | Exit 0, original complete/incomplete PDFium-set/version cases; exact staged owner bytes, hashes and sizes, now including backend_json.c/public header. No Docker/Linux execution is implied by fixture archive names. |
| `install-macos.log` | Exit 0: native runtime-only clean install/upgrade/boot. Existing optional model content/inode/mtime assertions remain unchanged. |
| `actual-macos.log`, `actual-sanitize-macos.log` | Both exit 0: the existing retained native Qwen qualifier now sends its original request through the extracted C transport client. Both return15 prompt /4 generated tokens, `Hello! How can`, zero swap growth, native HTTP shutdown and complete Gateway/owned-backend stop. SAN instruments the C client, not the external model engine or the whole Gateway. |

Actual execution reuses `life07j-primary-process/qualify-qwen.mjs`, changing only
the request sender to this module's native client. It retains the existing
read-only LIFE-07h engine/model candidate,2 OpenMP threads/context512, private App
state and6144MiB/zero-swap-growth memory guard. The C client's permissive probe
is a transport fixture; the driver separately checks Gateway's owner PID. No
public lifecycle readiness, general model accuracy, long-context performance,
combined-provider admission or new identity guarantee is inferred from this.
The real API is also exercised through Gateway by the original budget/web tests.
`actual-source.txt` and `actual-sanitize-source.txt` identify the private receipts;
input pins and their fresh verification are recorded separately.

All execution is native on this Mac under `tools/without_python.sh`. No Python,
model download/conversion, Docker/VM/Linux execution, catalogue update or published
release/tag occurred. The independent source preparation initially omitted the
already-declared `engines/json.h` dependency; `standalone-initial-macos.log` records
that build failure, not a passing gate. Adding the original header to the source
subset resolved it without changing implementation or Make dependencies.

## Continue the full extraction

Reuse this transport together with `backend_process` and Activation for common
primary controls, observed identity, mutation/stale-readiness rejection, a bounded
actual generation probe and last-good failure handling. Preserve the original
Qwen/Maple/llama launch, environment, runtime, PID/marker, logs and unlimited
resident lifetime; do not substitute the static-system managed-llama profile.
Keep selection/settings/rollback as explicit caller policy.

The structured Whisper LIFE-07o profile remains separate from long-window txt/SRT.
Default long-window/visual common controls, guardian-death and pre-admission
recovery still need completion. Areas4–7, Python removal, Agent/App, pinned assembly
and qualified legacy retirement remain required afterward. Sources stays closed;
Inkling catalogue work stays deferred until the whole refactor is finished.
