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

## Identity publication recovery

Normal and ASan/UBSan identity gates pass with 61 checks each. New cases cover a
complete pending record, cancellation preserving it, the known metadata two-link
publication interval and read-only verification reporting recovery-required without
changing either name. A real child is killed at the post-write/pre-publication
control boundary; its observed SIGKILL termination leaves a complete pending
identity. The parent verifies the condition read-only, then binds/revalidates and
checks pending-name removal. Changed-pin, copied-root, unknown pending and symlink
refusal remain covered. Final log:
`/private/tmp/samosa-identity-publication-recovery-final.log`.

Local/cache and complete HTTPS assembly gates passed both normal and ASan/UBSan
with identity recovery enabled; log:
`/private/tmp/samosa-identity-publication-recovery.log`. Clang analysis of the
modified identity module emitted no diagnostics. This closes these identity
publication cases within installation, not torn/bootstrap metadata, reservation
or completion recovery, reclamation or production catalog requirements. Area 2
remains the only active implementation area.
