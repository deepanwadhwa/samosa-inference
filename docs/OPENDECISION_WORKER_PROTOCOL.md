# Private native decision worker

`engines/decision/main.cpp` loads one native text model/tokenizer pair and serves
repeated typed JSON decisions over inherited stdin/stdout. It opens no listening
socket. The owning provider must verify asset identity, launch it under the
common process guardian, apply admission/deadlines/cancellation and interpret
responses. Public provider registration is still pending.

Launch with `--model ABSOLUTE_ONNX_PATH --tokenizer ABSOLUTE_INITIALIZER_PATH`.
Unknown/repeated options, missing values and relative paths are refused. The
private worker relies on its owning provider for verified asset snapshots; its
CLI does not install or download models. An actual decision is needed to verify
model readiness; there is no unsolicited startup readiness announcement.

## Frames and shutdown

Each request and response is a four-byte unsigned big-endian length followed by
that many payload bytes. Lengths must be between one and 1,048,576 bytes. This
reuses the summary engine's framing convention, with decision-specific limits
and typed JSON bodies. The model remains resident across requests.

Successful replies are the complete OpenDecision result JSON described in
[typed requests](OPENDECISION_TYPED_REQUESTS.md). Semantic failures return
`{"error":"CODE"}` and allow another request on the same stream. Only fixed
semantic codes are forwarded; unexpected errors and exceptions become
`engine_failed`, and oversize generated replies become `decision_result_limit`.
Private exception text and paths are not included in wire errors.

Clean EOF between frames shuts down and releases the model. A partial header,
partial body, zero length or oversized length terminates the worker without
trying to resynchronize. Reads/writes handle EINTR and partial transfers. The
worker ignores SIGPIPE so a closed reader becomes an I/O failure. The transport
does not close caller-owned descriptors itself.

Exit statuses: `0` clean EOF; `2` invalid arguments; `3` model/initializer load
failure; `4` framing or I/O failure; `5` fatal exception outside request handling.

## Build and checks

Build `build/samosa-decision-engine` or its `-sanitize` variant with
`DECISION_ORT_INCLUDE=SDK_DIRECTORY DECISION_ORT_LIBRARY=ABSOLUTE_RUNTIME_PATH`.
The committed Rust lockfile builds the static tokenizer/request library.
`DECISION_TOKENIZER_TARGET` or exported `CARGO_TARGET_DIR` can select an existing
build directory. SDK/runtime pins remain in the existing backend qualification;
these optional targets do not download them.

`make test-decision-transport sanitize-decision-transport` uses a separate fixture
with controlled logits and the actual Rust request decoder. Normal and
ASan/UBSan runs on macOS ARM64 passed 25 pipe checks: fragmented headers/body and
Unicode, maximum input, concatenated requests, semantic/exception/oversize reply
recovery, malformed/truncated frames, clean EOF and a broken output reader.
These targets belong to the shared `make test sanitize` gates; normal checks
also compile the production worker entry point. Rust and prebuilt ORT are not
sanitizer-instrumented.

`tests/opendecision_worker_reference.py` qualifies a real worker against the
pinned original report. It verifies worker/model/initializer/report hashes
before/after, executes all ten cases through one process, checks complete typed
results with the fixed `0.005` numeric tolerance, then verifies malformed-JSON
recovery and clean EOF. On macOS 26.5.1 ARM64, the normal run passed all cases
with maximum numeric result error approximately `0.000583839146`. Its worker
SHA-256 was `b3476ef18195eca8780135c1a054e247bf771de24a66b73f1bfe4eab91774669`.
The Make-built ASan/UBSan worker also passed all ten cases, recovery and clean
EOF with the same maximum error. Its SHA-256 was
`01e82d42508c07e197608e16d4e043820b32fa8056421fd1d91e4aa3d175d897`.
Linux transport qualification remains pending.

The preceding typed-request revision passed full local CI on macOS and Ubuntu
ARM64. The worker/transport changes require a subsequent full shared run before
pushing. Provider ownership/controls, package installation and public API
qualification remain open; this worker alone does not close text integration.
