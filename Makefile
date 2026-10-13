CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -O2 -g
WARN = -std=c11 -Wall -Wextra -Werror -Wpedantic
LIBS = -lsqlite3
SOURCES = src/registry.c src/embedding.c src/inference.c src/sessions.c src/admission.c src/operations.c src/engine_process.c src/budget.c
HEADERS = src/registry.h src/embedding.h src/inference.h src/json.h src/llama.h src/sessions.h src/admission.h src/operations.h src/sha256.h src/engine_process.h src/budget.h
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
REGISTRY_FLAGS = $(NATIVE_FLAGS) -DSAMOSA_WITH_WHISPER -DSAMOSA_WITH_VISION -DSAMOSA_WITH_TEMPORAL -DSAMOSA_WITH_SUMMARY
REGISTRY_SOURCES = $(SOURCES) src/llama.c src/whisper.c src/vision.c src/visual_bridge.c src/temporal.c src/temporal_input.c src/temporal_package.c src/summary.c src/summary_protocol.c src/artifact.c
REGISTRY_HEADERS = $(HEADERS) src/registry_host.h src/whisper.h src/vision.h src/temporal.h src/temporal_package.h src/summary.h src/summary_protocol.h src/artifact.h src/private_key.h
.PHONY: providers
providers: build/samosa-inference-providers
build/samosa-inference-providers: src/registry_main.c src/registry_host.c $(REGISTRY_SOURCES) $(REGISTRY_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(REGISTRY_FLAGS) src/registry_main.c src/registry_host.c $(REGISTRY_SOURCES) $(LIBS) $(CURL_LIBS) -o $@
build/samosa-inference-providers-sanitize: src/registry_main.c src/registry_host.c $(REGISTRY_SOURCES) $(REGISTRY_HEADERS) | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(REGISTRY_FLAGS) -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer src/registry_main.c src/registry_host.c $(REGISTRY_SOURCES) $(LIBS) $(CURL_LIBS) -o $@
.PHONY: test-registry-cli sanitize-registry-cli
build/test-registry-cli: tests/test_registry_cli.c tests/whisper_fixture.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc tests/test_registry_cli.c -o $@
build/test-registry-cli-sanitize: tests/test_registry_cli.c tests/whisper_fixture.h src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_registry_cli.c -o $@
test-registry-cli: build/samosa-inference-providers build/test-registry-cli
	$(abspath build/test-registry-cli) $(abspath build/samosa-inference-providers)
sanitize-registry-cli: build/samosa-inference-providers-sanitize build/test-registry-cli-sanitize
	$(abspath build/test-registry-cli-sanitize) $(abspath build/samosa-inference-providers-sanitize)
build/test-registry-modalities: tests/test_registry.c tests/whisper_fixture.h $(REGISTRY_SOURCES) $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(REGISTRY_FLAGS) -Isrc tests/test_registry.c $(REGISTRY_SOURCES) $(LIBS) $(CURL_LIBS) -o $@
test-registry-modalities: build/test-registry-modalities
	./build/test-registry-modalities
sanitize-registry-modalities: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(REGISTRY_FLAGS) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_registry.c $(REGISTRY_SOURCES) $(LIBS) $(CURL_LIBS) -o build/test-registry-modalities-sanitize
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
build/samosa-inference-download: src/artifact_main.c src/remote_artifact.c src/remote_artifact.h src/artifact.c src/install.c src/artifact.h src/sha256.h src/json.h src/private_key.h | build
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
build/test-remote-artifact: tests/test_remote_artifact.c src/remote_artifact.c src/remote_artifact.h src/artifact.c src/install.c src/artifact.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(CURL_CFLAGS) -DSAMOSA_INFERENCE_TEST_FAULTS -Isrc tests/test_remote_artifact.c src/remote_artifact.c src/artifact.c src/install.c $(CURL_LIBS) -o $@
test-remote-artifact: build/test-remote-artifact
	./build/test-remote-artifact
test-remote-tls: build/samosa-inference-download
	sh tests/remote_tls.sh
test-remote-key: build/samosa-inference-download
	sh tests/remote_key_cli.sh
sanitize-remote-artifact: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(CURL_CFLAGS) -DSAMOSA_INFERENCE_TEST_FAULTS -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc tests/test_remote_artifact.c src/remote_artifact.c src/artifact.c src/install.c $(CURL_LIBS) -o build/test-remote-artifact-sanitize
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

SETUP_SOURCES = src/setup_main.c $(CATALOG_SOURCES) src/remote_artifact.c src/artifact.c src/install.c
SETUP_HEADERS = src/catalog.h src/artifact.h src/remote_artifact.h src/json.h src/private_key.h src/sha256.h
build/samosa-inference-setup: $(SETUP_SOURCES) $(SETUP_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(CURL_CFLAGS) $(SETUP_SOURCES) $(CURL_LIBS) -o $@

.PHONY: test-setup sanitize-setup
test-setup: build/samosa-inference-setup
	sh tests/setup_cli.sh
sanitize-setup: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(CURL_CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer $(SETUP_SOURCES) $(CURL_LIBS) -o build/samosa-inference-setup-sanitize
	sh tests/setup_cli.sh ./build/samosa-inference-setup-sanitize
test-native: test-setup
sanitize-native: sanitize-setup

build/test-llama-sanitize: tests/test_llama.c $(SOURCES) src/llama.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(NATIVE_FLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer -Isrc tests/test_llama.c $(SOURCES) src/llama.c $(LIBS) $(CURL_LIBS) -o $@

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
build/test-speech-real: tests/test_speech_real.c $(SPEECH_RUNTIME_SOURCES) $(SPEECH_RUNTIME_HEADERS) $(SPEECH_PACKAGE_SOURCES) $(SPEECH_PACKAGE_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc $< $(SPEECH_RUNTIME_SOURCES) $(SPEECH_PACKAGE_SOURCES) $(DL_LIBS) -o $@
build/test-speech-real-sanitize: tests/test_speech_real.c $(SPEECH_RUNTIME_SOURCES) $(SPEECH_RUNTIME_HEADERS) $(SPEECH_PACKAGE_SOURCES) $(SPEECH_PACKAGE_HEADERS) | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -DJSON_TOKENS_MAX=8192 $< $(SPEECH_RUNTIME_SOURCES) $(SPEECH_PACKAGE_SOURCES) $(DL_LIBS) -o $@

test: test-speech-runtime
sanitize: sanitize-speech-runtime

.PHONY: test-speech-package sanitize-speech-package
build/test-speech-package: tests/test_speech_package.c $(SPEECH_PACKAGE_SOURCES) $(SPEECH_PACKAGE_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc $< $(SPEECH_PACKAGE_SOURCES) -o $@
test-speech-package: build/test-speech-package
	./build/test-speech-package
sanitize-speech-package: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -DJSON_TOKENS_MAX=8192 -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_speech_package.c $(SPEECH_PACKAGE_SOURCES) -o build/test-speech-package-sanitize
	./build/test-speech-package-sanitize
test: test-speech-package
sanitize: sanitize-speech-package
