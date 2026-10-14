# Trusted catalog to package-index binding v1

`package_catalog_read_index` reuses an existing trusted `ProviderEntry` selected
from the native provider catalog. For a package-index entry, its artifact size and
SHA-256 identify the complete index bytes. The caller supplies a held private
read-only artifact descriptor, optional trusted request controls and output index.
The entry remains the source of catalog ID/version/license and acquisition URL;
the index does not introduce URLs, credentials or installation policy.

Verify bounded index bytes (at most 1 MiB) against the catalog pin, read with fixed
64 KiB controlled operations without changing the descriptor seek position, then
parse the complete pinned index. Index platform, minimum OS and backend recipe
must exactly match the catalog declarations. Runtime policy must be the existing
`system-runtime-v1`. Output changes only after parsing, declaration agreement and
final request control succeed. A cache artifact's registered two-link descriptor
is permitted; no descriptor ownership is transferred by this reader.

Catalog/host selection remains with the existing catalog primitives; whole-payload
integrity, loader/backend compatibility, installation and actual-model qualification
remain separate required gates. Reading a consistent index alone never qualifies
a production entry or reports loaded readiness. This API is the binding dependency
for the complete package installer/catalog coordinator.
