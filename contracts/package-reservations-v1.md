# Persistent shared payload-copy reservations v1

A trusted native assembly may configure a private reservation pool, nonzero
logical byte ceiling and capacity (1–1024). Before payload creation or copying,
assembly calls `package_reserve` while holding its package lease. The pool's
exclusive nonblocking `reserve.lock` serializes cooperating reservations. Pool
and package directories must be private and owned by the current user; ledger
records are private regular single-link files. Busy, malformed, foreign or
unregistered ledger state refuses rather than guessing ownership.

Reservations identify a package by its held directory device/inode and pinned
index SHA-256, and charge its declared payload byte sum. Repeating the same
identity/size is idempotent. A different index/size for an already reserved package
identity refuses. Every bounded ledger record is parsed and checked against its
canonical name/content; totals and count must fit the trusted common pool policy.
Quota refusal leaves output untouched. A new record is written and fsynced,
published without replacement, made read-only, checked against its named inode,
and the pool directory is fsynced before assembly may write payload bytes.

Reservations survive process exit and remain charged after cancellation, failure
or deletion of an external package path. No automatic expiry or release guesses
whether bytes remain. Reclamation needs a future owned package removal/recovery
protocol. Crash-retained pending records or two-link publication intervals refuse
until that protocol can establish ownership; this is not complete transaction
recovery. The caller owns named pool identity and exclusive package ownership.
The lease coordinates cooperating APIs, not arbitrary same-user writers.

This ceiling covers logical declared payload copies only. Artifact cache/stages
retain their separate existing quota. Metadata, filesystem allocation/overhead,
other directories, models outside the pool and live engine memory are excluded.
All cooperating installers must use the same trusted pool policy. Public requests
must not select its descriptors, ceiling or capacity. A zero assembly reservation
ceiling preserves existing callers' explicit ownership of external reservations;
the eventual public installation coordinator must configure the required pool.

## Request control

`package_reserve_controlled` polls the trusted control before opening pool state,
between bounded ledger entries, after the scan and before reservation publication.
Assembly passes its existing request control and context. Request errors propagate
unchanged; output remains untouched. Pre-publication cancellation removes only
the pending inode created by that call, leaves no charged record, and prevents
payload creation. A reservation already committed remains charged if outer work
subsequently cancels. Filesystem calls retain platform latency. The original
`package_reserve` entry point remains a wrapper without a control hook.

## Interrupted reservation publication

A complete matching `.reservation.pending` proposal for the exact held package
root/index/byte count can now resume publication under the pool lease. The entire
existing pool is parsed and checked against the current byte/count ceiling before
any recovery mutation. A lower ceiling, cancellation, another package tuple or
malformed metadata refuses while retaining the proposal. A known record/pending
pair bound to the same matching two-link inode is counted once, then its pending
alias is removed only after quota validation. Syncing and single-link/name checks
finish before returning the observed charge. No reservation is expired or released.

Other-package pending state is not adopted by a new requester. The coordinator
must resume the owning package. Torn/unverifiable proposals and bootstrap state
still refuse; reclamation and the remaining installation requirements are unchanged.
