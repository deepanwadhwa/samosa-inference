# Private typed decision requests

`engines/decision/request.{h,cpp}` connects strict JSON requests to the existing
native scoring primitives. Rust decodes public-shaped JSON into owned request
data and length-bearing, read-only C views. C++ copies those views into model
inputs and writes typed result JSON. Embedded NUL is preserved; no untrusted
JSON is passed to the model bridge's small internal parser.

This is an engine building block, not a registered public inference endpoint.
The [framed engine transport](OPENDECISION_WORKER_PROTOCOL.md) now preserves
these requests through a persistent native worker. Common provider controls
and public registration remain required.

Every request contains `method`. Unknown root fields and duplicate request or
criteria keys are rejected. Candidate objects retain insertion order, which
controls tie handling and Choice adjudication order.

| Method | Required input | Optional input |
| --- | --- | --- |
| `choice` | `state`, `instructions`, `criteria` object with 2–128 named string/null descriptions | None |
| `score` | `state`, `instructions`, `criteria` array with 2–128 unique string descriptions | None |
| `noul` | `state`, `instructions` | `criteria`: null or an object with exactly string `true` and `false` definitions |
| `relation` | `state`, string `proposition` and `contradiction` | `threshold` |
| `relations` | `items`: up to 64 objects with `state`, string `proposition` and `contradiction` | `threshold` |

State uses the qualified native JSON-to-text renderer. Threshold defaults to
`0.5` and must be finite and strictly between zero and one. Explicit binary Noul
uses its true/false descriptions; instructions do not enter model scoring.
Empty batched relations return `[]`. Input and result JSON are each bounded to
1 MiB, individual strings to 65,536 UTF-8 bytes, and state rendering has its
separate depth/output limits.

Results preserve the original OpenDecision shapes: Choice includes the selected
name, ordered candidate probabilities and entropy confidence; Score includes
its zero-based expected position, legend, probabilities and confidence; Noul
contains the predicate probability; Relation includes its discrete relation,
independent support/opposition scores, threshold and `explicit_opposite` backend.
Batched relations return an array. Output publication is atomic on errors.

The C ABI in `engines/decision/tokenizer/request.h` owns one opaque request
handle. Views borrow all data until the handle is freed once. Names, descriptions
and states are length-bearing byte strings. Load failures leave the caller's
handle unchanged, and concurrent view reads are allowed.

`make test-decision-request sanitize-decision-request` executes the C++ adapter
against the actual Rust decoder and controlled logits. The committed Cargo
lockfile and Rust 1.90.0 build the static archive. These targets belong to
`make test sanitize`, so the shared local/hosted command includes them.
The sanitizer target instruments C++ code, not the release Rust archive.

On macOS ARM64, six Rust tests passed and both normal and ASan/UBSan C++ runs
passed 29 checks. Coverage includes all methods, candidate order, exact typed
result shapes, structured state, embedded NUL, strict refusal and preservation
of previous output after inference failure. The request-layer revision passed
the full shared suites on macOS and Ubuntu ARM64. Subsequent worker/transport
changes retain their separate qualification gates.

## Fresh-model JSON round trip

The native qualification runner now accepts `--json-requests` alongside its
existing native tokenizer/model arguments. In this mode every pinned reference
case becomes a complete JSON request. The real Rust decoder and C++ request
dispatcher run fresh native inference, then the harness compares the complete
result JSON with the original OpenDecision result. Keys, shapes, legends,
discrete results and backend metadata must match; numeric result fields use the
previously fixed absolute tolerance `0.005`. Every model batch still requires
exact token IDs, attention masks and token type IDs and logit tolerance `0.01`.

On macOS 26.5.1 ARM64 / Apple Clang 21, isolated normal and ASan/UBSan runs each
passed all ten cases and 64,259 assertions, including fixture serialization
checks. The maximum absolute differences were `0.005859375` for logits and
approximately `0.000583839` across numeric result fields. The cases cover Choice
adjudication and duplicate descriptions, Score, both Noul modes, both relation
directions, structured Unicode state and eight long relation pairs.

The pinned static tokenizer/request archive for this run has SHA-256
`3bf1aa96d7288f145bf9d60f1ceddf3336074fb064795f36bc6990427ce6af83`.
Model, initializer, runtime and report pins match
[the connected native text gate](OPENDECISION_NATIVE_TEXT_VERIFICATION.md).
The runner snapshots source and libraries, verifies all inputs before/after,
and records `scope: fresh native JSON requests/results; public provider still
unqualified` in its qualification record. The shared CI additionally compiles
this actual-model harness without downloading models.
