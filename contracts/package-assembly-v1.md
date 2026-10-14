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
