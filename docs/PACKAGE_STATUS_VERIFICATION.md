# Read-only native installation status evidence

The local/cache native assembly harness checks unregistered status without lease
creation, busy status while an actual independent lease descriptor is held,
installed verification after publication, inspection without source/copy policy,
cancellation with untouched output and corrupted-payload refusal without output
changes or repair. The complete HTTPS fixture queries incomplete-unverified status
after cancelled reservation publication, then resumes and qualifies online/offline
installation normally. Missing identity verification never creates identity state.

Normal and ASan/UBSan local/cache and complete HTTPS fixture gates passed on macOS
arm64. Identity unit gates also passed with 38 checks each. Logs:
`/private/tmp/samosa-package-status-final.log` and
`/private/tmp/samosa-package-status-identity.log`.
Static analysis of the modified assembly/status code emitted no diagnostics.
These checks do not qualify actual-model Linux operation, the authenticated public
installer, reclamation or registered recovery. Model readiness remains separate.

Independent CI passed preceding persistent-identity revision `a3be2f2` in run
38596448801 on macOS arm64 and Linux x86_64, now pinned in Chat. Status code requires
its own independent CI outcome before the pin moves again.
