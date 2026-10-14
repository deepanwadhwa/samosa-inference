# Trusted provider catalog and setup v1

Catalogs are explicit host-provisioned private configuration, never accepted from
public requests or fetched as remote updates. Owner provisioning is the trust
boundary; there is no catalog signing protocol. A loader accepts an absolute
owner-private single-link regular file, up to65536 bytes, and verifies its named
inode and nanosecond before/after metadata. Output remains unchanged on failure.

The exact top-level object has catalog_version=1, revision (1..64 ASCII identifier
characters), and entries (0..16 objects). Each entry has exactly these fields:

- id and version: 1..64 ASCII letters/digits/dot/underscore/hyphen. IDs are unique.
- url: 1..2048 bytes of HTTPS acquisition configuration; no credentials,
  fragments, spaces or control characters. Acquisition independently validates
  URL/TLS and each redirect before changing install state.
- license: 1..2048 UTF-8 bytes of required notice, without control characters.
  A provisioned notice is not independent legal qualification of redistribution.
- platform: an exact host platform ID (currently macos-arm64 or linux-arm64).
- minimum_os: one to three decimal components, each0..65535. macOS means product
  version; Linux means kernel version. Distribution/library closure is separate.
- runtime: an identifier, currently selectable only as system-runtime-v1.
- recipe: 1..128 identifier bytes identifying the trusted host recipe/provenance.
- sha256: exactly64 lowercase hex characters.
- size: exact positive integer<=INT64_MAX.

All strings are bounded, unknown/duplicate fields and incomplete pins refuse.
Empty catalogs are valid: optional provider installation is never a prerequisite
for model-free component operation. No production engine/model entry is inferred
from controlled fixture catalogs.

Optional curl-linked samosa-inference-setup takes host flags --catalog-file,
--state, --byte-ceiling, --disk-ceiling, --artifact-capacity, --ca-file, --bearer-key-file and up to8
--redirect-host values. Singleton flags cannot repeat. Native OS/architecture is
observed, with no public or host flag override. A minimum OS failure precedes
install state creation. All entries share the install root's global lease and
aggregate quota; the root already namespaces by pinned artifact identity.
Capacity defaults to16; an explicitly configured1..1024 bound supports multi-file
acquisition coordination while retaining the original byte reservation checks.
This field is a trusted host argument, never part of an install request. It does
not turn the existing single-file setup adapter into a complete package installer.

One JSON request, maximum4096 bytes, returns one JSON response. Exactly
{contract_version:1,op:"provider_catalog"} lists generic descriptors, supported
booleans and typed unavailable reasons without URLs, credentials or private paths.
Exactly {contract_version:1,op,provider_id} supports provider_status,
provider_install and provider_resume. Extra fields refuse. No public URL, digest,
path, quota, platform or executable selection is possible. Unsupported operations
and contract versions have explicit errors.

Install/resume returns verified bytes, activation=not_requested and
readiness=unqualified. An offline exact verified retry uses no network.
Status uses the existing full digest observation and reports absent, installing,
busy, interrupted or verified. Acquisition TLS/integrity/lease/quota errors stay
typed. SIGINT/SIGTERM request interruption through the owned transfer callback;
retained bytes use the same durable checkpoint/resume protocol. The foreground
caller owns this one-shot process; no background worker or engine is launched.

Listing and status do not load credential files. Install/resume reads a private
single-link bounded bearer-key file and clears its buffer before exit. Credentials
never enter public JSON or argv values. Verified artifacts survive a failed other
catalog entry. Catalog updates affect future requests; already admitted bytes
are a copied bounded configuration snapshot, and each install still verifies its
full size/digest. There is no destructive cleanup, eviction, install rollback or
automatic repair of ambiguous foreign/torn state. Engine activation, executable
mode qualification and actual runtime closure use separate owner operations.
