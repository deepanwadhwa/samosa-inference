# Artifact capacity for multi-file acquisition

The verified/resumable artifact store now accepts a trusted configurable identity
capacity1..1024. Zero preserves the existing16-entry default. The same global
install lock, registered inode checks, full hashes, aggregate logical byte
reservations, corruption refusal and no-eviction behavior remain authoritative.
Artifact/setup CLI host flags expose the bound; public requests cannot choose it.

Local macOS results:

- `make test-install sanitize-install`:2340 normal/2342 sanitizer checks, including actual
  installation/publication of513 distinct three-byte artifacts, refusal at the
  configured boundary, the old16-entry default, independent byte quota under a
  larger capacity, retained status and offline verification without original input,
  and capacity above1024 refusing before creating state. Original interruption,
  death/resume, foreign/root and executable publication tests remain intact.
- Artifact CLI26 checks: valid explicit host capacity and unchanged offline result,
  zero/out-of-range/negative/fractional/duplicate host flag refusal, and a public
  JSON capacity override refusing without altering host policy.
- `make test-setup sanitize-setup`:47 checks per mode: existing real HTTPS/TLS,
  credentials, cancellation/resume, last-good and offline retry, plus host capacity
  and public-override/invalid/duplicate flag checks.

Logs: `/private/tmp/samosa-artifact-capacity-tests.log`,
`/private/tmp/samosa-artifact-capacity-public-tests.log`, and
`/private/tmp/samosa-artifact-capacity-setup-tests.log`.
The public test batch first used nonexistent `test-setup-cli` targets; the actual
targets above were then run and passed. No setup pass is inferred from that
initial stopped command.

The preceding package-index CI run38493402192 passed its new index gates but
failed Linux's existing Whisper sanitizer marker parser. Its test producer
exposed the marker before closing JSON. The fixture now atomically renames a
closed file, retaining the original engine/input/output checks. The same
partial-marker pattern was removed from Summary and Temporal fixtures following
source inspection; production inference/validation is unchanged by those fixes.
Native Whisper508/514 and Summary478/478 normal/sanitizer gates passed locally;
Temporal is qualified separately with its existing metadata fixture input.
Its corrected local normal/sanitizer run passes1023/1029 checks in
`/private/tmp/samosa-temporal-atomic-marker-tests.log`. A temporary local variable
name collided with an existing `struct stat`; that compile error was fixed before
recording those results. Summary output is in
`/private/tmp/samosa-fixture-atomic-marker-tests.log`.
Shared-header integration checks also pass: remote artifact167 checks per mode
and activation110/118 normal/sanitizer checks, recorded in
`/private/tmp/samosa-artifact-capacity-integration-tests.log`.

This capacity change enables later full package acquisition; it does not implement
coordinated package reservations, complete private staging, atomic publication,
loader/backend compatibility, public package controls or installed readiness.
Logical payload reservations do not include filesystem allocation/metadata or
additional published package copies. Those original area2/resource requirements
remain open; no complete numbered area is closed here.
