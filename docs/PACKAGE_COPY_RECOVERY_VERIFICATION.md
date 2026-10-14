# Registered copy recovery evidence

Normal and ASan/UBSan staging gates passed on macOS arm64 with 101 checks each.
The gate kills a real child after the first 65,536 bytes of a 131,081-byte copy,
waits for its observed SIGKILL termination, and resumes using its persisted inode
record. Before successful recovery it substitutes a different inode and corrupts
a retained byte; both refuse and preserve the offending state. Restoring the
registered inode/prefix permits exact completed-tree verification. Source seek
position remains unchanged and completed copy/journal metadata is cleaned up.
A separately cancelled complete copy tests the recorded two-link publication
interval: full revalidation succeeds and only the registered temporary alias is
removed. Existing unregistered foreign temporary-file refusal remains covered.

Log: `/private/tmp/samosa-registered-copy-recovery-final.log`. Native assembly now
uses the registered staging API for all acquired/cache/remote files. Local/cache
and complete HTTPS package gates passed normal and ASan/UBSan after integration;
log: `/private/tmp/samosa-registered-copy-recovery.log`. Static analysis of the
new copy-record module and modified staging code emitted no diagnostics.

This qualifies registered copy recovery, not bootstrap/record-registration crash
gaps, all installation metadata publication windows, reclamation, production
catalog entries or actual remote-model readiness. Those remain area-2 work. Work
continues in installation; no lifecycle/scheduling/service work is opened here.

## Copy-record publication recovery

Normal and ASan/UBSan staging gates now pass 113 checks each. The killed-child
partial-copy case additionally moves its real persisted record into the pending
publication name, corrupts one metadata byte and verifies refusal/preservation,
then restores it. The valid record completes publication during recovery and
still refuses a substituted copy inode or corrupt data prefix. A separate known
record/pending hardlink pair exercises the metadata two-link interval before
successful copy completion. No unregistered copy or malformed tuple is adopted.

Local/cache and complete production HTTPS assembly gates also passed normal and
ASan/UBSan with this recovery enabled. Log:
`/private/tmp/samosa-copy-record-publication-recovery.log`. Static analysis of the
modified copy-record module emitted no diagnostics. This step stays within area
2 and closes these specific owned-record publication cases; the remaining
installation recovery and catalog requirements are unchanged.
