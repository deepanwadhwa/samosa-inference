# Native text contract version 1

One bounded JSON request/result per CLI process, max request 1 MiB. Every request
has contract_version=1 and op; unknown fields/operations fail. Fake provider's
artificial tokenizer counts decoded prompt UTF-8 bytes, context 131072 tokens.
It is explicitly fake_contract_only and is not a model quality result.

- describe: descriptor includes provider_id, capabilities text/count_tokens,
  context_tokens, tokenizer and quality. Admission metadata reports configured,
  active_capacity=1, queue_capacity (0..8), wait_deadline_ms (1..60000, default
  10000), and order=fifo. Sessions/operations/cancellation capabilities appear
  only with a configured private admission directory and owner. Describe does
  not contact the provider or create private state; it describes configuration,
  not live readiness or artifact verification.
- count_tokens: provider_id/prompt; returns input_tokens/context_tokens/provider_id.
- provider_status: pinned provider_id, no prompt or caller path/configuration.
  Native call shares FIFO admission and checks public props alias/context.
  An external endpoint reports status=metadata_compatible, ready=false,
  artifact_identity=unqualified_external_engine. It does not tokenize/generate,
  modify sessions, install or activate an engine. Fake reports fake_contract_only,
  ready=false and artifact_identity=not_applicable. Missing/changed metadata,
  invalid HTTP and unavailable engine return typed provider errors. The managed
  static-system adapter additionally observes its owned guardian and pinned
  artifact/state metadata, reporting status=ready, ready=true and
  artifact_identity=owned_system_runtime_v1 only while those checks succeed.
  See managed-v1.md and engine-system-runtime-v1.md for the trust and limits.
- generate: provider_id/prompt/max_output_tokens (1..4096); returns answer,
  provider_id, usage input_tokens/output_tokens and quality. Enforce input plus
  reserved output <=context. No provider silently replaces a pinned ID.

Errors: {contract_version:1,error:{code}}. Unsupported/malformed request,
unknown fields, provider_unavailable, invalid_budget, context_limit are typed.
Provider-specific prompt/tokenization and continuation policy stay with Inference.

Optional native executable implements the same public contract. Its descriptor
reports llama-native-template-v1 tokenizer/native quality. Host configures endpoint,
provider/model alias/context/admission state; requests cannot override these.
Public /props alias/context must match before work. Counts use actual rendered
engine tokens; generation sends that exact array. Native usage distinguishes
input_tokens (total prompt) and processed_input_tokens (real timings.prompt_n).
One active with configurable zero through eight FIFO waiters per configured host
admission directory; see admission-v1.md and operations-v1.md. Extra native
errors: busy, state_unavailable, invalid_configuration, provider_changed,
provider_http_error, invalid_provider_response, response_too_large,
deadline_exceeded. Connect<=2s, each HTTP call<=60s, response<=1MiB. Native decoder
caps8192 values/depth16; larger token responses refuse rather than allocate freely.
Alias checks do not replace artifact checksum/engine-version qualifications.

Host --api-key-file optionally configures authenticated native HTTP from a private
absolute regular file belonging to the effective owner (no group/other bits,
<=128 key bytes plus optional newline). Public requests cannot select keys/files.
Adapter validates a bounded token alphabet and sends a Bearer header; 401/403
become provider_auth_failed. It never emits the credential in results/descriptors.
Owner readiness probes can reduce the transfer deadline within1..60000ms;
normal native calls keep the existing60s maximum and2s connect limit.

S06a opaque sessions: host --admission-state private-dir --owner identity enables
sessions capability for native or fake. session_create requires provider_id;
session_get requires session_id. Generate optionally accepts BOTH session_id and
expected_session_revision. Successful answer adds session_id/session_revision;
failure/interruption never advances last-good revision/prompt/result. IDs are
owner/provider/context-bound. Unknown/foreign/stale sessions refuse. Exclusive
kernel work lease authorizes busy recovery; metadata reads preserve live busy.
Cache quotas128 handles,128KiB prompt,64KiB answer,8MiB aggregate,16MiB SQLite;
no silent eviction. Semantic prompt cache does not claim binary KV persistence.

S14c2: count_tokens/generate optionally accept integer timeout_ms1..60000;
request_budgets capability advertises support. Receipt time establishes one
monotonic deadline across queue admission, properties, template, tokenizer,
generation and last-good commit; an earlier host deadline wins. This relative
budget is included in tracked retry identity. Saved exact retries remain offline.
Expiry returns deadline_exceeded and a tracked failed operation with
cancel_requested false unless explicitly cancelled. Session revision/prompt/result
remain last-good. Other operations refuse the timeout field. Transport cleanup
and kernel/filesystem latency are not claimed as hard real-time guarantees.
