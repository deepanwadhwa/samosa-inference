# Trusted per-file remote package acquisition v1

`PackageRemoteCacheSource` is a native `PackageSourceOpen` adapter holding a parsed,
pinned package index, trusted artifact-store policy and an optional array of
`RemoteArtifactSource` entries in that index's order. Array count must equal index
count, bounded by 512. Indexes and public requests do not choose download URLs,
CA paths, credential paths, credentials, redirect policy or store limits.

For each requested file, match canonical name, role, declared bytes and SHA-256
against the pinned index. Refuse an identity mismatch before opening the cache or
network. A verified cache hit returns a held read-only descriptor without needing
any remote source or credentials. A null sources array permits cache-only use.
Only `artifact_absent` may invoke the mapped remote source; corrupt, foreign,
unsafe or contended cache entries remain failures. Missing/partial files retain
the configured explicit resume policy, store identity capacity and byte/disk
ceilings. Caller owns global package/destination reservation and directory state.

Remote acquisition uses the existing `artifact_install_remote` implementation:
HTTPS policy, certificate verification, bounded headers/body, pinned bytes,
redirect restrictions, credentials, durable prefix checkpoints and resumed range
verification remain there. The adapter bridges request control to remote progress
and final hashing alongside store cancellation. Original request-control errors
are preserved, and descriptors are transferred only after controlled cache
reverification. Failure leaves the output descriptor untouched. Filesystem calls
retain platform latency. Cached descriptors are reverified/copied by assembly;
remote acquisition alone never publishes a compatible package or proves readiness.

This is an optional libcurl integration; the core local assembly/cache targets
retain their existing dependencies. It does not define a public catalog service,
credential storage policy, complete-package transaction recovery or a production
catalog entry. No catalog package is qualified from URL mapping tests alone.
