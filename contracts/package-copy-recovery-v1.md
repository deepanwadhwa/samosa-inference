# Registered native package-copy recovery v1

Assembly holds its package lease and a private `copy-journal/` directory outside
`payload/`. Before writing a new copy it persists a record binding the pinned
index, relative file name, declared bytes/hash and held payload-root, parent and
copy device/inode identities. Records are published without replacement, private,
read-only and single-link. Registration is fsynced before payload copying starts.

A registered partial copy can resume only after the source is fully verified,
the record matches all current identities, and every retained byte matches the
source prefix. Prefix reads use fixed 64 KiB buffers and poll request controls.
No partial is truncated or silently repaired. Corrupt prefixes, substituted
inodes, unsafe metadata and unregistered temporary files refuse and remain intact.
The source descriptor's seek position is preserved. Cancellation leaves a
registered partial available for a subsequent controlled retry.

Completed registered copies are fully rehashed before publication. Recovery also
handles the recorded temporary/final two-link interval: both names must identify
the recorded inode, full content must verify, and only the known temporary name
is removed. The final file is synced and remains private, read-only and single-
link before the matching journal record is forgotten. Whole-package compatibility
and completion publication still run afterward; recovered copying is not readiness.
The original non-journaled staging API preserves its strict refusal behavior.

Caller ownership and the package lease remain required. The journal is observed
metadata, not protection against arbitrary same-user writers. Crashes during
initial temporary creation/record registration can leave unregistered or malformed
state and currently refuse; they are not automatically adopted. Reservation,
identity and completion-record interrupted publication/reclamation also remain
separate unfinished installation recovery cases. Area 2 is not closed by this
registered-copy capability alone.

## Interrupted copy-record publication

A complete pending copy record can now finish publication after every expected
byte and the current root/parent/copy tuple match. The pending record is synced,
linked without replacement and its known pending name removed before continuing.
If published and pending names are the same private two-link metadata inode,
recovery verifies both bindings and contents, removes only that pending alias,
syncs the journal and verifies the remaining single-link record. Another alias,
a different pending inode, mismatched content or a malformed record refuses and
remains untouched. Recovery is under the caller's package lease and does not
claim protection against arbitrary same-user writers.

This closes complete pending-record and known metadata two-link publication
recovery. A temporary copy with no complete matching record, or a torn record
whose ownership tuple cannot be verified, still refuses. Initial bootstrap,
reservation/identity/completion metadata recovery and reclamation remain required
before installation as a whole is complete.
