# Dependencies

C11 compiler and make; libc and system SQLite >=3.24 at runtime. Native C tests, no interpreter/package
manager. JSON transport primitive is locally owned, mechanically duplicated to
keep independent component builds; it contains no shared task/model policy.
Current build qualification: macOS ARM64 and native Debian12 ARM64 Linux,
including ASan/UBSan and Linux LeakSanitizer. AMD64 emulation is not qualified.

Optional transcription uses an externally configured existing whisper.cpp CLI
and English GGML model, with explicit executable/model SHA256. The adapter links
only libc/system SQLite and reuses the existing owned-engine guardian; no new
speech engine, interpreter or sibling code dependency is introduced. Native C
fixture tools qualify controls on macOS/Linux. The actual installed macOS engine
and model are separate qualification evidence; their platform/loader dependencies
are trusted host configuration, not covered by executable SHA alone. Linux
real-engine/model qualification and broader languages remain open.

Optional native text adapter: system libcurl headers/library, discovered with
curl-config (override CURL_CFLAGS/CURL_LIBS). `make native` builds the separate
samosa-inference-llama executable; default/fake executable does not link libcurl.
`make providers` links the existing native text, embedding, summary, transcription,
image and temporal adapters behind a common registry with the same libcurl/SQLite
dependencies. Configured engines/model packages remain external pinned native
dependencies; no parent headers, source files, Makefile or gateway are required.
Configured external compatible llama-server and model artifact provide real
inference. Adapter reads public HTTP only, no source/chat/provider-private files.
Host pins provider ID, public model alias, context and private admission directory.
The optional managed foreground transport additionally uses POSIX threads
(-pthread), supplied by the platform runtime. Stream fixtures use the same
native dependency without requiring libcurl or a model.
Optional HTTPS artifact acquisition uses the same libcurl URL/TLS API (>=7.62).
Its local TLS qualification fixture additionally needs OpenSSL>=1.1.1 CLI
(OPENSSL_BIN can select the installed binary); production runtime does not.

Optional managed static-system profile declares the installed macOS ARM64 system
framework/libSystem/libc++ runtime, or Debian12 ARM64 glibc/libstdc++/libgcc_s and
optional libgomp runtime, trusted outside its executable digest. Its pinned
llama-server must statically link provider/backend libraries, disable dynamic
backend plugins/OpenSSL and embed Metal shaders on macOS. Direct loader policy
is in contracts/engine-system-runtime-v1.md. External dylib-backed engines remain
unqualified by executable digest alone. Actual managed engine/model qualification
has run on macOS; Linux managed CLI/loader fixtures/build are separate evidence.

S06a sessions add system SQLite >=3.24 to the base native executable. Fake/model-
free operation still has no libcurl/model/Python requirement. Native C session
state/leases remain Inference-owned; public handles expose metadata only.

Actual Whisper qualification reuses the monolith voice installer source pin:
commit f049fff95a089aa9969deb009cdd4892b3e74916, archive
https://api.github.com/repos/ggml-org/whisper.cpp/tarball/f049fff95a089aa9969deb009cdd4892b3e74916,
SHA256 d8cd961352377b1cc612224016a9ebdfe0ae508dc2b2f9ef514b341d672e3fdc.
The Linux native ARM64 build uses CMake BUILD_SHARED_LIBS=OFF,
WHISPER_BUILD_EXAMPLES=ON, WHISPER_BUILD_TESTS=OFF, WHISPER_BUILD_SERVER=OFF,
GGML_NATIVE=OFF and GGML_CPU_ARM_ARCH=armv8.2-a+fp16, then builds whisper-cli.
This requires ARMv8.2 FP16-capable hardware; it is not a universal ARM64 binary.
The default native feature probe generated inconsistent GCC12 ARM FP16 flags in
the qualification container; the explicit tested architecture avoids that build
failure. Engine source is unmodified. Models remain separately pinned artifacts.

Optional owned text embeddings reuse the managed C adapter and a pinned native
llama.cpp server/model. No Python interpreter, Python library, model-serving
Python process or Python launcher belongs to the installed Samosa runtime.
`tests/embedding_real.py` is an opt-in developer/CI qualification harness only;
it requires Python 3.11+ on the test host and is never called by shipped binaries.
Source/test archives may contain that harness without making it a runtime
dependency. The pinned evaluation build recipe is in
`tests/embeddinggemma2-candidate.json`; its CPU-only system-runtime build does
not require Metal shaders or a media projector.

2026-10-13 independent CI qualification additionally covers macOS14 ARM64 with
Apple clang15/Xcode15.4 and Ubuntu24.04 x86-64 with clang18.1.3, system SQLite and
libcurl. Exact source/run scope and remaining actual-model platform requirements
are recorded in [native qualification](docs/SPEECH_LOADER_VERIFICATION.md).

## Optional native text decisions

The combined `make providers` registry now includes the existing OpenDecision
text adapter. Building this target requires the pinned Rust1.90.0 toolchain and
locked `engines/decision/tokenizer/Cargo.lock` dependencies, in addition to its
existing C11/SQLite/libcurl dependencies. The tokenizer/request implementation
links statically; Rust is a build dependency, not an installed interpreter.
Minimal/default builds without the combined registry keep their existing deps.

Decision execution uses an explicitly configured private four-file native asset
root: worker executable, ORT1.27.0 runtime, ONNX model and effective tokenizer.
The host pins all four files and checks the existing native loader graph before
execution and artifact identity after execution. Assets are supplied separately;
registry builds and controlled tests download no model. Production native worker
builds additionally require the pinned ORT SDK/runtime described in
`engines/decision/ort_sdk_pins.json`. Python is used only for developer/CI parity
and control harnesses, never installed as a decision runtime or model server.

`test-decision-public` and `sanitize-decision-public` reuse the existing transport
fixture with tiny synthetic loader/model files. The sanitizer target instruments
the public adapter/guardian; its external worker and Rust dependencies retain
their separately declared instrumentation. A sanitizer-linked external worker
imports a compiler runtime outside the closed asset root and is correctly refused
by this production loader policy. Raw worker/scoring sanitizer gates remain
separate. Fixtures do not establish model quality or actual-model availability.
