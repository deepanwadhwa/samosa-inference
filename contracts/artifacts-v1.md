# Inference artifact acquisition v1

`samosa-inference-artifacts` consumes one JSON request on stdin (maximum4096
bytes) and returns one result. Host arguments pin private absolute state/source
paths, expected lowercase SHA256, exact positive size, per-artifact byte ceiling
and aggregate disk ceiling. Requests contain only contract_version=1 and op.
Supported ops: artifact_status, artifact_install, artifact_resume. No public
request selects a path, URL, executable, checksum or byte limit.

artifact_make_executable verifies the already published/registered inode and
supported native ARM64 executable header before setting only that owned inode to
0500 and syncing it. macOS accepts ARM64 Mach-O executable; Linux accepts ARM64
64-bit little-endian ELF executable/PIE with a nonzero entry point. Text, scripts,
libraries and other architecture declarations refuse. Source file permissions
are never changed. Status and offline exact install retry remain readable afterward.
Executable publication qualifies entry bytes, not dependent library closure or
provider readiness.

Model-free builds use libc only for this manager, with no curl or model weights.
Local regular-file import is implemented. The optional curl-linked downloader
and catalog setup adapter provide HTTPS acquisition; see remote-artifacts-v1.md
and provider-catalog-v1.md. The default artifact manager remains libc-only.
Content artifact_id is SHA256 with an exact configured size. At most16 identities
are retained in one private state root with one nonblocking install lease.
Conservative disk admission reserves twice declared size for each retained entry
(stage/publication can share an inode). No eviction or deletion/replacement of
unknown state. Quota/corruption ambiguity refuses without destructive recovery.

Successful install/resume returns status=verified, activation=not_requested and
readiness=unqualified. Verified bytes do not establish a relationship with an
external engine or make it ready. Activation/readiness remain separate work.
Status distinguishes absent, installing (matching active target), busy (another
target/unknown lease holder), interrupted and verified. Invalid observations
include an error and status=invalid. Artifact status performs a full streamed
checksum read for published content; metadata timestamps alone are insufficient.

Installation copies in fixed buffers, verifies exact full bytes, fsyncs and links
without overwriting an existing path. Identity records bind the stage device and
inode. A completed retry succeeds even with its acquisition source absent.
Interrupted partial work requires artifact_resume; saved prefixes must exactly
match the newly opened configured source. Released install flock proves no live
installer owns staging before resume. No worker request is automatically replayed.
Incomplete identity creation remains quarantined by refusal, never guessed.
Publication interrupted after link can be recovered through an explicit request
when the public path is the recorded stage inode and its complete checksum passes.
Each copy or full-checksum scan has a600-second monotonic deadline; filesystem
syscalls themselves depend on host storage responsiveness. Copy uses two32KiB
buffers; verification uses one64KiB buffer. No artifact-length allocation occurs.

Configured paths must be regular/no final symlink and private roots/files must
belong to the effective host owner. Root/entry/stage/source scopes are rechecked
before publication. This is checkpoint observation, not an atomic filesystem
snapshot or continuous watcher. Mutable bytes can be changed after observation;
engine activation must independently pin/verify its input relationship.
