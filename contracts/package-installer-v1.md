# Native catalog package installation v1

`package_install_catalog` connects a trusted selected ProviderEntry, artifact-store
policy, optional remote acquisition policy, per-file sources and private assembly
configuration. Public requests select catalog IDs; they do not supply this native
configuration. Shared payload reservation ceiling/capacity must be configured.

Before acquisition, enforce the declared host platform, runtime policy and minimum
OS. Open a verified cached index by the entry's size/hash; only an absent artifact
may acquire the index through the existing HTTPS downloader. Its URL comes from
the catalog entry, while CA/credential/redirect policy remains trusted configuration.
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
