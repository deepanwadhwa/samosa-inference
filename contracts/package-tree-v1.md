# Closed indexed payload tree v1

`package_tree_verify` validates an installer-held read-only directory descriptor
against an already parsed, pinned `PackageIndex`. It never changes files, seeks
the caller's directory enumeration state, launches a model or chooses a path from
a public request. The index itself is stored outside this payload tree unless it
is explicitly one of the payload files.

Every indexed file must be present at its exact namespace spelling, privately
owned by the effective user, regular, single-link, on the root filesystem, and
match its declared complete byte count and SHA-256. Only directories required by
an indexed path are accepted. Unlisted files, empty/unneeded directories,
symlinks, hardlinks, FIFO/socket/device nodes, unsafe modes, other filesystem
devices and observed inode aliases refuse. Read-only and owner-writable staging
files may be observed; verification does not seal them or confer executability.

The walk is bounded to512 files and3584 required directories, with255-byte
relative names inherited from the index. Directory enumeration uses independent
descriptors. File hashes reuse the controlled artifact verifier's fixed buffers
and per-file time bound. Caller cancellation/deadline errors pass through between
entries and bounded reads.

After hashing, a second namespace/metadata walk checks every file/directory
identity and root metadata again, detecting late observed changes while later
payloads were read. Successful output contains file/directory counts, total
payload bytes and the pinned index identity. Output is untouched on failure.
This is an integrity observation, not an immutable snapshot or a guarantee
against arbitrary concurrent same-user writers after observation. The installer
must retain its coordination lease and prove named-root binding before publication.

This verifier does not implement acquisition, coordinated reservations, resumable
package status, atomic publication, backend/loader compatibility, activation or
readiness. Those original installation/lifecycle requirements remain separate.
