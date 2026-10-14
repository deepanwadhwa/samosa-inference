# Registered package consumer leases

`package_lease_acquire` opens the existing private single-link `assemble.lock`
and acquires a nonblocking shared flock. It verifies the held package root's
persistent identity against the selected index and rechecks the lock's named
inode binding. It never creates state, repairs metadata or infers ownership from
a stale process/receipt. Failure leaves the output descriptor untouched.

The successful result is an owned close-on-exec descriptor. The caller must keep
it open while any worker can use package files, retain named-root bindings and
verify publication/integrity before use. Registered identity alone is not
completion, integrity, backend compatibility or readiness. Worker/guardian lease
retention is a later lifecycle integration requirement; this primitive does not
launch or manage an engine.

Multiple readers can share a package. Existing assembly/reclamation writers use
exclusive locks on the same file and refuse while a reader remains. Read-only
assembly status now takes a shared lock, so it can verify an installed package
alongside consumers while still reporting busy for an exclusive writer. These
leases coordinate cooperating APIs, not arbitrary external same-user writers.

Normal and ASan/UBSan lease gates pass 54 checks each: cancellation/no creation,
absent registration, writer contention, multiple readers, close-on-exec ownership,
changed index identity, hardlink/symlink refusal and a real child's SIGKILL releasing
its held lease. Log `/private/tmp/samosa-package-consumer-lease.log`.

Controlled complete HTTPS regression gates pass in both builds. Actual llama/
EmbeddingGemma assembly also passes in both builds with an added consumer lease:
assembly refuses busy, read-only status returns installed_verified, and closing
the consumer releases its hold. Logs `/private/tmp/samosa-package-reader-integration.log`,
`/private/tmp/samosa-llama-reader-normal.log` and
`/private/tmp/samosa-llama-reader-sanitize.log`. This supplies removal's ownership
prerequisite within component 2; it does not close removal or lifecycle work.

## Removal admission barrier

After acquiring the package lock, consumer leases, ordinary assembly and read-only
installed-status verification check for `removal.record` or its pending name.
Either presence returns `package_removal_in_progress`; malformed bytes or a
symlink still block admission and are preserved. Presence never authorizes file
deletion or adopts that record. Only the removal coordinator may recover or finish
the removal transaction. Outputs remain untouched, and no payload/completion is
changed by the refusal. This prevents ordinary resume from recreating files during
partial removal and prevents status from reporting that package installed_verified.

Normal and ASan/UBSan consumer gates now pass 64 checks, including foreign pending
metadata and a removal-record symlink. Controlled complete HTTPS package gates also
test status/assembly refusal with unchanged outputs and retained completion;
removing only the fixture-owned marker restores verified status. Log:
`/private/tmp/samosa-removal-admission-barrier.log`. File deletion and recovery remain
the next removal protocol steps within component 2.
