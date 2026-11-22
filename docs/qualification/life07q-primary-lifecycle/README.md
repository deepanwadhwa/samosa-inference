# LIFE-07q — primary owner lifecycle handoff and unknown-cleanup fence

Native Mac, 2026-11-22. Baselines: Inference db99bf7 / Chat 8aab8f9.
Full common primary readiness and Inference area 3 remain open.

## Existing implementation and necessary change

The existing backend_process implementation now exposes typed start/status/stop
alongside its original boolean/void wrappers. A module-private delegated launch
lets Activation use **one canonical EngineProcess record**, with no PID adoption,
record copy, second guardian or replacement launch algorithm. Original bootstrap,
Qwen/Maple/llama argv/environment, actual PID/group/marker, health probe, unlimited
resident lifetime and append/fallback diagnostics remain. Gateway still calls the
same compatibility API and its C source is byte-unchanged in this slice.

The missing boundary was a typed, retained process outcome that Activation could
safely consume. A native reproduction also found a real bug: after guardian death,
the old owner dropped its handle/fence while a resistant descendant remained alive.
The original worker's port was free, so occupied-port refusal could not protect
against launching another engine. The host now retains a duplicate marker lease
and the canonical record on engine_shutdown_pending or engine_guardian_failed.
PID/marker readiness is withdrawn; status and repeated start/stop refuse honestly.
Confirmed cleanup releases ownership, including a known terminal error reported to
the caller. The legacy wrappers delegate to the typed implementation.

[Boundary and exact next work](../../PRIMARY_LIFECYCLE_BOUNDARY.md) describes
serialization, borrowed-storage lifetime and what qualification still needs.
The private startup/failure-cleanup phase must exclude outside owner observers;
join users and finish the backend owner before releasing adapter storage.

## Evidence and scope

All commands ran under Chat's tools/without_python.sh. No Python, Docker/VM/Linux
execution, model download/conversion, catalogue update or release/tag occurred.

| Evidence | Terminal result and scope |
| --- | --- |
| guardian-loss-final-baseline.log | Expected exit 1 using the original db99bf7 owner plus the final test fixture, at `!lease_available(c.marker)`. The original released the fence while the resistant helper was alive. guardian-loss-baseline.log is the earlier reproduction before final fixture additions. Both controlled fixture trees were cleaned. |
| native-final-macos.log | Exit 0: new lifecycle163 normal /163 ASan+UBSan; original primary676 /676. Original launch/environment/health/marker/stop/hooks, descendant/parent death, held-PID, long logs and pending-stop assertions are unchanged. native-macos.log predates added typed pending/Activation cases and is intermediate evidence only. |
| standalone-macos.log | Exit 0: fresh36-file source-only subset builds the original module archive and lifecycle163 /163. No parent implementation, prebuilt objects, Rust, Python or network dependency. Native SQLite is the existing Activation test dependency. Exact matching source manifests/local root are recorded. |
| assembled-build-budget-macos.log | Exit 0: compiled Gateway and all15 original prompt-budget scenarios, with zero overflows and original cancellation/failure/domain assertions. |
| selection-macos.log | Exit 0: original selection/readiness/rollback/fingerprint/commit-failure suite. This preserves its old availability semantics, not a full common capability qualification claim. |
| gateway-stop-macos.log, gateway-crash-macos.log | Both exit 0: the actual compiled Gateway's normal stop and SIGKILL paths clean its controlled native primary/helper/marker. These are fixture engines, not actual model-quality results. |
| actual-macos.log | Exit 0: retained native Qwen through the unchanged LIFE-07p qualifier and JSON transport. Original15 prompt/4 generated tokens, `Hello! How can`, zero swap growth, native HTTP shutdown and complete Gateway/owned-backend stop. External model/Gateway are normal native builds; the new owner is sanitizer-qualified in the native fixture suite, not an actual-model SAN claim. |
| package-macos.log, install-macos.log | Both exit 0: exact staged bytes/hashes/sizes including the required private owner header; native runtime-only clean install/upgrade/boot with no optional model requests and unchanged existing model inode/mtime/content. |
| source-audit.txt | Original backend launch/bootstrap/health/environment/marker implementation restored byte-for-byte outside typed observation/cleanup, canonical storage and retained fence adaptation. Original engines, Activation, guardian, transport/turn/llama code and all original primary assertions unchanged. Gateway unchanged; package/install logic/assertions differ only by required private-header membership. |
| static-analysis-macos.log | Native analyzer exit 0, no diagnostics for backend_process.c. |

The new tests reuse the **entire existing test_backend_process.c fixture/helper**
by inclusion; they do not create another fake worker implementation. They cover:

- Live owner loses guardian and original worker; a resistant helper remains alive.
  Another host is excluded by the retained marker lease and a separate peer stays
  alive. The test explicitly kills only that controlled helper, verifies the
  fence still remains, then exits the fixture host. This does **not** establish
  generic unknown-group recovery or exclusion after terminating an unknown owner.
- Delegated launch exposes the exact same guardian/engine PID record. Typed stop
  closes that record and releases the lease. A stopped guardian returns typed
  pending, keeps the record/fence and rejects restart until resumed cleanup.
- A real Activation transaction uses the delegated record, publishes last-good,
  stops correctly and preserves the exact prior snapshot when its trusted fixture
  qualification callback fails. That callback checks fixture ownership/availability;
  it is explicitly not a model capability probe. No primary provider is advertised
  ready on that basis.

The actual smoke reuses LIFE-07p's original retained-Qwen driver/guard, two OpenMP
threads/context512, private App state and the read-only LIFE-07h candidate. Its
local receipts are named in actual-source.txt; original pinned inputs and fresh
full hash verification are in LIFE-07p actual-inputs.sha256/actual-inputs-check.log.
No engine or model bytes changed in this slice. This is a short compatibility
check, not a performance, long-context, generic recovery or model-quality claim.

Next reuse this proven boundary with backend_json and Activation verification for
original primary bundle/native-dependency identity, real generation readiness and
common controls; preserve App selection/settings/rollback. The installed Qwen's
OpenMP dependency and directory bundle must not be replaced by the static-system
llama profile. Then finish long-window Whisper/default visual controls, generic
unknown/pre-admission recovery and all area-3 requirements. Areas4–7, project-wide
Python removal, Agent/App, pinned assembly and qualified legacy retirement remain.
Sources stays closed; Inkling remains deferred until the entire refactor finishes.
