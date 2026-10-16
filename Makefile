DECISION_TOKENIZER_TARGET ?= $(if $(CARGO_TARGET_DIR),$(CARGO_TARGET_DIR),engines/decision/tokenizer/target)
DECISION_TOKENIZER_ARCHIVE = $(DECISION_TOKENIZER_TARGET)/release/libsamosa_decision_tokenizer.a
CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -O2 -g
WARN = -std=c11 -Wall -Wextra -Werror -Wpedantic
LIBS = -lsqlite3
SOURCES = src/registry.c src/embedding.c src/inference.c src/sessions.c src/admission.c src/operations.c src/engine_process.c src/budget.c
HEADERS = src/registry.h src/platform.h src/embedding.h src/inference.h src/json.h src/llama.h src/sessions.h src/admission.h src/operations.h src/sha256.h src/engine_process.h src/budget.h
.PHONY: all test sanitize release clean native test-native sanitize-native test-admission test-operations test-artifact sanitize-artifact test-install sanitize-install test-artifacts-cli test-engine-process sanitize-engine-process test-activation sanitize-activation
.PHONY: test-remote-artifact sanitize-remote-artifact test-remote-tls test-remote-key sanitize-remote-cli
.PHONY: test-whisper sanitize-whisper
all: build/samosa-inference build/samosa-inference-artifacts
.PHONY: test-registry sanitize-registry test-registry-modalities sanitize-registry-modalities
build/test-registry: tests/test_registry.c tests/whisper_fixture.h $(SOURCES) $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc tests/test_registry.c $(SOURCES) $(LIBS) -o $@
test-registry: build/test-registry
	./build/test-registry
sanitize-registry: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_registry.c $(SOURCES) $(LIBS) -o build/test-registry-sanitize
	./build/test-registry-sanitize
REGISTRY_FLAGS = -DSAMOSA_WITH_DECISION $(NATIVE_FLAGS) -DSAMOSA_WITH_SPEECH -DSAMOSA_WITH_WHISPER -DSAMOSA_WITH_VISION -DSAMOSA_WITH_TEMPORAL -DSAMOSA_WITH_SUMMARY
REGISTRY_SOURCES = src/decision.c src/decision_assets.c src/speech.c src/audio_store.c src/speech_worker.c src/speech_runtime.c src/speech_package.c src/speech_package_loader.c src/library_runtime.c src/engine_runtime.c $(SOURCES) src/llama.c src/whisper.c src/vision.c src/visual_bridge.c src/temporal.c src/temporal_input.c src/temporal_package.c src/summary.c src/summary_protocol.c src/artifact.c
REGISTRY_HEADERS = src/decision.h src/decision_assets.h engines/decision/tokenizer/request.h engines/decision/tokenizer/tokenizer.h src/speech.h src/audio_store.h $(SPEECH_WORKER_HEADERS) $(HEADERS) src/registry_host.h src/whisper.h src/vision.h src/temporal.h src/temporal_package.h src/summary.h src/summary_protocol.h src/artifact.h src/private_key.h
.PHONY: providers
providers: build/samosa-inference-providers
build/samosa-inference-providers: src/registry_main.c src/registry_host.c $(REGISTRY_SOURCES) $(REGISTRY_HEADERS) $(DECISION_TOKENIZER_ARCHIVE) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(REGISTRY_FLAGS) src/registry_main.c src/registry_host.c $(REGISTRY_SOURCES) $(DECISION_TOKENIZER_ARCHIVE) $(DECISION_RUST_LIBS) $(LIBS) $(CURL_LIBS) $(DL_LIBS) -o $@
build/samosa-inference-providers-sanitize: src/registry_main.c src/registry_host.c $(REGISTRY_SOURCES) $(REGISTRY_HEADERS) $(DECISION_TOKENIZER_ARCHIVE) | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(REGISTRY_FLAGS) -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer src/registry_main.c src/registry_host.c $(REGISTRY_SOURCES) $(DECISION_TOKENIZER_ARCHIVE) $(DECISION_RUST_LIBS) $(LIBS) $(CURL_LIBS) $(DL_LIBS) -o $@
.PHONY: test-registry-cli sanitize-registry-cli
build/test-registry-cli: tests/test_registry_cli.c tests/whisper_fixture.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc tests/test_registry_cli.c -o $@
build/test-registry-cli-sanitize: tests/test_registry_cli.c tests/whisper_fixture.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_registry_cli.c -o $@
test-registry-cli: build/samosa-inference-providers build/test-registry-cli
	$(abspath build/test-registry-cli) $(abspath build/samosa-inference-providers)
sanitize-registry-cli: build/samosa-inference-providers-sanitize build/test-registry-cli-sanitize
	$(abspath build/test-registry-cli-sanitize) $(abspath build/samosa-inference-providers-sanitize)
build/test-registry-modalities: tests/test_registry.c tests/whisper_fixture.h $(REGISTRY_SOURCES) $(HEADERS) $(DECISION_TOKENIZER_ARCHIVE) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(REGISTRY_FLAGS) -Isrc tests/test_registry.c $(REGISTRY_SOURCES) $(DECISION_TOKENIZER_ARCHIVE) $(DECISION_RUST_LIBS) $(LIBS) $(CURL_LIBS) $(DL_LIBS) -o $@
test-registry-modalities: build/test-registry-modalities
	./build/test-registry-modalities
sanitize-registry-modalities: $(DECISION_TOKENIZER_ARCHIVE) | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(REGISTRY_FLAGS) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_registry.c $(REGISTRY_SOURCES) $(DECISION_TOKENIZER_ARCHIVE) $(DECISION_RUST_LIBS) $(LIBS) $(CURL_LIBS) $(DL_LIBS) -o build/test-registry-modalities-sanitize
	./build/test-registry-modalities-sanitize
test: test-registry
sanitize: sanitize-registry
.PHONY: test-summary-protocol sanitize-summary-protocol
build/test-summary-protocol: tests/test_summary_protocol.c src/summary_protocol.c src/summary_protocol.h src/json.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc tests/test_summary_protocol.c src/summary_protocol.c -o $@
test-summary-protocol: build/test-summary-protocol
	./build/test-summary-protocol
sanitize-summary-protocol: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_summary_protocol.c src/summary_protocol.c -o build/test-summary-protocol-sanitize
	./build/test-summary-protocol-sanitize
.PHONY: vision test-vision sanitize-vision
.PHONY: temporal
TEMPORAL_SOURCES = src/temporal_main.c src/temporal.c src/temporal_input.c src/temporal_package.c src/visual_bridge.c src/artifact.c $(SOURCES)
build/samosa-inference-temporal: $(TEMPORAL_SOURCES) $(HEADERS) src/temporal.h src/temporal_package.h src/vision.h src/artifact.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DJSON_TOKENS_MAX=8192 -DSAMOSA_WITH_TEMPORAL $(TEMPORAL_SOURCES) $(LIBS) -o $@
build/samosa-inference-temporal-sanitize: $(TEMPORAL_SOURCES) $(HEADERS) src/temporal.h src/temporal_package.h src/vision.h src/artifact.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DJSON_TOKENS_MAX=8192 -DSAMOSA_WITH_TEMPORAL -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer $(TEMPORAL_SOURCES) $(LIBS) -o $@
temporal: build/samosa-inference-temporal
.PHONY: test-temporal-input sanitize-temporal-input
.PHONY: test-temporal sanitize-temporal
build/test-temporal: tests/test_temporal.c tests/whisper_fixture.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc $< -o $@
build/test-temporal-sanitize: tests/test_temporal.c tests/whisper_fixture.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer $< -o $@
test-temporal: build/samosa-inference-temporal build/test-temporal
	$(abspath build/test-temporal) $(abspath build/samosa-inference-temporal) '$(TEMPORAL_METADATA)'
sanitize-temporal: build/samosa-inference-temporal-sanitize build/test-temporal-sanitize
	$(abspath build/test-temporal-sanitize) $(abspath build/samosa-inference-temporal-sanitize) '$(TEMPORAL_METADATA)'
build/test-temporal-input: tests/test_temporal_input.c tests/whisper_fixture.h src/temporal_input.c src/temporal_package.c src/artifact.c src/temporal.h src/temporal_package.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc $< src/temporal_input.c src/temporal_package.c src/artifact.c -o $@
test-temporal-input: build/test-temporal-input
	./build/test-temporal-input
sanitize-temporal-input: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_temporal_input.c src/temporal_input.c src/temporal_package.c src/artifact.c -o build/test-temporal-input-sanitize
	./build/test-temporal-input-sanitize
VISION_SOURCES = src/vision_main.c src/vision.c src/visual_bridge.c src/artifact.c $(SOURCES)
build/samosa-inference-vision: $(VISION_SOURCES) $(HEADERS) src/vision.h src/artifact.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DSAMOSA_WITH_VISION $(VISION_SOURCES) $(LIBS) -o $@
build/samosa-inference-vision-sanitize: $(VISION_SOURCES) $(HEADERS) src/vision.h src/artifact.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DSAMOSA_WITH_VISION -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer $(VISION_SOURCES) $(LIBS) -o $@
vision: build/samosa-inference-vision
build/test-vision: tests/test_vision.c tests/vision_fixture.h tests/whisper_fixture.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc tests/test_vision.c -o $@
build/test-vision-sanitize: tests/test_vision.c tests/vision_fixture.h tests/whisper_fixture.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_vision.c -o $@
test-vision: build/samosa-inference-vision build/test-vision
	$(abspath build/test-vision) $(abspath build/samosa-inference-vision)
