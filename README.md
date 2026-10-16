# samosa-inference 0.1.0

Independent-module completion is tracked in
[the seven-area acceptance plan](docs/INFERENCE_MODULE_PLAN.md). Start extraction
with [the existing implementation reuse audit](docs/REUSE_AUDIT.md).

`make providers` builds the unified native provider registry over the existing
text, embedding, summary, transcription, image and video adapters. Requests
select a configured provider ID; discovery and controls use one public interface.
See [provider-registry-v1](contracts/provider-registry-v1.md) for private host
configuration, limits, qualification and remaining lifecycle/service work.

Independent native C11 Inference boundary, libc and system SQLite. `make test`,
`make sanitize`, `make release`. One JSON body on stdin, one result on stdout.
The initial provider fake-text-v1 counts UTF-8 bytes of the actual decoded prompt
and generates explicit fake output. It proves contracts only, never model quality.
See contracts/text-v1.md. New providers belong behind this boundary.

Optional transcription reuses the monolith's existing whisper.cpp CLI:
`make test-whisper sanitize-whisper` builds/qualifies its public adapter with
native C fixtures. Host-pinned engine/model hashes and private admission state
configure build/samosa-inference-whisper. It consumes canonical prepared WAV
bytes and returns derived, uncertain timed text through the existing operation
boundary. See [transcription contract](contracts/transcription-v1.md).
The native build/test-whisper-real tool separately accepts Inference/Sources
executables, an installed engine/model and an English WAV fixture for actual
capture/cache checks. Optional trailing media-worker and encoded-audio arguments
exercise compressed capture against an independent WAV decode. Configuring
SAMOSA_TEST_CHUTNI_LIBRARY and SAMOSA_TEST_CHUTNI_WORKER also exercises actual SDK
publication/readback and cold-state durable Sources audio queue capture,
publication, completion and offline saved-capture readback. Sources now owns
transcript caching and source timeline mapping. Broader modality/quality checks,
broader visual support and the complete Inference release remain open.

The real-model harness also supports `SAMOSA_TEST_FULL_DURATION=1` with a
66-second PCM WAV containing the known English speech at2s,31s and61s and the
Chutni library/worker environment variables above. It checks three overlapping
Sources windows, recognized initial/middle/late speech, absolute source-time
segments, SDK publication/completion and exact origin/model-free saved readback.
This is developer qualification through native executables, not a shipped tool
or a general speech-accuracy benchmark.
The same mode accepts the existing optional media-worker/encoded-file arguments.
Supply an independent full WAV decode as the reference: every selected window's
canonical PCM hash must match the exact input reported by public Inference.

Optional visual analysis reuses the monolith's native VisionPsy inspect/quit
engine. `make test-vision sanitize-vision` qualifies public RGBA input, derived
observations, durable replay, model/engine receipts and guardian controls using
native fixtures on macOS/Linux. The actual macOS BF16 checkpoint also passes the
existing PDF heading recognition gate through the public adapter, normally and
with the adapter/harness sanitized. See [visual contract](contracts/visual-v1.md).
Sources visual capture/index/publication integration is qualified; observations
retain their declared uncertain coverage rather than an exhaustive accuracy claim.

Optional temporal analysis: `make temporal` builds the native public
`samosa-inference-temporal` adapter for the existing Molmo2 processor/model.
It accepts at most16 ordered timestamped RGBA frames with axes<=512, verifies
the host-pinned package manifest and all declared artifacts, and uses existing
admission/operation/guardian controls. The actual macOS Q4 model passes ordered
red/blue analysis, pixel/preparation/time/model/runtime bindings and exact offline
replay, normally and with the C adapter/harness sanitized. The external MLX
runtime is not sanitizer-built. See [temporal contract](contracts/temporal-v1.md).
Sources integration and dedicated temporal lifecycle qualification pass, including
16-frame cleanup, cancellation, deadlines, requester death, package mutation,
admission refusal and owner-bound offline replay. Actual model availability is
macOS; Linux gates qualify the native adapters and controls.

Native implementation, runtime and builds; no shipped Python runtime. Core and
registry qualification are native. The optional embedding developer harness uses
Python on the test host only, as declared in DEPENDENCIES.md. No sibling source
or parent Makefile access. Native S02–S06a/S12a/S12b subsets; installation,
model residency and assembled resource/release gates remain open.

Optional actual text inference: `make native test-native sanitize-native` builds
the libcurl C adapter and qualifies it against a controlled native HTTP fixture.
The real engine is a configured external dependency, not fake output.

