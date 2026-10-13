# Inference activation controller v1

Native owner library, not yet a public production activation CLI. Host config
pins private absolute state/engine/model/self paths, exact artifact sizes/SHA256,
byte ceiling, bounded lifecycle limits, expected last-good revision and trusted
adapter qualification. No public request chooses a readiness callback or commands.
The argv template has exactly one @ENGINE@ in position0 and one @MODEL@.
Arguments obey engine lifecycle bounds. Model is passed as an inherited read-only
fd5 and substituted as /dev/fd/5; persisted configuration also retains its original
logical path for explicit future verification/recovery. Model pathname replacement
does not change the inherited model descriptor. Input scope/content observation
checks before/after qualification additionally refuse observed mutation.

One shared private state root has one nonblocking residency flock. Guardian
retains its duplicate until owned cleanup, including parent death. All callers
on a host must configure the same residency root. A pending shutdown retains
ownership handles instead of starting a replacement engine. No overlapping switch
is implemented: explicit stop precedes a new activation. A failed candidate stops
only its own process and preserves the previous committed configuration.

Last-good config is a single SQLite row, revision checked, content-digested,
maximum16KiB, database<=1MiB with4096-byte pages, zero database busy wait and FULL
sync transactions. Root/database/lease paths are revalidated before commit; SQLite
HAS_MOVED participates in database anchor validation. Symlinks, foreign permissions,
multiple database links and oversized/unsafe journals refuse. Successful activation
commits a new revision only after adapter qualification and input revalidation.
Get validates the digest and returns live_readiness=unobserved; persistent config
never proves a running engine. Pre-commit qualification/exec failures preserve
the previous last-good row. A commit/fsync error reports activation_commit_uncertain
and retains the live ownership handle; callers must inspect durable state and
explicitly stop/reconcile rather than assuming rollback or unchanged configuration.
Failed candidate/restart never automatically launches
the saved configuration. Fresh-process reads work without engine/model work.

Executable is verified and held with before/after pathname and mutation checks;
macOS cannot be assumed to execute /dev/fd. These are launch checkpoint observations,
not an atomic exec-from-fd guarantee or protection against a hostile local OS owner.
Read-only descriptor inheritance protects model inode binding; mutable inode bytes
still require the owner's artifact/readiness policy. The managed text adapter now
uses a fixed argv template, authenticated bounded readiness and system-runtime-v1
loader policy; its provider identity binds engine/model sizes and digests, alias,
context, tokenizer policy and platform. See managed-v1.md. External provider_status
continues to report ready=false. Managed packaging/acquisition and concurrent
ingress/cancellation remain open requirements.
