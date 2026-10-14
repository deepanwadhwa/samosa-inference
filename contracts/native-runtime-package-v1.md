# Pinned native runtime package verification

`package_native_runtime_verify` checks a held private package root against a
trusted parsed pinned PackageIndex. It verifies the complete closed tree, checks
declared host platform/minimum OS, obtains actual host loader policy, inspects
every engine/library, resolves all dependencies against the pinned file list,
then verifies the complete tree again. Native files are rehashed around image
inspection. Exactly one engine is required, and unreferenced libraries are also
inspected. Unsafe loader environments refuse. Request controls apply throughout.
The caller retains its package lease and named-root bindings.

This gate reuses the existing bounded tree, artifact hashing, native executable/
library inspection and shared dependency resolver. It neither executes nor loads
code, publishes installation state, nor acquires artifacts. Model/data bytes are
opaque pinned contents. Backend/model format, runtime revision/ABI declarations,
supported model architecture and loaded readiness require their respective gates.
No new installation recipe is admitted by this helper alone.

Normal and ASan/UBSan controlled runtime-package tests pass 41 checks each,
including cancellation, missing engine, incompatible pinned unreferenced library,
corrupt model and foreign namespace refusal. Existing runtime parser and complete
HTTPS package regression gates pass in both builds. Logs:
`/private/tmp/samosa-native-runtime-package.log` and
`/private/tmp/samosa-native-runtime-package-integration.log`.

## Actual llama/EmbeddingGemma runtime package

Normal and ASan/UBSan verification also pass against independent private copies
of the actual llama server and EmbeddingGemma GGUF on macOS arm64:

- Complete index: `4ca4c1bfb63fb53475120ca4f7edb7525c49542d32c10eceea85a10ac7889c89`
- Engine: 15,950,064 bytes, `958601dbd22ab2f5857e09860247f30c74153cb0f3914b8d62f2bacbba9aab97`
- Model: 309,855,456 bytes, `2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135`
- Total: 325,805,520 pinned bytes; declared minimum OS `14.0`.
- Logs: `/private/tmp/samosa-native-runtime-embedding.log` and
  `/private/tmp/samosa-native-runtime-embedding-sanitize.log`.

The test index uses `native-runtime-test`, an unadmitted QA recipe. These results
qualify static runtime packaging only, not backend/model compatibility, public
installation, a production catalog or another platform. Original assets were
unchanged. The retained disposable root is
`/private/tmp/samosa-native-embedding-n8wvirgd/payload` with its private sibling
`index.json`; the native test harness accepts root/index/pin arguments to repeat
this verification.