```
build/samosa-inference-llama --llama-endpoint http://127.0.0.1:18645 \
  --provider-id local-text-v1 --model-alias configured-model.gguf \
  --context-tokens 4096 --admission-state /private/inference-admission \
  --queue-capacity 4 --admission-wait-ms 10000
```

Public props must match configured alias/context; render with apply-template,
count with tokenize, and generate using exactly those token IDs. One admitted
native call with configurable0..8 FIFO waiters and a bounded admission deadline;
capacity0 means immediate busy through the same scheduler. Descriptor IDs are
arbitrary; no Agent/App model-name branches. Artifact checksums, streaming,
installation and model residency gates remain open.

Opaque sessions: fake or native CLI can configure --admission-state private-dir
--owner alice. Public session_create/session_get and bound generate keep the last
successful prompt/revision through restart and failed work. No private cache
schema access by Agent. `make test sanitize` includes native owner/revision/crash/
handle and byte retention checks. Sessions are semantic caches, not binary KV
checkpoint or complete model residency support.

Tracked count/generate accepts operation_id; operation_get/operation_cancel are
bound to the configured owner. Exact normalized retry returns saved output without
provider work. Queued cancellation removes its own ticket; native HTTP cancellation
closes its own transfer. Interrupted workers never silently replay model requests.
Capacity128 histories/8MiB results including live reservations/16MiB operation DB,
no eviction. See contracts/operations-v1.md and admission-v1.md. Native focused
`make test-admission test-operations test-native`: FIFO79/scoped716/HTTP238 checks,
including actual SIGKILL, queued/active cancellation and last-good session isolation.

`make test sanitize test-native sanitize-native release` runs the independent
owner qualification and packages source, tests, task specifications and all public
contracts in `dist/samosa-inference-0.1.0.tar.gz`. Fake contract checks include
98 assertions, including configured/unconfigured admission descriptors and
owner-gated operations/cancellation. Describe reports configuration; it does not
prove engine readiness or artifact integrity.

Before pushing inference changes, run `sh tools/check_native.sh` in clean macOS
and Ubuntu source exports. Install the tokenizer toolchain first with
`rustup toolchain install 1.90.0 --profile minimal --component rustfmt`.
The shared script checks tokenizer formatting, executes its ABI tests and builds
its release library using the committed Cargo lockfile. An optional source-directory argument selects the
export to test. GitHub Actions invokes this same script, including the GCC C++
engine tests on Ubuntu. Record the actual host architecture and compiler with
the results; hosted runners additionally qualify their configured matrix hosts.
Actual-model checks use their separately pinned assets and remain explicit gates.

Artifact acquisition: `build/samosa-inference-artifacts --state PRIVATE_DIR
--source ABSOLUTE_REGULAR_FILE --sha256 EXPECTED_SHA --size BYTES --byte-ceiling
BYTES --disk-ceiling BYTES`. Submit artifact_status/artifact_install/artifact_resume
with contract_version1; see contracts/artifacts-v1.md. Fixed-buffer local import
uses a private install lease, explicit interrupted resume and no-clobber verified
publication. At most16 retained identities; no eviction. Model-free manager needs
only libc. `make test-artifact test-install test-artifacts-cli` and
`make sanitize-artifact sanitize-install` verify this subset. Optional remote
acquisition and owned activation are described below. Complete distribution and
resource/recovery qualification remain open S13a gates. Verified bytes explicitly
report readiness=unqualified.

`provider_status` accepts a pinned provider_id and uses the shared admission
policy to inspect live alias/context. An external compatible engine reports
metadata_compatible, ready=false and unqualified_external_engine artifact identity.
The fake stays fake_contract_only/ready=false. No setup or generation runs during
this check. The managed adapter below independently owns and qualifies its engine.

Owned lifecycle primitive: `make test-engine-process sanitize-engine-process`
tests the private lifeline guardian, bounded lifetime/logs, process-group teardown
and parent/engine death with unrelated-process survival. See
contracts/engine-lifecycle-v1.md. This layer is available to Inference activation
policy; it alone does not establish verified engine/model identity or readiness.

Activation controller tests: `make test-activation sanitize-activation` exercise
verified input observations, fd-bound model, guardian-held host residency lease,
revision checks and durable last-good configuration across candidate failures and
fresh-process reads. See contracts/activation-v1.md.

