# Verified package inode inventory

`package_tree_snapshot` runs the complete pinned closed-tree verifier and returns
the observed payload root, per-file stat identities in index order, and canonical
directory paths with their stat identities. It records file/directory counts and
the pinned index hash only after all file hashes, namespace checks, second inode
walk, root recheck and request controls pass. Output remains untouched on failure.

The caller allocates the bounded large snapshot on the heap and retains its
coordination lease/named-root bindings. It holds at most 512 files and 3,584
directories. This is a native in-memory record; raw struct stat bytes are not a
persistent or portable serialization format. Snapshot capture does not mutate
package contents, authorize deletion, adopt a substituted inode or guarantee
future immutability. Removal must bind the inventory to registered package/
completion ownership, persist an explicit journal and recheck names/inodes before
each mutation. Partial-copy removal requires separate registered ownership evidence.

Normal and ASan/UBSan tree gates pass 146 checks each. Added cases compare root,
every indexed file and directory against held/named filesystem identities and
verify unchanged outputs on cancellation/corrupt content. Existing complete HTTPS
package gates pass in both builds. Logs:
`/private/tmp/samosa-package-owned-snapshot-tree.log` and
`/private/tmp/samosa-package-owned-snapshot.log`.

Actual Pocket snapshot capture passes normally: 19 files, 5 directories,
265,042,042 bytes, index
`7dabd32730879d00d88ef6c9b477a9a861e0009e2af7ebb6c383de95740fd3e9`.
Actual Kokoro capture passes under ASan/UBSan: 365 files, 40 directories,
218,337,007 bytes, index
`abacddef8cd0e33e7d4d07c0bc434c0996a2b4ab4429fece432897582bb88c81`.
Logs: `/private/tmp/samosa-pocket-owned-snapshot.log` and
`/private/tmp/samosa-kokoro-owned-snapshot-sanitize.log`. Original sources are
unchanged. This completes inventory capture, not the removal protocol or component 2.
