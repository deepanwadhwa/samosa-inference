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
