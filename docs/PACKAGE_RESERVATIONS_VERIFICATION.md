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