sanitize-vision: build/samosa-inference-vision-sanitize build/test-vision-sanitize
	$(abspath build/test-vision-sanitize) $(abspath build/samosa-inference-vision-sanitize)
build/test-vision-real: tests/test_vision_real.c tests/vision_fixture.h tests/whisper_fixture.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc tests/test_vision_real.c -o $@
build/test-vision-real-sanitize: tests/test_vision_real.c tests/vision_fixture.h tests/whisper_fixture.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_vision_real.c -o $@
WHISPER_SOURCES = src/whisper_main.c src/whisper.c src/artifact.c $(SOURCES)
build/samosa-inference-whisper: $(WHISPER_SOURCES) $(HEADERS) src/whisper.h src/artifact.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DSAMOSA_WITH_WHISPER $(WHISPER_SOURCES) $(LIBS) -o $@
build/samosa-inference-whisper-sanitize: $(WHISPER_SOURCES) $(HEADERS) src/whisper.h src/artifact.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DSAMOSA_WITH_WHISPER -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer $(WHISPER_SOURCES) $(LIBS) -o $@
build/test-whisper: tests/test_whisper.c tests/whisper_fixture.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc tests/test_whisper.c -o $@
build/test-whisper-sanitize: tests/test_whisper.c tests/whisper_fixture.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_whisper.c -o $@
build/test-whisper-real: tests/test_whisper_real.c tests/whisper_fixture.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc tests/test_whisper_real.c $(if $(filter Linux,$(shell uname -s)),-ldl,) -o $@
build/test-whisper-real-sanitize: tests/test_whisper_real.c tests/whisper_fixture.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_whisper_real.c $(if $(filter Linux,$(shell uname -s)),-ldl,) -o $@
build/test-temporal-engine-real: tests/test_temporal_engine_real.c tests/whisper_fixture.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc $< -o $@
build/test-temporal-package-real: tests/test_temporal_package_real.c tests/whisper_fixture.h src/temporal_package.c src/temporal_package.h src/artifact.c src/artifact.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc $< src/temporal_package.c src/artifact.c -o $@
build/test-temporal-package-real-sanitize: tests/test_temporal_package_real.c tests/whisper_fixture.h src/temporal_package.c src/temporal_package.h src/artifact.c src/artifact.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer $< src/temporal_package.c src/artifact.c -o $@
build/test-temporal-public-real: tests/test_temporal_public_real.c tests/whisper_fixture.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc $< -o $@
build/test-temporal-public-real-sanitize: tests/test_temporal_public_real.c tests/whisper_fixture.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer $< -o $@
build/test-temporal-engine-real-sanitize: tests/test_temporal_engine_real.c tests/whisper_fixture.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer $< -o $@
test-whisper: build/samosa-inference-whisper build/test-whisper
	$(abspath build/test-whisper) $(abspath build/samosa-inference-whisper)
sanitize-whisper: build/samosa-inference-whisper-sanitize build/test-whisper-sanitize
	$(abspath build/test-whisper-sanitize) $(abspath build/samosa-inference-whisper-sanitize)
build/samosa-inference-artifacts: src/artifact_main.c src/artifact.c src/install.c src/artifact.h src/sha256.h src/json.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) src/artifact_main.c src/artifact.c src/install.c -o $@
build:
	mkdir -p build
build/samosa-inference: src/main.c $(SOURCES) $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) src/main.c $(SOURCES) $(LIBS) -o $@
build/test-inference: tests/test_inference.c $(SOURCES) $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc tests/test_inference.c $(SOURCES) $(LIBS) -o $@
build/test-sessions: tests/test_sessions.c $(SOURCES) $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc tests/test_sessions.c $(SOURCES) $(LIBS) -o $@
build/test-admission: tests/test_admission.c $(SOURCES) $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DSAMOSA_INFERENCE_TEST_FAULTS -Isrc tests/test_admission.c $(SOURCES) $(LIBS) -o $@
test-admission: build/test-admission
	./build/test-admission
build/test-operations: tests/test_operations.c $(SOURCES) $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DSAMOSA_INFERENCE_TEST_FAULTS -Isrc tests/test_operations.c $(SOURCES) $(LIBS) -o $@
test-operations: build/test-operations
	./build/test-operations
build/test-artifact: tests/test_artifact.c src/artifact.c src/artifact.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc tests/test_artifact.c src/artifact.c -o $@
test-artifact: build/test-artifact
	./build/test-artifact
sanitize-artifact: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc tests/test_artifact.c src/artifact.c -o build/test-artifact-sanitize
	./build/test-artifact-sanitize
build/test-install: tests/test_install.c src/install.c src/artifact.c src/artifact.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DSAMOSA_INFERENCE_TEST_FAULTS -Isrc tests/test_install.c src/install.c src/artifact.c -o $@
test-install: build/test-install
	$(abspath build/test-install)
sanitize-install: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DSAMOSA_INFERENCE_TEST_FAULTS -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc tests/test_install.c src/install.c src/artifact.c -o build/test-install-sanitize
	$(abspath build/test-install-sanitize)
build/test-engine-process: tests/test_engine_process.c src/engine_process.c src/engine_process.h src/json.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc tests/test_engine_process.c src/engine_process.c -o $@
test-engine-process: build/test-engine-process
	$(abspath build/test-engine-process)
sanitize-engine-process: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc tests/test_engine_process.c src/engine_process.c -o build/test-engine-process-sanitize
	$(abspath build/test-engine-process-sanitize)
build/test-activation: tests/test_activation.c src/activation.c src/activation.h src/artifact.c src/artifact.h src/engine_process.c src/engine_process.h src/engine_runtime.c src/engine_runtime.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DSAMOSA_INFERENCE_TEST_FAULTS -Isrc tests/test_activation.c src/activation.c src/artifact.c src/engine_process.c src/engine_runtime.c $(LIBS) -o $@
test-activation: build/test-activation
	$(abspath build/test-activation)
sanitize-activation: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DSAMOSA_INFERENCE_TEST_FAULTS -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc tests/test_activation.c src/activation.c src/artifact.c src/engine_process.c src/engine_runtime.c $(LIBS) -o build/test-activation-sanitize
	$(abspath build/test-activation-sanitize)
test-artifacts-cli: build/samosa-inference-artifacts
	sh tests/artifacts_cli.sh
build/test-engine-runtime: tests/test_engine_runtime.c src/engine_runtime.c src/engine_runtime.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc tests/test_engine_runtime.c src/engine_runtime.c -o $@
test-engine-runtime: build/test-engine-runtime
	./build/test-engine-runtime
sanitize-engine-runtime: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc tests/test_engine_runtime.c src/engine_runtime.c -o build/test-engine-runtime-sanitize
	./build/test-engine-runtime-sanitize
build/test-managed-stream: tests/test_managed_stream.c src/managed_stream.c src/managed_stream.h src/json.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc tests/test_managed_stream.c src/managed_stream.c -pthread -o $@
test-managed-stream: build/test-managed-stream
	./build/test-managed-stream
sanitize-managed-stream: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DJSON_TOKENS_MAX=8192 -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc tests/test_managed_stream.c src/managed_stream.c -pthread -o build/test-managed-stream-sanitize
	./build/test-managed-stream-sanitize
test: all build/test-inference build/test-sessions build/test-admission build/test-operations test-artifact test-install test-artifacts-cli test-engine-process test-activation test-engine-runtime test-managed-stream
	./build/test-operations
	./build/test-admission
	./build/test-inference
	./build/test-sessions
sanitize: sanitize-artifact sanitize-install sanitize-engine-process sanitize-activation sanitize-engine-runtime sanitize-managed-stream | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DSAMOSA_INFERENCE_TEST_FAULTS -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc tests/test_operations.c $(SOURCES) $(LIBS) -o build/test-operations-sanitize
	./build/test-operations-sanitize
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DSAMOSA_INFERENCE_TEST_FAULTS -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc tests/test_admission.c $(SOURCES) $(LIBS) -o build/test-admission-sanitize
	./build/test-admission-sanitize
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc tests/test_inference.c $(SOURCES) $(LIBS) -o build/test-inference-sanitize
	./build/test-inference-sanitize
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc tests/test_sessions.c $(SOURCES) $(LIBS) -o build/test-sessions-sanitize
	./build/test-sessions-sanitize
release: test
	mkdir -p dist
	tar -czf dist/samosa-inference-0.1.0.tar.gz Makefile README.md DEPENDENCIES.md .gitignore src tests docs contracts
clean:
	find build dist -depth -delete
