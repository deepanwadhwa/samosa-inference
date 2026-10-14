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
