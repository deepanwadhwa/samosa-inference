# Persistent removal inventory record

The removal record captures verified payload inventory together with the held
package-root and registered completion-file identities. The caller must retain
an exclusive package lease, validate completion/identity ownership, and supply
the verified snapshot. Capture/serialization do not grant removal authority or
delete contents; the removal coordinator must perform those ownership checks.

`package_removal_record_store` publishes private `removal.record` without replacing
an existing name. It writes a private exclusive pending file, sets read-only mode,
fsyncs it, links the final name without replacement, removes only its own bound
temporary and fsyncs the package directory. Foreign/existing state refuses.
Controls apply between bounded writes and before publication. Normal error cleanup
does not remove a retained foreign record.

`package_removal_record_load` checks private ownership/single-link/name binding,
size/checksum/framing, selected index identity and held package-root device/inode.
It validates all file sizes/types/inodes and all required directory ancestors,
rejecting omitted/duplicate directory paths and identities. It rechecks record
metadata after reading. Loading is read-only and leaves output untouched on failure.

The versioned `SMRMV001` format uses explicit little-endian 64-bit values and
length-prefixed paths, followed by a SHA-256 checksum. Each identity stores device,
inode, UID, mode, link count, size and nanosecond mtime/ctime components. Signed
seconds use modulo-2^64 encoding. Native struct padding/layout is never persisted.
Records are bounded to 2MiB, 512 files and 3,584 directories. The checksum detects
corruption; it is not external authentication or authority to adopt unknown files.

Normal and ASan/UBSan gates pass 64 checks, including capture/round-trip, cancelled
store/load, absent/existing record, read-only publication, wrong index, malformed
counts with a valid checksum, truncation, corruption, hardlink refusal, required
ancestor omission and unchanged outputs. Log:
`/private/tmp/samosa-package-removal-record.log`.

Interrupted pending/two-link record publication, partial file deletion/recovery,
completion retirement and quota release remain the following removal steps.
No package data is removed by this record API; component 2 remains active.

## Interrupted publication recovery

Store now recovers a complete pending proposal only after comparing every encoded
byte with the expected inventory and rechecking its private inode/name identity.
It supports a single-link proposal before final publication and the exact known
pending/final two-link pair. Other aliases, another final inode, malformed bytes
or changed bindings refuse while preserving retained state. Controls run during
bounded comparison and before publication; syncing finishes before success.

`package_removal_record_load_pending` reconstructs a complete proposal from disk
after the original process/in-memory inventory is lost. It performs the same
bounded checksum/index/root/schema verification as normal loading. Two-link state
requires the final name to identify the same inode, rechecked after reading.
Loading never repairs state; the caller can then pass the reconstructed record
to store for explicit recovery under its exclusive lease.

Normal and ASan/UBSan gates pass 93 checks. A real child is killed after pending
record fsync and before final linking. Cancelled/corrupt retries preserve the
proposal; restored bytes load from disk and complete publication. The known
two-link case also loads/recoveries successfully. An unrelated second alias with
no known final name refuses both pending load and recovery. Log:
`/private/tmp/samosa-removal-record-recovery.log`. Torn/unverifiable proposals
remain fail-closed. File deletion, completion retirement and quota release are
the following removal steps; no package data is deleted here.

## Incomplete-package inventory

`SMRMP001` uses the same bounded encoding with an explicitly absent completion
identity (all ten values zero). File slots remain in the original pinned index
order; an absent indexed file has an all-zero identity. Every present file must
still match the full pinned size/hash and private inode checks. Ancestors of
present files and captured directories must exist in the inventory. Missing
files and their absent ancestors need no invented inode identity.

`package_tree_snapshot_incomplete` verifies the entire remaining namespace twice,
including unchanged file-presence sets and entry counts, and captures only pinned
published files and known ancestor directories. It is a removal-only observation
and cannot qualify an installation. `package_removal_record_capture_incomplete`
encodes this inventory after the coordinator verifies registered package ownership
and settles copy journals under the exclusive assembly lease.

Readers require the magic and completion identity to agree. An incomplete record
with a completed-format magic refuses even with a recomputed valid checksum.
Existing `SMRMV001` records keep their complete-file/completion requirements.
Both formats use the same durable pending-publication recovery and removal barrier.
