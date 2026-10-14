# Package completion checkpoint

The new completion gate builds on the indexed tree verifier. It requires a
trusted compatibility callback, checks actual host deployment/minimum OS,
validates/syncs complete payload bytes after compatibility inspection, and commits
a private read-only no-replacement completion record. Retained completion queries
revalidate bytes, compatibility, record contents and namespace bindings.

`make test-package-publish sanitize-package-publish` passes63 checks per mode on
local macOS. Cases cover missing completion, missing compatibility validator,
compatibility refusal before commit, cancellation, positive synced completion,
current integrity queries and idempotent publication, publication lease contention,
corrupt payload refusal with retained state, replaced completion detection across
the validator callback, unknown temporary preservation and symlink record refusal.
Fixture validators are explicit controlled test code, not actual native backend
qualification. Log: `/private/tmp/samosa-package-publication-qualified-tests.log`.
Existing tree117 checks per mode also pass in
`/private/tmp/samosa-package-publication-tests.log`.

The synchronizing tree variant preserves ordinary read-only verification behavior;
file/directory fsync happens only for installer publication. Targets join the
default model-free normal/sanitizer archive CI. Actual package acquisition and
compatibility profiles, crash/resume ledger, package-wide reservation accounting,
public controls and actual installed-engine tests remain open. This checkpoint
does not close a numbered area or the complete module/replacement goal.
