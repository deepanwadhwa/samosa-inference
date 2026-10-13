# Managed text provider v1

Optional native executable `samosa-inference-managed --config ABSOLUTE_PRIVATE_JSON`
uses libc/system SQLite/libcurl. The default/fake build remains model-free. The
host owns this foreground process and its private stdin/stdout stream. Startup
is an explicit activation; no saved configuration is silently replayed.

`--single` reads one public request from stdin and emits its response without a
startup event. It checks the configured `expected_revision` exactly.
For repeated host-supervised process calls, `--single-current` explicitly opts
into serialized reactivation of the same pinned host configuration. Activation
reads and advances the committed revision while holding the residency lease;
the host file remains unchanged. A concurrent resident still returns
`activation_busy`. Model/runtime hashes, file ownership, loader policy, readiness,
input-change checks and last-good rollback remain required. Public JSON requests
cannot select this mode or override its pins. Revision exhaustion refuses.

The host file must be an absolute owner-private regular file, <=16KiB. Unknown
or duplicate JSON fields refuse. All fields are required:

```json
{
  "contract_version": 1,
  "profile": "llama-static-system-v1",
  "state": "/private/host/inference",
  "engine": "/private/installed/llama-server",
  "engine_sha256": "64 lowercase hexadecimal characters",
  "engine_size": 20513984,
  "model": "/private/installed/model.gguf",
  "model_sha256": "64 lowercase hexadecimal characters",
  "model_size": 5629108704,
  "context_tokens": 4096,
  "model_alias": "configured-model",
  "owner": "configured-owner",
  "port": 53558,
  "expected_revision": 0,
  "gpu_layers": 99,
  "threads": 4
}
```

The example sizes describe the qualified macOS QA artifact, not a requirement
to install it. Real hashes are supplied by the versioned package configuration.
Profile asserts trusted provenance: static llama/ggml/server/common libraries,
embedded Metal shaders on macOS, no dynamic backend plugins or OpenSSL. Generic
binary inspection cannot prove arbitrary code has this provenance. The pinned
artifact recipe/version and actual-provider evidence must establish it.

Context128..131072, port1024..65535, GPU layers0..999, threads1..32,
owner1..64 bytes, alias1..128 bytes, revision0..2147483645. Paths are trusted host
inputs, never public request fields. Engine/model private regular files must
match exact sizes/SHA256. System-runtime-v1 validates direct loader dependencies
and rejects loader/plugin environment overrides. The engine/model artifact
relationship and one-residency lease are inherited from activation-v1.

The owner chooses a loopback port. Fixed trusted argv configures fd5 model,
host127.0.0.1, one model slot, alias/context, configured threads/GPU layers,
ubatch64, reasoning off, no web UI and two HTTP handler threads. A random64-byte
private key is created using exclusive anchored owner files; readiness requires
authenticated props, alias/context, wrong-key401/403 and template/tokenization.
It has a120s startup deadline with1s individual HTTP transfer deadline. Successful
qualification unlinks the credential file; the key remains private in memory.
Cleanup checks the created inode under its held directory before unlinking.

The provider ID is managed-text- plus a SHA256 binding profile/platform/runtime
policy/tokenizer policy, engine/model hashes and sizes, context and alias. Durable
last-good records selected runtime policy and activation revision. Credential
file paths in saved argv are ephemeral; restarting regenerates a credential from
explicit host configuration with a checked expected revision. Saved argv is not
an automatically executable recovery instruction.

Successful startup emits one provider_started event with provider ID, activation
revision and runtime policy. Subsequent newline-delimited frames carry public
text-v1 requests in a version1 envelope:

```json
{"contract_version":1,"request_id":"caller-1","request":{"contract_version":1,"op":"describe"}}
```

Replies correlate independently of completion order:

```json
{"contract_version":1,"request_id":"caller-1","response":{"contract_version":1,"component":"samosa-inference"}}
```

