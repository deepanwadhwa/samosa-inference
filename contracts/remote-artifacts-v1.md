# Remote artifact acquisition v1

Optional `make native` builds samosa-inference-download with libcurl. It accepts
the same strict public artifact_install/artifact_resume/artifact_status/
artifact_make_executable request as artifacts-v1. Trusted host flags supply
--state, --url, --sha256, --size, --byte-ceiling and --disk-ceiling; optional
--ca-file, --bearer-key-file and up to eight --redirect-host entries configure
trusted acquisition. Credential files must be absolute, private regular files,
owned by the current user, single-link and unchanged across bounded reads;
1..128 identifier bytes with at most one trailing LF. Secrets never enter argv,
public responses or source/checkpoint records, and the CLI clears its key buffer.
--source and --url together refuse. Default artifact CLI remains curl-free.
URLs and credentials are never accepted in public request bodies.

Remote acquisition runs inside the existing install lease and registered staged
inode. It creates no second artifact copy; existing sixteen-entry/2x payload
reservations and no-clobber publication apply. Full size/SHA256, fsync and scope/
inode checks remain mandatory before publication. Existing verified artifacts
retry offline without contacting the URL. No acquisition activates a provider;
successful descriptors remain readiness=unqualified/activation=not_requested.

Only HTTPS is accepted by production builds, with certificate/hostname checks
enabled and system trust or the configured CA file. Redirects<=five, explicitly
validated before another request, remain HTTPS and on source/allowlisted hosts.
Optional trusted C adapter bearer credentials never cross the original authority.
URL userinfo/fragments, foreign redirect hosts, excessive headers/body/length,
unsupported encoding, interrupted/corrupt responses and wrong digest refuse.
Connect<=2s, aggregate network transfer<=600s; host syscall responsiveness remains
a limitation. Callback storage is fixed; model length never determines a memory
allocation. Root/record files remain owner private. Source metadata contains only
the64-byte URL digest and must be regular/single-link; foreign records refuse.

Install on a nonempty unverified stage requires explicit resume. A private288-byte
checkpoint file has two alternating checksummed records: prefix length and SHA256.
Each >=1MiB of progress, and after an interrupted transfer, stage fsync precedes
record fsync. Recovery selects the newest valid record and hashes its prefix
locally (fixed buffer, <=600s); modified prefix/foreign metadata refuses. A torn
new record falls back to the preceding valid record. Uncheckpointed retained tail
is compared with the server before append. No valid record refuses; an old stage
with no checkpoint uses a full replay once. Fully retained stages go straight to
the complete pinned digest without networking.

Suffix requests use Range at the verified checkpoint.206 requires an exact
Content-Range start/end/total and, when supplied, exact suffix Content-Length;
missing/duplicate/overflowed/inconsistent ranges refuse before payload writes.
Servers returning200 fall back to bounded replay and prefix comparison. URL change
refuses. Unsolicited206 refuses. Identity is the host-pinned size/SHA256 plus URL;
ETags are not trusted or used as identity. Full final digest remains mandatory,
including for a changed remote object. Interrupted or killed workers never report
verified/ready. Actual SIGKILL recovery covers both tiny QA checkpoints and the
production1MiB interval with a retained uncheckpointed32KiB tail.

Native controlled HTTP is enabled only by a test compilation and requires numeric
127.0.0.1, never a production/public option. Production HTTPS is separately tested
through the public CLI with a local TLS fixture, including untrusted-certificate
refusal, trusted success and offline retry. TLS fixtures use OpenSSL>=1.1.1 CLI;
runtime acquisition needs libcurl only. Broader failure/resource fixtures, real
remote provider/versioned catalog distribution and product setup/recovery remain
unfinished gates. Checkpoint/syscall power-loss durability beyond POSIX fsync and
full physical filesystem-overhead quotas are not independently qualified.
