# Provider registry v1

The public native executable is `samosa-inference-providers --config
ABSOLUTE_PRIVATE_JSON`, built by `make providers`. One JSON request on stdin,
one adapter result on stdout. Optional `--stream` emits incremental provisional
PCM and a terminal envelope for `speech_generate`; see `speech-output-v1.md` for
event, retry, deadline and disconnect semantics. The C API is `inference_registry_dispatch` in
`src/registry.h`. Both route to existing provider implementations; there is no
gateway dependency or additional model runtime. The optional executable links
libcurl and system SQLite; base registry tests do not require libcurl or models.

## Trusted configuration

The configuration is a <=65536-byte, private, single-link regular file owned by
the current OS user. Symlinks, changed file identity, unknown fields, duplicate
JSON keys and unsafe credentials refuse. Nothing in a public request selects an
owner, executable, URL, credential or state path.

```
{
  "contract_version": 1,
  "owner": "alice",
  "providers": [
    {
      "provider_id": "local-summary",
      "kind": "summarization",
      "state": "/private/inference/summary",
      "engine": "/private/runtime/bin/samosa-summarizer",
      "model": "/private/models/summary.gguf",
      "engine_sha256": "<64 lowercase hex characters>",
      "model_sha256": "<64 lowercase hex characters>"
    }
  ]
}
```

At most16 unique provider IDs; every entry has one adapter kind and a distinct
absolute state directory. Canonical path spelling disallows empty, `.` and `..`
components and trailing slashes; existing filesystem aliases of the same state
refuse. The state holds the adapter's existing owner-bound operations/sessions.
Names are host choices; clients cannot infer a model from a provider name.

Common entry fields: `provider_id`, `kind`, `state`; optional `context_tokens`
(128..131072, default4096), `queue_capacity` (0..8, default4) and
`admission_wait_ms` (1..60000, default10000).

Optional private `platform` declares the deployment target: `macos-arm64`,
`macos-x86_64`, `linux-arm64` or `linux-x86_64`. Unknown values, explicit null,
nonstring values and empty strings refuse configuration. Omission preserves
existing unspecified-platform behavior. This declaration is host policy, not
artifact verification; native package, loader and minimum-OS checks still apply.
It describes where this provider configuration may perform work, not the OS of
a remote endpoint. It is never accepted as a public request override.

| Kind | Additional configuration | Work operations |
| --- | --- | --- |
| `text` | `endpoint`, `model_alias`, required `context_tokens`, optional `api_key_file` | `count_tokens`, `generate`, `provider_status`, `session_create`, `session_get` |
| `embeddings` | Text endpoint fields plus private `embedding_config_file` | `embed_text` |
| `summarization` | `engine`, `model`, `engine_sha256`, `model_sha256` | `summarize`, `summarize_opening` |
| `transcription` | Same engine/model pins | `transcribe` |
| `image_analysis` | Engine/model pins plus `config_sha256`, `tokenizer_sha256`, `weights_sha256` | `analyze_image` |
| `video_analysis` | Engine/model pins plus `manifest_sha256` | `analyze_video_frames` |
| `speech_output` | `package`, `manifest_sha256` | `speech_generate`, `audio_read`, `audio_delete` |
| `fake_text` | No engine/model fields; explicit contract fixture only | Same text operations, fake outputs and ready=false |

Image and video package identities must equal the existing adapters' computed
identity. Credential and embedding profile files use the existing strict native
loaders. The injected native self executable supplies the existing guardian and
summary/visual bridges. Loading configuration does not install, launch or qualify
engines; external text/embedding endpoints retain their existing ownership model.
The declared runtime/platform limits in each adapter contract still apply.

## Discovery and routing

`{"contract_version":1,"op":"describe"}` lists configured identities, adapter
kinds and supported operation names. `availability=configured_unqualified` is
configuration metadata and never proves readiness, package integrity or actual
model execution. A linked native video adapter on Linux does not imply MLX model
availability. Discovery neither creates provider state nor contacts engines.
Discovery entries and `provider_describe` also report `host_platform` (the
executing binary's OS/architecture), `declared_platform` (or null), and
`platform_policy`: `unspecified`, `compatible` or `incompatible`. Compatibility
does not grant model availability, readiness or actual-model qualification.
Unsupported host targets report null host metadata and cannot match an explicit
deployment target. Under translation, the binary architecture is reported.

An incompatible target refuses new inference or engine-health work with
`unsupported_provider_platform` before engine, package or endpoint access.
Owner-scoped retained operation/session reads, cancellation and audio retrieval/
deletion remain available. Existing exact tracked receipts replay before new-work
platform checks, including streamed speech's terminal-only replay. A newly refused
tracked operation saves its failure; changing policy does not silently rerun it.
Platform policy does not change the existing normalized request/replay schema.

`{"contract_version":1,"op":"provider_describe","provider_id":"local-summary"}`
returns the existing adapter's detailed limits/capabilities. Embedding discovery
exposes its actual representation profile and dimensions rather than text
generation metadata. Adapter configuration refusal remains authoritative.

Every work/control request selects `provider_id` and otherwise uses that adapter's
existing request contract. Unconfigured IDs return `provider_unavailable`;
operation/kind mismatches return `unsupported_operation` before adapter work.
Extra work fields remain subject to the adapter's strict validation. Descriptors
accept only their declared fields.

Registry `operation_get`, `operation_cancel` and `session_get` require
`provider_id`; the registry removes this routing field when delegating to the
existing control contract. Other fields remain intact, including unknown fields
that the underlying validator must reject. Identical operation IDs on different
providers are independent. Cross-provider session/operation handles do not grant
access. Owners come from private host configuration, never public JSON.
Embedding `embed_text` also removes the routing field before its strict legacy
validator; its normalized identity uses the selected trusted provider ID.

Results preserve the existing adapter DTOs, receipts, request budgets, scoped
cancellation, replay identity and last-good session semantics. A control result
may not repeat provider_id; the caller already supplied its routing identity.
Changing a model/configuration still follows existing operation conflict rules.
Already saved results can replay without the model/engine when the pinned config
and normalized request match. No new replay or storage schema is introduced.

## Qualification and remaining work

`make test-registry sanitize-registry test-registry-modalities
sanitize-registry-modalities test-registry-cli sanitize-registry-cli` tests real
namespaced state and separate native processes. CLI qualification uses the
existing summary framing, retained offline results, exact selected-engine
cancellation and a surviving subsequent run. In-process modality qualification
links all seven current adapter kinds together; it does not claim model quality.

The existing actual T5 qualification also accepts a fifth `--registry` argument:
`build/test-summary-real PROVIDERS_CLI PACKAGED_T5_ENGINE PINNED_T5_MODEL --registry`.
Its original scalar/large-input/map-reduce/provenance/replay assertions remain.
Use sanitized CLI/harness builds for the corresponding native adapter gate; the
packaged model engine itself is not sanitizer-built.

Speech output is described in [speech-output-v1.md](speech-output-v1.md). Its
owner-scoped retained audio store and public controls are now linked here. Owned
multi-engine activation/residency, global scheduling,
incremental public events and authenticated network service remain separate
unfinished acceptance gates. Per-provider state/queues do not prove global
memory admission or fairness. This interface does not close area1 or release7.
