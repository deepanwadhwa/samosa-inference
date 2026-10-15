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

## Explicit empty-package reclamation

`package_reservation_release_empty` takes the package assembly lease followed by
the pool lease. It permits only assembly-lock and identity metadata in the package
directory; payloads, journals, completions and unknown names refuse. It compares
the reservation's complete canonical index/root/byte tuple and single-link inode
before removing that charge and fsyncing the pool. Pending pool metadata refuses.
Request controls and named-root bindings remain the trusted caller's contract.
Output is 1 for a released charge, 0 if that exact charge is already absent, and
unchanged on failure. It never deletes package contents, expires reservations,
or guesses whether installed data is still in use. A later installation must
reserve again before creating payload state. Reclamation of partial/installed
trees still requires the full owned removal protocol.

Normal and ASan/UBSan reservation gates pass 93 checks each, including cancelled
release, payload refusal, held assembly-lease contention, mismatched byte counts,
foreign pending metadata, idempotent release and reuse of reclaimed capacity.
Complete HTTPS package gates also pass normally and under ASan/UBSan. Local log:
`/private/tmp/samosa-release-empty-final.log`.

## Retired-package release

`package_remove_release` selects a trusted native retirement validator and calls
the shared checked-release primitive. It takes assembly then pool leases, verifies
registered identity and the durable removal record, requires payload/completion/
pending-removal names absent, and rejects unknown package entries or nonempty copy
journals. Only private recognized metadata can remain. Validation runs before
opening pool state and again under both leases before unlinking the exact
index/root/byte charge. Public requests cannot supply this validator.

The existing empty-package release uses the same primitive with its original
strict empty-package validator. Both recheck lock bindings and fsync the pool
even on an already-absent retry, settling a death between unlink and directory
sync before callers may retire the removal journal. Outputs remain untouched on
failure; success reports 1 released or 0 already absent. The removal record and
admission barrier remain in place until explicit verified metadata cleanup.

Normal and ASan/UBSan removal gates pass 149 checks, including release before
retirement refusal, cancellation, unknown entry/nonempty journal refusal, reader
and pool contention, preserved charge counts, exact release and idempotent retry.
All 97 existing reservation checks and complete HTTPS package gates pass in both
builds. Log: `/private/tmp/samosa-retired-reservation-release.log`. Only controlled
fixture reservations were released; no actual-model/user package was removed.
