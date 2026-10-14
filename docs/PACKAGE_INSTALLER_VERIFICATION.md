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
