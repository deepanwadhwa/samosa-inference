# Native summarization v1

The optional `samosa-inference-summary` executable exposes the existing monolith
native T5 summarizer through public Inference JSON. Build with `make summary`.
It uses the existing engine unchanged, with scalar network-order framing and
`summarize: ` prefix; it does not substitute generic llama-server generation.

Hosts supply absolute `--summary-cli` and `--summary-model` paths, exact lowercase
SHA256 pins via `--summary-cli-sha256` and `--summary-model-sha256`, and a private
`--admission-state` directory. `--owner`, `--provider-id`, `--queue-capacity` and
`--admission-wait-ms` retain the existing Inference meanings. Model and executable
must be owner-held regular files without group/other write permissions or hard
links. A private copy of the monolith's hard-linked checkpoint satisfies this
existing artifact boundary. Runtime dynamic libraries remain release dependencies.

```json
{"contract_version":1,"op":"summarize","provider_id":"native-t5-summary-v1","text":"The city council approved a public library. Construction begins in March.","max_output_tokens":64,"operation_id":"summary-1","timeout_ms":60000}
```

`text` is nonempty valid UTF-8, at most 60,000 bytes. `max_output_tokens` is required,
16–256 inclusive. Unknown fields refuse; requests cannot select paths, models,
model pins, engine arguments or sampling parameters. The native engine uses two
CPU threads, GPU layers zero and batch size one for this scalar adapter.
`describe`, `operation_get` and `operation_cancel` are also supported. Existing
operation parents, owner isolation, ingress deadlines, admission, cancellation,
idempotency and durable response replay apply. Request identity binds text,
output budget, provider and both configured artifact pins. An absent timeout uses
60 seconds; a supplied timeout must be 1–60,000 milliseconds.

A response contains `kind: derived`, `artifact_type: summary`, the input SHA256
and byte count, generated `text`, and engine/model provenance. Coverage is always
`partial`, with reason `bounded_encoder_and_generated_summary_uncertain`.
The scalar engine has a 512-token encoder context and refuses a prompt that exceeds it; an empty engine reply is rejected as invalid_provider_response.
`processed_input_tokens` is null because the inherited protocol provides no
consumption receipt. The input hash binds the submitted bytes, not a claim that
all bytes were consumed or that generated statements are factual.

The existing process guardian retains the model descriptor and admission lease,
binds private regular-file stdin/stdout separately from bounded stderr logs,
observes deadlines and parent death, kills/reaps the owned group, and removes
`request.frame`/`result.frame`. Accepted output requires exactly one nonempty
length-prefixed UTF-8 reply of at most 8,192 bytes; trailing, truncated, malformed
and oversized replies refuse. Output growth is monitored during execution;
private child-created files also retain the existing 64 MiB file-size ceiling.
Executable and model identity/bytes are verified before launch and after reaping.
Linux launches the verified executable descriptor; macOS uses the existing
receipt-checked path launch and retained-model path resolution.

Run `make test-summary sanitize-summary` for native fixture-backed transport,
controls, refusal, replay and parent-death checks. These are not model-quality
checks. `build/test-summary-real` and `build/test-summary-real-sanitize` take
absolute adapter, engine and model paths and exercise the actual inherited T5
checkpoint, provenance, a bounded reference input and durable replay. The latter
sanitizes public adapter/test code, not the separately packaged model engine.

This API is a prerequisite for Sources enrichment. It does not itself implement
Sources automatic enrichment, representation publication or query embeddings.

`summarize_opening` accepts the same request fields and bounds as `summarize`,
with a distinct operation identity and recipe
`samosa-native-summary-opening-map-reduce-v1`. It reuses the existing native
batch protocol (one opening prompt, memory-budget flag). The unchanged native
engine splits the supplied prompt at actual tokenizer/UTF-8/sentence boundaries
into encoder-safe chunks and maps all chunks, using at most
`min(max_output_tokens, 64)` output tokens per chunk. A nonempty, exactly framed
mapped result still must fit the inherited 8,192-byte reply limit.

The adapter ports the monolith's `native_summary_chunk_length` and bounded
reduction policy: pass every mapped chunk through sentence-aware, at most
1,400-byte scalar reduction; accept only shrinking rounds; repeat at most four
rounds; then cap the final digest at 1,800 bytes at a UTF-8 boundary. Each native
call retains the existing pin checks, admission, request deadline, cancellation
and owned process cleanup. A failed native reduction refuses the request rather
than publishing a digest from a failed call. The initial map result is not
silently truncated before reduction.

Opening responses retain partial coverage with reason
`native_partitioning_and_generated_summary_uncertain`. Provenance additionally
records `chunk_max_output_tokens`, `input_partitioning: native_token_chunks`,
`map_output_bytes`, accepted `reduction_rounds`, `max_output_bytes: 1800`, and
`output_truncated`. Consumed input token count remains null because the inherited
protocol supplies no measured consumption receipt. Output truncation and model
summarization uncertainty remain explicit even if the native chunk map completes.
This operation does not itself schedule Sources enrichment or publish Chutni
artifacts.
