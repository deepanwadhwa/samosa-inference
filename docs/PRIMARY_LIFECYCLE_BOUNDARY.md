# Existing primary owner to Activation boundary

LIFE-07q, native Mac, 2026-11-22. This is an ownership adapter boundary and a
reproduced cleanup fix; common primary capability/readiness integration is still
required. Preserve the original Qwen/Maple/llama implementations and caller policy.

The public backend_process API now supplies typed start/physical-status/stop
counterparts to the existing boolean/void calls. Those legacy calls use the same
implementation. Start acceptance and a living guardian do not establish successful
generation, verified model identity or common provider readiness.

The module-private backend_process_start_delegated launch takes stable
adapter-owned EngineProcess storage. It uses the existing bootstrap, argv/environment,
guardian, actual PID/group, marker, append/fallback logs and unlimited resident
lifetime. No process record is copied and no second owner/guardian is launched.
The adapter must serialize startup/automatic failure cleanup, exclude outside
owner observers during that phase, and join users before shutdown/storage release.
Normal shutdown withdraws and finishes the backend owner before Activation release.

The new native fixture actually calls activation_start with that delegated record,
verifies the same observed engine PID, commits the original transaction, then stops
through the backend owner and Activation. A failing trusted qualification callback
exercises Activation's automatic process cleanup; reconciling the backend owner
then releases its marker lease. The exact previous last-good snapshot remains.
These are ownership/transaction fixtures, not model capability qualification.

A concrete guardian-loss bug required a boundary fix. Previously, running/stop
freed the owner after engine_guardian_failed, while a resistant orphan descendant
could still be alive and the original worker's port was already free. Retain the
host's duplicate marker lease and canonical owner record on both pending and
unknown cleanup. Withdraw the public PID/marker; status/start/stop keep the typed
failure and refuse a replacement. A confirmed terminal failure may release the
record and still report its error. No public force-reset is added.

This proves exclusion while that owner host remains alive, even after the test
explicitly removes its controlled orphan helper. It does not prove general
unknown-group cleanup, persistent exclusion after terminating that host, or
pre-admission crash recovery. Those requirements remain open. Never release the
adapter's borrowed process storage while its backend owner still retains it.

Next connect these existing owners to actual common primary qualification:

- Keep original selection/settings/fingerprint/rollback in the caller. Reuse
  backend_process and backend_json; do not duplicate launch rules or add a client.
- Bind the admitted model bundle and native dependencies using existing artifact,
  package/runtime inspection and Activation verification hooks. Model directories
  are not GGUF files. The retained Mac Qwen engine directly uses the installed
  /opt/homebrew/opt/libomp/lib/libomp.dylib, observed with otool; the system-only
  managed llama profile cannot silently replace it. Preserve that native dependency
  and qualify its explicit boundary rather than rebuilding the engine or changing
  runtime policy by assertion. Maple availability/quarantine must remain honest.
- Run a bounded real generation and validate content/usage before qualified status
  or last-good publication. Reuse the original request shape and original useful
  tests. An HTTP200/health handshake or saved metadata is insufficient.
- Recheck retained engine/bundle/native-input identity and reject stale readiness;
  preserve receipts, exclusions, complete cleanup and last-good failure handling.
- Wire the caller to the common controls and qualify original model switching,
  background/interactive work, turn pause/resume, cancellation and shutdown before
  retiring the compatibility path. Complete the full area-3 audit afterward.

No Python, Docker/VM/Linux execution, model fetch/conversion, catalogue change or
new model/server/guardian implementation is part of this boundary.
