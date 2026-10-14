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