CURL_CFLAGS ?= $(shell curl-config --cflags)
CURL_LIBS ?= $(shell curl-config --libs)
NATIVE_FLAGS = -DSAMOSA_WITH_LLAMA -DJSON_TOKENS_MAX=8192 $(CURL_CFLAGS)
native: build/samosa-inference-llama build/samosa-inference-managed build/samosa-inference-download build/samosa-inference-setup
build/samosa-inference-download: src/artifact_main.c src/remote_artifact.c src/remote_artifact.h src/package_remote_cache.h src/package_assemble.h src/package_index.h src/catalog.h src/artifact.c src/install.c src/artifact.h src/sha256.h src/json.h src/private_key.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(CURL_CFLAGS) -DSAMOSA_REMOTE_ARTIFACTS src/artifact_main.c src/remote_artifact.c src/artifact.c src/install.c $(CURL_LIBS) -o $@
build/samosa-inference-managed: src/managed_main.c src/managed_stream.c src/managed_stream.h src/managed_provider.c src/managed_provider.h src/activation.c src/activation.h src/artifact.c src/artifact.h src/engine_runtime.c src/engine_runtime.h $(SOURCES) src/llama.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(NATIVE_FLAGS) src/managed_main.c src/managed_stream.c src/managed_provider.c src/activation.c src/artifact.c src/engine_runtime.c $(SOURCES) src/llama.c $(LIBS) $(CURL_LIBS) -pthread -o $@
build/samosa-inference-managed-sanitize: src/managed_main.c src/managed_stream.c src/managed_stream.h src/managed_provider.c src/managed_provider.h src/activation.c src/activation.h src/artifact.c src/artifact.h src/engine_runtime.c src/engine_runtime.h $(SOURCES) src/llama.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(NATIVE_FLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer src/managed_main.c src/managed_stream.c src/managed_provider.c src/activation.c src/artifact.c src/engine_runtime.c $(SOURCES) src/llama.c $(LIBS) $(CURL_LIBS) -pthread -o build/samosa-inference-managed-sanitize
build/samosa-inference-llama: src/main.c $(SOURCES) src/llama.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(NATIVE_FLAGS) src/main.c $(SOURCES) src/llama.c $(LIBS) $(CURL_LIBS) -pthread -o $@
build/samosa-inference-llama-sanitize: src/main.c $(SOURCES) src/llama.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(NATIVE_FLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer src/main.c $(SOURCES) src/llama.c $(LIBS) $(CURL_LIBS) -pthread -o $@
build/test-llama: tests/test_llama.c $(SOURCES) src/llama.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(NATIVE_FLAGS) -Isrc tests/test_llama.c $(SOURCES) src/llama.c $(LIBS) $(CURL_LIBS) -pthread -o $@
test-native: native build/test-llama test-remote-artifact test-remote-tls test-remote-key
	./build/test-llama
	sh tests/key_cli.sh
	sh tests/managed_cli.sh
build/test-remote-artifact: tests/test_remote_artifact.c src/package_remote_cache.c src/package_index.c src/catalog_host.c src/remote_artifact.c src/remote_artifact.h src/package_remote_cache.h src/package_assemble.h src/package_index.h src/catalog.h src/artifact.c src/install.c src/artifact.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(CURL_CFLAGS) -DSAMOSA_INFERENCE_TEST_FAULTS -Isrc tests/test_remote_artifact.c src/package_remote_cache.c src/package_index.c src/catalog_host.c src/remote_artifact.c src/artifact.c src/install.c $(CURL_LIBS) -o $@
test-remote-artifact: build/test-remote-artifact
	./build/test-remote-artifact
test-remote-tls: build/samosa-inference-download
	sh tests/remote_tls.sh
test-remote-key: build/samosa-inference-download
	sh tests/remote_key_cli.sh
sanitize-remote-artifact: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(CURL_CFLAGS) -DSAMOSA_INFERENCE_TEST_FAULTS -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc tests/test_remote_artifact.c src/package_remote_cache.c src/package_index.c src/catalog_host.c src/remote_artifact.c src/artifact.c src/install.c $(CURL_LIBS) -o build/test-remote-artifact-sanitize
	./build/test-remote-artifact-sanitize
sanitize-remote-cli: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(CURL_CFLAGS) -DSAMOSA_REMOTE_ARTIFACTS -fsanitize=address,undefined -fno-omit-frame-pointer src/artifact_main.c src/remote_artifact.c src/artifact.c src/install.c $(CURL_LIBS) -o build/samosa-inference-download-sanitize
	sh tests/remote_key_cli.sh ./build/samosa-inference-download-sanitize
	sh tests/remote_tls.sh ./build/samosa-inference-download-sanitize
sanitize-native: sanitize-remote-artifact sanitize-remote-cli build/test-llama-sanitize build/samosa-inference-managed-sanitize | build

	./build/test-llama-sanitize
	sh tests/managed_cli.sh ./build/samosa-inference-managed-sanitize
build/launch-engine: tests/launch_engine.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) tests/launch_engine.c -o $@
build/activation-native: tests/activation_native.c src/activation.c src/activation.h src/artifact.c src/artifact.h src/engine_runtime.c src/engine_runtime.h src/managed_provider.c src/managed_provider.h $(SOURCES) src/llama.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(NATIVE_FLAGS) -Isrc tests/activation_native.c src/activation.c src/artifact.c src/engine_runtime.c src/managed_provider.c $(SOURCES) src/llama.c $(LIBS) $(CURL_LIBS) -pthread -o $@

.PHONY: test-catalog sanitize-catalog
CATALOG_SOURCES = src/catalog.c src/catalog_file.c src/catalog_host.c
build/test-catalog: tests/test_catalog.c $(CATALOG_SOURCES) src/catalog.h src/artifact.h src/json.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc tests/test_catalog.c $(CATALOG_SOURCES) -o $@
test-catalog: build/test-catalog
	./build/test-catalog
sanitize-catalog: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc tests/test_catalog.c $(CATALOG_SOURCES) -o build/test-catalog-sanitize
	./build/test-catalog-sanitize

test: test-catalog
sanitize: sanitize-catalog

.PHONY: test-package-index sanitize-package-index
PACKAGE_INDEX_SOURCES = src/package_index.c src/catalog_host.c
build/test-package-index: tests/test_package_index.c $(PACKAGE_INDEX_SOURCES) src/package_index.h src/catalog.h src/platform.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc tests/test_package_index.c $(PACKAGE_INDEX_SOURCES) -o $@
test-package-index: build/test-package-index
	./build/test-package-index
sanitize-package-index: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Isrc tests/test_package_index.c $(PACKAGE_INDEX_SOURCES) -o build/test-package-index-sanitize
	./build/test-package-index-sanitize
test: test-package-index
sanitize: sanitize-package-index

.PHONY: test-package-tree sanitize-package-tree
PACKAGE_TREE_SOURCES = src/package_tree.c src/artifact.c $(PACKAGE_INDEX_SOURCES)
build/test-package-tree: tests/test_package_tree.c $(PACKAGE_TREE_SOURCES) src/package_tree.h src/package_index.h src/artifact.h src/catalog.h src/platform.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc tests/test_package_tree.c $(PACKAGE_TREE_SOURCES) -o $@
test-package-tree: build/test-package-tree
	./build/test-package-tree
sanitize-package-tree: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Isrc tests/test_package_tree.c $(PACKAGE_TREE_SOURCES) -o build/test-package-tree-sanitize
	./build/test-package-tree-sanitize
test: test-package-tree
sanitize: sanitize-package-tree

.PHONY: test-package-stage sanitize-package-stage
PACKAGE_STAGE_SOURCES = src/package_copy_record.c src/package_stage.c $(PACKAGE_TREE_SOURCES)
build/test-package-stage: tests/test_package_stage.c $(PACKAGE_STAGE_SOURCES) src/package_copy_record.h src/package_stage.h src/package_tree.h src/package_index.h src/artifact.h src/catalog.h src/platform.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc tests/test_package_stage.c $(PACKAGE_STAGE_SOURCES) -o $@
test-package-stage: build/test-package-stage
	./build/test-package-stage
sanitize-package-stage: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Isrc tests/test_package_stage.c $(PACKAGE_STAGE_SOURCES) -o build/test-package-stage-sanitize
	./build/test-package-stage-sanitize
test: test-package-stage
sanitize: sanitize-package-stage

.PHONY: test-package-publish sanitize-package-publish
PACKAGE_PUBLISH_SOURCES = src/package_publish.c $(PACKAGE_TREE_SOURCES)
build/test-package-publish: tests/test_package_publish.c $(PACKAGE_PUBLISH_SOURCES) src/package_publish.h src/package_tree.h src/package_index.h src/artifact.h src/catalog.h src/platform.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc tests/test_package_publish.c $(PACKAGE_PUBLISH_SOURCES) -o $@
test-package-publish: build/test-package-publish
	./build/test-package-publish
sanitize-package-publish: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Isrc tests/test_package_publish.c $(PACKAGE_PUBLISH_SOURCES) -o build/test-package-publish-sanitize
	./build/test-package-publish-sanitize
test: test-package-publish
sanitize: sanitize-package-publish


build/test-llama-sanitize: tests/test_llama.c $(SOURCES) src/llama.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(NATIVE_FLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc tests/test_llama.c $(SOURCES) src/llama.c $(LIBS) $(CURL_LIBS) $(DL_LIBS) -o $@

SUMMARY_SOURCES = src/summary_main.c src/summary.c src/summary_protocol.c src/artifact.c $(SOURCES)
.PHONY: summary
summary: build/samosa-inference-summary
build/samosa-inference-summary: $(SUMMARY_SOURCES) $(HEADERS) src/summary.h src/summary_protocol.h src/artifact.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DSAMOSA_WITH_SUMMARY $(SUMMARY_SOURCES) $(LIBS) -o $@
build/samosa-inference-summary-sanitize: $(SUMMARY_SOURCES) $(HEADERS) src/summary.h src/summary_protocol.h src/artifact.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DSAMOSA_WITH_SUMMARY -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer $(SUMMARY_SOURCES) $(LIBS) -o $@
.PHONY: test-summary sanitize-summary
build/test-summary: tests/test_summary.c tests/summary_fixture.h tests/whisper_fixture.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc tests/test_summary.c -o $@
build/test-summary-sanitize: tests/test_summary.c tests/summary_fixture.h tests/whisper_fixture.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_summary.c -o $@
test-summary: build/samosa-inference-summary build/test-summary test-summary-protocol
	$(abspath build/test-summary) $(abspath build/samosa-inference-summary)
sanitize-summary: build/samosa-inference-summary-sanitize build/test-summary-sanitize sanitize-summary-protocol
	$(abspath build/test-summary-sanitize) $(abspath build/samosa-inference-summary-sanitize)
build/test-summary-real: tests/test_summary_real.c tests/summary_fixture.h tests/whisper_fixture.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc tests/test_summary_real.c -o $@
build/test-summary-real-sanitize: tests/test_summary_real.c tests/summary_fixture.h tests/whisper_fixture.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_summary_real.c -o $@

# Speech keeps the existing Sherpa C ABI and has no linker-time model/runtime
# dependency. The host verifies and isolates the external package separately.
SPEECH_LOADER_SOURCES = src/speech_package_loader.c src/library_runtime.c
SPEECH_LOADER_HEADERS = src/speech_package_loader.h src/library_runtime.h
SPEECH_PACKAGE_SOURCES = src/speech_package.c src/artifact.c
SPEECH_PACKAGE_HEADERS = src/speech_package.h src/speech_runtime.h src/artifact.h src/json.h src/sha256.h
SPEECH_RUNTIME_SOURCES = src/speech_runtime.c src/engine_runtime.c
SPEECH_RUNTIME_HEADERS = src/speech_runtime.h src/sherpa_abi.h src/engine_runtime.h src/json.h src/sha256.h
DL_LIBS = $(if $(filter Linux,$(shell uname -s)),-ldl,)
.PHONY: test-speech-runtime sanitize-speech-runtime
build/fake-speech-native.so: tests/fake_speech_native.c src/sherpa_abi.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc -shared -fPIC $< -o $@
build/test-speech-runtime: tests/test_speech_runtime.c $(SPEECH_RUNTIME_SOURCES) $(SPEECH_RUNTIME_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc $< $(SPEECH_RUNTIME_SOURCES) $(DL_LIBS) -o $@
test-speech-runtime: build/test-speech-runtime build/fake-speech-native.so
	./build/test-speech-runtime $(abspath build/fake-speech-native.so)
sanitize-speech-runtime: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -shared -fPIC tests/fake_speech_native.c -o build/fake-speech-native-sanitize.so
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_speech_runtime.c $(SPEECH_RUNTIME_SOURCES) $(DL_LIBS) -o build/test-speech-runtime-sanitize
	./build/test-speech-runtime-sanitize $(abspath build/fake-speech-native-sanitize.so)
build/test-speech-real: tests/test_speech_real.c $(SPEECH_RUNTIME_SOURCES) $(SPEECH_RUNTIME_HEADERS) $(SPEECH_PACKAGE_SOURCES) $(SPEECH_PACKAGE_HEADERS) $(SPEECH_LOADER_SOURCES) $(SPEECH_LOADER_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc $< $(SPEECH_RUNTIME_SOURCES) $(SPEECH_PACKAGE_SOURCES) $(SPEECH_LOADER_SOURCES) $(DL_LIBS) -o $@
build/test-speech-real-sanitize: tests/test_speech_real.c $(SPEECH_RUNTIME_SOURCES) $(SPEECH_RUNTIME_HEADERS) $(SPEECH_PACKAGE_SOURCES) $(SPEECH_PACKAGE_HEADERS) $(SPEECH_LOADER_SOURCES) $(SPEECH_LOADER_HEADERS) | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -DJSON_TOKENS_MAX=8192 $< $(SPEECH_RUNTIME_SOURCES) $(SPEECH_PACKAGE_SOURCES) $(SPEECH_LOADER_SOURCES) $(DL_LIBS) -o $@

test: test-speech-runtime
sanitize: sanitize-speech-runtime

.PHONY: test-speech-package sanitize-speech-package
build/test-speech-package: tests/test_speech_package.c $(SPEECH_PACKAGE_SOURCES) $(SPEECH_PACKAGE_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc $< $(SPEECH_PACKAGE_SOURCES) -o $@
test-speech-package: build/test-speech-package
	./build/test-speech-package
sanitize-speech-package: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_speech_package.c $(SPEECH_PACKAGE_SOURCES) -o build/test-speech-package-sanitize
	./build/test-speech-package-sanitize
test: test-speech-package
sanitize: sanitize-speech-package

.PHONY: test-package-compatibility sanitize-package-compatibility
PACKAGE_COMPATIBILITY_SOURCES = src/molmo_package.c src/molmo_package_descriptor.c src/temporal_package.c src/vision_package.c src/whisper_package.c src/package_remove.c src/package_removal_record.c src/package_lease.c src/llama_package.c src/llama_package_descriptor.c src/gguf_metadata.c src/package_native_runtime.c src/package_catalog.c src/package_identity.c src/package_reserve.c src/package_cache.c src/install.c src/package_assemble.c src/package_compatibility.c src/package_copy_record.c src/package_stage.c src/package_tree.c src/package_publish.c $(PACKAGE_INDEX_SOURCES) $(SPEECH_PACKAGE_SOURCES) $(SPEECH_LOADER_SOURCES) src/engine_runtime.c
build/test-package-compatibility: tests/test_package_compatibility.c $(PACKAGE_COMPATIBILITY_SOURCES) src/package_catalog.h src/package_identity.h src/package_reserve.h src/package_cache.h src/package_assemble.h src/package_compatibility.h src/package_copy_record.h src/package_stage.h src/package_tree.h src/package_publish.h src/package_index.h src/catalog.h src/platform.h $(SPEECH_PACKAGE_HEADERS) $(SPEECH_LOADER_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc tests/test_package_compatibility.c $(PACKAGE_COMPATIBILITY_SOURCES) -o $@
test-package-compatibility: build/test-package-compatibility build/test-speech-worker build/speech-worker-fixture.so
	./build/test-package-compatibility
	python3 tests/package_compatibility_fixture.py $(abspath build/test-package-compatibility) $(abspath build/test-speech-worker) $(abspath build/speech-worker-fixture.so)
sanitize-package-compatibility: build/test-speech-worker build/speech-worker-fixture.so | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DJSON_TOKENS_MAX=8192 -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Isrc tests/test_package_compatibility.c $(PACKAGE_COMPATIBILITY_SOURCES) -o build/test-package-compatibility-sanitize
	./build/test-package-compatibility-sanitize
	python3 tests/package_compatibility_fixture.py $(abspath build/test-package-compatibility-sanitize) $(abspath build/test-speech-worker) $(abspath build/speech-worker-fixture.so)
test: test-package-compatibility
sanitize: sanitize-package-compatibility

LIBRARY_RUNTIME_SOURCES = src/library_runtime.c src/engine_runtime.c
LIBRARY_RUNTIME_HEADERS = src/library_runtime.h src/engine_runtime.h
.PHONY: test-library-runtime sanitize-library-runtime test-speech-loader sanitize-speech-loader
build/test-library-runtime: tests/test_library_runtime.c tests/library_fixture.h $(LIBRARY_RUNTIME_SOURCES) $(LIBRARY_RUNTIME_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc $< $(LIBRARY_RUNTIME_SOURCES) -o $@
test-library-runtime: build/test-library-runtime
	./build/test-library-runtime
sanitize-library-runtime: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_library_runtime.c $(LIBRARY_RUNTIME_SOURCES) -o build/test-library-runtime-sanitize
	./build/test-library-runtime-sanitize
build/test-speech-loader: tests/test_speech_loader.c tests/library_fixture.h $(SPEECH_LOADER_SOURCES) $(SPEECH_PACKAGE_SOURCES) src/engine_runtime.c $(SPEECH_LOADER_HEADERS) $(SPEECH_PACKAGE_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc $< $(SPEECH_LOADER_SOURCES) $(SPEECH_PACKAGE_SOURCES) src/engine_runtime.c -o $@
test-speech-loader: build/test-speech-loader
	./build/test-speech-loader
sanitize-speech-loader: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_speech_loader.c $(SPEECH_LOADER_SOURCES) $(SPEECH_PACKAGE_SOURCES) src/engine_runtime.c -o build/test-speech-loader-sanitize
	./build/test-speech-loader-sanitize
test: test-library-runtime test-speech-loader
sanitize: sanitize-library-runtime sanitize-speech-loader

SPEECH_WORKER_SOURCES = src/speech_worker.c src/speech_runtime.c src/speech_package_loader.c src/library_runtime.c src/speech_package.c src/artifact.c src/engine_runtime.c src/engine_process.c
SPEECH_WORKER_HEADERS = src/speech_worker.h src/speech_runtime.h src/sherpa_abi.h src/speech_package_loader.h src/library_runtime.h src/speech_package.h src/artifact.h src/engine_runtime.h src/engine_process.h src/json.h src/sha256.h
.PHONY: speech-worker
speech-worker: build/samosa-inference-speech-worker
build/samosa-inference-speech-worker: src/speech_worker_main.c $(SPEECH_WORKER_SOURCES) $(SPEECH_WORKER_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc $< $(SPEECH_WORKER_SOURCES) $(DL_LIBS) -o $@
build/samosa-inference-speech-worker-sanitize: src/speech_worker_main.c $(SPEECH_WORKER_SOURCES) $(SPEECH_WORKER_HEADERS) | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer $< $(SPEECH_WORKER_SOURCES) $(DL_LIBS) -o $@

# The external fixture is intentionally unsanitized in the closed-package gate,
# as actual Sherpa libraries are. Primitive ABI tests also sanitize the fixture.
SPEECH_FIXTURE_LINK = $(if $(filter Darwin,$(shell uname -s)),-Wl$(comma)-install_name$(comma)@rpath/libspeech-fixture.so -Wl$(comma)-rpath$(comma)@loader_path,-Wl$(comma)-soname$(comma)libspeech-fixture.so -Wl$(comma)-rpath$(comma)'$$ORIGIN')
comma := ,
.PHONY: test-speech-worker sanitize-speech-worker
build/speech-worker-fixture.so: tests/fake_speech_native.c src/sherpa_abi.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc -shared -fPIC $< $(SPEECH_FIXTURE_LINK) -o $@
build/test-speech-worker: tests/test_speech_worker.c $(SPEECH_WORKER_SOURCES) $(SPEECH_WORKER_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc $< $(SPEECH_WORKER_SOURCES) $(DL_LIBS) -o $@
test-speech-worker: build/test-speech-worker build/speech-worker-fixture.so
	$(abspath build/test-speech-worker) $(abspath build/speech-worker-fixture.so)
sanitize-speech-worker: build/speech-worker-fixture.so | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_speech_worker.c $(SPEECH_WORKER_SOURCES) $(DL_LIBS) -o build/test-speech-worker-sanitize
	$(abspath build/test-speech-worker-sanitize) $(abspath build/speech-worker-fixture.so)
test: test-speech-worker
sanitize: sanitize-speech-worker
build/test-speech-process-real: tests/test_speech_process_real.c $(SPEECH_WORKER_SOURCES) $(SPEECH_WORKER_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc $< $(SPEECH_WORKER_SOURCES) $(DL_LIBS) -o $@
build/test-speech-process-real-sanitize: tests/test_speech_process_real.c $(SPEECH_WORKER_SOURCES) $(SPEECH_WORKER_HEADERS) | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer $< $(SPEECH_WORKER_SOURCES) $(DL_LIBS) -o $@

AUDIO_STORE_SOURCES = src/audio_store.c $(SPEECH_WORKER_SOURCES)
.PHONY: test-audio-store sanitize-audio-store
build/test-audio-store: tests/test_audio_store.c $(AUDIO_STORE_SOURCES) src/audio_store.h $(SPEECH_WORKER_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc $< $(AUDIO_STORE_SOURCES) $(DL_LIBS) -lsqlite3 -o $@
test-audio-store: build/test-audio-store
	./build/test-audio-store
sanitize-audio-store: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_audio_store.c $(AUDIO_STORE_SOURCES) $(DL_LIBS) -lsqlite3 -o build/test-audio-store-sanitize
	./build/test-audio-store-sanitize
test: test-audio-store
sanitize: sanitize-audio-store

# Decision postprocessing is model-free here. The separate reference executable
# accepts a pinned actual-model capture and never downloads checkpoint assets.
.PHONY: test-decision-evidence sanitize-decision-evidence
build/test-decision-evidence: tests/test_decision_evidence.cpp engines/decision/text_evidence.cpp engines/decision/text_evidence.h | build
	$(CXX) -O2 -std=c++20 -Wall -Wextra -Werror -Wpedantic -Iengines engines/decision/text_evidence.cpp tests/test_decision_evidence.cpp -o $@
build/test-decision-evidence-reference: tests/test_decision_evidence_reference.cpp engines/decision/text_evidence.cpp engines/decision/text_evidence.h engines/json.h src/sha256.h | build
	$(CXX) -O2 -std=c++20 -Wall -Wextra -Werror -Wpedantic -Iengines -Isrc engines/decision/text_evidence.cpp tests/test_decision_evidence_reference.cpp -o $@
build/decision-native-text.o: engines/decision/native_text.cpp engines/decision/native_text.h engines/decision/ort_nli.h engines/decision/text_evidence.h engines/decision/tokenizer/tokenizer.h engines/json.h | build
	$(CXX) -O2 -std=c++20 -Wall -Wextra -Werror -Wpedantic -Iengines -c engines/decision/native_text.cpp -o $@
build/decision-native-text-real-harness.o: tests/test_decision_evidence_reference.cpp engines/decision/native_text.h engines/decision/ort_nli.h engines/decision/text_evidence.h engines/json.h src/sha256.h | build
	$(CXX) -O2 -std=c++20 -Wall -Wextra -Werror -Wpedantic -DSAMOSA_DECISION_NATIVE_REAL -Iengines -Isrc -c tests/test_decision_evidence_reference.cpp -o $@
build/decision-native-json-real-harness.o: tests/test_decision_evidence_reference.cpp engines/decision/native_text.h engines/decision/ort_nli.h engines/decision/text_evidence.h engines/decision/request.h engines/json.h src/sha256.h | build
	$(CXX) -O2 -std=c++20 -Wall -Wextra -Werror -Wpedantic -DSAMOSA_DECISION_NATIVE_REAL -DSAMOSA_DECISION_JSON_REAL -Iengines -Isrc -c tests/test_decision_evidence_reference.cpp -o $@
test-decision-evidence: build/test-decision-evidence build/test-decision-evidence-reference build/decision-native-text.o build/decision-native-text-real-harness.o build/decision-native-json-real-harness.o
	./build/test-decision-evidence
sanitize-decision-evidence: | build
	$(CXX) -O1 -g -std=c++20 -Wall -Wextra -Werror -Wpedantic -Iengines -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer engines/decision/text_evidence.cpp tests/test_decision_evidence.cpp -o build/test-decision-evidence-sanitize
	./build/test-decision-evidence-sanitize
test: test-decision-evidence
sanitize: sanitize-decision-evidence

DECISION_RUST_LIBS = $(if $(filter Darwin,$(shell uname -s)),-framework Security -framework CoreFoundation -liconv -lresolv,-ldl -lpthread -lm)
DECISION_REQUEST_SOURCES = engines/decision/request.cpp engines/decision/text_evidence.cpp tests/test_decision_request.cpp
DECISION_REQUEST_HEADERS = engines/decision/request.h engines/decision/text_evidence.h engines/decision/tokenizer/request.h
$(DECISION_TOKENIZER_ARCHIVE): engines/decision/tokenizer/Cargo.toml engines/decision/tokenizer/Cargo.lock $(wildcard engines/decision/tokenizer/src/*.rs)
	cargo +1.90.0 build --locked --release --manifest-path engines/decision/tokenizer/Cargo.toml --target-dir "$(DECISION_TOKENIZER_TARGET)"
.PHONY: test-decision-request sanitize-decision-request
build/test-decision-request: $(DECISION_REQUEST_SOURCES) $(DECISION_REQUEST_HEADERS) $(DECISION_TOKENIZER_ARCHIVE) | build
	$(CXX) -O2 -std=c++20 -Wall -Wextra -Werror -Wpedantic -Iengines $(DECISION_REQUEST_SOURCES) "$(DECISION_TOKENIZER_ARCHIVE)" $(DECISION_RUST_LIBS) -o $@
test-decision-request: build/test-decision-request
	./build/test-decision-request
sanitize-decision-request: $(DECISION_TOKENIZER_ARCHIVE) | build
	$(CXX) -O1 -g -std=c++20 -Wall -Wextra -Werror -Wpedantic -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Iengines $(DECISION_REQUEST_SOURCES) "$(DECISION_TOKENIZER_ARCHIVE)" $(DECISION_RUST_LIBS) -o build/test-decision-request-sanitize
	./build/test-decision-request-sanitize
test: test-decision-request
sanitize: sanitize-decision-request

DECISION_TRANSPORT_SOURCES = engines/decision/transport.cpp engines/decision/request.cpp engines/decision/text_evidence.cpp tests/decision_transport_fixture.cpp
.PHONY: test-decision-transport sanitize-decision-transport
build/decision-worker-main.o: engines/decision/main.cpp engines/decision/native_text.h engines/decision/request.h engines/decision/transport.h | build
	$(CXX) -O2 -std=c++20 -Wall -Wextra -Werror -Wpedantic -Iengines -c engines/decision/main.cpp -o $@
build/decision-transport-fixture: $(DECISION_TRANSPORT_SOURCES) $(DECISION_REQUEST_HEADERS) engines/decision/transport.h $(DECISION_TOKENIZER_ARCHIVE) | build
	$(CXX) -O2 -std=c++20 -Wall -Wextra -Werror -Wpedantic -Iengines $(DECISION_TRANSPORT_SOURCES) "$(DECISION_TOKENIZER_ARCHIVE)" $(DECISION_RUST_LIBS) -o $@
test-decision-transport: build/decision-transport-fixture build/decision-worker-main.o
	python3 tests/test_decision_transport.py ./build/decision-transport-fixture
sanitize-decision-transport: $(DECISION_TOKENIZER_ARCHIVE) | build
	$(CXX) -O1 -g -std=c++20 -Wall -Wextra -Werror -Wpedantic -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Iengines $(DECISION_TRANSPORT_SOURCES) "$(DECISION_TOKENIZER_ARCHIVE)" $(DECISION_RUST_LIBS) -o build/decision-transport-fixture-sanitize
	python3 tests/test_decision_transport.py ./build/decision-transport-fixture-sanitize
test: test-decision-transport
sanitize: sanitize-decision-transport

DECISION_ASSET_SOURCES = src/decision_assets.c src/artifact.c src/library_runtime.c src/engine_runtime.c
.PHONY: test-decision-assets sanitize-decision-assets
build/test-decision-assets: tests/test_decision_assets.c tests/library_fixture.h $(DECISION_ASSET_SOURCES) src/decision_assets.h src/artifact.h src/library_runtime.h src/engine_runtime.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc -Itests tests/test_decision_assets.c $(DECISION_ASSET_SOURCES) -o $@
build/test-decision-assets-real: tests/test_decision_assets_real.c $(DECISION_ASSET_SOURCES) src/decision_assets.h src/artifact.h src/library_runtime.h src/engine_runtime.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc tests/test_decision_assets_real.c $(DECISION_ASSET_SOURCES) -o $@
test-decision-assets: build/test-decision-assets build/test-decision-assets-real
	./build/test-decision-assets
sanitize-decision-assets: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Isrc -Itests tests/test_decision_assets.c $(DECISION_ASSET_SOURCES) -o build/test-decision-assets-sanitize
	./build/test-decision-assets-sanitize
test: test-decision-assets
sanitize: sanitize-decision-assets

# Explicit pinned SDK/runtime inputs; neither CI nor this rule downloads a model.
DECISION_WORKER_SOURCES = engines/decision/main.cpp engines/decision/transport.cpp engines/decision/request.cpp engines/decision/text_evidence.cpp engines/decision/native_text.cpp engines/decision/ort_nli.cpp
DECISION_WORKER_HEADERS = $(DECISION_REQUEST_HEADERS) engines/decision/transport.h engines/decision/native_text.h engines/decision/ort_nli.h engines/decision/ort_runtime.h engines/decision/tokenizer/tokenizer.h engines/json.h
DECISION_ORT_RPATH ?= $(if $(filter Darwin,$(shell uname -s)),@loader_path,$$ORIGIN)
DECISION_IMAGE_ORT_SOURCES = tests/test_decision_image_ort_real.cpp engines/decision/ort_image.cpp
DECISION_IMAGE_ORT_HEADERS = engines/decision/ort_image.h engines/decision/ort_runtime.h engines/json.h src/sha256.h
build/test-decision-image-ort-real: Makefile $(DECISION_IMAGE_ORT_SOURCES) $(DECISION_IMAGE_ORT_HEADERS) | build
	test -d "$(DECISION_ORT_INCLUDE)" && test -f "$(DECISION_ORT_LIBRARY)"
	$(CXX) -O2 -std=c++20 -Wall -Wextra -Werror -Wpedantic -Iengines -Isrc -I"$(DECISION_ORT_INCLUDE)" $(DECISION_IMAGE_ORT_SOURCES) "$(DECISION_ORT_LIBRARY)" -Xlinker -rpath -Xlinker '$(DECISION_ORT_RPATH)' -o $@
build/test-decision-image-ort-real-sanitize: Makefile $(DECISION_IMAGE_ORT_SOURCES) $(DECISION_IMAGE_ORT_HEADERS) | build
	test -d "$(DECISION_ORT_INCLUDE)" && test -f "$(DECISION_ORT_LIBRARY)"
	$(CXX) -O1 -g -std=c++20 -Wall -Wextra -Werror -Wpedantic -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Iengines -Isrc -I"$(DECISION_ORT_INCLUDE)" $(DECISION_IMAGE_ORT_SOURCES) "$(DECISION_ORT_LIBRARY)" -Xlinker -rpath -Xlinker '$(DECISION_ORT_RPATH)' -o $@
build/samosa-decision-engine: Makefile $(DECISION_WORKER_SOURCES) $(DECISION_WORKER_HEADERS) $(DECISION_TOKENIZER_ARCHIVE) | build
	test -d "$(DECISION_ORT_INCLUDE)" && test -f "$(DECISION_ORT_LIBRARY)"
	$(CXX) -O2 -std=c++20 -Wall -Wextra -Werror -Wpedantic -Iengines -I"$(DECISION_ORT_INCLUDE)" $(DECISION_WORKER_SOURCES) "$(DECISION_TOKENIZER_ARCHIVE)" "$(DECISION_ORT_LIBRARY)" $(DECISION_RUST_LIBS) -Xlinker -rpath -Xlinker '$(DECISION_ORT_RPATH)' -o $@
build/samosa-decision-engine-sanitize: Makefile $(DECISION_WORKER_SOURCES) $(DECISION_WORKER_HEADERS) $(DECISION_TOKENIZER_ARCHIVE) | build
	test -d "$(DECISION_ORT_INCLUDE)" && test -f "$(DECISION_ORT_LIBRARY)"
	$(CXX) -O1 -g -std=c++20 -Wall -Wextra -Werror -Wpedantic -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Iengines -I"$(DECISION_ORT_INCLUDE)" $(DECISION_WORKER_SOURCES) "$(DECISION_TOKENIZER_ARCHIVE)" "$(DECISION_ORT_LIBRARY)" $(DECISION_RUST_LIBS) -Xlinker -rpath -Xlinker '$(DECISION_ORT_RPATH)' -o $@

.PHONY: test-speech-public sanitize-speech-public
test-speech-public: build/samosa-inference-providers build/test-speech-worker build/speech-worker-fixture.so
	python3 tests/test_speech_public.py $(abspath build/samosa-inference-providers) $(abspath build/test-speech-worker) $(abspath build/speech-worker-fixture.so)
sanitize-speech-public: build/samosa-inference-providers-sanitize build/test-speech-worker build/speech-worker-fixture.so
	python3 tests/test_speech_public.py $(abspath build/samosa-inference-providers-sanitize) $(abspath build/test-speech-worker) $(abspath build/speech-worker-fixture.so)


.PHONY: test-package-remote sanitize-package-remote
PACKAGE_REMOTE_SOURCES = src/catalog_file.c src/catalog.c src/package_installer.c src/package_remote_cache.c src/remote_artifact.c $(PACKAGE_COMPATIBILITY_SOURCES)
build/test-package-remote: tests/test_package_compatibility.c $(PACKAGE_REMOTE_SOURCES) src/package_installer.h src/remote_artifact.h src/artifact.h src/package_remote_cache.h src/package_catalog.h src/package_identity.h src/package_reserve.h src/package_cache.h src/package_assemble.h src/package_compatibility.h src/package_copy_record.h src/package_stage.h src/package_tree.h src/package_publish.h src/package_index.h src/catalog.h src/platform.h $(SPEECH_PACKAGE_HEADERS) $(SPEECH_LOADER_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(CURL_CFLAGS) -DSAMOSA_PACKAGE_REMOTE_TESTS -DJSON_TOKENS_MAX=8192 -Isrc tests/test_package_compatibility.c $(PACKAGE_REMOTE_SOURCES) $(CURL_LIBS) -o $@
test-package-remote: build/test-package-remote build/test-speech-worker build/speech-worker-fixture.so
	python3 tests/package_compatibility_fixture.py $(abspath build/test-package-remote) $(abspath build/test-speech-worker) $(abspath build/speech-worker-fixture.so) --https
sanitize-package-remote: build/test-speech-worker build/speech-worker-fixture.so | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(CURL_CFLAGS) -DSAMOSA_PACKAGE_REMOTE_TESTS -DJSON_TOKENS_MAX=8192 -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Isrc tests/test_package_compatibility.c $(PACKAGE_REMOTE_SOURCES) $(CURL_LIBS) -o build/test-package-remote-sanitize
	python3 tests/package_compatibility_fixture.py $(abspath build/test-package-remote-sanitize) $(abspath build/test-speech-worker) $(abspath build/speech-worker-fixture.so) --https
test-native: test-package-remote
sanitize-native: sanitize-package-remote

.PHONY: test-package-reserve sanitize-package-reserve
build/test-package-reserve: tests/test_package_reserve.c src/package_reserve.c src/package_reserve.h src/package_index.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc tests/test_package_reserve.c src/package_reserve.c -o $@
test-package-reserve: build/test-package-reserve
	./build/test-package-reserve
sanitize-package-reserve: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Isrc tests/test_package_reserve.c src/package_reserve.c -o build/test-package-reserve-sanitize
	./build/test-package-reserve-sanitize
test: test-package-reserve
sanitize: sanitize-package-reserve

.PHONY: test-package-identity sanitize-package-identity
build/test-package-identity: tests/test_package_identity.c src/package_identity.c src/package_identity.h src/package_index.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc tests/test_package_identity.c src/package_identity.c -o $@
test-package-identity: build/test-package-identity
	./build/test-package-identity
sanitize-package-identity: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Isrc tests/test_package_identity.c src/package_identity.c -o build/test-package-identity-sanitize
	./build/test-package-identity-sanitize
test: test-package-identity
sanitize: sanitize-package-identity

# Define setup after complete package source groups so make tracks every input.
SETUP_SOURCES = src/setup_main.c $(PACKAGE_REMOTE_SOURCES)
SETUP_HEADERS = src/molmo_package.h src/molmo_package_descriptor.h src/temporal_package.h src/vision_package.h src/whisper_package.h src/llama_package.h src/llama_package_descriptor.h src/gguf_metadata.h src/catalog.h src/artifact.h src/remote_artifact.h src/json.h src/private_key.h src/sha256.h $(wildcard src/package*.h) $(SPEECH_PACKAGE_HEADERS) $(SPEECH_LOADER_HEADERS) src/library_runtime.h src/engine_runtime.h src/platform.h
build/samosa-inference-setup: $(SETUP_SOURCES) $(SETUP_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(CURL_CFLAGS) $(SETUP_SOURCES) $(CURL_LIBS) -o $@

.PHONY: test-setup sanitize-setup
test-setup: build/test-molmo-package build/test-vision-package build/test-whisper-package build/test-package-native-runtime build/samosa-inference-setup build/test-package-remote build/test-speech-worker build/speech-worker-fixture.so
	sh tests/setup_cli.sh
	python3 tests/molmo_setup_fixture.py build/samosa-inference-setup build/test-molmo-package
	python3 tests/vision_setup_fixture.py build/samosa-inference-setup build/test-vision-package
	python3 tests/whisper_setup_fixture.py build/samosa-inference-setup build/test-whisper-package
	python3 tests/llama_setup_fixture.py build/samosa-inference-setup build/test-package-native-runtime
	python3 tests/package_compatibility_fixture.py build/test-package-remote build/test-speech-worker build/speech-worker-fixture.so --https --setup build/samosa-inference-setup
sanitize-setup: build/test-molmo-package build/test-vision-package build/test-whisper-package build/test-package-native-runtime build/test-package-remote build/test-speech-worker build/speech-worker-fixture.so | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(CURL_CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer $(SETUP_SOURCES) $(CURL_LIBS) -o build/samosa-inference-setup-sanitize
	sh tests/setup_cli.sh ./build/samosa-inference-setup-sanitize
	python3 tests/molmo_setup_fixture.py build/samosa-inference-setup-sanitize build/test-molmo-package
	python3 tests/vision_setup_fixture.py build/samosa-inference-setup-sanitize build/test-vision-package
	python3 tests/whisper_setup_fixture.py build/samosa-inference-setup-sanitize build/test-whisper-package
	python3 tests/llama_setup_fixture.py build/samosa-inference-setup-sanitize build/test-package-native-runtime
	python3 tests/package_compatibility_fixture.py build/test-package-remote build/test-speech-worker build/speech-worker-fixture.so --https --setup build/samosa-inference-setup-sanitize
test-native: test-setup
sanitize-native: sanitize-setup

.PHONY: test-package-native-runtime sanitize-package-native-runtime
PACKAGE_NATIVE_RUNTIME_SOURCES = src/llama_package.c src/llama_package_descriptor.c src/gguf_metadata.c src/package_native_runtime.c $(PACKAGE_TREE_SOURCES) $(LIBRARY_RUNTIME_SOURCES)
build/test-package-native-runtime: tests/test_package_native_runtime.c tests/library_fixture.h $(PACKAGE_NATIVE_RUNTIME_SOURCES) src/llama_package.h src/llama_package_descriptor.h src/gguf_metadata.h src/package_native_runtime.h $(LIBRARY_RUNTIME_HEADERS) src/package_tree.h src/package_index.h src/artifact.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc $< $(PACKAGE_NATIVE_RUNTIME_SOURCES) -o $@
test-package-native-runtime: build/test-package-native-runtime
	./build/test-package-native-runtime
sanitize-package-native-runtime: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_package_native_runtime.c $(PACKAGE_NATIVE_RUNTIME_SOURCES) -o build/test-package-native-runtime-sanitize
	./build/test-package-native-runtime-sanitize
test: test-package-native-runtime
sanitize: sanitize-package-native-runtime

.PHONY: test-gguf-metadata sanitize-gguf-metadata
build/test-gguf-metadata: tests/test_gguf_metadata.c src/gguf_metadata.c src/gguf_metadata.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc $< src/gguf_metadata.c -o $@
test-gguf-metadata: build/test-gguf-metadata
	./build/test-gguf-metadata
sanitize-gguf-metadata: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_gguf_metadata.c src/gguf_metadata.c -o build/test-gguf-metadata-sanitize
	./build/test-gguf-metadata-sanitize
test: test-gguf-metadata
sanitize: sanitize-gguf-metadata

.PHONY: test-llama-package-descriptor sanitize-llama-package-descriptor
LLAMA_DESCRIPTOR_SOURCES = src/llama_package_descriptor.c src/gguf_metadata.c $(PACKAGE_INDEX_SOURCES)
build/test-llama-package-descriptor: tests/test_llama_package_descriptor.c $(LLAMA_DESCRIPTOR_SOURCES) src/llama_package_descriptor.h src/gguf_metadata.h src/package_index.h src/json.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc $< $(LLAMA_DESCRIPTOR_SOURCES) -o $@
test-llama-package-descriptor: build/test-llama-package-descriptor
	./build/test-llama-package-descriptor
sanitize-llama-package-descriptor: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_llama_package_descriptor.c $(LLAMA_DESCRIPTOR_SOURCES) -o build/test-llama-package-descriptor-sanitize
	./build/test-llama-package-descriptor-sanitize
test: test-llama-package-descriptor
sanitize: sanitize-llama-package-descriptor

.PHONY: build-llama-install-tests
build/test-llama-install: tests/test_package_native_runtime.c $(PACKAGE_COMPATIBILITY_SOURCES) src/llama_package.h src/package_assemble.h src/package_compatibility.h tests/library_fixture.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DSAMOSA_LLAMA_INSTALL_TEST -Isrc $< $(PACKAGE_COMPATIBILITY_SOURCES) -o $@
build/test-llama-install-sanitize: tests/test_package_native_runtime.c $(PACKAGE_COMPATIBILITY_SOURCES) src/llama_package.h src/package_assemble.h src/package_compatibility.h tests/library_fixture.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DSAMOSA_LLAMA_INSTALL_TEST -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer $< $(PACKAGE_COMPATIBILITY_SOURCES) -o $@
build-llama-install-tests: build/test-llama-install build/test-llama-install-sanitize
test-native: build/test-llama-install
sanitize-native: build/test-llama-install-sanitize

.PHONY: test-package-lease sanitize-package-lease
PACKAGE_LEASE_SOURCES = src/package_lease.c src/package_identity.c
build/test-package-lease: tests/test_package_lease.c $(PACKAGE_LEASE_SOURCES) src/package_lease.h src/package_identity.h src/package_index.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc $< $(PACKAGE_LEASE_SOURCES) -o $@
test-package-lease: build/test-package-lease
	./build/test-package-lease
sanitize-package-lease: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_package_lease.c $(PACKAGE_LEASE_SOURCES) -o build/test-package-lease-sanitize
	./build/test-package-lease-sanitize
test: test-package-lease
sanitize: sanitize-package-lease

.PHONY: test-package-removal-record sanitize-package-removal-record
PACKAGE_REMOVAL_RECORD_SOURCES = src/package_removal_record.c $(PACKAGE_TREE_SOURCES)
build/test-package-removal-record: tests/test_package_removal_record.c $(PACKAGE_REMOVAL_RECORD_SOURCES) src/package_removal_record.h src/package_tree.h src/package_index.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc $< $(PACKAGE_REMOVAL_RECORD_SOURCES) -o $@
test-package-removal-record: build/test-package-removal-record
	./build/test-package-removal-record
sanitize-package-removal-record: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_package_removal_record.c $(PACKAGE_REMOVAL_RECORD_SOURCES) -o build/test-package-removal-record-sanitize
	./build/test-package-removal-record-sanitize
test: test-package-removal-record
sanitize: sanitize-package-removal-record

.PHONY: test-package-remove sanitize-package-remove
PACKAGE_REMOVE_SOURCES = src/package_stage.c src/package_copy_record.c src/package_remove.c src/package_reserve.c src/package_identity.c $(PACKAGE_REMOVAL_RECORD_SOURCES)
build/test-package-remove: tests/test_package_remove.c $(PACKAGE_REMOVE_SOURCES) src/package_stage.h src/package_copy_record.h src/package_remove.h src/package_removal_record.h src/package_identity.h src/package_tree.h src/package_index.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc $< $(PACKAGE_REMOVE_SOURCES) -o $@
build/test-package-remove-incomplete: tests/test_package_remove_incomplete.c $(PACKAGE_REMOVE_SOURCES) src/package_remove.h src/package_stage.h src/package_copy_record.h src/package_removal_record.h src/package_identity.h src/package_tree.h src/package_index.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc $< $(PACKAGE_REMOVE_SOURCES) -o $@
test-package-remove: build/test-package-remove build/test-package-remove-incomplete
	./build/test-package-remove
	./build/test-package-remove-incomplete
sanitize-package-remove: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_package_remove.c $(PACKAGE_REMOVE_SOURCES) -o build/test-package-remove-sanitize
	./build/test-package-remove-sanitize
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_package_remove_incomplete.c $(PACKAGE_REMOVE_SOURCES) -o build/test-package-remove-incomplete-sanitize
	./build/test-package-remove-incomplete-sanitize
test: test-package-remove
sanitize: sanitize-package-remove

# Redirect only operations.c's explicit filesystem sync in this regression.
# The test runs the real public cancellation API with an independent worker DB.
.PHONY: test-operations-cancel sanitize-operations-cancel
OPERATIONS_CANCEL_SOURCES = $(filter-out src/operations.c,$(SOURCES))
build/operations-cancel-test.o: src/operations.c src/operations.h $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Dfsync=operations_cancel_test_fsync -Isrc -c $< -o $@
build/test-operations-cancel: tests/test_operations_cancel.c build/operations-cancel-test.o $(OPERATIONS_CANCEL_SOURCES) $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc tests/test_operations_cancel.c build/operations-cancel-test.o $(OPERATIONS_CANCEL_SOURCES) $(LIBS) -o $@
test-operations-cancel: build/test-operations-cancel
	./build/test-operations-cancel
sanitize-operations-cancel: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Dfsync=operations_cancel_test_fsync -Isrc -c src/operations.c -o build/operations-cancel-test-sanitize.o
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Isrc tests/test_operations_cancel.c build/operations-cancel-test-sanitize.o $(OPERATIONS_CANCEL_SOURCES) $(LIBS) -o build/test-operations-cancel-sanitize
	./build/test-operations-cancel-sanitize
test: test-operations-cancel
sanitize: sanitize-operations-cancel

.PHONY: test-whisper-package sanitize-whisper-package
WHISPER_PACKAGE_SOURCES = src/whisper_package.c src/package_native_runtime.c $(PACKAGE_TREE_SOURCES) $(LIBRARY_RUNTIME_SOURCES)
build/test-whisper-package: tests/test_whisper_package.c tests/library_fixture.h $(WHISPER_PACKAGE_SOURCES) src/whisper_package.h src/package_native_runtime.h $(LIBRARY_RUNTIME_HEADERS) src/package_tree.h src/package_index.h src/artifact.h src/json.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc $< $(WHISPER_PACKAGE_SOURCES) -o $@
test-whisper-package: build/test-whisper-package
	./build/test-whisper-package
sanitize-whisper-package: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_whisper_package.c $(WHISPER_PACKAGE_SOURCES) -o build/test-whisper-package-sanitize
	./build/test-whisper-package-sanitize
test: test-whisper-package
sanitize: sanitize-whisper-package

.PHONY: test-vision-package sanitize-vision-package
VISION_PACKAGE_SOURCES = src/vision_package.c src/package_native_runtime.c $(PACKAGE_TREE_SOURCES) $(LIBRARY_RUNTIME_SOURCES)
build/test-vision-package: tests/test_vision_package.c tests/library_fixture.h $(VISION_PACKAGE_SOURCES) src/vision_package.h src/package_native_runtime.h $(LIBRARY_RUNTIME_HEADERS) src/package_tree.h src/package_index.h src/artifact.h src/json.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc $< $(VISION_PACKAGE_SOURCES) -o $@
test-vision-package: build/test-vision-package
	./build/test-vision-package
sanitize-vision-package: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_vision_package.c $(VISION_PACKAGE_SOURCES) -o build/test-vision-package-sanitize
	./build/test-vision-package-sanitize
test: test-vision-package
sanitize: sanitize-vision-package

.PHONY: test-molmo-package-descriptor sanitize-molmo-package-descriptor
MOLMO_DESCRIPTOR_SOURCES = src/molmo_package_descriptor.c $(PACKAGE_INDEX_SOURCES)
build/test-molmo-package-descriptor: tests/test_molmo_package_descriptor.c $(MOLMO_DESCRIPTOR_SOURCES) src/molmo_package_descriptor.h src/temporal_package.h src/package_index.h src/json.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc $< $(MOLMO_DESCRIPTOR_SOURCES) -o $@
test-molmo-package-descriptor: build/test-molmo-package-descriptor
	./build/test-molmo-package-descriptor
sanitize-molmo-package-descriptor: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_molmo_package_descriptor.c $(MOLMO_DESCRIPTOR_SOURCES) -o build/test-molmo-package-descriptor-sanitize
	./build/test-molmo-package-descriptor-sanitize
test: test-molmo-package-descriptor
sanitize: sanitize-molmo-package-descriptor

.PHONY: test-molmo-package sanitize-molmo-package
MOLMO_PACKAGE_SOURCES = src/molmo_package.c src/molmo_package_descriptor.c src/temporal_package.c src/package_native_runtime.c $(PACKAGE_TREE_SOURCES) $(LIBRARY_RUNTIME_SOURCES)
build/test-molmo-package: tests/test_molmo_package.c tests/library_fixture.h $(MOLMO_PACKAGE_SOURCES) src/molmo_package.h src/molmo_package_descriptor.h src/temporal_package.h src/package_native_runtime.h $(LIBRARY_RUNTIME_HEADERS) src/package_tree.h src/package_index.h src/artifact.h src/json.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc $< $(MOLMO_PACKAGE_SOURCES) -o $@
build/test-molmo-package-sanitize: tests/test_molmo_package.c tests/library_fixture.h $(MOLMO_PACKAGE_SOURCES) src/molmo_package.h src/molmo_package_descriptor.h src/temporal_package.h src/package_native_runtime.h $(LIBRARY_RUNTIME_HEADERS) src/package_tree.h src/package_index.h src/artifact.h src/json.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer $< $(MOLMO_PACKAGE_SOURCES) -o $@
test-molmo-package: build/test-molmo-package
	./build/test-molmo-package
sanitize-molmo-package: build/test-molmo-package-sanitize
	./build/test-molmo-package-sanitize
test: test-molmo-package
sanitize: sanitize-molmo-package

.PHONY: test-molmo-engine-components sanitize-molmo-engine-components
MOLMO_CONTRACT_HEADERS = engines/molmo2/molmo2_contract.h engines/json.h
MOLMO_PROCESSOR_HEADERS = engines/molmo2/molmo2_processor.h engines/molmo2/molmo2_model.h engines/stb_image.h
build/test-molmo2-contract: tests/test_molmo2_contract.cpp engines/molmo2/molmo2_contract.cpp $(MOLMO_CONTRACT_HEADERS) | build
	$(CXX) $(CPPFLAGS) -O2 -std=c++20 -Wall -Wextra -Werror -UNDEBUG -Iengines tests/test_molmo2_contract.cpp engines/molmo2/molmo2_contract.cpp -o $@
build/test-molmo2-processor: tests/test_molmo2_processor.cpp engines/molmo2/molmo2_processor.cpp $(MOLMO_PROCESSOR_HEADERS) | build
	$(CXX) $(CPPFLAGS) -O2 -std=c++20 -Wall -Wextra -Werror -Wno-unused-function -Iengines tests/test_molmo2_processor.cpp engines/molmo2/molmo2_processor.cpp -o $@
test-molmo-engine-components: build/test-molmo2-contract build/test-molmo2-processor
	./build/test-molmo2-contract
	./build/test-molmo2-processor
sanitize-molmo-engine-components: | build
	$(CXX) $(CPPFLAGS) -O1 -g -std=c++20 -Wall -Wextra -Werror -UNDEBUG -Iengines -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_molmo2_contract.cpp engines/molmo2/molmo2_contract.cpp -o build/test-molmo2-contract-sanitize
	./build/test-molmo2-contract-sanitize
	$(CXX) $(CPPFLAGS) -O1 -g -std=c++20 -Wall -Wextra -Werror -Wno-unused-function -Iengines -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_molmo2_processor.cpp engines/molmo2/molmo2_processor.cpp -o build/test-molmo2-processor-sanitize
	./build/test-molmo2-processor-sanitize
test: test-molmo-engine-components
sanitize: sanitize-molmo-engine-components

# Extend the existing decision transport fixture for public ownership/control
# gates. Tiny fake loader dependency and dummy model bytes, never actual-model
# evidence. Real model pins remain a separate explicit qualification runner.
DECISION_FIXTURE_RUNTIME_NAME = $(if $(filter Darwin,$(shell uname -s)),libonnxruntime.1.27.0.dylib,libonnxruntime.so.1.27.0)
DECISION_FIXTURE_RUNTIME = build/decision-public-loader/$(DECISION_FIXTURE_RUNTIME_NAME)
DECISION_FIXTURE_LINK = $(if $(filter Darwin,$(shell uname -s)),-Xlinker -install_name -Xlinker @rpath/$(DECISION_FIXTURE_RUNTIME_NAME),-Xlinker -soname -Xlinker $(DECISION_FIXTURE_RUNTIME_NAME))
$(DECISION_FIXTURE_RUNTIME): tests/decision_loader_fixture.c | build
	mkdir -p build/decision-public-loader
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -shared -fPIC $< $(DECISION_FIXTURE_LINK) -o $@
build/decision-public-fixture: $(DECISION_TRANSPORT_SOURCES) $(DECISION_REQUEST_HEADERS) engines/decision/transport.h $(DECISION_TOKENIZER_ARCHIVE) $(DECISION_FIXTURE_RUNTIME) | build
	$(CXX) -O2 -std=c++20 -Wall -Wextra -Werror -Wpedantic -DSAMOSA_DECISION_PUBLIC_FIXTURE -Iengines $(DECISION_TRANSPORT_SOURCES) "$(DECISION_TOKENIZER_ARCHIVE)" $(DECISION_FIXTURE_RUNTIME) $(DECISION_RUST_LIBS) -Xlinker -rpath -Xlinker '$(DECISION_ORT_RPATH)' -o $@
.PHONY: test-decision-public sanitize-decision-public
test-decision-public: build/samosa-inference-providers build/decision-public-fixture
	python3 tests/test_decision_controls.py $(abspath build/samosa-inference-providers) $(abspath build/decision-public-fixture) $(abspath $(DECISION_FIXTURE_RUNTIME))
sanitize-decision-public: build/samosa-inference-providers-sanitize build/decision-public-fixture
	python3 tests/test_decision_controls.py $(abspath build/samosa-inference-providers-sanitize) $(abspath build/decision-public-fixture) $(abspath $(DECISION_FIXTURE_RUNTIME))
