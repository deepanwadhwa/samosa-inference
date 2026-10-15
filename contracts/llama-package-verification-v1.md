# Complete static llama package verification

`llama_package_verify` combines pinned descriptor/model inspection with complete
native runtime verification. The caller supplies a host-owned LlamaPackagePolicy:
qualified runtime revision, modality, model architecture and engine SHA-256.
These policy values never come from the package manifest or public request.
The caller retains the package lease and named-root binding.

Policy kinds are text generation, embeddings and native T5 summaries (`summary`
with architecture `t5`). Each kind requires its separately qualified engine pin;
sharing the GGUF inspector does not make these engine protocols interchangeable.
The CPU T5 package's standalone build, cold installation and actual-model gates
are recorded in [T5 installation evidence](../docs/T5_PACKAGE_INSTALLATION_VERIFICATION.md).

The index must contain `llama-package.json` as pinned metadata of at most 4KiB.
Its hash is checked around parsing. Engine/model names and roles must resolve
exactly through the index. The declared revision, kind and architecture must match
the host policy, and the indexed engine pin must match its qualified engine pin.
The model is hashed around bounded GGUF metadata inspection and its architecture
is compared with the descriptor. Complete native runtime verification then checks
all package hashes/namespace, actual host platform/OS, every engine/library and
the transitive loader graph. Controls apply throughout; output remains untouched
on failure. No package contents are modified.

This gate proves static package/runtime/model-metadata compatibility. Tensor
layout, quantization support and actual engine/model loading remain their own
qualification/readiness gates. This verifier does not publish installation state
or admit a public recipe without a trusted runtime policy selection.

Normal and ASan/UBSan native-package gates pass 66 checks each. Added cases include
cancelled verification, host-policy modality/revision/engine-pin mismatch and
observed GGUF architecture mismatch while preserving the output. Synthetic model
metadata tests the static boundary only. Log:
`/private/tmp/samosa-complete-llama-verifier.log`.

## Actual embedding package

Normal and ASan/UBSan complete verification pass against independent private copies
of the actual llama server, EmbeddingGemma model and pinned descriptor:

- Package index: `6e61ef6a8aba534521b6dd3a4bf1f0e0c0df04c785b5666d3b01ac458f875865`
- Contents: three files, 325,805,737 bytes.
- Qualified engine: `958601dbd22ab2f5857e09860247f30c74153cb0f3914b8d62f2bacbba9aab97`
- Runtime revision: `4fbc76dec51d0add466f0210855c0596589b60d4`
- Model: `2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135`
- Kind/architecture: `embedding` / `gemma-embedding2`.
- Host/minimum OS declaration: `macos-arm64` / `14.0`.
- Logs: `/private/tmp/samosa-llama-complete-actual.log` and
  `/private/tmp/samosa-llama-complete-actual-sanitize.log`.

The retained disposable root is `/private/tmp/samosa-llama-complete-17odkd6z/payload`
with its sibling `index.json`. The native test harness accepts root/index/pin
arguments followed by `llama` to use the fixed qualified test policy. Original
assets remain unchanged. Public installer recipe selection/wiring is the next
component-2 dependency; this result does not close the component.
