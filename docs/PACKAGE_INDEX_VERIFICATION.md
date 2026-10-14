# Multi-file acquisition index checkpoint

The existing verified/resumable acquisition implementation owns publication of
single pinned files, with a bounded16-artifact store. The new package index
describes up to512 complete payload files for later coordinated acquisition and
publication. It reuses artifact SHA-256 helpers, catalog minimum-OS policy and
the registry's native platform names. It does not raise existing store limits or
claim installation/backend compatibility from metadata alone.

Local native macOS normal and ASan/UBSan runs passed682 checks each:
`make test-package-index sanitize-package-index`. Tests cover exact full-index
pins, truncated inputs with matching truncated-byte pins, unchanged output on
failure, unknown schema fields/roles, private path grammar and native eSpeak
variant names, duplicate/case-alias/file-directory collisions, inconsistent sizes
for the same payload pin,512/513-file boundaries,64GiB aggregate bounds and
foreign-platform/minimum-OS refusal. Fixture pins are explicit test data, not
published model entries. Log: `/private/tmp/samosa-package-index-qualified-tests.log`.

Both targets are included in the existing model-free `make test`/`make sanitize`
workflow, so clean archive CI will exercise them on the existing macOS/Linux
matrix. Pending work: coordinated file-store capacity/quota reservation, verified
file acquisition, complete private staging and atomic publication, backend/loader
compatibility, resumable package status/controls and actual package installation.
Area2 is active; this checkpoint does not close it or the seven-area goal.
