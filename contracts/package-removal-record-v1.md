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
