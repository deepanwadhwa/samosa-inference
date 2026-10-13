# S13a remote acquisition continuation

Next implementation after qualified concurrent managed ingress. Remote acquisition
belongs to Inference, remains optional/libcurl based, and never makes Sources,
Agent or App require weights. Public callers choose configured artifact IDs and
install/resume/status operations; URLs, expected digests, lengths, quotas, host
paths and credentials come only from trusted owner configuration/catalogs.

Reuse the existing artifact publication/integrity/executable policy instead of
creating a second ready/install state machine. Verified acquisition is separate
from managed activation; a network success never implies provider readiness.
Maximum16 artifacts and one acquisition/import under the owner installation
lease. Aggregate reservations must account for all retained download, import and
published bytes; no quota eviction or deletion of foreign files.

Production remote sources are HTTPS with certificate/hostname verification,
bounded connect/transfer/redirect counts, explicit HTTPS-only redirects, exact
expected length, bounded headers and a fixed streaming buffer. No file/gopher/
FTP URLs or redirect outside an explicit trusted HTTPS authority policy. A
configured private HTTPS registry is supported. Controlled HTTP
loopback is confined to an explicit native QA configuration, never a public
request or silent production downgrade.

Retained bytes and metadata are private anchored owner files. Cancellation or
worker death leaves an interrupted acquisition; only explicit resume may reuse
it. Resume must bind the configured URL/object identity and expected SHA/length,
validate the retained prefix and response range/total before append, and refuse
an inconsistent server/object. Full streamed checksum is mandatory before
publication even when a server supplies matching ETag/Content-Length. Server
range refusal may trigger an explicit bounded full retry; never splice an
unqualified suffix into retained bytes. Interrupted publication follows the
existing exact inode/no-clobber protocol. Failed new acquisition preserves any
previous verified artifact and committed last-good provider configuration.

Declare before implementation the precise disk reservation and staged-byte
relationship so temporary downloads cannot bypass the artifact-store quota.
Network credential metadata stays private; response bodies/headers cannot leak
credentials into public descriptors or logs. No automatic activation/replay.

Required native fixtures: HTTPS certificate refusal and success, streamed length/
checksum success, truncated/oversized/corrupt responses, wrong range/total/ETag,
redirect protocol/domain policy, actual cancellation and worker death, explicit
resume/exact offline retry, concurrent lease/capacity/disk refusal, source/object
replacement, private root/symlink/foreign-file refusal and last-good preservation.
Independently build/test/sanitize/package on macOS ARM64 and native Debian12
ARM64, using self-contained local fixtures without Python or downloaded models.
Distribution manifests/license/platform pins and real remote-provider installer
qualification remain separate evidence from these controlled fixtures.

Current implementation uses direct leased stage acquisition, not a second full
download copy. HTTPS certificate/hostname verification is mandatory. Up to five
redirects must remain HTTPS and match the source hostname or an explicit host
allowlist; bearer credentials are sent only to the original scheme/host/port.
Header tables<=16KiB, individual lines<=4KiB and discarded redirect bodies<=32KiB.
Payload writes never exceed pinned length. Full checksum/publication remains in
the existing installer; the2x payload reservation already exceeds the one-inode
remote staging peak. Additional source identity metadata is bounded64 bytes per
configured artifact, private/single-link and retained without URL/credential text.

Explicit resume now validates a durable local prefix checkpoint before requesting
the suffix. The fixed288-byte private checkpoint alternates two checksummed
length/SHA records; >=1MiB progress fsyncs stage before metadata. Interrupted
transfers also checkpoint received progress. Torn newest records fall back to the
prior valid record; corrupt retained prefixes refuse. Uncheckpointed tails compare
against fetched bytes before append.206 start/end/total and optional length must
match exactly.200 falls back to prefix-compared replay; old stages without a
checkpoint replay once. Fully retained stages use the final pinned digest offline.
Source URL hashes bind retries. ETags are not identity: pinned SHA/size remain
mandatory and changed objects cannot publish mismatched bytes. No valid checkpoint
record refuses, including a death during initial metadata creation; automatic
repair of that narrow incomplete state remains a setup/recovery gate.

CLI --bearer-key-file now accepts only bounded absolute private owned single-link
regular files, with anchored before/after reads and strict identifier bytes. Key
buffers clear after use and secrets are absent from public responses/metadata.
Native fixtures verify actual Authorization use and refusal to forward credentials
across a same-host/different-port redirect. Remaining foreign/quota/resource
fixtures and versioned distribution/setup recovery gates remain open. This is
implemented progress, not complete S13a qualification.
