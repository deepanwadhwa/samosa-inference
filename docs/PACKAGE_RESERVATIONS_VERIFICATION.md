# Persistent package reservation evidence

Normal and ASan/UBSan native reservation gates passed on macOS arm64 with 40 checks
each. They exercise exact retry/idempotency, committed reservation persistence
across child process exit, shared byte and count exhaustion, changed package-index
identity refusal, lease contention, untouched output on failure, and refusal to
delete unregistered pending metadata. The pool remains charged for both packages
after the child exits. Log: `/private/tmp/samosa-package-reservations.log`.

The complete 11-file HTTPS fixture additionally configures one pool for two
package copies. Remote acquisition publishes the first package; a cold second
package assembles from cache without download configuration. A third package
refuses with `package_reservation_quota` before its payload directory is created.
Repeated verification/assembly does not double-charge the first package. Normal
and ASan/UBSan complete package gates passed, with server assertions confirming
one GET per unique file identity. Log:
`/private/tmp/samosa-reserved-remote-package.log`. The existing core local assembly
fixture also passed normal/sanitizer runs after integration. Clang static analysis
of the new reservation module emitted no diagnostics.

Independent CI passed preceding complete HTTPS fixture revision `3301abe` in run
38593648303 on macOS arm64 and Linux x86_64; Chat's pin now follows that qualified
revision. Reservation code requires its own independent CI outcome. These logical
payload reservations do not qualify actual models or a public installation
service. Cache-inclusive physical allocation, reclamation, registered crash
recovery and full catalog/lifecycle integration remain open.

## Controlled reservations

The reservation API now honors trusted request controls before state access,
between bounded ledger observations and before publishing a reservation. New
unit checks cancel before lock creation and when the owned pending record exists,
assert untouched output, absence of pending metadata and absence of charged
records, then reserve successfully. Normal and ASan/UBSan runs passed with 52
checks each; final log:
`/private/tmp/samosa-controlled-package-reserve-final.log`.

The complete HTTPS assembly fixture cancels at pending reservation publication,
checks that no payload directory or pending record remains, then completes both
online and offline copies under the original shared ceiling. Normal/sanitizer
runs passed; log: `/private/tmp/samosa-controlled-reserved-assembly.log`.
The existing local compatibility fixture also passed both variants. Clang analysis
of the modified reservation module emitted no diagnostics. This adds cooperative
request control and does not establish hard filesystem syscall deadlines,
reclamation, crash recovery or the public installation coordinator.

## Reservation publication recovery

Normal and ASan/UBSan reservation gates pass with 66 checks each. A real child
publisher is killed after writing/syncing its reservation proposal. The parent
observes SIGKILL termination, verifies that another package cannot adopt it,
checks cancellation and a reduced ceiling both retain the proposal, then commits
exactly one charge under an admissible ceiling. A reconstructed known record/
pending two-link interval also recovers without double charging. Existing shared
quota, conflicting identity, foreign pending metadata, process-exit persistence
and contention checks remain covered. Final unit log:
`/private/tmp/samosa-reservation-publication-recovery-final.log`.

Local/cache and complete production HTTPS assembly gates passed normal and
ASan/UBSan with reservation recovery enabled; log:
`/private/tmp/samosa-reservation-publication-recovery.log`. Clang static analysis
of the modified reservation code emitted no diagnostics. This is installation
recovery work, not live engine scheduling or automatic reclamation. Remaining
bootstrap/torn-state, owned removal and catalog requirements stay open in area 2.
