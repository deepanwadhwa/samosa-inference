# EmbeddingGemma 2 installation and distribution

The [qualified catalog entry](../catalog/embeddinggemma2-cpu-v1) reuses the
original static native engine, GGUF installer and managed/public embedding
implementation. Runtime revision is4fbc76dec51d0add466f0210855c0596589b60d4;
engine SHA-256 is
`958601dbd22ab2f5857e09860247f30c74153cb0f3914b8d62f2bacbba9aab97`.
The immutable upstream Q8_0 model remains pinned to
`2188ac1deca4b77dffefd603c2776a9d76d9d74ec01841392982ebb840b09135`.
The ten-file package is325842726 bytes, index
`fb11fc6c5885830282cc8e0d03d12dfbf7ceae0202bd4fb97c369d1af382a3ea`.

Native metadata inspection reports GGUFv3, architecture `gemma-embedding2`,413
tensors and46 keys. The original engine imports only system libraries and has
observed deployment minimum26.0; both catalog and index enforce26.0. Actual
model execution is qualified on macOS26.5.1 ARM64 only. Text embeddings use the
original8192-token context,768 dimensions, search-query prefix, titled-document
format, mean pooling, L2 normalization and float32 output. Media projectors and
multimodal embeddings are outside this entry.

The exact-package HTTPS gate passes normal and sanitizer-equipped host runs,
covering private host policy, TLS refusal, publication, last-good preservation,
offline status/resume, completed/incomplete removal and cache-only reinstall.
The existing `embedding_real.py` then runs against freshly installed engine/model
files through managed activation and the common registry. It verifies finite
normalized vectors,4/4 paraphrase rankings, input/model/runtime provenance,
context-limit refusal, single/managed stream parity, exact replay after shutdown
without credentials, and clean owned engine shutdown.

The shared published-provider gate independently passes both modes against the
actual GitHub runtime and immutable Hugging Face model URLs. It verifies every
installed file's size, hash and single-link identity; compares the installed
embedding recipe with the exercised harness configuration; runs the same vector
checks; removes source configuration; and performs cache-only resume/removal/
reinstallation. Both exclusive temporary installations are removed. No model
weight is uploaded to GitHub or retained as a new local copy.

Exact source/model/license/configuration/executable/log pins and both remote
receipts are in the entry. Original and converted model cards declare Apache-2.0;
both complete pinned cards and model/runtime notices are included. Preparation
shares the existing helper, with a per-package minimum OS, and reproduces all
published Whisper/T5 metadata bytes unchanged. The existing full setup suite
also passes normal/SAN on macOS/Linux. Those fixture controls do not establish
Linux model execution. This closes embedding's distribution slice only; area2,
loaded lifecycle readiness and the remaining full goal stay open.

## Real-URL crash, contention and corrupt-prefix recovery

The shared remote gate's optional `--exercise-resume` passes normal and
sanitizer-equipped hosts against the real published model/runtime sources. It
starts a new native installer in an exclusive temporary workspace, observes a
checksummed production checkpoint of at least1MiB, stops only that live child and
independently verifies the recorded prefix. A second public install refuses with
`artifact_busy`. Observed SIGKILL termination releases the original install lease.

Changing one committed byte in that owned temporary stage causes native resume
to refuse with `artifact_checkpoint_mismatch`; inode, size and corrupt contents
remain preserved. Restoring that byte permits native resume, full pinned
installation and the existing768-dimensional/4-of-4 ranking/provenance/replay/
shutdown gate. Cache-only removal/reinstallation and workspace cleanup also pass.
The retained stage includes a tail beyond the durable checkpoint, exercising
the native retained-tail protocol against the real service. This proves verified
recovery, without asserting a measured network-byte saving.

Exact checkpoint/retained counts, refusal codes, source/index/host-executable pins
and cleanup outcomes are retained in the entry's `remote-recovery-normal.json`
and `remote-recovery-sanitize.json`. Logs remain at
`/private/tmp/samosa-embedding-remote-recovery-{normal,sanitize}.log`. The test
changes only its newly created private cache stage; original model files remain
untouched. This strengthens the shared acquisition gate, not all provider
distribution or lifecycle acceptance.
