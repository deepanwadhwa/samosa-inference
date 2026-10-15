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

## Removing an interrupted unpublished copy

`package_stage_remove_partial` removes one unpublished temporary copy under the
caller's exclusive assembly lease. It traverses existing private directories
without creating them, requires the exact root/parent/index/inode copy record,
checks any retained intent, and refuses published names, symlinks, substituted
inodes, multiple links and oversized files. It does not require the partial
content to be a valid complete model: the copy record establishes ownership.

The payload unlink and parent-directory fsync precede journal retirement. If the
process dies there, `package_copy_forget_absent` reconstructs and validates the
exact canonical copy record while both temporary and final payload names are
absent. This retires metadata only; it cannot authorize deletion of a new inode.
Known pending-record publication and two-link intent states use the existing
recovery logic. Repeated cleanup is safe without a source artifact descriptor.

Normal and ASan/UBSan staging gates pass 213 checks, including a real SIGKILL
between durable unlink and metadata cleanup, restart from pending metadata,
malformed and overflowing tuple refusal, published-file preservation, inode
substitution, symlinks, hardlinks and cancellation. Completed-package removal
regressions pass 199 checks in both builds. Log:
`/private/tmp/samosa-partial-copy-remove-final.log`.
Public setup, portable llama HTTPS removal/cache reinstall and native speech
HTTPS installation also pass normally and under ASan/UBSan with the shared copy
record changes; log `/private/tmp/samosa-partial-copy-setup-regression.log`.

This is the per-file primitive for partial-install removal. Whole-package
coordination, published files within an incomplete installation, empty directory
retirement and quota release still need integration. An intent-only bootstrap
inode remains unregistered and is preserved; it can already resume installation
through verified source acquisition. Component 2 remains active.

## Whole incomplete-package coordination

The same catalog-selected `provider_remove` operation now handles incomplete
packages with a registered assembly identity. Under the exclusive assembly lease,
the coordinator checks the entire package namespace, payload and copy journal
before deleting any temporary copy. Unknown paths or metadata, corrupt published
files, substituted copy inodes, symlinks and unrelated aliases refuse. It settles
known two-link file/metadata publication intervals through the existing staging
logic and retires only registered unpublished copies.

It then captures the remaining pinned published files and known directories in a
distinct durable incomplete inventory (`SMRMP001`). Missing indexed files have
zero identity slots, never invented ownership. Once the inventory is committed,
the existing removal barrier blocks assembly and consumer leases. Removal refuses
new files in previously absent slots and replacement inodes, deletes recorded
files/directories, retires the empty payload root, and releases the exact quota
charge before removing metadata and the barrier. A completed removal interrupted
after completion-marker retirement still resumes through this same coordinator.

The new native incomplete-package gate passes 182 checks normally and under
ASan/UBSan: mixed published/partial/missing files, empty initial root, unknown
payload/journal/package entries, cancellation and reader exclusion, actual process
death at inventory publication and after file deletion, pending-record recovery,
checksummed cross-format refusal, new-file/inode/directory replacement refusal,
known two-link publication recovery, quota release and idempotent retry. Existing
199-check completed removal, 213-check staging, 146-check tree and 93-check record
gates pass. Public portable HTTPS setup also removes an owned fixture without its
completion marker, repeats removal and reinstalls solely from cache; all normal
and sanitizer setup/speech regression gates pass. Logs:
`/private/tmp/samosa-incomplete-removal-qualified.log` and
`/private/tmp/samosa-incomplete-removal-native-final.log`.

Intent-only unregistered bootstrap inodes, torn/unverifiable metadata and pending
completion declarations are preserved. Existing installation resumption can
recover qualified known pending completion and intent states before removal;
this coordinator does not infer ownership from an arbitrary name. Remaining
installation recovery and backend/catalog acceptance work must finish before
component 3 starts.

Copy retirement syncs the parent even on an already-absent retry, and known
publication cleanup syncs the retained file and parent even if its temporary
alias is already absent. Recovery metadata is retired only after those directory
changes are durable.

Final normal and ASan/UBSan staging/removal/public setup gates pass with these
sync rules; log `/private/tmp/samosa-incomplete-removal-final.log`. The actual
three-file EmbeddingGemma package (325,805,737 bytes; index
`6e61ef6a8aba534521b6dd3a4bf1f0e0c0df04c785b5666d3b01ac458f875865`)
also passes completed/incomplete public removal, repeated removal and cache-only
reinstallation in both builds, using only disposable owned installed copies.
Logs: `/private/tmp/samosa-incomplete-remove-actual-final.log` and
`/private/tmp/samosa-incomplete-remove-actual-final-sanitize.log`.
