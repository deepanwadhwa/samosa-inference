# Model admission v1

Host config shares one private admission directory across model operations.
--queue-capacity0..8 (default0) and --admission-wait-ms1..60000 (default10000)
bound accepted work. One active operation holds provider.lock; accepted waiters
receive monotonic FIFO tickets. Count and generation share this policy. Later
arrivals and zero-capacity callers cannot overtake queued tickets. Queue_full or
busy refuses before provider work. Wait deadline uses the monotonic clock.

admission.sqlite3 stores at most8 pending tickets and a monotonic sequence, with
a1MiB database ceiling. Nine fixed waiter lease files and one short gate lease
are owner-private0600 beneath0700 state. Released waiter leases prove worker
death and allow safe dead-ticket removal without PID assumptions. Ticket cleanup
does not replay requests. Provider process death releases its own active lease;
last-good semantic session recovery remains separately governed by sessions-v1.

Capacity zero is explicit immediate-busy policy through the same scheduler.
Public operation cancellation/residency/model installation are not qualified by
this FIFO subset. S12b adds scoped operation cancellation; no global model flags.
