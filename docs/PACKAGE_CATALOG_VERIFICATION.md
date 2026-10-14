# Catalog/index binding evidence

The native local/cache and complete HTTPS fixture harness now constructs the
existing ProviderEntry artifact declarations for its pinned complete package
index and uses production `package_catalog_read_index`. Checks verify the exact
pin/file count, unchanged input seek position, recipe-mismatch refusal without
replacing the previous parsed result, and request cancellation. The native
assembly/compatibility gates continue afterward; no catalog declaration replaces
payload verification or loader checks.

Normal and ASan/UBSan local/cache and complete HTTPS gates passed on macOS arm64.
Log: `/private/tmp/samosa-package-catalog-binding-final.log`. Static analysis of
the new reader emitted no diagnostics. The bounded catalog reader reuses the
existing ProviderEntry/artifact contracts rather than introducing a separate
runtime or package-manifest URL language. The harness entry is controlled fixture
configuration; it is not a published production model catalog entry.

Full catalog selection/acquisition/coordinator integration, supported package
recipes and qualified production entries remain area-2 work. Actual model
availability/readiness is not inferred from this metadata gate.
