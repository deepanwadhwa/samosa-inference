# Per-file package staging checkpoint

The staging copy reuses the pinned package index's path grammar and existing
controlled artifact hashes. It copies from held verified cache descriptors into
private staging directories with fixed buffers, validates complete bytes, then
publishes an absent indexed name without replacement. Existing matching payloads
are reverified; corrupt/unsafe/partial state is preserved and refused.

`make test-package-stage sanitize-package-stage` passes61 checks per mode locally
on macOS. Cases cover three-byte and131081-byte/multiple-buffer copies, preserved
input seek position, hardlinked cache input versus single-link read-only output,
complete-tree verification, exact idempotent staging, preexisting partial
preservation, symlink/corrupt existing destination refusal, cancellation before
work and after temporary creation, and safe temporary cleanup. Log:
`/private/tmp/samosa-package-stage-final-tests.log`.
The shared path-parser refactor also passed682 package-index checks per mode in
`/private/tmp/samosa-package-stage-tests.log`.

Targets join default model-free test/sanitizer qualification. These controlled
payloads are not actual model installations. The caller still owes package lease,
quota and named-root ownership; a crash ledger/resume protocol, complete package
publication, compatibility and actual installed-engine gates remain open.
See [staging semantics](../contracts/package-stage-v1.md) for the temporary/two-link
crash windows and exactly what this primitive does and does not persist.
