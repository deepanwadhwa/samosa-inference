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
| `text_decisions` | `assets`, `engine_sha256`, `runtime_sha256`, `model_sha256`, `tokenizer_sha256`; fixed context512 | `decide_text` |
| `image_decisions` | Same four flat asset pins; image model profile and fixed context64 | `decide_image` |
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

## Managed engine lifecycle

Trusted text/embedding entries may use `managed_config_file` instead of endpoint,
alias, credentials and embedding config fields. This names the existing private
managed-v1 configuration; it is never a public request field. The declared
provider ID must equal the existing managed identity, owner must match, context
must match, and registry state must equal the managed state's `admission` child.
The provider kind must match its managed profile. Mixed endpoint/managed fields
refuse. Configuration loading/discovery neither creates state nor starts engines.
The host retains the parsed configuration for its lifetime.

Host `describe` adds `managed_lifecycle.operations` and `provider_ids` for the
configured owned providers. The pure `inference_registry_dispatch` API retains
its existing routing/discovery contract; lifecycle ownership belongs to
`inference_registry_host_dispatch` in registry_host.h.

Requests contain exactly `contract_version`, `op` and `provider_id`:

- `provider_start` uses existing managed activation/guardian and actual provider
  qualification. A healthy repeated start is idempotent. Failed startup leaves
  last-good activation unchanged and retains the parsed configuration for retry.
  Restart advances the existing current activation revision under its lease.
- `provider_lifecycle_status` observes live owned identity/process/artifacts.
  A saved installation/activation never grants readiness.
- `provider_stop` stops/reaps the owned engine and removes credentials while
  retaining the host's parsed configuration. Repeated stop is safe.
  A confirmed crashed/reaped engine reports its terminal error once while
  releasing residency; status then reports stopped and another start can qualify.
  Unconfirmed shutdown retains ownership and must be retried before replacement.

Successful replies include `provider_id`, boolean `ready`, `state` (`stopped`,
`ready` or `unavailable`) and `health_error` (null or a typed reason). An engine
exit or artifact mutation reports unavailable. Start refuses a stale attempted
engine; stop before restarting it. Unmanaged entries return
`provider_lifecycle_unmanaged`. New starts honor configured platform policy.
Start/stop/status refuse `provider_busy` while requests use that provider;
ordinary operation reads/cancellation remain concurrent. Lifecycle transitions
exclude new same-provider requests until the transition completes. Existing
per-provider tracked receipts can still replay after stopping the engine.

`samosa-inference-providers --config PRIVATE_JSON --session` reuses the managed
CLI's bounded correlated JSON-lines transport, with `request_id`/`request`
envelopes and `request_id`/`response` replies. Keep stdin open between requests.
Use it to start, infer, control and stop within the same owner lifetime. EOF,
signal, output failure and owner death use existing cancellation/guardian cleanup.
A single-request invocation also cleans its owned engines before exit.
`--session` is separate from the existing speech PCM `--stream` mode.

This initial integration covers managed text/embedding ownership wiring. Other
modalities' common residency and full area3 qualification remain required;
configuration support alone does not assert actual-model lifecycle acceptance.

## Optional resident summary and speech ownership

Summary entries may select `lifecycle: "resident-budget-v1"` with separately
pinned engine/model byte sizes. Speech entries may select
`lifecycle: "resident-speech-v1"` with `worker_sha256`, `worker_size` and
`manifest_size` in addition to the existing package/manifest pin. The worker is
the trusted running registry host executable; libraries and model/voice files
remain pinned by the complete speech manifest. Loading the private configuration
performs no startup/model work. Entries omitting lifecycle retain their existing
one-shot behavior.

Both adapters consume the same public identity-only start/status/stop controls
and per-provider request lifetime gate. Start commits existing activation state
only after an actual bounded successful summary/synthesis and identity recheck.
Speech records `pinned-host-adapter-v1` for the verified host executable plus
qualified native bundle, keeping external model-engine system-runtime policy
unchanged. Saved activation never substitutes for observed live readiness.
Operation replay and retained speech audio reads remain available after stop
without reopening model/package inputs. Precise private configuration and
qualification scope: [registry lifecycle](../docs/REGISTRY_LIFECYCLE.md).

Persistent correlated session replies currently carry complete speech receipts;
incremental resident PCM events on that transport remain open. Existing one-shot
speech `--stream` behavior is unchanged. Other modalities, global scheduling,
service/authentication and release acceptance remain separate requirements.

Text decision entries may select `lifecycle: "resident-text-decision-v1"` with
separate engine/model byte sizes and their existing four native asset pins.
This reuses the unchanged persistent decision worker through common lifecycle
controls. Readiness requires successful native typed inference and full asset/
loader verification. The last-good activation stores all four pins/profile as a
hash-bound bundle snapshot under `closed-native-bundle-v1`. Exact normalized
results/provenance and stopped-worker replay remain unchanged; complete typed
context refusals preserve worker framing. Image decisions retain their existing
one-shot path until their changing-image resident boundary is qualified.

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


Image decision entries may opt into `lifecycle: "resident-image-decision-v1"`
with their existing four pins plus engine_size/model_size. The same private
DecisionHost requires verified native warm inference before readiness and serves
changing prepared images on one loaded context. All existing public dimensions,
RGBA/typed request limits, normalization, result/provenance and replay rules
remain. The versioned private image envelope is a trusted adapter detail, never
a public command or caller-chosen path. Old engines fail this mode's readiness;
new candidate engines need independent distribution qualification. Full remaining
modality lifecycle and later numbered areas stay open.
