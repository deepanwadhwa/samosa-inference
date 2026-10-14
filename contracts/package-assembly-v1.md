# Private native package assembly v1

`package_assemble` takes a parsed, pinned `PackageIndex`, held read-only private
source and package directory descriptors, the absolute destination `payload/`
path, a nonzero logical payload-copy ceiling and optional trusted request control.
Only `sherpa-speech-package-v1` currently has concrete backend compatibility.
Unsupported recipes and platform/minimum-OS mismatches refuse before copying.

The installer creates/holds `assemble.lock` with an exclusive nonblocking lease
for the entire observed source verification, resumable per-file copying and
completion publication. Another cooperating assembly returns
`package_assembly_busy`. Lock files must be private regular single-link files;
held locks and payloads must retain their named bindings. Payload path identity
must agree with the directory descriptor before and after publication.

A missing completion record permits staging: verify the entire closed source,
create a private `payload/`, resume only exact verified destination files, and
reverify the source. Then use `package_publish` with the concrete backend gate;
callers cannot substitute a permissive compatibility callback. An existing
completion record takes the verification path without copying or repairing
payload bytes. Corrupt records, symlinks, foreign files and corrupted completed
payloads refuse and remain untouched. Success returns verified tree observations;
output remains untouched on failure. Request controls apply to bounded reads,
copies and backend inspection. Successful assembly does not load a model or prove
engine readiness.

The copy ceiling bounds declared copied payload bytes. It is not a shared disk
reservation and excludes source/cache bytes, metadata and filesystem allocation.
The caller retains package-directory ownership, global acquisition/store quota,
source immutability and named-source binding, and durable transaction recovery.
Crash-retained temporary names and two-link publication intervals currently
refuse; assembly never guesses ownership or deletes such files. The lease
coordinates cooperating API callers, not arbitrary same-user filesystem writers.
This API assembles an acquired tree; it is not yet the public catalog/download
installation service.


## Acquired artifact descriptor adapter

A trusted native `source_open` adapter may supply one owned read-only file
descriptor for each indexed file instead of a closed source directory. The
assembler checks request control before the adapter call, passes the same control
to it, stages and verifies every returned file and closes returned descriptors
on success and failure. The adapter must honor controls during acquisition or
verification. Destination whole-tree and concrete compatibility gates still run;
no caller-supplied adapter can waive those gates. The adapter is not a public
request callback. Completed package verification never invokes the adapter.

`artifact_open_verified` provides the artifact-store bridge: it obtains the
existing install lease, opens the registered identity and published artifact,
checks the registered two-link inode relationship, verifies declared bytes and
SHA-256 with bounded controlled reads, and rechecks named bindings/private state.
Only success transfers a held read-only descriptor to the caller; failure leaves
output untouched. Absent, partial, busy, foreign and corrupt entries refuse. The
store lease is released before returning; the held descriptor is reverified by
per-file staging and does not establish future immutability. This lets cache
stage/artifact aliases remain two-link files while installed payload files are
separate single-link copies. It does not reserve destination disk capacity.


## Production local acquisition adapter

`PackageCacheSource` combines the trusted artifact-store policy with an optional
absolute local source directory. `package_cache_open` verifies the registered
cache entry first. With no source directory it operates only on the cache. With
a local source it acquires only `artifact_absent` entries through
`artifact_install`, then reopens and verifies the published descriptor. Partial
entries follow the existing explicit `resume` policy; corrupt, foreign, busy or
unsafe cache entries refuse instead of falling back to acquisition. Public package
indexes never select source directories, credentials, store quotas or callbacks.

Indexed file names determine paths beneath the trusted source root. Each file's
size and SHA-256 override the template identity; byte/disk ceilings, identity
capacity and resume policy remain trusted store configuration. Request controls
are passed to verified cache reads and bridged to local copying alongside the
store cancellation hook; the original request error is retained. The local installer's final staged hash and verified installation retries poll
this cancellation bridge between bounded reads and before hash completion.
Package staging, cache descriptor re-opening and publication retain their
controlled verification. Filesystem syscalls and fsync retain platform latency;
this is cooperative request control, not a hard wall-clock syscall guarantee.

The adapter supports local acquired files and offline cache hits. It currently
rejects a template remote `acquire` hook on a cache miss: one fixed hook/URL cannot
safely stand in for per-file trusted package download mapping. Remote acquisition,
global destination reservations and registered crash recovery remain open.
