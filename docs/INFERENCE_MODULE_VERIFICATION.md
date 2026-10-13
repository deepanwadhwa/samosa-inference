# Independent Inference qualification record

## Provider registry checkpoint — 2026-10-12

Area1 remains active; areas2–7 remain pending. The registry is a public integration
over existing native providers, not a substitute for speech, lifecycle, scheduling,
streaming, authentication or final replacement release.

Current macOS ARM64 evidence:

| Gate | Command/result | Scope |
| --- | --- | --- |
| Registry namespaces | `make test-registry sanitize-registry`:134 checks each | Two configured providers, independent same-ID work/replay, sessions, owner controls, duplicate identities/state aliases and strict discovery/refusal |
| All linked adapters | `make test-registry-modalities sanitize-registry-modalities`:202 checks each | Text/embedding/summary/transcription/image/video in one process, descriptor routing and capability mismatch refusal; protocol/configuration coverage only |
| Native public registry | `make test-registry-cli sanitize-registry-cli`:427/430 checks | Separate CLI/config/guardian/engine processes, exact offline restart replay, cross-provider cancellation refusal, selected-engine cancellation, survivor, empty/malformed/duplicate/shared-state config refusal, private credential mode/link enforcement and no credential discovery output |
| Actual T5 through registry | Existing `build/test-summary-real` and sanitized harness/registry with fifth `--registry` argument:1292 checks each | Existing library/March scalar quality, scalar long-input refusal, large opening summary with positive map/reduce rounds, input/model/runtime provenance and exact replay; external packaged T5 engine is not sanitizer-built |
| Existing core | `make test`:passes | Sessions1106, operations832, admission79 and existing artifact/install/guardian/activation/runtime/managed-stream/catalog gates, plus new registry134 |
| Existing native text/acquisition | `make test-native sanitize-native`:passes | Exact native token IDs/context/last-good sessions, controlled HTTP439, credentials39 normal, managed CLI41, remote artifact167, HTTPS/TLS, remote keys42 and setup40 |
| Existing summary/transcription/image | `make test-summary test-whisper test-vision` and corresponding sanitizer targets:passes | Summary478/478, Whisper508/511, visual634/644; bounded native adapter/control fixtures, not new actual-model platform claims |
| Existing temporal lifecycle | `make test-temporal sanitize-temporal TEMPORAL_METADATA=/Users/deepanwadhwa/.samosa/models/molmo2-4b-mlx-q4-v1`:1023/1029 | Existing fixture package/engine16-frame controls, request death cleanup and mutation; actual MLX execution was not rerun in this checkpoint |

Independent source archive check: the registry builds and runs outside this
checkout at `/private/tmp/samosa-inference-registry-independent.EAPNVC`. Its
`qualification.log` records base134, linked202, CLI282, linked-sanitizer202 and
CLI-sanitizer285 passes from the initial archive. Later credential/config refusal
cases increased the workspace CLI gate to427/430; the enhanced independent CLI
recheck also passes427/430 in `qualification-final.log`, using `source-final.tar.gz`.
Source/test independence does not close the full independent engine/release
acceptance gate. Archives precede this final evidence annotation; qualified code
and tests are identical to the workspace at this checkpoint.

Actual T5 runtime:

- Engine `build/native-summarizer-runtime/bin/samosa-summarizer`, SHA256
  `514e5c8512d44ecc3da246039d30fcd7a8218e0be6f363364a53e18c212df397`.
- Model `/private/tmp/samosa-summary-real-model.gguf`, SHA256
  `30c73e0b2ebd4f65538d87f5157aa7193261e8ff362e441df86c6941f3e9265c`.
- Commands use the absolute registry CLI, packaged engine and pinned model paths
  followed by `--registry`; the sanitized run selects
  `build/test-summary-real-sanitize` and `build/samosa-inference-providers-sanitize`.
- The original native model implementation and its actual quality assertions were
  retained. No fake output was accepted as actual-model evidence.

Development failures: new tests initially expected the wrong existing session,
operation/status and summary DTO names; expectations were corrected to existing
contracts before the passing runs. The first temporal invocation omitted required
TEMPORAL_METADATA and refused; the configured normal/sanitizer runs above pass.

Linux registry qualification is open. Docker Desktop was launched; actual
`docker ps` calls on API1.52 and1.41 return500 from its Unix-socket API. Historical
Linux adapter evidence does not qualify this changed registry. No Linux pass,
area closure or legacy retirement is claimed here. The independent repository
and product pin were established on 2026-10-13; they do not close other release gates.

Next: extract existing Pocket/Kokoro native speech implementation, using its
pinned Sherpa ABI, existing voice configuration and bounded progress semantics.
Keep it behind the same public provider/operation boundary and guardian controls.