Published native engine entries can use artifact_make_executable; it checks the
registered digest/native header and changes only owned published bytes to0500.
Host --api-key-file configures private authentication for the native text CLI.
Credentials never enter public JSON fields. An executable digest is not a digest
of its dynamically linked provider package.

`make native` also builds `samosa-inference-managed --config ABSOLUTE_PRIVATE_JSON`.
It explicitly activates a pinned static-system llama package and fd-bound model,
owns its engine until input EOF/signal/engine exit, and accepts correlated concurrent
public JSON requests over its private host stream. See contracts/managed-v1.md for
the single-request `--single` and explicit serialized `--single-current` modes,
including repeated calls with an unchanged pinned host configuration, and for
the exact host configuration and limits. Owned status now reports ready=true
only with successful authenticated qualification and current artifact/lifecycle
observations; external CLI endpoints retain ready=false. Direct loader checks
and environment refusal are qualified by87 fixture checks; private managed CLI
configuration/refusal/key cleanup by25 checks. Real macOS static-system engine
and public front-door qualification are recorded in the architecture checkpoint.
Remote acquisition, package distribution,
restart UX and complete product setup remain open; no overall S13a closure yet.
Actual macOS active-model cancellation through the separate configured native CLI
is qualified, including engine slot observation and a subsequent surviving run.
The same active-work cancellation now passes through the foreground stream.
Bounded stream fixtures exercise six work/two control workers, correlation,
capacity/duplicate refusal, response ceilings, EOF cancellation and output stalls.

Optional samosa-inference-download adds HTTPS acquisition into the existing
artifact lease/stage/publication protocol. See contracts/remote-artifacts-v1.md.
Trusted host URLs/digests/limits stay outside public bodies; verified installation
does not activate a provider. Explicit resume validates a durable prefix checkpoint
locally and requests an exact byte range;200 fallback compares the retained prefix.
Native fixtures cover integrity, production-interval worker death with retained
tail, range/header/redirect/cancellation/deadline refusal and corrupt/torn private
checkpoints. --bearer-key-file accepts a bounded private single-link credential;
redirects never forward credentials beyond the original authority. Public TLS
CLI proves certificate refusal, trusted success and offline exact retry. Complete
distribution/setup recovery and broader resource qualification remain open.

Trusted setup: `samosa-inference-setup --catalog-file ABSOLUTE_PRIVATE_JSON
--state PRIVATE_ARTIFACT_ROOT --byte-ceiling BYTES --disk-ceiling BYTES` selects
pinned artifact configuration by public provider ID. See
contracts/provider-catalog-v1.md. Catalog listing and status are read-only;
install/resume enforce native architecture/minimum OS before acquisition and
reuse global installer lease/quota, private credentials, durable prefix recovery
and HTTPS verification. Installation never launches a provider. No production
catalog is bundled until its artifact/provenance/platform gates qualify.
`make test-catalog sanitize-catalog test-setup sanitize-setup` exercises the native
parser/loader and real HTTPS one-shot setup, SIGTERM interruption/resume,
last-good preservation and offline retry. Default build/test stays model-free;
setup is built/tested with the optional native curl target.

Native summarization reuses the monolith T5 engine through the optional public
`samosa-inference-summary` adapter. See [summary-v1](contracts/summary-v1.md)
for host pins, request controls, bounded coverage and qualification commands.

Native speech extraction now provides the private `speech_runtime` primitive,
reusing the existing Pocket/Kokoro C ABI, voice settings and bounded PCM progress.
`make test-speech-runtime sanitize-speech-runtime` qualifies controlled native
behavior; optional actual Pocket tests and limitations are recorded in
[Speech runtime verification](docs/SPEECH_RUNTIME_VERIFICATION.md). Public speech
routing, verified package installation, guardian isolation and owner-bound audio
artifacts remain pending.

The private speech package verifier now binds complete nested runtime/model/voice
contents to a host-pinned manifest and retains identities for rechecks. See
[its contract](contracts/speech-package-v1.md) and
[qualification](docs/SPEECH_PACKAGE_VERIFICATION.md). Actual pinned-package Pocket
generation and cancellation/recovery pass on macOS ARM64. Production catalog
provenance, loader compatibility and owned guardian execution remain open.

Speech native loader compatibility now inspects pinned library graphs and checks
reported Sherpa ABI before model initialization. See
[loader evidence and supported search profiles](docs/SPEECH_LOADER_VERIFICATION.md).
Independent CI exports this repository's source and runs controlled macOS/Linux
normal and sanitizer gates; actual-model readiness and release gates remain open.
