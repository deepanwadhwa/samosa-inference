# Private native tokenizer

This C ABI wraps Hugging Face `tokenizers` 0.23.2, matching the existing
OpenDecision reference environment. It is a private building block, not yet a
registered inference provider. The caller supplies verified effective tokenizer
JSON; the library does not acquire models or execute Python.

From the inference repository root:

```
cargo +1.90.0 test --locked --manifest-path engines/decision/tokenizer/Cargo.toml
cargo +1.90.0 build --locked --release --manifest-path engines/decision/tokenizer/Cargo.toml
```

The lockfile pins transitive dependencies. The nested toolchain file applies
when Cargo is invoked from this directory; the explicit version above also
works from the repository root. Supply an external `--target-dir` to keep build
outputs outside the checkout.

`sm_tokenizer_encode` accepts JSON with `texts` (a batch of single-element or
two-element string arrays), `max_length`, `truncation` (`none`, `longest_first`,
`only_first`, `only_second`) and `padding` (`none`, `longest`, `max_length`).
Optional `padding_side` and `truncation_side` are `left` or `right` (default).
Padding defaults to `[PAD]`, ID 0, type ID 0; explicit `pad_token`, `pad_id`
and `pad_type_id` must agree with the loaded vocabulary.

Limits are eight examples, 512 tokens, 65,536 UTF-8 bytes per string, 524,288
decoded text bytes per batch, and 1 MiB per request or response. Initializer
size is limited to 16 MiB. Untruncated sequences exceeding the requested context
return `SM_TOKENIZER_CONTEXT_LIMIT`. Successful JSON contains `input_ids`,
`attention_mask`, and `token_type_ids`. See `tokenizer.h` for ownership rules.

QA uses `tests/opendecision_tokenizer_parity.py LIBRARY CHECKPOINT INITIALIZER`
with the existing OpenDecision Python environment. It verifies checkpoint and
initializer pins before comparing all three matrices with `AutoTokenizer`.
Python is only a reference tool, not a shipped dependency. On macOS ARM64 the
initial run passed 1,187 successful encodings plus expected refusals, covering
Unicode, sentence pairs, both padding/truncation directions, all supported
strategies, long context and post-error recovery. The shared local/hosted CI
entry point now builds this library and runs its ABI tests, including owned
successful encoding and padding-state reset with a small WordLevel fixture.
That fixture does not qualify DeBERTa tokenization. A corpus exported only after
the macOS HF comparisons passed was replayed with the independently compiled
Ubuntu 24.04 ARM64 library: all 1,222 cases passed (including refusals).
The corpus SHA-256 is
`c13aab5084baebea59b6d37f44a761c37f225fa9fe054c0e6517965275e816c9`.
These results do not qualify the public OpenDecision provider or other hosts.

To reproduce cross-platform comparison, generate a new corpus using
`opendecision_tokenizer_parity.py ... --corpus-output NEW_FILE`, record its hash,
then run `python3 tests/opendecision_tokenizer_replay.py LIBRARY INITIALIZER
CORPUS --corpus-sha256 EXPECTED_HASH`. The replay tool requires only Python's
standard library; the initial reference comparison requires the pinned HF
environment. Corpus generation refuses replacement of an existing file.
