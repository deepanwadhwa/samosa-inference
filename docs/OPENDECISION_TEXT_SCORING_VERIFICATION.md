# Native text evidence scoring

`engines/decision/text_evidence.{h,cpp}` implements the first typed scoring
primitives for Chat's existing two-label DeBERTa checkpoint. It accepts an
ordered pair-inference callback so the same algorithms can be checked against
captured original-model logits before connection to the native tokenizer and
ONNX backend. It does not register or advertise a public provider.

Preserved behavior:

- Predicate Noul uses float32 softmax of the pair's two logits.
- Explicit true/false Noul executes two separate unpadded singleton calls and
  applies float32 softmax to their entailment logits. Instructions do not enter
  this computation.
- Relations score each proposition and its supplied opposite independently,
  preserve input order, use `only_first` truncation and batches of at most eight,
  and apply inclusive thresholds for supports, contradicts, unknown or conflicted.
- Failures preserve the caller's previous output. Invalid thresholds, malformed
  model output and non-finite logits are rejected. The private relation call
  bounds each request to 64 items; an empty item list returns an empty result.

The caller supplies already serialized state. Structured-state rendering,
Choice's multiple profiles, ordered Score, typed request/result JSON, native
model/tokenizer connection and public service controls remain to be implemented
and qualified. A complete OpenDecision provider is still required before image
decisions or later components begin.

`make test-decision-evidence sanitize-decision-evidence` checks predicate
semantics, exact batch order, all four threshold outcomes, ownership of results
and refusal behavior. These targets also belong to `make test sanitize`, so the
shared local/hosted CI command executes them. The normal target additionally
compiles the separate capture-parity harness.

For capture parity, run:

```
build/test-decision-evidence-reference REPORT_JSON REPORT_SHA256
```

The harness authenticates the report before parsing, replays its actual logits
through the native scorer, and compares probabilities with absolute tolerance
`1e-7`. It covers six original-model captures: positive/negative predicates,
explicit binary criteria, supporting/contradicting relations, and four long
relations forming a full batch of eight pairs. Report SHA-256:
`f2c54e4d1be67059c184b9a2326b1b7b96ee7b1de02b6efcaeef5cbf3951dc11`.

On macOS 26.5.1 ARM64 with Apple Clang 21, both normal and ASan/UBSan runs passed
63 model-free checks and 195 capture checks. This qualifies scoring on captured
logits, not a fresh native-model invocation or end-to-end typed result parity.
Ubuntu 24.04 ARM64 with GCC 13.3.0 passed the same 63 model-free and 195 capture
checks in normal and ASan/UBSan builds, from a separate source snapshot. The
full shared suites for the preceding tokenizer changes also passed on both
hosts. The newly added scoring targets still require a subsequent full shared
suite run before these changes may be pushed.
