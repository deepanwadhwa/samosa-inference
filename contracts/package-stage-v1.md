# Bounded per-file package staging

`package_stage_file` copies one indexed payload from a held read-only artifact
descriptor into an installer-owned private staging root. The caller must retain
its package coordination lease, quota reservation and named-root binding. It uses
the shared index path/role grammar, verifies the input pin before namespace work,
creates required private directories without following aliases, and copies with
a fixed64KiB buffer using positional reads. Source seek position is preserved.
Verified artifact-cache hardlinks are valid inputs; published payloads must be
private read-only regular single-link files.

The destination remains a reserved private temporary name until its full count
and checksum, the input's final checksum, controls and descriptor/name binding
pass. Data/modes are synced; `linkat` publishes only if the indexed name is absent,
then the temporary name is removed and its directory synced. It never replaces a
concurrently created name. Existing indexed files are reverified for idempotent
staging; corrupt/unsafe files are refused and preserved. Existing partial names
are not adopted, overwritten or deleted. Ordinary error/cancellation cleanup
removes only a temporary inode created and still bound by this call.

A crash can leave a partial temporary file or a brief two-link published inode.
Complete-tree validation refuses such state. Recovery needs the package
coordinator's registered inode/transaction ledger; this primitive does not claim
package crash recovery or resume. Directory creation may remain after a failed
copy. Whole-package acquisition, coordinated disk reservations, loader/backend
compatibility, sealing and atomic package publication are still required. No
engine is launched and no installed/readiness state is returned by staging alone.
