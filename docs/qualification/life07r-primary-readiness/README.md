# Primary readiness on the native Mac

LIFE-07r, 2026-11-22. Chat baseline `1351cb1`, Inference baseline `b3cb760`.

The public C adapter in `primary_host.h` connects the existing backend process,
JSON transport, artifact verifier, native loader inspector and Activation
transaction. Build it with `make primary-host` and link
`build/libsamosa-inference-primary.a` with pthread and SQLite. A host calls the
existing backend dispatch before normal startup. Start, status, stop and
last-good share the same guarded process and physical cleanup result.

The caller supplies trusted pins for the original engine, model anchor, model
inputs and native dependencies. Qwen checks every safetensor shard the original
loader will open and checks the tokenizer and generation configuration. Maple's
adapter uses the original SSD streaming launch and completion response schema.
The file-model profile preserves the existing llama launch and selected alias.
The loader inspector resolves declared absolute Mach-O dependencies alongside
its existing sibling and system profiles. Qwen retains the installed OpenMP
dependency at its current absolute install name. The host rechecks pinned files,
the state lease and the guarded process. macOS process inspection checks that
the loopback listener is owned by the observed worker.

The readiness probe sends the original short Qwen request through the extracted
chat transport. The opt-in response check requires HTTP 200 and a complete body.
Qwen and llama require nonempty content and positive completion usage within the
four-token request. Maple exposes a finish reason and content in its original
non-streaming schema. Activation commits the launch settings and input pins
after generation and the post-request identity checks. A failed qualification
preserves the previous snapshot.

| Gate | Result |
| --- | --- |
| Native C public API | 147 assertions each, normal and ASan/UBSan. Covers Qwen, Maple and llama fixture routes, incomplete replies, failed qualification, unchanged last-good, mutation refusal and restart. |
| Source-only independent build | 41 source and build files, 147 assertions each, normal and ASan/UBSan. Source hashes and the local build directory are recorded here. |
| Original module regressions | JSON transport 195 checks each. Activation 158 normal and 168 ASan/UBSan. Library inspector 4435 each. |
| Retained Qwen engine and weights | 53 assertions in each guarded run. Uses the original Qwen candidate, model pins and installed OpenMP. Missing OpenMP declaration is refused before launch. Pinned launch completes a real generation, publishes readiness and releases its process and marker. See the final actual logs for memory samples. |

The existing Gateway build, all 15 prompt-budget scenarios, model-selection
suite, and package-byte/hash test pass. The working Gateway still consumes its
existing specialist archive. The new primary archive qualified separately.

Maple and llama were exercised with native protocol fixtures. Actual model runs
and production inputs for those paths remain open. This C API is ready for
trusted callers on this Mac. Gateway still needs to use it for default startup,
model selection and rollback. Intentional visual pause/resume needs to reuse
verified inputs without rehashing the multi-gigabyte bundle on each turn.
Further work includes full runtime input mapping for the production profiles,
pre-admission crash recovery, and recovery after the host itself exits with an
unknown group outcome. The App, Agent and pinned assembly work remain in the
shared priorities.

The startup verification checks declared data and native libraries. The engine
still resolves its original paths and ambient runtime settings at launch.
Pin and stat checks are observed identity checks. Application integration
needs to keep paths and settings stable, including the external Metal library
that pairs with the installed Maple executable.

Run the recorded commands from Inference under the parent no-Python guard:

```sh
../../tools/without_python.sh make test-primary-host sanitize-primary-host \
  test-backend-json sanitize-backend-json test-activation sanitize-activation \
  test-library-runtime sanitize-library-runtime
../../tools/without_python.sh node docs/qualification/life07r-primary-readiness/qualify-qwen.mjs \
  "$PWD/build/test-primary-host" "$PWD/build/test-primary-host"
../../tools/without_python.sh node docs/qualification/life07r-primary-readiness/qualify-qwen.mjs \
  "$PWD/build/test-primary-host-sanitize" "$PWD/build/test-primary-host"
```

The actual driver uses the retained LIFE-07h artifacts. The C host runs under
ASan/UBSan in the second command and drives the same native Qwen engine. The
commands ran on this Mac without Python or Docker.
