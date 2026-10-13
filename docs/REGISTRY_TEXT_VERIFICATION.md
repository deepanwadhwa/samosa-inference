# Actual text through the common registry

The optional `SAMOSA_TEST_REGISTRY_BINARY` mode in `tests/managed_native.sh`
retains the existing actual managed-engine lifecycle gate and adds public
registry work against that same authenticated engine. Host configuration owns
the endpoint, credentials, owner and state. The explicit fake-text peer is used
only for combined discovery, never for the actual generation assertions.

The gate checks native rendered token count (24 for the pinned arithmetic
prompt), answer `4`, native quality, completed operation status, actual active
generation observed through authenticated engine slots before registry
cancellation, a surviving fresh arithmetic request, foreign-owner status refusal,
and byte-identical replay after engine shutdown with no credential or usable
endpoint. Existing managed in-stream cancellation, survivor, FIFO EOF shutdown
and standalone offline replay checks still execute.

Final normal and ASan/UBSan client runs both passed. Normal results are retained
in `/private/tmp/samosa-managed-native.tr6rnd` (log
`/private/tmp/samosa-registry-text-normal-final.log`); sanitized results are in
`/private/tmp/samosa-managed-native.fL9Zua` (log
`/private/tmp/samosa-registry-text-sanitize-final.log`). The result files include
native usage, completed status, cancelled work, fresh survivor output,
foreign-owner refusal and exact offline replay. No credential files remain.

The test retains a private credential derived from its existing private QA
authorization header only while the engine is live. Cleanup removes both private
credential files, including on failure. No production credential policy changes.
Registry readiness is still external-endpoint metadata, not owned readiness;
starting the engine with the managed CLI does not grant that ownership to a
separate registry process.

Pinned local macOS ARM64 assets:

- Engine `/private/tmp/samosa-embedding-runtime/build-samosa/bin/llama-server`,
  15950064 bytes, SHA-256
  `958601dbd22ab2f5857e09860247f30c74153cb0f3914b8d62f2bacbba9aab97`.
- Model `~/.samosa/models/ornith-9b/Ornith-1.0-9B-Q4_K_M.gguf`, 5629108704 bytes,
  SHA-256 `5720d1f671b4996481274fffe01868c3c36e87c135cc8538471cc7bd6087b106`.

The historical static text engine build directory no longer contains its
executable. This gate reuses the currently available verified native engine
already used by EmbeddingGemma; it does not assert identity with the old binary.
The first exploratory launch supplied an incorrect engine byte count and
correctly refused `artifact_size_mismatch`. An initial registry harness assertion
expected `succeeded`; the actual operation contract says `completed`. Both test
configuration errors were corrected before recording a passing full gate.

Command: set `SAMOSA_TEST_REGISTRY_BINARY` to the absolute providers CLI, then run
`sh tests/managed_native.sh <absolute managed CLI> <engine> <engine SHA> <engine
size> <model> <model SHA> <model size>`. Sanitizer mode selects the `-sanitize`
providers/managed executables and sets `SAMOSA_TEST_OFFLINE_BINARY` to the absolute
`build/samosa-inference-llama-sanitize` executable. Model engines are not
sanitizer-built. These bounded arithmetic and cancellation checks do not prove
general model accuracy, KV reuse, incremental token streaming, actual-model
Linux support or complete seven-area acceptance.
