# Closed package payload checkpoint

The generic package-tree verifier reuses the existing controlled artifact hashes
and pinned package index. A bounded recursive walk checks exact namespace closure,
ownership/modes, complete hashes and descriptor/named-inode binding. A second walk
rechecks the observed file/directory set. It reads only and performs no package
publication or engine/model initialization.

`make test-package-tree sanitize-package-tree` passes117 checks per mode on local
macOS. Cases cover complete nested payloads including native eSpeak variant names,
repeated verification, missing/unlisted files, empty directories, symlink/hardlink/
case aliases, FIFO refusal without blocking, unsafe root/file modes, length/hash
corruption, caller cancellation, controlled mutation and unchanged result on
failure. Log: `/private/tmp/samosa-package-tree-qualified-tests.log`.

Actual existing pinned package trees also pass both modes:

| Tree | Files/directories | Payload bytes | Generic index SHA-256 |
| --- | --- | --- | --- |
| Pocket |19/5 |265042042 |`aaaa9232862667ab5fc2da829f8cce074b008af17d99f38acc3daa83ba17b943` |
| Kokoro |365/40 |218337007 |`aac8d703649d3578dffcec7516980646ee3c1171d27f1648ecb253e98eec12ab` |

These generic indices were constructed from the already pinned native speech
manifests, after checking each original manifest's exact digest, and include that
native manifest as a metadata payload file. Roots remain the existing read-only
test packages `/private/tmp/samosa-pocket-pinned.0mx98nek` and
`/private/tmp/samosa-kokoro-pinned.s5_d_czq`. No package files or permissions were
changed. Their original manifest pins are recorded in the speech verification
records. The generic index recipe/minimum-OS values are explicit integrity-fixture
metadata, not new production compatibility declarations.

Commands: `build/test-package-tree --verify <root> <private index JSON> <index pin>`
and `build/test-package-tree-sanitize` with identical arguments. Index paths are
`/private/tmp/samosa-package-tree-{pocket,kokoro}-index.json`; logs are
`/private/tmp/samosa-package-tree-{pocket,kokoro}-{normal,sanitize}.log`.
These passes qualify whole-tree byte integrity observations, not a new actual
model execution, installation, loader/backend compatibility or readiness claim.

The targets join default model-free `make test`/`make sanitize`, so independent
archive CI will exercise controlled filesystem behavior on both existing
platforms. Pending: coordinated package quotas/acquisition, normalized private
staging and atomic publication, retained status/resume/cancellation, compatibility
profiles and actual installation. No complete numbered area is closed here.
