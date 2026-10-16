# Private typed decision requests

`engines/decision/request.{h,cpp}` connects strict JSON requests to the existing
native scoring primitives. Rust decodes public-shaped JSON into owned request
data and length-bearing, read-only C views. C++ copies those views into model
inputs and writes typed result JSON. Embedded NUL is preserved; no untrusted
JSON is passed to the model bridge's small internal parser.

This is an engine building block, not a registered public inference endpoint.
The framed engine transport, common provider controls and actual-model JSON
round-trip qualification remain required.

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
of previous output after inference failure. Linux and the full shared suites
for this request-layer change remain pending. The preceding structured-state
revision passed the full shared suites on macOS and Ubuntu ARM64.
