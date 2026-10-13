# Actual embedding and temporal registry gates

Local macOS ARM64 qualification, 2026-10-13. Both gates reuse the existing
native engines and actual-model assertions. They add combined-provider routing;
the accompanying explicit fake-text provider supplies discovery evidence only.

## Embedding

The registry was forwarding `provider_id` into the embedding adapter's strict
legacy schema, which refused otherwise valid work. Dispatch now removes that
routing field after selecting the provider. It preserves all other fields and
the adapter's normalized operation identity. A model-free regression proves
valid work reaches an expired trusted deadline, an injected endpoint refuses,
and the failed operation remains queryable (219 combined-registry checks in
normal and sanitizer builds).

`tests/embedding_real.py --registry-binary` launches the existing managed engine
and sends embedding work through the common registry. A private test credential
is retained during startup because the supervisor deletes its temporary key
before reporting readiness; the harness removes its copy on success or failure.
No production credential lifetime or public request schema changes.

Normal and ASan/UBSan runs both passed: finite 768-dimensional L2-normalized
vectors, four out of four paraphrase rankings, pinned representation/engine/model
provenance, context-limit refusal, identical retained replay after engine shutdown
without credentials, single/managed-stream vector parity and clean shutdown.
Results and full vectors are in private caller-owned directories:

- `/private/tmp/samosa-registry-embedding-normal-r3-20261013`
- `/private/tmp/samosa-registry-embedding-sanitize-20261013`

Command: `python3 tests/embedding_real.py --registry-binary <providers CLI>
--binary <managed CLI> --offline-binary <llama CLI> --engine
/private/tmp/samosa-embedding-runtime/build-samosa/bin/llama-server --model
/private/tmp/samosa-embedding-real/model.gguf --model-sha256
2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135
--runtime-revision 4fbc76dec51d0add466f0210855c0596589b60d4 --output <new private
directory>`. Sanitizer mode uses each CLI's `-sanitize` build. Engine SHA-256 is
`958601dbd22ab2f5857e09860247f30c74153cb0f3914b8d62f2bacbba9aab97`.

## Temporal

`SAMOSA_TEST_USE_REGISTRY=1 build/test-temporal-public-real <providers CLI>
<existing native Molmo2 engine> <model package> <new private directory>` retains
the original actual-model tests while using trusted registry configuration for
owner selection and offline paths. Both normal and sanitizer runs passed 524778
checks each: two 512px frames in a request exceeding 2MiB, red-before-blue native
observation, per-frame pixel/PPM/time binding, pinned provenance, typed
uncertainty, foreign-owner refusal and exact model/runtime-free replay.
The large count includes individual fixture pixel construction checks.

Engine: Chat's existing `build/samosa-molmo2`, SHA-256
`c8aa53e06d30305f3077e260f5fbda258cd911941b109dfe18d286fc96d08a05`.
Package: `~/.samosa/models/molmo2-4b-mlx-q4-v1`, manifest SHA-256
`a07230dd952e97969921223328e7812020bb58711728450cdfb2205e3a15b770`.
Private result directories are `/private/tmp/samosa-registry-molmo-normal-20261013`
and `/private/tmp/samosa-registry-molmo-sanitize-20261013`.

## Limits

Sanitizers cover the Inference clients and native temporal harness, not the
preexisting model engines. These are bounded local quality checks, not general
model accuracy guarantees. Actual-model Linux coverage remains open; Molmo2's
existing MLX engine is platform-specific. CI independently compiles the optional
temporal actual-model harness without claiming actual-model execution there.
Provider integration remains active; streaming speech, actual text through the
combined interface and the remaining seven-area acceptance requirements are
still open. Replaced Chat paths have not been retired.
