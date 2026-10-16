# Connected native text decisions

`engines/decision/native_text.{h,cpp}` connects the pinned Hugging Face Rust
tokenizer to the existing native ONNX Runtime DeBERTa backend and typed scoring
layer. It accepts bounded premise/hypothesis pairs, applies the reference's
truncation and batch padding, validates all three token matrices, and passes IDs
and masks to the two-label model. No Python runtime is used for native execution.

Calls are serialized by the owning engine. A failed asset replacement preserves
the previous model/tokenizer pair. Failed inference preserves caller outputs.
The bridge rejects unloaded calls, empty/oversized batches, overlong strings and
invalid UTF-8. The linked tokenizer's generated numeric JSON is private trusted
transport; the small internal parser is not used for public request JSON.

## Fresh-model gate

The native mode of `tests/test_decision_evidence_reference.cpp` executes fresh
Choice, Score, Noul and Relation decisions and compares each batch with the
pinned original OpenDecision report. It requires exact IDs, attention masks and
token type IDs. Fixed absolute tolerances are `0.01` for model logits and
`0.005` for the fixture results, including probabilities, score and confidence.
Discrete Choice/Relation outputs must match exactly. These are fixture acceptance
tolerances, not a general error bound for arbitrary decisions.

The ten cases cover routing with Choice adjudication, duplicate
descriptions, positive/negative predicates, explicit true/false criteria, ordered
scoring, supporting/contradicting relations and a full eight-by-512 batch.
The structured Unicode case passes through the native state renderer. The test also checks invalid
inputs and recovery after failed model/tokenizer replacement.

On macOS 26.5.1 ARM64 / Apple Clang 21, the isolated normal and ASan/UBSan
connected runs each passed 15,560 checks, including structured Unicode state.
All token matrices matched exactly. Maximum absolute differences were
`0.005859375` for logits and approximately `0.000583839` for typed results.
The independent normal/sanitizer repeat is recorded by the qualification runner;
both runs measured the same maxima.
Ubuntu 24.04 ARM64 / GCC 13.3.0 compiled the bridge and native test harness with
strict warnings. Linux fresh-model execution remains unqualified.

## Reproduction and inputs

`tests/opendecision_native_backend.py SDK RUNTIME MODEL REPORT NEW_OUTPUT`
retains its raw-tensor mode. Add `--tokenizer-library STATIC_ARCHIVE
--tokenizer-library-pin SHA --initializer JSON --initializer-pin SHA` to execute
the connected typed gate. Supply `--runtime-pin`, `--model-pin` and
`--reference-pin` as before. It verifies inputs, copies source, SDK, runtime and
the static archive into an exclusive snapshot, builds normal and ASan/UBSan
executables and writes logs plus `qualification.json`. The tokenizer is linked
statically so the test cannot accidentally load a different tokenizer dylib.

| Input | SHA-256 |
| --- | --- |
| Converted FP16 model | `15f64c4741ace31bb4130c241bdfdcd48b1d76910210d96f4ee86dacddd54398` |
| Effective tokenizer initializer | `7caa16d3d730231fc7b9c2db4716e3e38736880bd4f2e016bb37987645e4900a` |
| Original-model reference report | `f2c54e4d1be67059c184b9a2326b1b7b96ee7b1de02b6efcaeef5cbf3951dc11` |
| Existing ORT runtime | `5b4c4df69e6d1bc10eb0d08da525b11faf8365a0a78cc597a5cc57926330a9ca` |
| Tested Rust static archive with state renderer | `8d398d208b926fbc5ef58b471f082bdc13badc6a9359fd434816cd924e79ddf4` |

The SDK pins and model conversion provenance remain described in
[backend qualification](OPENDECISION_BACKEND_VERIFICATION.md). The Rust archive
is built from the committed lockfile with Rust 1.90.0 for the current host;
this archive does not establish compatibility with older macOS versions.
Sanitizers instrument local C++ code and the harness, not the prebuilt ORT
runtime or Rust archive. Shared CI compiles the bridge and real harness without
requiring model downloads; actual-model execution remains an explicit gate.

## State rendering

`sm_decision_state` / `NativeText::render_state` preserve string bytes, including
embedded NUL. Other JSON values become compact, Unicode JSON with recursively
sorted object keys. Raw JSON parsing retains integer precision; floats are
rounded to binary64 and rendered with Python's notation and exponent rules.
Inputs are bounded to 1 MiB, nesting to 64 and rendered UTF-8 to 65,536 bytes.
Malformed JSON, invalid Unicode and non-finite numbers are refused atomically.

The standard-library-only differential test
`python3 tests/opendecision_state_parity.py NATIVE_TOKENIZER_LIBRARY` passed
25,651 Python comparisons and ABI checks on macOS. It includes arbitrary-width
integers, exponent boundaries, signed zero, Unicode ordering, control characters,
nested objects and 25,000 deterministic random binary64 samples. It now runs in
the shared local/hosted CI command after the release tokenizer build.

This is a private native engine building block. Typed request/result JSON,
framed engine protocol, public provider ownership and
controls, installable model/runtime packages and further platform qualification
remain required. It does not complete the public OpenDecision text provider.
