# Persistent package assembly identity v1

While holding `assemble.lock`, native assembly binds its held private package
directory to the parsed pinned index before reserving copy bytes, acquiring files,
or copying payloads. `assembly.identity` contains a format version, index SHA-256
and the held directory device/inode. It remains independent of payload completion.
A changed pin or a record copied to a different directory refuses before further
installation work. Repeating the exact identity is idempotent.

The record is a private regular single-link file. Create a private exclusive
pending file, write expected bytes, make it read-only, fsync, check request control
and observed identity/content, then publish without replacement. Remove only that
call's pending inode and fsync the directory before returning success. Existing
records are compared against the expected bytes and named inode before and after
request control. Unknown pending files, symlinks, aliases or conflicting content
refuse and remain untouched. Failure does not authorize repair or deletion.

The caller owns the package directory and its named-root binding; the assembly
lease coordinates cooperating callers. This is observed persistent identity, not
future immutability against arbitrary same-user writers. A newly bound legacy
private directory still requires normal per-file and whole-tree verification;
identity creation does not assert that preexisting files were produced by this
installer. Identity metadata never proves byte integrity, compatibility, completion
or model readiness, and never authorizes recovery of an unregistered partial inode.

Crash-retained pending files and the two-link publication interval still require
registered recovery. Public owner/request isolation and full installation status
belong to the upcoming coordinator; the native identity record is its prerequisite,
not that complete service. Metadata bytes remain outside logical copy reservations.

## Interrupted publication recovery

Binding under the assembly lease can now finish a complete pending identity record
only when its full expected index/root bytes and private single-link inode match.
A published/pending pair can be cleaned up only when both names bind the same
matching private two-link metadata inode. Controls are checked before mutation;
then recovery syncs the record/directory and revalidates the remaining single-link
identity. Malformed records, another pending inode, unknown aliases or a different
root/index refuse and remain untouched. Initial torn/unverifiable metadata still
requires owned recovery; no unrelated partial file is adopted.

`package_identity_verify` remains read-only. A verifiable interrupted publication
returns `package_identity_recovery_required` instead of repairing names, changing
permissions or creating state. Binding performs recovery; status does not.