IDs1..64 ASCII identifier bytes must be unique among outstanding requests. Body
validation still belongs to text-v1. Invalid frames and duplicate IDs produce a
null-ID transport error without replaying an existing request. Six work workers
and two reserved control workers bound accepted concurrency; control operations
are describe/operation_get/operation_cancel. Capacity rejection reports stream_busy
under the rejected request's ID. One bounded emergency response pauses input until
written. Active model admission remains one model slot with four FIFO waiters;
stream capacity does not grant extra model slots. Describe performs no model work. Provider
status reports ready=true only while authenticated metadata and owned guardian,
held/named artifact inode/mutation observations and state/lease anchors pass.
These observations do not protect against a hostile local OS owner rewriting
bytes undetectably between checkpoints. Offline tracked retries remain available
through the ordinary native CLI and do not require reactivating the model.

The foreground stream accepts cancellation while a generation worker runs;
control responses and generation terminal responses have independent request IDs.
Actual macOS qualification observes owned engine is_processing=true, sends an
in-stream cancellation, receives a cancelled generation, then a successful new
generation on the surviving engine. Separate configured native CLI cancellation
also remains supported. Host signal
cancellation is chained through tracked requests instead of being overwritten.
EOF between requests, SIGINT/SIGTERM, engine exit and guardian lifetime/log limit
stop only the owned engine. Frames<=1MiB; incomplete/oversized frames refuse.
Input/output are nonblocking with20ms collection polls; macOS FIFO EOF is detected
even without POLLHUP. Completed responses are serialized into complete correlated
lines by one writer. Responses<=512KiB, fixed4MiB worker stacks, <=eight requests
in work memory and one ingress frame<=1MiB. A2s output stall triggers cancellation.
EOF/signal/terminal failure cancels accepted workers; cooperative shutdown has a5s
deadline. If a worker remains live at the deadline, the owner exits immediately
without freeing live references, closing the guardian lifeline and removing all
threads. Durable operation outcomes require explicit reconciliation after lost
delivery; no provider work is silently replayed. Guardian lifetime<=24h/log
processing<=1MiB; kernel syscall/scheduling responsiveness retains host limits.

No App model-specific policy or weight requirement follows from this optional
host executable. Remote acquisition, public installation workflow, distributed
assembly/version compatibility and full product qualification remain open.

## Managed embedding profile

Host profile `llama-embedding-static-system-v1` additionally requires
`embedding_config_file`, an absolute private file satisfying embedding-v1.
The embedding model/runtime digests must equal the activation artifact pins;
missing, malformed or mismatched configuration refuses before startup. The text
profile refuses this extra field. The owned runtime uses `--embedding --pooling
mean`, with both batch and ubatch sizes equal to the configured context so a
bidirectional input is never split into causal microbatches. No projector is used
by this text-only profile. Recipe identity must declare mean pooling and output
projection, quantization and f32/L2 settings appropriate to the selected model.

Readiness additionally executes a short embedding and validates the configured
dimension, finite f32 values, unit norm and token count before activation commits.
The same owned artifact/lifecycle observation runs on each subsequent transport
call. Public embed_text requests use the existing stream, admission and durable
operation machinery. This supports owned model execution; actual-model quality
and platform qualification are separate gates, not consequences of readiness.

`tests/embedding_real.py` accepts explicit model SHA256 and runtime revision,
checks the model bytes, then launches the managed provider with computed runtime
pin. It saves actual receipts and paraphrase rankings, requires four correct
positive/negative rankings, tests context refusal, shuts down the owned engine,
and replays a saved operation through the public CLI against unreachable port1.
Pass matching normal/sanitizer binaries as `--binary` and `--offline-binary`.
The gate does not select a production default or qualify Sources publication.

Optional `--single` accepts one bounded public JSON request on stdin and emits
its ordinary public response after owned readiness, without the stream startup
event/envelope. It releases the owned runtime before exit. This permits existing
Sources isolated public-process transport to call the owned adapter directly.
The host still supplies explicit state and expected activation revision; this
mode neither discovers configuration nor silently restarts a saved runtime.
The actual-model gate requires single/stream vector and profile equality.
