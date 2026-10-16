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
strategies, long context and post-error recovery. Linux execution and inclusion
in the shared local CI entry point remain pending; this result does not qualify
the public OpenDecision provider.
