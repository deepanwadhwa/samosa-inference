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
