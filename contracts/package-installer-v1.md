# Native catalog package installation v1

`package_install_catalog` connects a trusted selected ProviderEntry, artifact-store
policy, optional remote acquisition policy, per-file sources and private assembly
configuration. Public requests select catalog IDs; they do not supply this native
configuration. Shared payload reservation ceiling/capacity must be configured.

Before acquisition, enforce the declared host platform, runtime policy and minimum
OS. Open a verified cached index by the entry's size/hash; only an absent artifact
may acquire the index through the existing HTTPS downloader. Its URL comes from
the catalog entry, while CA/credential/redirect policy remains trusted configuration.
Unsupported backend recipes refuse before opening or acquiring the index and
before reserving or creating assembly state. Package-mode catalog discovery uses
the same recipe support definition. This is admission capability, not a claim
that a particular catalog package has passed static compatibility or model tests.
Preserve request control and the original store cancellation hook. Corrupt/unsafe/
busy cache state is never converted into a download miss.

Read and bind the complete index to the catalog declarations, check per-file mapping
count, and supply verified remote/cache descriptors to native assembly. Assembly
retains persistent package identity, shared copy reservations, registered copy
recovery, complete-tree verification, concrete backend compatibility and completion
publication/recovery. Return verified tree observations only on success; output
remains untouched on failure. Missing remote sources permit cache-only retries.

Caller owns catalog selection, credentials, store/package/pool directories and their
named-root bindings. Sources and pointer lifetimes must remain valid for the call.
This is native installation coordination, not authenticated public ingress or
loaded readiness. Supported backend recipes, production catalog entries and the
remaining bootstrap/removal/recovery requirements must still qualify before area 2
closes. Existing local staging APIs remain available for already-acquired inputs.

## Catalog identity selection

`package_install_selected` accepts a public catalog ID, a trusted parsed
ProviderCatalog and installer configuration. It resolves host platform through the
existing catalog selector, rejects unknown/incompatible entries before acquisition,
and supplies that selected entry to the complete installer. The template's entry
pointer is ignored, so the caller cannot substitute a different artifact descriptor
for the selected ID. Cache-only retries use the same selected catalog identity.
Catalog parsing retains its original 1024-token bound even in builds that use a
larger JSON token budget for other translation units.

## Private catalog configuration file

`package_install_from_file` loads the host-selected absolute catalog path through
the existing `provider_catalog_load` checks before selection/install. Catalog files
must be owned/private regular single-link files and retain their named identity
and timestamps throughout reading. Symlinks, aliases and public permissions refuse.
The path is host configuration; public requests choose IDs. Request controls apply
before/after catalog reading and throughout the installer. The parsed catalog is
owned only for the duration of the call and freed on success or failure.
### Catalog-selected observation

`package_status_from_file` loads the private host-selected catalog, selects the
public provider ID for the actual host, opens its registered verified cached
index, binds that index to the catalog, and calls `package_assembly_status`.
It does not acquire artifacts, reserve copy capacity, copy files, or recover
interrupted publications. Download credentials and reservation configuration
are unnecessary. A missing index reports `artifact_absent`; other errors leave
the result untouched. `installed_verified` proves observed installation bytes
and static compatibility, without claiming loaded engine readiness.

## Trusted digest endpoint

The installer may configure `digest_source` instead of a per-file source array.
Its host-owned HTTPS URL is an endpoint prefix without a trailing slash, query,
or fragment. Each missing file is acquired from that prefix plus `/` and the
SHA-256 recorded in the verified index. TLS, credentials, redirect policy,
resumable verification and byte limits use the existing downloader. Cache hits
need no endpoint or credentials. Configuring both source forms refuses; index
contents cannot substitute the endpoint, transport policy or expected hash.

## Setup command connection

`samosa-inference-setup --package-directory ABSOLUTE_PRIVATE_DIRECTORY` routes
provider install/resume/status requests through the complete installer. The host
prepares an owner-private directory for the selected pinned package. Install and
resume additionally require `--reservation-directory`, `--copy-ceiling`,
`--reservation-ceiling` and `--reservation-capacity`. `--digest-endpoint` supplies
the optional payload endpoint; `--state` supplies the verified artifact cache.
Existing CA, credential, redirect and artifact quota flags still apply. Status
requires no reservation configuration. Requests remain strict contract-version,
operation and provider-ID objects; paths and URLs are host CLI configuration.
Success reports `installed_verified`, `activation:not_requested` and
`readiness:unqualified`. Without the package-directory option, the existing
single-artifact setup behavior remains available pending integration retirement.
Setup request controls also verify the package and reservation names still refer
to their held private directory descriptors throughout installation. Renamed or
substituted roots fail with `package_tree_changed` before further controlled work.

## Catalog-selected empty release

`package_release_empty_from_file` loads the private catalog and verified cached
index using the same binding checks as status, then releases only the exact
empty-package reservation. Setup exposes `provider_release_empty` with the usual
strict version/op/provider-ID request. Package and reservation directories remain
host configuration; no acquisition or credentials are required. Success reports
`reservation_released` or the idempotent `reservation_absent`; activation and
readiness remain unrequested/unqualified. Any payload, journal, completion,
unknown package name or unsafe identity metadata refuses. No package files are
deleted. This does not implement partial/installed package removal.

## Llama installation recipe

`llama-gguf-package-v1` requires a host-qualified LlamaPackagePolicy before
catalog acquisition or assembly state creation. Missing or malformed policy
refuses. Publication and installed-status verification invoke the complete
llama descriptor/GGUF/native graph verifier with that policy; no manifest may
qualify its own runtime. Staged engine files become private read/execute (`0500`),
while other payloads retain private read-only (`0400`) permissions.

Setup accepts host-only `--llama-runtime-revision`, `--llama-kind`,
`--llama-architecture` and `--llama-engine-sha256`. All four are required together
and validated before request handling. Public request fields remain version/op/ID;
catalog discovery marks this recipe unavailable without policy. Policy flags are
operator configuration for an independently qualified runtime, not declarations
accepted from a downloaded package. Success still reports readiness unqualified.

Normal and ASan/UBSan actual EmbeddingGemma assembly/publication/status gates pass
for index `6e61ef6a8aba534521b6dd3a4bf1f0e0c0df04c785b5666d3b01ac458f875865`,
including missing-policy refusal before state creation and executable engine mode.
Logs: `/private/tmp/samosa-llama-installed-normal.log` and
`/private/tmp/samosa-llama-installed-sanitize.log`. Controlled complete HTTPS and
setup regression gates pass in both builds, including absent/invalid policy
before index acquisition; logs `/private/tmp/samosa-llama-install-native.log`
and `/private/tmp/samosa-llama-setup-final.log`. Actual catalog/HTTPS llama setup
qualification remains the next installation gate; this does not close component 2.
