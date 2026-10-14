# Verified package completion v1

An installer-owned package directory contains `payload/` plus completion and
coordination metadata. `package_publish` observes the complete indexed payload,
checks native deployment/minimum-OS policy, invokes a required trusted backend
compatibility validator, then revalidates and syncs every payload file/directory.
The callback is host code, never a public request-selected function; its concrete
backend profile must honor its own bounded controls and qualification contract.

A private nonblocking `publish.lock` serializes completion inspection/publication.
The caller still owns the broader acquisition/staging/quota lease and named
package-root binding. Payload/root and lock bindings are rechecked before commit.
A bounded read-only `complete` record binds the pinned index identity to the held
payload directory's device/inode. It appears through an exclusive temporary name
and no-replacement `linkat`, followed by temporary unlink and directory sync.
No existing completion or unknown temporary record is overwritten/adopted.

`package_publication_verify` requires that matching record and rechecks current
whole-tree integrity, native policy and backend compatibility without creating
metadata. Record presence alone never grants installed integrity or readiness.
Existing completion is inspected both before and after validation; a replaced,
linked, corrupt or mismatched record refuses. Failed validation leaves the last
record/data intact and returns no successful result. Output is untouched on error.

Completion is a logical atomic publication boundary; consumers must use the
verified completion interface rather than open partially staged payload paths.
A crash before linking can leave `.complete.pending`; a crash between linking and
unlinking can leave two linked record names. These observations refuse until the
coordinator's registered transaction/inode recovery protocol resolves them.
The completion helper does not provide that recovery ledger, download/resume,
coordinated reservations, activation, model readiness or a public install service.
Sync guarantees retain filesystem/host limitations. Same-user writers outside
the coordinator are not made immutable by this observation protocol.

## Interrupted completion publication

Explicit publication can recover a complete matching `.complete.pending` proposal
or a verified `complete`/pending pair bound to the same private two-link inode.
The expected index and held payload-root identity must match. Recovery never
bypasses fresh whole-tree hashing, concrete backend compatibility, payload syncing,
request controls or named-root validation. Only afterward may it publish the
retained proposal or remove its known metadata alias, sync the package directory
and verify the remaining single-link completion record. Malformed or mismatched
proposals refuse and remain intact; foreign pending metadata now reports an
identity conflict rather than implying a live publication worker.

Read-only `package_publication_verify` returns
`package_publication_recovery_required` for a matching interrupted publication
without repairing it. Assembly retries invoke explicit publication for existing
or proposed completion and therefore recover without acquiring or recopying
already-present payload files. Source configuration is required only when actual
copying is needed. Loaded readiness is still a separate capability. Torn/unverifiable
metadata, reservation recovery, bootstrap ownership and reclamation remain area-2
requirements; this does not close the full installer.
