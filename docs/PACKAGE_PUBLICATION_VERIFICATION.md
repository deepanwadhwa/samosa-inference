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

## Completion publication recovery

Normal and ASan/UBSan publication gates passed with 83 checks each. New cases
exercise a matching pending completion proposal, the known completion two-link
metadata interval and read-only recovery-required reporting. A real child is
killed after writing/syncing its completion proposal and before linking it.
The parent observes SIGKILL termination; a deliberately failing compatibility
callback refuses recovery and preserves the pending proposal/no-completion state.
Restoring compatibility permits publication and independent revalidation.
Malformed foreign pending metadata is retained and reports identity conflict.

Local/cache and complete production HTTPS assembly fixtures also passed normal
and ASan/UBSan. They retry pending/two-link completion with no source descriptor
and verify the repaired package. Logs:
`/private/tmp/samosa-completion-recovery-final.log`. Static analysis of the modified
publication code emitted no diagnostics after adding an explicit invalid-descriptor
guard. This remains installation work; no engine-readiness claim follows.

The preceding identity-recovery revision `e231782` passed independent macOS arm64
and Linux x86_64 CI in run 38602657383 and is pinned in Chat. This completion-recovery
revision requires its own independent CI result. Remaining installation recovery
and catalog requirements are unchanged.
