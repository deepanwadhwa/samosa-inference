# Resumable owned payload deletion

`package_remove_payload` acquires the existing exclusive assembly lease, verifies
registered package identity, loads the durable removal inventory and binds the
payload/completion names to their recorded inodes. It also checks the completion
record's exact index/root declaration. Existing readers block the operation.
The caller retains named-package-root bindings through its trusted control hook.

Before mutation, the entire remaining namespace must be a subset of recorded
files/directories. Every retained file must match its captured inode, ownership,
mode, link count, size and nanosecond timestamps, and its indexed full SHA-256.
Unknown files, symlinks, hardlinks, replaced directories/inodes and corrupt data
refuse. Each file is checked again before unlink; its parent is fsynced. Recorded
directories are removed deepest first only when empty, then their parents are
fsynced. Directory timestamps/link counts may reflect the operation's own child
removals; directory device/inode/UID/mode must still match.

Already absent recorded files/ancestors are accepted during restart. Controls
apply throughout validation and between mutations. A successful final walk proves
the retained payload root empty. The root, completion metadata, durable inventory
and quota reservation remain charged until the next verified retirement phase.
This operation never deletes an unknown entry or recaptures a changed inode into
its journal. It requires an already published removal record; creation/recovery
of that record remains the coordinator's responsibility.

Normal and ASan/UBSan payload gates pass 75 checks: cancellation, reader exclusion,
unknown file/symlink refusal before indexed deletion, same-content substituted
inode refusal, a real SIGKILL after the first deletion, restart completion and
idempotent emptying. The fixture owner explicitly recaptures its inventory after
restoring a test-renamed inode; the removal API never performs such adoption.
All 93 record tests also pass. Log:
`/private/tmp/samosa-owned-payload-removal.log`.

No user/actual-model assets were deleted by these controlled fixtures. Complete
retirement, quota release, partial-copy cleanup and public removal coordination
remain component-2 work; lifecycle/readiness components are not opened here.

## Completion and empty-root retirement

`package_remove_retire` takes the same exclusive assembly lease and verifies
registered package identity plus the durable inventory. If the recorded payload
root still exists, it must match its recorded device/inode/UID/mode and be empty.
The completion file must match its recorded full stat identity and exact
index/payload-root declaration. After controls and binding rechecks, the function
unlinks that completion and fsyncs the package directory, then removes only the
recorded empty payload root and fsyncs again.

A checkpoint between those mutations allows restart after marker retirement.
Recorded names already absent are accepted; unrelated recreated roots or
completion files refuse and remain untouched. Directory scanning never removes
an entry. The immutable removal journal and quota charge remain for subsequent
verified quota/metadata retirement, keeping the removal admission barrier active.

Normal and ASan/UBSan payload/removal gates now pass 105 checks, including refusal
before emptying, cancellation, reader exclusion, substituted empty-root refusal,
a real child killed after completion unlink, resumed root removal, idempotent
retirement and foreign names recreated after retirement. The 93 record regression
checks also pass. Log: `/private/tmp/samosa-payload-retirement.log`. Only owned
controlled fixture data was removed; no actual-model/user assets were deleted.

## Final metadata cleanup

`package_remove_finish` settles exact quota-charge absence and invokes a trusted
native finalizer while assembly and pool leases remain held. Only after the pool
directory has been fsynced may it remove an empty private copy journal and a
private zero-byte publication lock. The validated removal record is removed last,
followed by package-directory fsync. The registered identity and assembly lock
remain; removing that coordination lock could split future readers/writers.

Unknown entries, nonempty housekeeping directories and unsafe aliases refuse.
If death occurs during housekeeping cleanup, the retained record continues to
block ordinary reuse and validates the retry. If the record is already absent,
only the strict clean namespace of assembly lock plus verified identity is accepted.
That state supports idempotent completion and same-index empty reservation reuse.
Public requests cannot supply the native finalizer callback.

Normal and ASan/UBSan removal gates pass 165 checks, including cancellation,
a real child killed after copy-journal removal, retained barrier, resumed cleanup,
already-completed retry and exact recharge/release of the clean namespace. All
97 reservation regression checks pass in both builds. Log:
`/private/tmp/samosa-removal-metadata-cleanup.log`. This completes these native
completed-payload removal phases; public coordination, partial-copy removal,
other backend packages and the remaining component-2 acceptance work stay open.

## Starting a removal transaction

`package_remove_begin` acquires the exclusive assembly lease and verifies the
registered package identity. For a new transaction it binds the payload name,
captures its complete pinned inode inventory, reads and checks the exact completion
declaration, and rechecks completion/payload/lock bindings before durable record
publication. It does not delete package data. No backend readiness callback is
needed to establish ownership of a fully pinned, registered completed payload.

On restart it first checks pending publication, reconstructs the complete validated
record from disk and explicitly recovers it, including the known two-link interval.
An existing valid committed record is reused without recapturing already removed
files. Malformed pending state refuses even if another committed record exists;
it is never silently ignored or adopted. The caller retains named-root bindings
through trusted request control.

Normal and ASan/UBSan coordinator/removal gates pass 188 checks, including
cancelled startup, reader exclusion, foreign payload/pending metadata refusal
before journal creation or deletion, a real killed inventory publisher, disk-only
reconstruction, committed retry and two-link publication recovery. Log:
`/private/tmp/samosa-removal-begin-coordinator.log`. Public catalog-selected removal
coordination and partial-install cleanup remain the following component-2 steps.

## Catalog-selected completed-package removal

`provider_remove` accepts only the contract version, operation and catalog
provider ID. Trusted host configuration supplies the catalog, package root and
reservation pool. The installer resolves the verified cached index without
network access or runtime credentials, then `package_remove_run` serializes the
complete removal sequence with a private persistent `remove.lock`. Concurrent
removers receive `package_removal_busy`; retaining the lock file prevents separate
processes from locking different inodes at the same name.

The coordinator resumes inventory publication, payload deletion, completion/root
retirement and quota release/metadata cleanup using the existing phase validators.
A clean retry does not recreate an inventory. Success reports `removed`; a later
installation can reuse the verified artifact cache. Partial installations without
a completed ownership inventory still fail closed and remain component-2 work.

Normal and ASan/UBSan native removal gates pass 199 checks, and reservation
regressions pass 97 checks in each build. Portable public HTTPS setup gates cover
removal, repeated removal and cache-only reinstall without additional downloads.
Log: `/private/tmp/samosa-public-package-removal-final.log`.

The actual three-file EmbeddingGemma package (325,805,737 bytes; index
`6e61ef6a8aba534521b6dd3a4bf1f0e0c0df04c785b5666d3b01ac458f875865`)
also passes public HTTPS installation, removal, repeated removal and cache-only
reinstallation normally and under ASan/UBSan. Both runs delete only their owned
temporary installed copies. Logs: `/private/tmp/samosa-public-remove-actual.log`
and `/private/tmp/samosa-public-remove-actual-sanitize.log`.
