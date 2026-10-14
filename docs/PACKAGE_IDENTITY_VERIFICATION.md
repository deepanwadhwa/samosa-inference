# Persistent assembly identity evidence

Native assembly now binds a private package directory to its pinned index before
reservation, acquisition and copying. Normal and ASan/UBSan unit gates passed on
macOS arm64 with 35 checks each: cancellation before identity publication, exact
retry, changed-pin refusal, copied-record refusal in another root, preservation
of unknown pending metadata and symlink refusal. The created record is private,
read-only and single-link. Existing identity contents/bindings are checked again
after control callbacks; new pending contents/bindings are checked before linking.

The native local/cache assembly fixture checks a changed index pin on an already
completed package, then restores the pin and successfully revalidates the original.
Local and complete HTTPS fixture assembly passed normal and ASan/UBSan gates with
persistent identity enabled, including controlled reservations, cold offline
installation and shared quota refusal. Log:
`/private/tmp/samosa-package-identity-final.log`. Static analysis of the new identity
module emitted no diagnostics. Unit gates join default independent CI targets.

Independent CI passed controlled-reservation revision `81266fe` in run
38595432160 on macOS arm64 and Linux x86_64, now pinned in Chat. This identity
revision requires its own CI result. These gates do not establish recovery of
unknown partial files, reclamation, a public installation coordinator, actual
remote-model qualification or loaded model readiness.
