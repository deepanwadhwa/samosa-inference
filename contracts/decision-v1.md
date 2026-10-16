# Native text decisions v1

OpenDecision text inference belongs to the independent Inference module.
Application routing rules, document selection and actions remain with consumers.
This adapter reuses the existing native DeBERTa backend, exact tokenizer and typed
scoring. Image matching is a separate required adapter, not caption-to-text inference.

## Public request

```
{"contract_version":1,"op":"decide_text","provider_id":"local-decisions",
 "operation_id":"example","timeout_ms":120000,
 "request":{"method":"choice","state":{"message":"find the invoice"},
            "instructions":"Which action?",
            "criteria":{"find":"Find a matching file","count":"Count files"}}}
```

The host configures a `text_decisions` provider in the existing registry, with
private `assets`, `engine_sha256`, `runtime_sha256`, `model_sha256` and
`tokenizer_sha256`. The flat asset root and its policy are defined by
[asset binding](../docs/OPENDECISION_ASSET_BINDING.md). `context_tokens` is512;
an explicit different value refuses. Public JSON selects no paths, commands,
credentials or owner. Discovery reports configured/unqualified availability;
a descriptor is not evidence of current model readiness.

The typed `request` uses the existing [worker request contract](../docs/OPENDECISION_TYPED_REQUESTS.md):
Choice, ordered Score, Noul, explicit-opposite Relation or Relations. Candidate
order and Python-compatible structured-state rendering are preserved. Escaped
NUL and Unicode are supported inside the typed payload, including candidate
names; root routing/control strings retain the common C contract's NUL refusal.
The envelope tokenizes only the bounded payload span, then the existing strict
Rust decoder validates its full syntax, duplicate keys, shape and limits before
admission. An opaque span is never trusted as a validated request.

Worker request/result bodies are at most1MiB; state/text limits, candidate and
relation counts remain those of the typed worker. Native model context is512
positions and batch size8. The ordinary request deadline defaults to60seconds;
an explicit `timeout_ms` may be1..120000, including model hashing/loading and
queue time. The120second allowance preserves the existing worker's full-batch
qualification budget. An enclosing host deadline may shorten it. Expired work
refuses and publishes no successful result.

## Results, ownership and retry

A successful response contains contract/provider identity, `input_sha256` over
the canonical typed payload, typed `result` and `provenance.asset_sha256` in
engine/runtime/model/tokenizer order. Choice distributions retain candidate
order; Score legends retain scale order; relation results preserve input order.
The parent validates exact result fields, finite probability ranges, distribution
normalization, choice/score/confidence consistency and relation thresholds/labels
against the admitted request before publishing. It does not reinterpret scores
as calibrated truth or grant permission to perform application actions.

Optional `operation_id` and `parent_operation_id` reuse existing owner-scoped
operation storage, public status/cancellation and immutable completed replay.
Normalized identity binds all four asset pins and caller deadline/parent values;
state object ordering and omitted semantic defaults normalize consistently,
while candidate reordering conflicts. Completed replay does not reopen a model.
A changed asset path with identical pins may replay; changed pins conflict.
Another provider or owner cannot inspect/cancel/replay this operation.

Execution reuses existing admission leases, framed-file process guardian,
request deadlines, cancellation and requester-death cleanup. Held asset
identities are rechecked before launch and before publishing output. The worker
receives one request followed by clean EOF; persistent residency/lifecycle is a
remaining module requirement. No legacy application consumer is switched by
registering this adapter.

Errors retain the existing envelope. Invalid requests, unknown providers and
unsupported operations refuse before model work. Timeouts/cancellation remain
`deadline_exceeded`/`cancelled`; mutation refuses `artifact_changed`; invalid typed
output refuses `invalid_provider_response`; failed native processes refuse
`provider_process_failed`. Fixed semantic context/input/result-limit errors map
to `context_limit`; private worker exception strings and paths are not forwarded.

## Acceptance

`make test-registry-modalities sanitize-registry-modalities` exercises routing,
normalization, strict malformed-payload refusal, NUL preservation and deadlines.
`make test-decision-public sanitize-decision-public` extends the existing native
transport fixture for actual guardian/operation controls and cleanup; no model
quality is claimed. `tests/test_decision_public.py` requires explicit pinned
asset inventory and original-engine reference report and compares every complete
typed result through fresh public processes, replay, status and retry conflicts.
Platform and actual-model evidence live in the active integration record.
