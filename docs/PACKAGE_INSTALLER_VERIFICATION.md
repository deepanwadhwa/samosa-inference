# Complete catalog installation evidence

The production HTTPS fixture now serves the pinned package index as well as all
11 payload files. Native `package_install_catalog` takes trusted ProviderEntry
index declarations and acquisition policy, independently fetches/verifies the index,
binds its platform/minimum-OS/recipe, and completes a fresh package through verified
cache descriptors and concrete compatibility/publication. The call has a mandatory
shared copy reservation pool. Removing all remote source configuration permits an
exact cache-only retry. Receipt pin and file count match the original index.

Negative cases reject missing reservation configuration and a foreign platform
before acquisition, with unchanged output. The server verifies every response hash
and sees exactly one GET per unique payload/index identity; the retry adds none.
Existing untrusted-TLS, local/cache, interrupted metadata, offline cold destination
and quota checks remain in the same harness. Normal and ASan/UBSan HTTPS gates
passed on macOS arm64. Log: `/private/tmp/samosa-package-installer-final.log`.
Clang analysis of the new coordinator emitted no diagnostics.

This is controlled native-format fixture evidence, not a published production model
entry or loaded model readiness. Public installer/configuration integration,
supported backend package recipes, actual production package qualification and the
remaining bootstrap/removal recovery requirements remain in area 2. No downstream
lifecycle/scheduling implementation is opened by this installation call.

## Parsed catalog selection

The production HTTPS fixture now builds and parses a trusted native ProviderCatalog
containing the pinned package-index entry, then installs by `speech-fixture` ID
through `package_install_selected`. Unknown ID refusal leaves the output unchanged.
Removing remote sources and clearing the template entry pointer still permits a
retry through catalog selection; no new GET is observed. Normal and ASan/UBSan
complete package gates passed. The original catalog parser suite also passed with
951 checks in each mode. Log:
`/private/tmp/samosa-selected-package-installation.log`.

This connects existing catalog parsing/selection to the complete native installer;
it does not publish the fixture as a production model entry or close the remaining
area-2 package compatibility, production qualification and recovery requirements.

## Private-file catalog integration

The HTTPS fixture now writes the trusted parsed catalog to an owner-private file
and calls production `package_install_from_file` for initial installation and
cache-only retry. Group-readable permissions, a hardlink alias and a symlink path
all refuse with `invalid_catalog_file` before installation. The permissions case
also checks unchanged output; restored owner-private/single-link configuration
installs successfully. Normal and ASan/UBSan complete HTTPS gates passed. Log:
`/private/tmp/samosa-private-catalog-installer-final.log`.

The host file loader and its identity checks are reused rather than duplicated.
This connects trusted catalog configuration to installation, while production
package qualification, remaining compatibility/recovery and public entry-point
requirements remain within area 2.
