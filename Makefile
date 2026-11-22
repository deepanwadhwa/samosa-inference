DECISION_TOKENIZER_TARGET ?= $(if $(CARGO_TARGET_DIR),$(CARGO_TARGET_DIR),engines/decision/tokenizer/target)
.DEFAULT_GOAL := all
DECISION_TOKENIZER_ARCHIVE = $(DECISION_TOKENIZER_TARGET)/release/libsamosa_decision_tokenizer.a
CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -O2 -g
WARN = -std=c11 -Wall -Wextra -Werror -Wpedantic
LIBS = -lsqlite3
SOURCES = src/registry.c src/embedding.c src/inference.c src/sessions.c src/admission.c src/operations.c src/engine_process.c src/budget.c
HEADERS = src/registry.h src/platform.h src/embedding.h src/inference.h src/json.h src/llama.h src/sessions.h src/admission.h src/operations.h src/sha256.h src/engine_process.h src/budget.h
.PHONY: all test sanitize release clean native test-native sanitize-native test-admission test-operations test-artifact sanitize-artifact test-install sanitize-install test-artifacts-cli test-engine-process sanitize-engine-process test-activation sanitize-activation
.PHONY: test-visual-host sanitize-visual-host
VISUAL_HOST_SOURCES = src/visual_host.c src/activation.c src/engine_runtime.c src/vision.c src/temporal.c src/temporal_input.c src/temporal_package.c src/visual_assets.c src/visual_bridge.c src/artifact.c src/specialist_supervisor.c $(SOURCES)
VISUAL_HOST_HEADERS = src/visual_host.h src/visual_assets.h src/activation.h src/input_identity.h src/vision.h src/temporal.h src/temporal_package.h src/controlled_pipe.h $(HEADERS) include/samosa_inference/specialist_supervisor.h
build/test-visual-host: tests/test_visual_host.c tests/vision_fixture.h tests/whisper_fixture.h $(VISUAL_HOST_SOURCES) $(VISUAL_HOST_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -DJSON_TOKENS_MAX=8192 -DSAMOSA_WITH_VISION -DSAMOSA_WITH_TEMPORAL -Iinclude -Isrc $< $(VISUAL_HOST_SOURCES) $(LIBS) -o $@
build/test-visual-host-sanitize: tests/test_visual_host.c tests/vision_fixture.h tests/whisper_fixture.h $(VISUAL_HOST_SOURCES) $(VISUAL_HOST_HEADERS) | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -pthread -DJSON_TOKENS_MAX=8192 -DSAMOSA_WITH_VISION -DSAMOSA_WITH_TEMPORAL -Iinclude -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer $< $(VISUAL_HOST_SOURCES) $(LIBS) -o $@
test-visual-host: build/test-visual-host build/samosa-inference-providers
	@test -n '$(TEMPORAL_METADATA)' || { echo 'configure retained TEMPORAL_METADATA'; exit 2; }
	SAMOSA_VISUAL_REGISTRY='$(abspath build/samosa-inference-providers)' $(abspath build/test-visual-host) '$(TEMPORAL_METADATA)'
sanitize-visual-host: build/test-visual-host-sanitize build/test-visual-host build/samosa-inference-providers-sanitize
	@test -n '$(TEMPORAL_METADATA)' || { echo 'configure retained TEMPORAL_METADATA'; exit 2; }
	SAMOSA_VISUAL_NATIVE_FIXTURE='$(abspath build/test-visual-host)' SAMOSA_VISUAL_REGISTRY='$(abspath build/samosa-inference-providers-sanitize)' $(abspath build/test-visual-host-sanitize) '$(TEMPORAL_METADATA)'
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
REGISTRY_FLAGS = -Iinclude -DSAMOSA_WITH_DECISION $(NATIVE_FLAGS) -DSAMOSA_WITH_SPEECH -DSAMOSA_WITH_WHISPER -DSAMOSA_WITH_VISION -DSAMOSA_WITH_TEMPORAL -DSAMOSA_WITH_SUMMARY
REGISTRY_SOURCES = src/whisper_activation.c src/whisper_host.c src/visual_host.c src/specialist_supervisor.c src/decision_host.c src/speech_host.c src/summary_host.c src/summary_process.c src/managed_host.c src/managed_provider.c src/activation.c src/managed_stream.c src/decision.c src/decision_assets.c src/speech.c src/audio_store.c src/speech_worker.c src/speech_session.c src/speech_runtime.c src/speech_package.c src/speech_package_loader.c src/library_runtime.c src/engine_runtime.c $(SOURCES) src/llama.c src/whisper.c src/vision.c src/visual_assets.c src/visual_bridge.c src/temporal.c src/temporal_input.c src/temporal_package.c src/summary.c src/summary_protocol.c src/artifact.c
REGISTRY_HEADERS = src/whisper_activation.h src/whisper_audio.h include/samosa_inference/whisper_process.h src/whisper_host.h src/visual_host.h src/visual_assets.h include/samosa_inference/specialist_supervisor.h src/decision_host.h src/speech_host.h src/controlled_pipe.h src/speech_session.h src/summary_host.h include/samosa_inference/summary_process.h src/managed_host.h src/managed_provider.h src/activation.h src/input_identity.h src/managed_stream.h src/rgba_input.h src/decision.h src/decision_assets.h engines/decision/tokenizer/request.h engines/decision/tokenizer/tokenizer.h src/speech.h src/audio_store.h $(SPEECH_WORKER_HEADERS) $(HEADERS) src/registry_host.h src/whisper.h src/vision.h src/visual_assets.h src/temporal.h src/temporal_package.h src/summary.h src/summary_protocol.h src/artifact.h src/private_key.h
.PHONY: providers
providers: build/samosa-inference-providers
build/samosa-inference-providers: src/registry_main.c src/registry_host.c $(REGISTRY_SOURCES) $(REGISTRY_HEADERS) $(DECISION_TOKENIZER_ARCHIVE) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(REGISTRY_FLAGS) src/registry_main.c src/registry_host.c $(REGISTRY_SOURCES) $(DECISION_TOKENIZER_ARCHIVE) $(DECISION_RUST_LIBS) $(LIBS) $(CURL_LIBS) $(DL_LIBS) -pthread -o $@
build/samosa-inference-providers-sanitize: src/registry_main.c src/registry_host.c $(REGISTRY_SOURCES) $(REGISTRY_HEADERS) $(DECISION_TOKENIZER_ARCHIVE) | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(REGISTRY_FLAGS) -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer src/registry_main.c src/registry_host.c $(REGISTRY_SOURCES) $(DECISION_TOKENIZER_ARCHIVE) $(DECISION_RUST_LIBS) $(LIBS) $(CURL_LIBS) $(DL_LIBS) -pthread -o $@
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
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(REGISTRY_FLAGS) -Isrc tests/test_registry.c $(REGISTRY_SOURCES) $(DECISION_TOKENIZER_ARCHIVE) $(DECISION_RUST_LIBS) $(LIBS) $(CURL_LIBS) $(DL_LIBS) -pthread -o $@
test-registry-modalities: build/test-registry-modalities
	./build/test-registry-modalities
sanitize-registry-modalities: $(DECISION_TOKENIZER_ARCHIVE) | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(REGISTRY_FLAGS) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_registry.c $(REGISTRY_SOURCES) $(DECISION_TOKENIZER_ARCHIVE) $(DECISION_RUST_LIBS) $(LIBS) $(CURL_LIBS) $(DL_LIBS) -pthread -o build/test-registry-modalities-sanitize
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
TEMPORAL_SOURCES = src/temporal_main.c src/temporal.c src/temporal_input.c src/temporal_package.c src/visual_assets.c src/visual_bridge.c src/artifact.c $(SOURCES)
build/samosa-inference-temporal: $(TEMPORAL_SOURCES) $(HEADERS) src/temporal.h src/temporal_package.h src/vision.h src/visual_assets.h src/artifact.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DJSON_TOKENS_MAX=8192 -DSAMOSA_WITH_TEMPORAL $(TEMPORAL_SOURCES) $(LIBS) -o $@
build/samosa-inference-temporal-sanitize: $(TEMPORAL_SOURCES) $(HEADERS) src/temporal.h src/temporal_package.h src/vision.h src/visual_assets.h src/artifact.h | build
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
VISION_SOURCES = src/vision_main.c src/vision.c src/visual_assets.c src/visual_bridge.c src/artifact.c $(SOURCES)
build/samosa-inference-vision: $(VISION_SOURCES) $(HEADERS) src/rgba_input.h src/vision.h src/visual_assets.h src/artifact.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DSAMOSA_WITH_VISION $(VISION_SOURCES) $(LIBS) -o $@
build/samosa-inference-vision-sanitize: $(VISION_SOURCES) $(HEADERS) src/rgba_input.h src/vision.h src/visual_assets.h src/artifact.h | build
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
build/samosa-inference-whisper: $(WHISPER_SOURCES) $(HEADERS) src/whisper.h src/whisper_audio.h src/artifact.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -DSAMOSA_WITH_WHISPER $(WHISPER_SOURCES) $(LIBS) -o $@
build/samosa-inference-whisper-sanitize: $(WHISPER_SOURCES) $(HEADERS) src/whisper.h src/whisper_audio.h src/artifact.h | build
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
build/test-activation: tests/test_activation.c src/activation.c src/activation.h src/input_identity.h src/artifact.c src/artifact.h src/engine_process.c src/engine_process.h src/engine_runtime.c src/engine_runtime.h src/json.h src/sha256.h | build
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
	tar -czf dist/samosa-inference-0.1.0.tar.gz Makefile README.md DEPENDENCIES.md .gitignore src include tests docs contracts
clean:
	find build dist -depth -delete
CURL_CFLAGS ?= $(shell curl-config --cflags)
CURL_LIBS ?= $(shell curl-config --libs)
NATIVE_FLAGS = -DSAMOSA_WITH_LLAMA -DJSON_TOKENS_MAX=8192 $(CURL_CFLAGS)
native: build/samosa-inference-llama build/samosa-inference-managed build/samosa-inference-download build/samosa-inference-setup
build/samosa-inference-download: src/artifact_main.c src/remote_artifact.c src/remote_artifact.h src/package_remote_cache.h src/package_assemble.h src/package_index.h src/catalog.h src/artifact.c src/install.c src/artifact.h src/sha256.h src/json.h src/private_key.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(CURL_CFLAGS) -DSAMOSA_REMOTE_ARTIFACTS src/artifact_main.c src/remote_artifact.c src/artifact.c src/install.c $(CURL_LIBS) -o $@
build/samosa-inference-managed: src/managed_main.c src/managed_host.c src/managed_stream.c src/managed_stream.h src/managed_provider.c src/managed_provider.h src/managed_host.h src/activation.c src/activation.h src/input_identity.h src/artifact.c src/artifact.h src/engine_runtime.c src/engine_runtime.h $(SOURCES) src/llama.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(NATIVE_FLAGS) src/managed_main.c src/managed_host.c src/managed_stream.c src/managed_provider.c src/activation.c src/artifact.c src/engine_runtime.c $(SOURCES) src/llama.c $(LIBS) $(CURL_LIBS) -pthread -o $@
build/samosa-inference-managed-sanitize: src/managed_main.c src/managed_host.c src/managed_stream.c src/managed_stream.h src/managed_provider.c src/managed_provider.h src/managed_host.h src/activation.c src/activation.h src/input_identity.h src/artifact.c src/artifact.h src/engine_runtime.c src/engine_runtime.h $(SOURCES) src/llama.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(NATIVE_FLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer src/managed_main.c src/managed_host.c src/managed_stream.c src/managed_provider.c src/activation.c src/artifact.c src/engine_runtime.c $(SOURCES) src/llama.c $(LIBS) $(CURL_LIBS) -pthread -o build/samosa-inference-managed-sanitize
build/samosa-inference-llama: src/main.c $(SOURCES) src/llama.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(NATIVE_FLAGS) src/main.c $(SOURCES) src/llama.c $(LIBS) $(CURL_LIBS) -pthread -o $@
build/samosa-inference-llama-sanitize: src/main.c $(SOURCES) src/llama.c $(HEADERS) | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(NATIVE_FLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer src/main.c $(SOURCES) src/llama.c $(LIBS) $(CURL_LIBS) -pthread -o $@
build/test-llama: tests/test_llama.c tests/llama_http_fixture.h $(SOURCES) src/llama.c $(HEADERS) | build
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
build/activation-native: tests/activation_native.c src/activation.c src/activation.h src/input_identity.h src/artifact.c src/artifact.h src/engine_runtime.c src/engine_runtime.h src/managed_provider.c src/managed_provider.h $(SOURCES) src/llama.c $(HEADERS) | build
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


build/test-llama-sanitize: tests/test_llama.c tests/llama_http_fixture.h $(SOURCES) src/llama.c $(HEADERS) | build
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
PACKAGE_COMPATIBILITY_SOURCES = src/decision_package.c src/decision_package_descriptor.c src/decision_assets.c src/molmo_package.c src/molmo_package_descriptor.c src/temporal_package.c src/vision_package.c src/whisper_package.c src/package_remove.c src/package_removal_record.c src/package_lease.c src/llama_package.c src/llama_package_descriptor.c src/gguf_metadata.c src/package_native_runtime.c src/package_catalog.c src/package_identity.c src/package_reserve.c src/package_cache.c src/install.c src/package_assemble.c src/package_compatibility.c src/package_copy_record.c src/package_stage.c src/package_tree.c src/package_publish.c $(PACKAGE_INDEX_SOURCES) $(SPEECH_PACKAGE_SOURCES) $(SPEECH_LOADER_SOURCES) src/engine_runtime.c
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

SPEECH_WORKER_SOURCES = src/speech_worker.c src/speech_session.c src/speech_runtime.c src/speech_package_loader.c src/library_runtime.c src/speech_package.c src/artifact.c src/engine_runtime.c src/engine_process.c
SPEECH_WORKER_HEADERS = src/controlled_pipe.h src/speech_session.h src/speech_worker.h src/speech_runtime.h src/sherpa_abi.h src/speech_package_loader.h src/library_runtime.h src/speech_package.h src/artifact.h src/engine_runtime.h src/engine_process.h src/json.h src/sha256.h
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
build/test-decision-evidence: tests/test_decision_evidence.cpp engines/decision/text_evidence.cpp engines/decision/text_evidence.h engines/decision/evidence_math.h | build
	$(CXX) -O2 -std=c++20 -Wall -Wextra -Werror -Wpedantic -Iengines engines/decision/text_evidence.cpp tests/test_decision_evidence.cpp -o $@
build/test-decision-evidence-reference: tests/test_decision_evidence_reference.cpp engines/decision/text_evidence.cpp engines/decision/text_evidence.h engines/decision/evidence_math.h tests/decision_reference_fixture.h engines/json.h src/sha256.h | build
	$(CXX) -O2 -std=c++20 -Wall -Wextra -Werror -Wpedantic -Iengines -Isrc engines/decision/text_evidence.cpp tests/test_decision_evidence_reference.cpp -o $@
build/decision-native-image.o: engines/decision/native_image.cpp engines/decision/native_image.h engines/decision/ort_image.h engines/decision/token_json.h engines/decision/evidence_math.h engines/decision/request.h engines/decision/text_evidence.h engines/decision/tokenizer/tokenizer.h engines/json.h | build
	$(CXX) -O2 -std=c++20 -Wall -Wextra -Werror -Wpedantic -Iengines -c engines/decision/native_image.cpp -o $@
build/decision-native-text.o: engines/decision/native_text.cpp engines/decision/token_json.h engines/decision/native_text.h engines/decision/ort_nli.h engines/decision/text_evidence.h engines/decision/tokenizer/tokenizer.h engines/json.h | build
	$(CXX) -O2 -std=c++20 -Wall -Wextra -Werror -Wpedantic -Iengines -c engines/decision/native_text.cpp -o $@
build/decision-native-text-real-harness.o: tests/test_decision_evidence_reference.cpp engines/decision/native_text.h engines/decision/ort_nli.h engines/decision/text_evidence.h engines/decision/evidence_math.h tests/decision_reference_fixture.h engines/json.h src/sha256.h | build
	$(CXX) -O2 -std=c++20 -Wall -Wextra -Werror -Wpedantic -DSAMOSA_DECISION_NATIVE_REAL -Iengines -Isrc -c tests/test_decision_evidence_reference.cpp -o $@
build/decision-native-json-real-harness.o: tests/test_decision_evidence_reference.cpp engines/decision/native_text.h engines/decision/ort_nli.h engines/decision/text_evidence.h engines/decision/request.h tests/decision_reference_fixture.h engines/json.h src/sha256.h | build
	$(CXX) -O2 -std=c++20 -Wall -Wextra -Werror -Wpedantic -DSAMOSA_DECISION_NATIVE_REAL -DSAMOSA_DECISION_JSON_REAL -Iengines -Isrc -c tests/test_decision_evidence_reference.cpp -o $@
test-decision-evidence: build/test-decision-evidence build/test-decision-evidence-reference build/decision-native-text.o build/decision-native-text-real-harness.o build/decision-native-json-real-harness.o build/decision-native-image.o
	./build/test-decision-evidence
sanitize-decision-evidence: | build
	$(CXX) -O1 -g -std=c++20 -Wall -Wextra -Werror -Wpedantic -Iengines -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer engines/decision/text_evidence.cpp tests/test_decision_evidence.cpp -o build/test-decision-evidence-sanitize
	./build/test-decision-evidence-sanitize
test: test-decision-evidence
sanitize: sanitize-decision-evidence

DECISION_RUST_LIBS = $(if $(filter Darwin,$(shell uname -s)),-framework Security -framework CoreFoundation -liconv -lresolv,-ldl -lpthread -lm)
DECISION_REQUEST_SOURCES = engines/decision/request.cpp engines/decision/text_evidence.cpp tests/test_decision_request.cpp
DECISION_REQUEST_HEADERS = engines/decision/evidence_math.h engines/decision/request.h engines/decision/text_evidence.h engines/decision/tokenizer/request.h
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
build/decision-worker-main.o: engines/decision/main.cpp engines/decision/native_text.h engines/decision/native_image.h engines/decision/image_pixels.h engines/decision/request.h engines/decision/transport.h | build
	$(CXX) -O2 -std=c++20 -Wall -Wextra -Werror -Wpedantic -Iengines -c engines/decision/main.cpp -o $@
build/decision-transport-fixture: $(DECISION_TRANSPORT_SOURCES) $(DECISION_REQUEST_HEADERS) engines/decision/transport.h src/sha256.h $(DECISION_TOKENIZER_ARCHIVE) | build
	$(CXX) -O2 -std=c++20 -Wall -Wextra -Werror -Wpedantic -Iengines $(DECISION_TRANSPORT_SOURCES) "$(DECISION_TOKENIZER_ARCHIVE)" $(DECISION_RUST_LIBS) -o $@
test-decision-transport: build/decision-transport-fixture build/decision-worker-main.o
	python3 tests/test_decision_transport.py ./build/decision-transport-fixture
	python3 tests/test_decision_image_transport.py ./build/decision-transport-fixture
sanitize-decision-transport: $(DECISION_TOKENIZER_ARCHIVE) | build
	$(CXX) -O1 -g -std=c++20 -Wall -Wextra -Werror -Wpedantic -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Iengines $(DECISION_TRANSPORT_SOURCES) "$(DECISION_TOKENIZER_ARCHIVE)" $(DECISION_RUST_LIBS) -o build/decision-transport-fixture-sanitize
	python3 tests/test_decision_transport.py ./build/decision-transport-fixture-sanitize
	python3 tests/test_decision_image_transport.py ./build/decision-transport-fixture-sanitize
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

DECISION_DESCRIPTOR_SOURCES = src/decision_package_descriptor.c $(DECISION_ASSET_SOURCES) src/package_index.c src/catalog_host.c
.PHONY: test-decision-package-descriptor sanitize-decision-package-descriptor
build/test-decision-package-descriptor: tests/test_decision_package_descriptor.c $(DECISION_DESCRIPTOR_SOURCES) src/decision_package_descriptor.h src/decision_assets.h src/package_index.h src/json.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc tests/test_decision_package_descriptor.c $(DECISION_DESCRIPTOR_SOURCES) -o $@
test-decision-package-descriptor: build/test-decision-package-descriptor
	./build/test-decision-package-descriptor
sanitize-decision-package-descriptor: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Isrc tests/test_decision_package_descriptor.c $(DECISION_DESCRIPTOR_SOURCES) -o build/test-decision-package-descriptor-sanitize
	./build/test-decision-package-descriptor-sanitize
test: test-decision-package-descriptor
sanitize: sanitize-decision-package-descriptor

DECISION_PACKAGE_SOURCES = src/decision_package.c $(DECISION_DESCRIPTOR_SOURCES) src/package_native_runtime.c src/package_tree.c
.PHONY: test-decision-package sanitize-decision-package
build/test-decision-package: tests/test_decision_package.c tests/library_fixture.h $(DECISION_PACKAGE_SOURCES) src/decision_package.h src/package_native_runtime.h src/package_tree.h src/decision_package_descriptor.h src/decision_assets.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc -Itests tests/test_decision_package.c $(DECISION_PACKAGE_SOURCES) -o $@
test-decision-package: build/test-decision-package
	./build/test-decision-package
sanitize-decision-package: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Isrc -Itests tests/test_decision_package.c $(DECISION_PACKAGE_SOURCES) -o build/test-decision-package-sanitize
	./build/test-decision-package-sanitize
test: test-decision-package
sanitize: sanitize-decision-package

# Explicit pinned SDK/runtime inputs; neither CI nor this rule downloads a model.
DECISION_WORKER_SOURCES = engines/decision/main.cpp engines/decision/transport.cpp engines/decision/request.cpp engines/decision/text_evidence.cpp engines/decision/native_text.cpp engines/decision/ort_nli.cpp engines/decision/native_image.cpp engines/decision/ort_image.cpp engines/decision/image_pixels.cpp
DECISION_WORKER_HEADERS = $(DECISION_REQUEST_HEADERS) engines/decision/transport.h engines/decision/native_text.h engines/decision/native_image.h engines/decision/ort_image.h engines/decision/image_pixels.h engines/decision/ort_nli.h engines/decision/ort_runtime.h engines/decision/token_json.h engines/decision/tokenizer/tokenizer.h engines/json.h
DECISION_ORT_RPATH ?= $(if $(filter Darwin,$(shell uname -s)),@loader_path,$$ORIGIN)
DECISION_IMAGE_ORT_SOURCES = tests/test_decision_image_ort_real.cpp engines/decision/ort_image.cpp engines/decision/image_pixels.cpp
DECISION_IMAGE_ORT_HEADERS = engines/decision/ort_image.h engines/decision/ort_runtime.h engines/decision/image_pixels.h engines/json.h src/sha256.h
DECISION_IMAGE_NATIVE_SOURCES = tests/test_decision_image_native_real.cpp engines/decision/native_image.cpp engines/decision/ort_image.cpp engines/decision/image_pixels.cpp engines/decision/request.cpp engines/decision/text_evidence.cpp
DECISION_IMAGE_NATIVE_HEADERS = $(DECISION_IMAGE_ORT_HEADERS) engines/decision/native_image.h engines/decision/token_json.h $(DECISION_REQUEST_HEADERS) tests/decision_reference_fixture.h
build/test-decision-image-native-real: Makefile $(DECISION_IMAGE_NATIVE_SOURCES) $(DECISION_IMAGE_NATIVE_HEADERS) $(DECISION_TOKENIZER_ARCHIVE) | build
	$(CXX) -O2 -std=c++20 -Wall -Wextra -Werror -Wpedantic -Iengines -Isrc -I"$(DECISION_ORT_INCLUDE)" $(DECISION_IMAGE_NATIVE_SOURCES) "$(DECISION_TOKENIZER_ARCHIVE)" "$(DECISION_ORT_LIBRARY)" $(DECISION_RUST_LIBS) -Xlinker -rpath -Xlinker '$(DECISION_ORT_RPATH)' -o $@
build/test-decision-image-native-real-sanitize: Makefile $(DECISION_IMAGE_NATIVE_SOURCES) $(DECISION_IMAGE_NATIVE_HEADERS) $(DECISION_TOKENIZER_ARCHIVE) | build
	$(CXX) -O1 -g -std=c++20 -Wall -Wextra -Werror -Wpedantic -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Iengines -Isrc -I"$(DECISION_ORT_INCLUDE)" $(DECISION_IMAGE_NATIVE_SOURCES) "$(DECISION_TOKENIZER_ARCHIVE)" "$(DECISION_ORT_LIBRARY)" $(DECISION_RUST_LIBS) -Xlinker -rpath -Xlinker '$(DECISION_ORT_RPATH)' -o $@
.PHONY: test-decision-image-pixels sanitize-decision-image-pixels
build/test-decision-image-pixels: tests/test_decision_image_pixels.cpp engines/decision/image_pixels.cpp engines/decision/image_pixels.h src/sha256.h | build
	$(CXX) -O2 -std=c++20 -Wall -Wextra -Werror -Wpedantic -Iengines -Isrc tests/test_decision_image_pixels.cpp engines/decision/image_pixels.cpp -o $@
test-decision-image-pixels: build/test-decision-image-pixels
	./build/test-decision-image-pixels
sanitize-decision-image-pixels: | build
	$(CXX) -O1 -g -std=c++20 -Wall -Wextra -Werror -Wpedantic -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -Iengines -Isrc tests/test_decision_image_pixels.cpp engines/decision/image_pixels.cpp -o build/test-decision-image-pixels-sanitize
	./build/test-decision-image-pixels-sanitize
test: test-decision-image-pixels
sanitize: sanitize-decision-image-pixels
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
SETUP_SOURCES = src/setup_main.c src/package_sources.c $(PACKAGE_REMOTE_SOURCES)
build/test-catalog build/test-package-remote build/samosa-inference-setup build/samosa-inference-setup-sanitize: src/private_config.h
SETUP_HEADERS = src/decision_package.h src/decision_package_descriptor.h src/decision_assets.h src/molmo_package.h src/molmo_package_descriptor.h src/temporal_package.h src/vision_package.h src/whisper_package.h src/llama_package.h src/llama_package_descriptor.h src/gguf_metadata.h src/catalog.h src/artifact.h src/remote_artifact.h src/json.h src/private_key.h src/sha256.h $(wildcard src/package*.h) $(SPEECH_PACKAGE_HEADERS) $(SPEECH_LOADER_HEADERS) src/library_runtime.h src/engine_runtime.h src/platform.h
build/samosa-inference-setup: $(SETUP_SOURCES) $(SETUP_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(CURL_CFLAGS) $(SETUP_SOURCES) $(CURL_LIBS) -o $@
build/samosa-inference-setup-sanitize: $(SETUP_SOURCES) $(SETUP_HEADERS) | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(CURL_CFLAGS) -fsanitize=address,undefined -fno-omit-frame-pointer $(SETUP_SOURCES) $(CURL_LIBS) -o $@

.PHONY: test-setup sanitize-setup
test-setup: build/test-decision-package build/test-molmo-package build/test-vision-package build/test-whisper-package build/test-package-native-runtime build/samosa-inference-setup build/test-package-remote build/test-speech-worker build/speech-worker-fixture.so
	sh tests/setup_cli.sh
	python3 tests/decision_setup_fixture.py build/samosa-inference-setup build/test-decision-package
	python3 tests/molmo_setup_fixture.py build/samosa-inference-setup build/test-molmo-package
	python3 tests/vision_setup_fixture.py build/samosa-inference-setup build/test-vision-package
	python3 tests/whisper_setup_fixture.py build/samosa-inference-setup build/test-whisper-package
	python3 tests/llama_setup_fixture.py build/samosa-inference-setup build/test-package-native-runtime
	python3 tests/package_compatibility_fixture.py build/test-package-remote build/test-speech-worker build/speech-worker-fixture.so --https --setup build/samosa-inference-setup
sanitize-setup: build/samosa-inference-setup-sanitize build/test-decision-package build/test-molmo-package build/test-vision-package build/test-whisper-package build/test-package-native-runtime build/test-package-remote build/test-speech-worker build/speech-worker-fixture.so | build
	sh tests/setup_cli.sh ./build/samosa-inference-setup-sanitize
	python3 tests/decision_setup_fixture.py build/samosa-inference-setup-sanitize build/test-decision-package
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

# Original Mac development builds, using an explicitly supplied, already-built
# MLX dependency. Release builds retain tools/build_{vision,molmo}_runtime.sh.
# No parent source checkout, dependency rebuild or model download is required.
MLX_DIR ?=
MLX_BUILD_DIR ?=
VISUAL_MLX_INCLUDE = -I"$(MLX_DIR)" -I"$(MLX_BUILD_DIR)" -Iengines
VISUAL_MLX_JACCL = $(wildcard $(MLX_BUILD_DIR)/jaccl/libjaccl.a)
VISUAL_MLX_LIBS = -L"$(MLX_BUILD_DIR)" -lmlx $(VISUAL_MLX_JACCL) -framework Foundation -framework Metal -framework Accelerate -lc++
VISUAL_CXXFLAGS = -std=c++20 -O2 -Wall -Wextra
MOLMO_CXXFLAGS = $(VISUAL_CXXFLAGS) -Wno-unused-function -Wno-unused-parameter -Wno-deprecated-copy
ifeq ($(shell uname -s),Darwin)
MOLMO_MEDIA_SOURCE = engines/molmo2/molmo2_media.mm
MOLMO_MEDIA_LIBS = -framework AVFoundation -framework CoreMedia -framework CoreGraphics
else
MOLMO_MEDIA_SOURCE = engines/molmo2/molmo2_media_stub.cpp
MOLMO_MEDIA_LIBS =
endif
.PHONY: visual-mlx-check vision-engine molmo2-engine molmo2-pack molmo2-model-compile test-vision-engine-components vision-engine-real-check
visual-mlx-check:
	@test "$$(uname -s)" = Darwin || { echo "visual development profile requires native macOS" >&2; exit 2; }
	@test -f "$(MLX_DIR)/mlx/mlx.h" -a -f "$(MLX_BUILD_DIR)/libmlx.a" -a -f "$(MLX_BUILD_DIR)/mlx/backend/metal/kernels/mlx.metallib" || { echo "set MLX_DIR and MLX_BUILD_DIR to the existing native MLX source/build pair" >&2; exit 2; }
VISION_ENGINE_SOURCES = engines/visionpsy/samosa_visionpsy.cpp engines/visionpsy/visionpsy_model.cpp
VISION_ENGINE_HEADERS = engines/visionpsy/visionpsy_model.h engines/stb_image.h engines/json.h engines/tok.h engines/tok_unicode.h
MOLMO_ENGINE_SOURCES = engines/molmo2/samosa_molmo2.cpp engines/molmo2/molmo2_model.cpp engines/molmo2/molmo2_processor.cpp engines/molmo2/molmo2_contract.cpp $(MOLMO_MEDIA_SOURCE)
MOLMO_ENGINE_HEADERS = $(wildcard engines/molmo2/*.h) engines/stb_image.h engines/json.h engines/tok.h engines/tok_unicode.h
vision-engine: $(VISION_ENGINE_SOURCES) $(VISION_ENGINE_HEADERS) visual-mlx-check | build
	$(CXX) $(VISUAL_CXXFLAGS) $(VISUAL_MLX_INCLUDE) $(VISION_ENGINE_SOURCES) -o build/samosa-visionpsy $(VISUAL_MLX_LIBS)
molmo2-engine: $(MOLMO_ENGINE_SOURCES) $(MOLMO_ENGINE_HEADERS) visual-mlx-check | build
	$(CXX) $(MOLMO_CXXFLAGS) $(VISUAL_MLX_INCLUDE) $(MOLMO_ENGINE_SOURCES) -o build/samosa-molmo2 $(VISUAL_MLX_LIBS) $(MOLMO_MEDIA_LIBS)
molmo2-pack: tools/molmo2_pack.cpp engines/molmo2/molmo2_contract.cpp $(MOLMO_CONTRACT_HEADERS) visual-mlx-check | build
	$(CXX) $(VISUAL_CXXFLAGS) -Wno-unused-parameter -Wno-deprecated-copy $(VISUAL_MLX_INCLUDE) tools/molmo2_pack.cpp engines/molmo2/molmo2_contract.cpp -o build/molmo2-pack $(VISUAL_MLX_LIBS)
molmo2-model-compile: engines/molmo2/molmo2_model.cpp $(MOLMO_ENGINE_HEADERS) visual-mlx-check | build
	$(CXX) $(MOLMO_CXXFLAGS) $(VISUAL_MLX_INCLUDE) -c engines/molmo2/molmo2_model.cpp -o build/molmo2_model.o
test-vision-engine-components: tests/test_visionpsy_components.cpp engines/visionpsy/visionpsy_model.cpp $(VISION_ENGINE_HEADERS) visual-mlx-check | build
	$(CXX) $(VISUAL_CXXFLAGS) $(VISUAL_MLX_INCLUDE) tests/test_visionpsy_components.cpp engines/visionpsy/visionpsy_model.cpp -o build/test-visionpsy-components $(VISUAL_MLX_LIBS)
	METAL_PATH="$(MLX_BUILD_DIR)/mlx/backend/metal/kernels" ./build/test-visionpsy-components
vision-engine-real-check: tests/test_visionpsy_real.cpp engines/visionpsy/visionpsy_model.cpp $(VISION_ENGINE_HEADERS) visual-mlx-check | build
	$(CXX) $(VISUAL_CXXFLAGS) $(VISUAL_MLX_INCLUDE) tests/test_visionpsy_real.cpp engines/visionpsy/visionpsy_model.cpp -o build/test-visionpsy-real $(VISUAL_MLX_LIBS)

# The existing Qwen native engine, cache and vision implementation. CPU builds
# need only the system C toolchain; OpenMP preserves the existing host profile.
QWEN_ENGINE_SOURCES = engines/qwen36b.c engines/expert_cache.c engines/vision.c
QWEN_ENGINE_HEADERS = engines/expert_cache.h engines/vision.h engines/kernels.h engines/st.h engines/compat.h engines/repetition_guard.h engines/thinking_budget.h engines/samosa_http.h engines/json.h engines/tok.h engines/tok_unicode.h engines/stb_image.h
ifeq ($(shell uname -s),Darwin)
QWEN_OMP_PREFIX ?= $(shell [ -d /opt/homebrew/opt/libomp ] && echo /opt/homebrew/opt/libomp || echo /usr/local/opt/libomp)
QWEN_OMP_CFLAGS ?= -Xclang -fopenmp -I$(QWEN_OMP_PREFIX)/include
QWEN_OMP_LDFLAGS ?= -L$(QWEN_OMP_PREFIX)/lib -lomp
else
QWEN_OMP_CFLAGS ?= -fopenmp
QWEN_OMP_LDFLAGS ?=
endif
.PHONY: qwen-engine qwen-omp qwen-omp-sched-runtime test-qwen-engine-core test-qwen-engine-serve sanitize-qwen-engine-core sanitize-qwen-engine-serve
qwen-engine: $(QWEN_ENGINE_SOURCES) $(QWEN_ENGINE_HEADERS) | build
	$(CC) -O3 -Wno-unused-function -pthread $(QWEN_ENGINE_SOURCES) -o build/qwen36b -lm
qwen-omp: $(QWEN_ENGINE_SOURCES) $(QWEN_ENGINE_HEADERS) | build
	$(CC) -O3 -Wno-unused-function -pthread $(QWEN_OMP_CFLAGS) $(QWEN_ENGINE_SOURCES) -o build/qwen36b -lm $(QWEN_OMP_LDFLAGS)
qwen-omp-sched-runtime: $(QWEN_ENGINE_SOURCES) $(QWEN_ENGINE_HEADERS) | build
	$(CC) -O3 -Wno-unused-function -pthread $(QWEN_OMP_CFLAGS) -DSAMOSA_SCHED_RUNTIME $(QWEN_ENGINE_SOURCES) -o build/qwen36b-sched-runtime -lm $(QWEN_OMP_LDFLAGS)
build/test_expert_cache: tests/test_expert_cache.c engines/expert_cache.c engines/expert_cache.h | build
	$(CC) -O1 -Iengines tests/test_expert_cache.c engines/expert_cache.c -o $@
build/test_repetition_guard: tests/test_repetition_guard.c engines/repetition_guard.h | build
	$(CC) -O1 -Iengines $< -o $@
build/test_thinking_budget: tests/test_thinking_budget.c engines/thinking_budget.h | build
	$(CC) -O1 -Iengines $< -o $@
build/test_groupwise_q4: tests/test_groupwise_q4.c engines/kernels.h | build
	$(CC) -O1 -Iengines $< -o $@ -lm
build/test_qwen_vision: tests/legacy/test_vision_logic.c engines/vision.c engines/vision.h engines/stb_image.h | build
	$(CC) -O1 -Iengines $< engines/vision.c -o $@ -lm
build/test_samosa_serve: tests/test_samosa_serve.c tests/fixtures/context_config/config.json $(QWEN_ENGINE_SOURCES) $(QWEN_ENGINE_HEADERS) | build
	$(CC) -O1 -pthread -Iengines $< engines/expert_cache.c engines/vision.c -o $@ -lm
test-qwen-engine-core: build/test_expert_cache build/test_repetition_guard build/test_thinking_budget build/test_groupwise_q4 build/test_qwen_vision
	./build/test_expert_cache
	./build/test_repetition_guard
	./build/test_thinking_budget
	./build/test_groupwise_q4
	./build/test_qwen_vision
QWEN_TEST_ASSETS ?=
test-qwen-engine-serve: build/test_samosa_serve
	@test -f "$(QWEN_TEST_ASSETS)/app.html" -a -f "$(QWEN_TEST_ASSETS)/samosa-chat.png" || { echo "set QWEN_TEST_ASSETS to the caller's existing App assets directory" >&2; exit 2; }
	QWEN_TEST_ASSETS="$(abspath $(QWEN_TEST_ASSETS))" ./build/test_samosa_serve
sanitize-qwen-engine-core: | build
	$(CC) -O1 -g -Iengines -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_expert_cache.c engines/expert_cache.c -o build/test_expert_cache-sanitize
	./build/test_expert_cache-sanitize
	$(CC) -O1 -g -Iengines -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_repetition_guard.c -o build/test_repetition_guard-sanitize
	./build/test_repetition_guard-sanitize
	$(CC) -O1 -g -Iengines -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_thinking_budget.c -o build/test_thinking_budget-sanitize
	./build/test_thinking_budget-sanitize
	$(CC) -O1 -g -Iengines -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_groupwise_q4.c -o build/test_groupwise_q4-sanitize -lm
	./build/test_groupwise_q4-sanitize
	$(CC) -O1 -g -Iengines -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/legacy/test_vision_logic.c engines/vision.c -o build/test_qwen_vision-sanitize -lm
	./build/test_qwen_vision-sanitize
sanitize-qwen-engine-serve: | build
	@test -f "$(QWEN_TEST_ASSETS)/app.html" -a -f "$(QWEN_TEST_ASSETS)/samosa-chat.png" || { echo "set QWEN_TEST_ASSETS to the caller's existing App assets directory" >&2; exit 2; }
	$(CC) -O1 -g -pthread -Iengines -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_samosa_serve.c engines/expert_cache.c engines/vision.c -o build/test_samosa_serve-sanitize -lm
	QWEN_TEST_ASSETS="$(abspath $(QWEN_TEST_ASSETS))" ./build/test_samosa_serve-sanitize
test: test-qwen-engine-core
sanitize: sanitize-qwen-engine-core

# Existing Maple model/server, packer and native tests. Reuse the declared
# matching native MLX source/build pair above; do not rebuild/download MLX.
MAPLE_STREAMING_SOURCES = engines/maple/maple_expert_views.cpp engines/maple/maple_expert_store.cpp
MAPLE_MODEL_SOURCES = engines/maple/maple_model.cpp $(MAPLE_STREAMING_SOURCES)
MAPLE_TOKENIZER_SOURCE = engines/maple/tokenizer.cpp
MAPLE_HEADERS = $(wildcard engines/maple/*.h) engines/json.h engines/tok.h engines/tok_unicode.h engines/samosa_http.h engines/expert_cache.h
MAPLE_CXXFLAGS = -std=c++17 -O2 -Wall -Wextra
MAPLE_INCLUDE = $(VISUAL_MLX_INCLUDE) -Iengines/maple
MAPLE_MODEL_TESTS = components model cache_boundary gate_a gate_c gate_c_debug gate_d gate_e expert_views
MAPLE_TOKENIZER_TESTS = native greedy_parity streamed_logits raw_parity cache_diagnosis cache_regression tokenizer
MAPLE_MODEL_TEST_BINS = $(addprefix build/test_maple_,$(MAPLE_MODEL_TESTS))
MAPLE_TOKENIZER_TEST_BINS = $(addprefix build/test_maple_,$(MAPLE_TOKENIZER_TESTS))
build/maple-expert-cache.o: engines/expert_cache.c engines/expert_cache.h | build
	$(CC) -std=c11 -O2 -Wall -Wextra -Iengines -c $< -o $@
.PHONY: maple-engine maple-pack maple-test-builds test-maple-portable test-maple-core test-maple-tensors sanitize-maple-core
maple-engine: engines/maple/samosa_maple.cpp $(MAPLE_MODEL_SOURCES) $(MAPLE_TOKENIZER_SOURCE) $(MAPLE_HEADERS) build/maple-expert-cache.o visual-mlx-check | build
	$(CXX) $(MAPLE_CXXFLAGS) $(MAPLE_INCLUDE) engines/maple/samosa_maple.cpp engines/maple/maple_model.cpp $(MAPLE_TOKENIZER_SOURCE) $(MAPLE_STREAMING_SOURCES) build/maple-expert-cache.o -o build/samosa-maple $(VISUAL_MLX_LIBS)
maple-pack: tools/maple_pack.cpp engines/maple/maple_expert_store.cpp $(MAPLE_HEADERS) build/maple-expert-cache.o | build
	$(CXX) $(MAPLE_CXXFLAGS) -Iengines tools/maple_pack.cpp engines/maple/maple_expert_store.cpp build/maple-expert-cache.o -o build/maple-pack
build/test_maple_openai_messages: tests/test_maple_openai_messages.cpp engines/maple/openai_message_content.h engines/json.h | build
	$(CXX) $(MAPLE_CXXFLAGS) -Iengines $< -o $@
build/test_maple_expert_store: tests/test_maple_expert_store.cpp engines/maple/maple_expert_store.cpp $(MAPLE_HEADERS) build/maple-expert-cache.o | build
	$(CXX) $(MAPLE_CXXFLAGS) -Iengines $< engines/maple/maple_expert_store.cpp build/maple-expert-cache.o -o $@
$(MAPLE_MODEL_TEST_BINS): build/test_maple_%: tests/test_maple_%.cpp $(MAPLE_MODEL_SOURCES) $(MAPLE_HEADERS) build/maple-expert-cache.o | build visual-mlx-check
	$(CXX) $(MAPLE_CXXFLAGS) $(MAPLE_INCLUDE) $< $(MAPLE_MODEL_SOURCES) build/maple-expert-cache.o -o $@ $(VISUAL_MLX_LIBS)
$(MAPLE_TOKENIZER_TEST_BINS): build/test_maple_%: tests/test_maple_%.cpp $(MAPLE_MODEL_SOURCES) $(MAPLE_TOKENIZER_SOURCE) $(MAPLE_HEADERS) build/maple-expert-cache.o | build visual-mlx-check
	$(CXX) $(MAPLE_CXXFLAGS) $(MAPLE_INCLUDE) $(if $(filter build/test_maple_raw_parity,$@),-I"$(MLX_BUILD_DIR)/_deps/json-src/include") $< engines/maple/maple_model.cpp $(MAPLE_TOKENIZER_SOURCE) $(MAPLE_STREAMING_SOURCES) build/maple-expert-cache.o -o $@ $(VISUAL_MLX_LIBS)
build/maple-mlx-smoke: tests/maple_mlx_smoke.cpp | build visual-mlx-check
	$(CXX) $(MAPLE_CXXFLAGS) $(VISUAL_MLX_INCLUDE) $< -o $@ $(VISUAL_MLX_LIBS)
build/test_attn_parity: tests/test_attn_parity.cpp $(MAPLE_MODEL_SOURCES) $(MAPLE_HEADERS) build/maple-expert-cache.o | build visual-mlx-check
	$(CXX) $(MAPLE_CXXFLAGS) $(MAPLE_INCLUDE) $< $(MAPLE_MODEL_SOURCES) build/maple-expert-cache.o -o $@ $(VISUAL_MLX_LIBS)
# These two old scratch diagnostics also fail against the parent baseline's
# private model API. Preserve their source and explicit reproduction targets;
# they were never members of the parent's supported native test suite.
MAPLE_LEGACY_DIAGNOSTICS = build/test_maple_gate_c_debug build/test_maple_raw_parity
maple-test-builds: $(filter-out $(MAPLE_LEGACY_DIAGNOSTICS),$(MAPLE_MODEL_TEST_BINS) $(MAPLE_TOKENIZER_TEST_BINS)) build/test_maple_openai_messages build/test_maple_expert_store build/maple-mlx-smoke build/test_attn_parity
test-maple-core: build/test_maple_openai_messages build/test_maple_expert_store build/test_maple_expert_views build/test_maple_cache_boundary build/maple-mlx-smoke
	./build/test_maple_openai_messages
	./build/test_maple_expert_store
	METAL_PATH="$(MLX_BUILD_DIR)/mlx/backend/metal/kernels" ./build/test_maple_expert_views
	METAL_PATH="$(MLX_BUILD_DIR)/mlx/backend/metal/kernels" ./build/test_maple_cache_boundary
	METAL_PATH="$(MLX_BUILD_DIR)/mlx/backend/metal/kernels" ./build/maple-mlx-smoke
test-maple-tensors: build/test_maple_components build/test_maple_model build/test_maple_native build/test_attn_parity
	METAL_PATH="$(MLX_BUILD_DIR)/mlx/backend/metal/kernels" ./build/test_maple_components
	METAL_PATH="$(MLX_BUILD_DIR)/mlx/backend/metal/kernels" ./build/test_maple_model
	METAL_PATH="$(MLX_BUILD_DIR)/mlx/backend/metal/kernels" ./build/test_maple_native
	METAL_PATH="$(MLX_BUILD_DIR)/mlx/backend/metal/kernels" ./build/test_attn_parity
sanitize-maple-core: | build visual-mlx-check
	$(CC) -std=c11 -O1 -g -Iengines -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer -c engines/expert_cache.c -o build/maple-expert-cache-sanitize.o
	$(CXX) -std=c++17 -O1 -g -Iengines -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_maple_openai_messages.cpp -o build/test_maple_openai_messages-sanitize
	./build/test_maple_openai_messages-sanitize
	$(CXX) -std=c++17 -O1 -g -Iengines -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_maple_expert_store.cpp engines/maple/maple_expert_store.cpp build/maple-expert-cache-sanitize.o -o build/test_maple_expert_store-sanitize
	./build/test_maple_expert_store-sanitize
	$(CXX) -std=c++17 -O1 -g $(MAPLE_INCLUDE) -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_maple_expert_views.cpp $(MAPLE_MODEL_SOURCES) build/maple-expert-cache-sanitize.o -o build/test_maple_expert_views-sanitize $(VISUAL_MLX_LIBS)
	METAL_PATH="$(MLX_BUILD_DIR)/mlx/backend/metal/kernels" ./build/test_maple_expert_views-sanitize
test-maple-portable: build/test_maple_openai_messages build/test_maple_expert_store
	./build/test_maple_openai_messages
	./build/test_maple_expert_store
test: test-maple-portable

# Extend the existing decision transport fixture for public ownership/control
# gates. Tiny fake loader dependency and dummy model bytes, never actual-model
# evidence. Real model pins remain a separate explicit qualification runner.
DECISION_FIXTURE_RUNTIME_NAME = $(if $(filter Darwin,$(shell uname -s)),libonnxruntime.1.27.0.dylib,libonnxruntime.so.1.27.0)
DECISION_FIXTURE_RUNTIME = build/decision-public-loader/$(DECISION_FIXTURE_RUNTIME_NAME)
DECISION_FIXTURE_LINK = $(if $(filter Darwin,$(shell uname -s)),-Xlinker -install_name -Xlinker @rpath/$(DECISION_FIXTURE_RUNTIME_NAME),-Xlinker -soname -Xlinker $(DECISION_FIXTURE_RUNTIME_NAME))
$(DECISION_FIXTURE_RUNTIME): tests/decision_loader_fixture.c | build
	mkdir -p build/decision-public-loader
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -shared -fPIC $< $(DECISION_FIXTURE_LINK) -o $@
build/decision-public-fixture: $(DECISION_TRANSPORT_SOURCES) $(DECISION_REQUEST_HEADERS) engines/decision/transport.h src/sha256.h $(DECISION_TOKENIZER_ARCHIVE) $(DECISION_FIXTURE_RUNTIME) | build
	$(CXX) -O2 -std=c++20 -Wall -Wextra -Werror -Wpedantic -DSAMOSA_DECISION_PUBLIC_FIXTURE -Iengines $(DECISION_TRANSPORT_SOURCES) "$(DECISION_TOKENIZER_ARCHIVE)" $(DECISION_FIXTURE_RUNTIME) $(DECISION_RUST_LIBS) -Xlinker -rpath -Xlinker '$(DECISION_ORT_RPATH)' -o $@
.PHONY: test-decision-public sanitize-decision-public
test-decision-public: build/samosa-inference-providers build/decision-public-fixture
	python3 tests/test_decision_controls.py $(abspath build/samosa-inference-providers) $(abspath build/decision-public-fixture) $(abspath $(DECISION_FIXTURE_RUNTIME))
	python3 tests/test_decision_controls.py $(abspath build/samosa-inference-providers) $(abspath build/decision-public-fixture) $(abspath $(DECISION_FIXTURE_RUNTIME)) --image
sanitize-decision-public: build/samosa-inference-providers-sanitize build/decision-public-fixture
	python3 tests/test_decision_controls.py $(abspath build/samosa-inference-providers-sanitize) $(abspath build/decision-public-fixture) $(abspath $(DECISION_FIXTURE_RUNTIME))
	python3 tests/test_decision_controls.py $(abspath build/samosa-inference-providers-sanitize) $(abspath build/decision-public-fixture) $(abspath $(DECISION_FIXTURE_RUNTIME)) --image

# Existing App specialist supervisor, extracted without an inference rewrite.
# Preserve the existing dynamically linked development build. Release CPU builds
# continue to use tools/build_t5_runtime.sh and its pinned static dependency.
T5_LLAMA_DIR ?=
T5_LLAMA_BUILD ?= $(T5_LLAMA_DIR)/build
.PHONY: t5-engine test-t5-engine-real
t5-engine: engines/t5_summary.cpp | build
	@test -f "$(T5_LLAMA_DIR)/include/llama.h" || { echo "set T5_LLAMA_DIR to the existing Prism checkout" >&2; exit 2; }
	@test -f "$(T5_LLAMA_BUILD)/bin/libllama.0.dylib" || { echo "missing built Prism libraries" >&2; exit 2; }
	$(CXX) -O3 -std=c++17 -I"$(T5_LLAMA_DIR)/include" -I"$(T5_LLAMA_DIR)/ggml/include" \
	  engines/t5_summary.cpp -o build/samosa-summarizer \
	  -Wl,-rpath,@loader_path/../lib -Wl,-rpath,"$(T5_LLAMA_BUILD)/bin" \
	  "$(T5_LLAMA_BUILD)/bin/libllama.0.dylib" \
	  "$(T5_LLAMA_BUILD)/bin/libggml.0.dylib" \
	  "$(T5_LLAMA_BUILD)/bin/libggml-cpu.0.dylib" \
	  "$(T5_LLAMA_BUILD)/bin/libggml-blas.0.dylib" \
	  "$(T5_LLAMA_BUILD)/bin/libggml-metal.0.dylib" \
	  "$(T5_LLAMA_BUILD)/bin/libggml-base.0.dylib"
build/test-native-summarizer: tests/test_native_summarizer.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $< -o $@
test-t5-engine-real: build/test-native-summarizer
	@test -n "$(T5_TEST_ENGINE)" -a -n "$(T5_TEST_MODEL)" || { echo "set T5_TEST_ENGINE and T5_TEST_MODEL" >&2; exit 2; }
	./build/test-native-summarizer "$(T5_TEST_ENGINE)" "$(T5_TEST_MODEL)"
.PHONY: specialist test-specialist-supervisor sanitize-specialist-supervisor
specialist: build/libsamosa-inference-specialist.a
.PHONY: test-summary-process sanitize-summary-process
.PHONY: test-summary-transport-real sanitize-summary-transport-real
build/test-summary-transport-real: tests/test_summary_transport_real.c tests/summary_owned_fixture.h src/engine_process.c src/engine_process.h build/libsamosa-inference-specialist.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude -Isrc $< src/engine_process.c build/libsamosa-inference-specialist.a -o $@
test-summary-transport-real: build/test-summary-transport-real
	@test -n "$(T5_TEST_ENGINE)" -a -n "$(T5_TEST_MODEL)" || { echo "set T5_TEST_ENGINE and T5_TEST_MODEL" >&2; exit 2; }
	./build/test-summary-transport-real "$(T5_TEST_ENGINE)" "$(T5_TEST_MODEL)"
sanitize-summary-transport-real: | build
	@test -n "$(T5_TEST_ENGINE)" -a -n "$(T5_TEST_MODEL)" || { echo "set T5_TEST_ENGINE and T5_TEST_MODEL" >&2; exit 2; }
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -pthread -Iinclude -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_summary_transport_real.c src/summary_process.c src/engine_process.c -o build/test-summary-transport-real-sanitize
	./build/test-summary-transport-real-sanitize "$(T5_TEST_ENGINE)" "$(T5_TEST_MODEL)"
build/fake-native-summarizer: tests/fake_native_summarizer.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $< -o $@
build/test-summary-process: tests/test_summary_process.c build/libsamosa-inference-specialist.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude $< build/libsamosa-inference-specialist.a -o $@
test-summary-process: build/test-summary-process build/fake-native-summarizer
	./build/test-summary-process $(abspath build/fake-native-summarizer)
sanitize-summary-process: build/fake-native-summarizer | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -pthread -Iinclude -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_summary_process.c src/summary_process.c -o build/test-summary-process-sanitize
	./build/test-summary-process-sanitize $(abspath build/fake-native-summarizer)
test: test-summary-process
sanitize: sanitize-summary-process
build/specialist-supervisor.o: src/specialist_supervisor.c src/engine_process.h src/json.h src/controlled_pipe.h include/samosa_inference/specialist_supervisor.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude -c $< -o $@
build/specialist-protocol.o: src/specialist_protocol.c engines/json.h include/samosa_inference/specialist_protocol.h include/samosa_inference/specialist_supervisor.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude -c $< -o $@
.PHONY: test-specialist-protocol sanitize-specialist-protocol
build/test-specialist-protocol: tests/test_specialist_protocol.c include/samosa_inference/specialist_protocol.h build/libsamosa-inference-specialist.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude $< build/libsamosa-inference-specialist.a -o $@
test-specialist-protocol: build/test-specialist-protocol
	./build/test-specialist-protocol
sanitize-specialist-protocol: tests/test_specialist_protocol.c src/specialist_protocol.c engines/json.h src/specialist_supervisor.c src/controlled_pipe.h include/samosa_inference/specialist_protocol.h include/samosa_inference/specialist_supervisor.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -pthread -Iinclude -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_specialist_protocol.c src/specialist_protocol.c src/specialist_supervisor.c src/engine_process.c -o build/test-specialist-protocol-sanitize
	./build/test-specialist-protocol-sanitize
test: test-specialist-protocol
sanitize: sanitize-specialist-protocol
.PHONY: test-specialist-launch sanitize-specialist-launch
build/test-specialist-launch: tests/test_specialist_launch.c include/samosa_inference/specialist_protocol.h build/libsamosa-inference-specialist.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude $< build/libsamosa-inference-specialist.a -o $@
test-specialist-launch: build/test-specialist-launch build/fake-multimodal-helper
	$(abspath build/test-specialist-launch) $(abspath build/fake-multimodal-helper)
sanitize-specialist-launch: tests/test_specialist_launch.c src/specialist_protocol.c engines/json.h src/specialist_supervisor.c src/controlled_pipe.h include/samosa_inference/specialist_protocol.h include/samosa_inference/specialist_supervisor.h build/fake-multimodal-helper | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -pthread -Iinclude -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_specialist_launch.c src/specialist_protocol.c src/specialist_supervisor.c src/engine_process.c -o build/test-specialist-launch-sanitize
	$(abspath build/test-specialist-launch-sanitize) $(abspath build/fake-multimodal-helper)
test: test-specialist-launch
sanitize: sanitize-specialist-launch
build/summary-process.o: src/summary_process.c src/controlled_pipe.h src/json.h include/samosa_inference/summary_process.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude -c $< -o $@
build/llama-launch.o: src/llama_launch.c include/samosa_inference/llama_launch.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Iinclude -c $< -o $@
build/backend-process.o: src/engine_process.h src/json.h src/backend_process.c src/backend_process_internal.h include/samosa_inference/backend_process.h include/samosa_inference/llama_launch.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude -c src/backend_process.c -o $@
.PHONY: test-backend-process sanitize-backend-process
build/test-backend-process: tests/test_backend_process.c include/samosa_inference/backend_process.h build/libsamosa-inference-specialist.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude $< build/libsamosa-inference-specialist.a -o $@
build/test-backend-process-sanitize: tests/test_backend_process.c src/backend_process.c src/backend_process_internal.h src/llama_launch.c src/engine_process.c src/engine_process.h src/json.h include/samosa_inference/backend_process.h include/samosa_inference/llama_launch.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -pthread -Iinclude -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_backend_process.c src/backend_process.c src/llama_launch.c src/engine_process.c -o $@
test-backend-process: build/test-backend-process
	$(abspath build/test-backend-process) $(abspath build/test-backend-process)
sanitize-backend-process: build/test-backend-process-sanitize build/test-backend-process
	$(abspath build/test-backend-process-sanitize) $(abspath build/test-backend-process)
test: test-backend-process
sanitize: sanitize-backend-process
build/whisper-process.o: src/whisper_process.c src/input_identity.h src/whisper_process_internal.h src/engine_process.h src/json.h include/samosa_inference/whisper_process.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Iinclude -c $< -o $@
build/vision-budget.o: src/vision_budget.c include/samosa_inference/vision_budget.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Iinclude -c $< -o $@
.PHONY: test-vision-budget sanitize-vision-budget
build/test-vision-budget: tests/test_vision_budget.c include/samosa_inference/vision_budget.h build/libsamosa-inference-specialist.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Iinclude $< build/libsamosa-inference-specialist.a -o $@
test-vision-budget: build/test-vision-budget
	$(abspath build/test-vision-budget)
sanitize-vision-budget: tests/test_vision_budget.c src/vision_budget.c include/samosa_inference/vision_budget.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Iinclude -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_vision_budget.c src/vision_budget.c -o build/test-vision-budget-sanitize
	$(abspath build/test-vision-budget-sanitize)
test: test-vision-budget
sanitize: sanitize-vision-budget
.PHONY: test-whisper-process sanitize-whisper-process
build/test-whisper-process: tests/test_whisper_process.c include/samosa_inference/whisper_process.h build/libsamosa-inference-specialist.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude $< build/libsamosa-inference-specialist.a -o $@
test-whisper-process: build/test-whisper-process
	$(abspath build/test-whisper-process) $(abspath build/test-whisper-process)
sanitize-whisper-process: build/test-whisper-process | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -pthread -Iinclude -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_whisper_process.c src/whisper_process.c src/engine_process.c -o build/test-whisper-process-sanitize
	$(abspath build/test-whisper-process-sanitize) $(abspath build/test-whisper-process)
test: test-whisper-process
sanitize: sanitize-whisper-process
build/test-whisper-process-real: tests/test_whisper_process_real.c include/samosa_inference/whisper_process.h build/libsamosa-inference-specialist.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Iinclude $< build/libsamosa-inference-specialist.a -o $@
build/test-whisper-process-real-sanitize: tests/test_whisper_process_real.c src/whisper_process.c src/engine_process.c src/input_identity.h src/whisper_process_internal.h src/engine_process.h src/json.h include/samosa_inference/whisper_process.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Iinclude -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_whisper_process_real.c src/whisper_process.c src/engine_process.c -o $@
.PHONY: test-llama-launch sanitize-llama-launch
build/test-llama-launch: tests/test_llama_launch.c src/llama_launch.c include/samosa_inference/llama_launch.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Iinclude tests/test_llama_launch.c src/llama_launch.c -o $@
test-llama-launch: build/test-llama-launch
	./build/test-llama-launch $(abspath build/test-llama-launch)
sanitize-llama-launch: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -Iinclude -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_llama_launch.c src/llama_launch.c -o build/test-llama-launch-sanitize
	./build/test-llama-launch-sanitize $(abspath build/test-llama-launch-sanitize)
test: test-llama-launch
sanitize: sanitize-llama-launch
build/engine-process.o: src/engine_process.c src/engine_process.h src/json.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc -c $< -o $@
build/backend-json.o: src/backend_json.c include/samosa_inference/backend_json.h include/samosa_inference/turn_control.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude -c $< -o $@
SPECIALIST_OBJECTS = build/engine-process.o build/specialist-supervisor.o build/specialist-protocol.o build/summary-process.o build/llama-launch.o build/backend-process.o build/backend-json.o build/whisper-process.o build/vision-budget.o build/turn-control.o
build/libsamosa-inference-specialist.a: $(SPECIALIST_OBJECTS) Makefile
	$(AR) rcs $@ $(filter %.o,$^)
build/fake-multimodal-helper: tests/fake_multimodal_helper.c | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread $< -o $@
build/test-specialist-supervisor: tests/test_multimodal_supervisor.c build/libsamosa-inference-specialist.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude $< build/libsamosa-inference-specialist.a -o $@
test-specialist-supervisor: build/test-specialist-supervisor build/fake-multimodal-helper
	./build/test-specialist-supervisor $(abspath build/fake-multimodal-helper)
sanitize-specialist-supervisor: build/fake-multimodal-helper | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -pthread -Iinclude -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_multimodal_supervisor.c src/specialist_supervisor.c src/engine_process.c -o build/test-specialist-supervisor-sanitize
	./build/test-specialist-supervisor-sanitize $(abspath build/fake-multimodal-helper)
test: test-specialist-supervisor
sanitize: sanitize-specialist-supervisor

# Shared original HTTP fixture exercised through actual activation/guardian.
MANAGED_READINESS_SOURCES = src/managed_provider.c src/activation.c src/artifact.c src/engine_runtime.c $(SOURCES) src/llama.c
MANAGED_READINESS_HEADERS = src/managed_provider.h src/activation.h src/input_identity.h src/artifact.h src/engine_runtime.h $(HEADERS)
.PHONY: test-managed-readiness sanitize-managed-readiness
build/fake-owned-llama: tests/fake_owned_llama.c tests/llama_http_fixture.h src/json.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc tests/fake_owned_llama.c -o $@
build/test-managed-readiness: tests/test_managed_readiness.c $(MANAGED_READINESS_SOURCES) $(MANAGED_READINESS_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(NATIVE_FLAGS) -Isrc tests/test_managed_readiness.c $(MANAGED_READINESS_SOURCES) $(LIBS) $(CURL_LIBS) -pthread -o $@
test-managed-readiness: build/test-managed-readiness build/fake-owned-llama
	$(abspath build/test-managed-readiness) $(abspath build/fake-owned-llama)
sanitize-managed-readiness: build/fake-owned-llama | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(NATIVE_FLAGS) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_managed_readiness.c $(MANAGED_READINESS_SOURCES) $(LIBS) $(CURL_LIBS) -pthread -o build/test-managed-readiness-sanitize
	$(abspath build/test-managed-readiness-sanitize) $(abspath build/fake-owned-llama)
test-native: test-managed-readiness
sanitize-native: sanitize-managed-readiness

.PHONY: test-managed-host sanitize-managed-host
MANAGED_HOST_SOURCES = src/managed_host.c $(MANAGED_READINESS_SOURCES)
build/test-managed-host: tests/test_managed_host.c src/managed_host.h $(MANAGED_HOST_SOURCES) $(MANAGED_READINESS_HEADERS) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(NATIVE_FLAGS) -Isrc tests/test_managed_host.c $(MANAGED_HOST_SOURCES) $(LIBS) $(CURL_LIBS) -pthread -o $@
test-managed-host: build/test-managed-host
	$(abspath build/test-managed-host)
sanitize-managed-host: | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(NATIVE_FLAGS) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_managed_host.c $(MANAGED_HOST_SOURCES) $(LIBS) $(CURL_LIBS) -pthread -o build/test-managed-host-sanitize
	$(abspath build/test-managed-host-sanitize)
test-native: test-managed-host
sanitize-native: sanitize-managed-host

.PHONY: test-registry-lifecycle sanitize-registry-lifecycle
build/fake-registry-llama: tests/fake_registry_llama.c tests/llama_http_fixture.h src/json.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -Isrc tests/fake_registry_llama.c -o $@
build/test-registry-lifecycle: tests/test_registry_lifecycle.c src/registry_host.c $(REGISTRY_SOURCES) $(REGISTRY_HEADERS) $(DECISION_TOKENIZER_ARCHIVE) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(REGISTRY_FLAGS) -Isrc tests/test_registry_lifecycle.c src/registry_host.c $(REGISTRY_SOURCES) $(DECISION_TOKENIZER_ARCHIVE) $(DECISION_RUST_LIBS) $(LIBS) $(CURL_LIBS) $(DL_LIBS) -pthread -o $@
test-registry-lifecycle: build/test-registry-lifecycle build/fake-registry-llama build/samosa-inference-providers
	$(abspath build/test-registry-lifecycle) $(abspath build/fake-registry-llama) $(abspath build/samosa-inference-providers)
sanitize-registry-lifecycle: build/fake-registry-llama build/samosa-inference-providers-sanitize $(DECISION_TOKENIZER_ARCHIVE) | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(REGISTRY_FLAGS) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_registry_lifecycle.c src/registry_host.c $(REGISTRY_SOURCES) $(DECISION_TOKENIZER_ARCHIVE) $(DECISION_RUST_LIBS) $(LIBS) $(CURL_LIBS) $(DL_LIBS) -pthread -o build/test-registry-lifecycle-sanitize
	$(abspath build/test-registry-lifecycle-sanitize) $(abspath build/fake-registry-llama) $(abspath build/samosa-inference-providers-sanitize)
test-native: test-registry-lifecycle
sanitize-native: sanitize-registry-lifecycle

.PHONY: test-registry-embedding-lifecycle sanitize-registry-embedding-lifecycle
test-registry-embedding-lifecycle: build/test-registry-lifecycle build/fake-registry-llama build/samosa-inference-providers
	$(abspath build/test-registry-lifecycle) $(abspath build/fake-registry-llama) $(abspath build/samosa-inference-providers) --embedding
sanitize-registry-embedding-lifecycle: sanitize-registry-lifecycle
	$(abspath build/test-registry-lifecycle-sanitize) $(abspath build/fake-registry-llama) $(abspath build/samosa-inference-providers-sanitize) --embedding
test-native: test-registry-embedding-lifecycle
sanitize-native: sanitize-registry-embedding-lifecycle

.PHONY: test-summary-owned sanitize-summary-owned
build/test-summary-owned: tests/test_summary_owned.c tests/summary_owned_fixture.h src/controlled_pipe.h src/summary_process.c src/engine_process.c src/engine_process.h src/json.h include/samosa_inference/summary_process.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude -Isrc tests/test_summary_owned.c src/summary_process.c src/engine_process.c -o $@
test-summary-owned: build/test-summary-owned build/fake-native-summarizer
	$(abspath build/test-summary-owned) $(abspath build/fake-native-summarizer)
sanitize-summary-owned: build/fake-native-summarizer | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -pthread -Iinclude -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_summary_owned.c src/summary_process.c src/engine_process.c -o build/test-summary-owned-sanitize
	$(abspath build/test-summary-owned-sanitize) $(abspath build/fake-native-summarizer)
test: test-summary-owned
sanitize: sanitize-summary-owned

.PHONY: test-summary-owned-real sanitize-summary-owned-real
test-summary-owned-real: build/test-summary-transport-real
	@test -n "$(T5_TEST_ENGINE)" -a -n "$(T5_TEST_MODEL)" || { echo "set T5_TEST_ENGINE and T5_TEST_MODEL" >&2; exit 2; }
	$(abspath build/test-summary-transport-real) "$(T5_TEST_ENGINE)" "$(T5_TEST_MODEL)" --guardian
sanitize-summary-owned-real: | build
	@test -n "$(T5_TEST_ENGINE)" -a -n "$(T5_TEST_MODEL)" || { echo "set T5_TEST_ENGINE and T5_TEST_MODEL" >&2; exit 2; }
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -pthread -Iinclude -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_summary_transport_real.c src/summary_process.c src/engine_process.c -o build/test-summary-transport-real-sanitize
	$(abspath build/test-summary-transport-real-sanitize) "$(T5_TEST_ENGINE)" "$(T5_TEST_MODEL)" --guardian

.PHONY: test-registry-summary-lifecycle sanitize-registry-summary-lifecycle
test-registry-summary-lifecycle: build/samosa-inference-providers build/fake-native-summarizer
	python3 tests/summary_registry_lifecycle.py $(abspath build/samosa-inference-providers) $(abspath build/fake-native-summarizer)
sanitize-registry-summary-lifecycle: build/samosa-inference-providers-sanitize build/fake-native-summarizer
	python3 tests/summary_registry_lifecycle.py $(abspath build/samosa-inference-providers-sanitize) $(abspath build/fake-native-summarizer)
test-native: test-registry-summary-lifecycle
sanitize-native: sanitize-registry-summary-lifecycle

.PHONY: test-registry-speech-lifecycle sanitize-registry-speech-lifecycle
test-registry-speech-lifecycle: build/samosa-inference-providers build/test-speech-worker build/speech-worker-fixture.so
	python3 tests/speech_registry_lifecycle.py $(abspath build/samosa-inference-providers) --fixture-host $(abspath build/test-speech-worker) --fixture-library $(abspath build/speech-worker-fixture.so)
sanitize-registry-speech-lifecycle: build/samosa-inference-providers-sanitize build/test-speech-worker build/speech-worker-fixture.so
	python3 tests/speech_registry_lifecycle.py $(abspath build/samosa-inference-providers-sanitize) --fixture-host $(abspath build/test-speech-worker) --fixture-library $(abspath build/speech-worker-fixture.so)
test-native: test-registry-speech-lifecycle
sanitize-native: sanitize-registry-speech-lifecycle

.PHONY: test-registry-decision-lifecycle sanitize-registry-decision-lifecycle
test-registry-decision-lifecycle: build/samosa-inference-providers build/decision-public-fixture
	python3 tests/decision_registry_lifecycle.py $(abspath build/samosa-inference-providers) --worker $(abspath build/decision-public-fixture) --runtime $(abspath $(DECISION_FIXTURE_RUNTIME))
sanitize-registry-decision-lifecycle: build/samosa-inference-providers-sanitize build/decision-public-fixture
	python3 tests/decision_registry_lifecycle.py $(abspath build/samosa-inference-providers-sanitize) --worker $(abspath build/decision-public-fixture) --runtime $(abspath $(DECISION_FIXTURE_RUNTIME))
test-native: test-registry-decision-lifecycle
sanitize-native: sanitize-registry-decision-lifecycle

.PHONY: test-registry-image-decision-lifecycle sanitize-registry-image-decision-lifecycle
test-registry-image-decision-lifecycle: build/samosa-inference-providers build/decision-public-fixture
	python3 tests/decision_registry_lifecycle.py $(abspath build/samosa-inference-providers) --image --worker $(abspath build/decision-public-fixture) --runtime $(abspath $(DECISION_FIXTURE_RUNTIME))
sanitize-registry-image-decision-lifecycle: build/samosa-inference-providers-sanitize build/decision-public-fixture
	python3 tests/decision_registry_lifecycle.py $(abspath build/samosa-inference-providers-sanitize) --image --worker $(abspath build/decision-public-fixture) --runtime $(abspath $(DECISION_FIXTURE_RUNTIME))
test-native: test-registry-image-decision-lifecycle
sanitize-native: sanitize-registry-image-decision-lifecycle

.PHONY: test-specialist-owned sanitize-specialist-owned
build/test-specialist-owned: tests/test_specialist_owned.c tests/summary_owned_fixture.h src/engine_process.c src/engine_process.h build/libsamosa-inference-specialist.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude -Isrc $< src/engine_process.c build/libsamosa-inference-specialist.a -o $@
test-specialist-owned: build/test-specialist-owned build/fake-multimodal-helper
	$(abspath build/test-specialist-owned) $(abspath build/fake-multimodal-helper)
sanitize-specialist-owned: build/fake-multimodal-helper | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -pthread -Iinclude -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_specialist_owned.c src/specialist_supervisor.c src/engine_process.c -o build/test-specialist-owned-sanitize
	$(abspath build/test-specialist-owned-sanitize) $(abspath build/fake-multimodal-helper)
test: test-specialist-owned
sanitize: sanitize-specialist-owned

.PHONY: test-specialist-guardian sanitize-specialist-guardian
build/test-specialist-guardian: tests/test_specialist_guardian.c include/samosa_inference/specialist_supervisor.h build/libsamosa-inference-specialist.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude $< build/libsamosa-inference-specialist.a -o $@
build/test-specialist-guardian-sanitize: tests/test_specialist_guardian.c src/specialist_supervisor.c src/specialist_protocol.c src/engine_process.c src/engine_process.h src/json.h include/samosa_inference/specialist_supervisor.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -pthread -Iinclude -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_specialist_guardian.c src/specialist_supervisor.c src/specialist_protocol.c src/engine_process.c -o $@
test-specialist-guardian: build/test-specialist-guardian build/fake-multimodal-helper
	$(abspath build/test-specialist-guardian) $(abspath build/fake-multimodal-helper)
sanitize-specialist-guardian: build/test-specialist-guardian-sanitize build/fake-multimodal-helper
	$(abspath build/test-specialist-guardian-sanitize) $(abspath build/fake-multimodal-helper)
build/test-specialist-guardian-real: tests/test_specialist_guardian_real.c include/samosa_inference/specialist_supervisor.h build/libsamosa-inference-specialist.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude $< build/libsamosa-inference-specialist.a -o $@
build/test-specialist-guardian-real-sanitize: tests/test_specialist_guardian_real.c src/specialist_supervisor.c src/specialist_protocol.c src/engine_process.c src/engine_process.h src/json.h include/samosa_inference/specialist_supervisor.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -pthread -Iinclude -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_specialist_guardian_real.c src/specialist_supervisor.c src/specialist_protocol.c src/engine_process.c -o $@

build/turn-control.o: src/turn_control.c include/samosa_inference/turn_control.h include/samosa_inference/specialist_protocol.h include/samosa_inference/vision_budget.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude -c $< -o $@
.PHONY: test-turn-control sanitize-turn-control
build/test-turn-control: tests/test_turn_control.c include/samosa_inference/turn_control.h build/libsamosa-inference-specialist.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude $< build/libsamosa-inference-specialist.a -o $@
build/test-turn-control-sanitize: tests/test_turn_control.c src/turn_control.c src/specialist_supervisor.c src/specialist_protocol.c src/engine_process.c src/engine_process.h src/json.h include/samosa_inference/turn_control.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -pthread -Iinclude -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_turn_control.c src/turn_control.c src/specialist_supervisor.c src/specialist_protocol.c src/engine_process.c -o $@
test-turn-control: build/test-turn-control build/fake-multimodal-helper
	$(abspath build/test-turn-control) $(abspath build/fake-multimodal-helper)
sanitize-turn-control: build/test-turn-control-sanitize build/fake-multimodal-helper
	$(abspath build/test-turn-control-sanitize) $(abspath build/fake-multimodal-helper)
build/test-turn-control-real: tests/test_turn_control_real.c include/samosa_inference/turn_control.h build/libsamosa-inference-specialist.a
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude $< build/libsamosa-inference-specialist.a -o $@
build/test-turn-control-real-sanitize: tests/test_turn_control_real.c src/turn_control.c src/specialist_supervisor.c src/specialist_protocol.c src/engine_process.c src/engine_process.h src/json.h include/samosa_inference/turn_control.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -pthread -Iinclude -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_turn_control_real.c src/turn_control.c src/specialist_supervisor.c src/specialist_protocol.c src/engine_process.c -o $@
.PHONY: test-whisper-host sanitize-whisper-host
build/test-whisper-host: tests/test_whisper_host.c tests/whisper_fixture.h src/registry_host.c $(REGISTRY_SOURCES) $(REGISTRY_HEADERS) $(DECISION_TOKENIZER_ARCHIVE) | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) $(REGISTRY_FLAGS) -Isrc $< src/registry_host.c $(REGISTRY_SOURCES) $(DECISION_TOKENIZER_ARCHIVE) $(DECISION_RUST_LIBS) $(LIBS) $(CURL_LIBS) $(DL_LIBS) -pthread -o $@
build/test-whisper-host-sanitize: tests/test_whisper_host.c tests/whisper_fixture.h src/registry_host.c $(REGISTRY_SOURCES) $(REGISTRY_HEADERS) $(DECISION_TOKENIZER_ARCHIVE) | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) $(REGISTRY_FLAGS) -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer $< src/registry_host.c $(REGISTRY_SOURCES) $(DECISION_TOKENIZER_ARCHIVE) $(DECISION_RUST_LIBS) $(LIBS) $(CURL_LIBS) $(DL_LIBS) -pthread -o $@
test-whisper-host: build/test-whisper-host build/test-whisper build/samosa-inference-providers
	$(abspath build/test-whisper-host) $(abspath build/test-whisper) $(abspath build/samosa-inference-providers)
sanitize-whisper-host: build/test-whisper-host-sanitize build/test-whisper build/samosa-inference-providers-sanitize
	$(abspath build/test-whisper-host-sanitize) $(abspath build/test-whisper) $(abspath build/samosa-inference-providers-sanitize)
test-native: test-whisper-host
sanitize-native: sanitize-whisper-host
.PHONY: test-backend-json sanitize-backend-json
build/test-backend-json: tests/test_backend_json.c include/samosa_inference/backend_json.h build/libsamosa-inference-specialist.a | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude $< build/libsamosa-inference-specialist.a -o $@
build/test-backend-json-sanitize: tests/test_backend_json.c src/backend_json.c src/turn_control.c src/specialist_supervisor.c src/specialist_protocol.c src/engine_process.c src/vision_budget.c include/samosa_inference/backend_json.h include/samosa_inference/turn_control.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -pthread -Iinclude -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer $(filter %.c,$^) -o $@
test-backend-json: build/test-backend-json
	$(abspath build/test-backend-json)
sanitize-backend-json: build/test-backend-json-sanitize
	$(abspath build/test-backend-json-sanitize)
test-native: test-backend-json
sanitize-native: sanitize-backend-json
.PHONY: test-backend-lifecycle sanitize-backend-lifecycle
build/test-backend-lifecycle: tests/test_backend_lifecycle.c tests/test_backend_process.c src/backend_process_internal.h src/activation.c src/activation.h src/input_identity.h src/artifact.c src/artifact.h src/engine_runtime.c src/engine_runtime.h src/sha256.h include/samosa_inference/backend_process.h build/libsamosa-inference-specialist.a | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude -DSAMOSA_BACKEND_LIFECYCLE_API tests/test_backend_lifecycle.c src/activation.c src/artifact.c src/engine_runtime.c build/libsamosa-inference-specialist.a $(LIBS) -o $@
build/test-backend-lifecycle-sanitize: tests/test_backend_lifecycle.c tests/test_backend_process.c src/backend_process.c src/backend_process_internal.h src/llama_launch.c src/engine_process.c src/engine_process.h src/json.h src/activation.c src/activation.h src/input_identity.h src/artifact.c src/artifact.h src/engine_runtime.c src/engine_runtime.h src/sha256.h include/samosa_inference/backend_process.h | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -pthread -Iinclude -DSAMOSA_BACKEND_LIFECYCLE_API -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer tests/test_backend_lifecycle.c src/backend_process.c src/llama_launch.c src/engine_process.c src/activation.c src/artifact.c src/engine_runtime.c $(LIBS) -o $@
test-backend-lifecycle: build/test-backend-lifecycle
	$(abspath build/test-backend-lifecycle) $(abspath build/test-backend-lifecycle)
sanitize-backend-lifecycle: build/test-backend-lifecycle-sanitize build/test-backend-lifecycle
	$(abspath build/test-backend-lifecycle-sanitize) $(abspath build/test-backend-lifecycle)
test-native: test-backend-lifecycle
sanitize-native: sanitize-backend-lifecycle

PRIMARY_HOST_SOURCES = src/whisper_activation.c src/whisper_lifecycle.c src/primary_host.c src/activation.c src/artifact.c $(LIBRARY_RUNTIME_SOURCES)
PRIMARY_HOST_HEADERS = src/whisper_activation.h src/whisper_audio.h src/whisper_process_internal.h include/samosa_inference/whisper_lifecycle.h include/samosa_inference/whisper_process.h include/samosa_inference/primary_host.h src/backend_process_internal.h src/activation.h src/input_identity.h src/artifact.h $(LIBRARY_RUNTIME_HEADERS)
.PHONY: primary-host test-primary-host sanitize-primary-host
primary-host: build/libsamosa-inference-primary.a
PRIMARY_HOST_OBJECTS = $(patsubst src/%.c,build/primary-%.o,$(PRIMARY_HOST_SOURCES))
build/primary-%.o: src/%.c $(PRIMARY_HOST_HEADERS) src/json.h src/sha256.h | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude -Isrc -c $< -o $@
build/libsamosa-inference-primary.a: $(PRIMARY_HOST_OBJECTS) $(SPECIALIST_OBJECTS) Makefile
	$(AR) rcs $@ $(filter %.o,$^)
build/test-primary-host: tests/test_primary_host.c tests/test_backend_process.c $(PRIMARY_HOST_HEADERS) build/libsamosa-inference-primary.a | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude -Isrc $< build/libsamosa-inference-primary.a $(LIBS) -o $@
build/test-primary-host-sanitize: tests/test_primary_host.c tests/test_backend_process.c $(PRIMARY_HOST_SOURCES) $(PRIMARY_HOST_HEADERS) src/backend_process.c src/backend_json.c src/llama_launch.c src/engine_process.c src/whisper_process.c src/turn_control.c src/vision_budget.c src/specialist_supervisor.c src/specialist_protocol.c | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -pthread -Iinclude -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer $(filter %.c,$(filter-out tests/test_backend_process.c,$^)) $(LIBS) -o $@
test-primary-host: build/test-primary-host
	$(abspath build/test-primary-host) $(abspath build/test-primary-host) $$(shasum -a 256 build/test-primary-host | cut -d ' ' -f 1)
sanitize-primary-host: build/test-primary-host-sanitize build/test-primary-host
	$(abspath build/test-primary-host-sanitize) $(abspath build/test-primary-host) $$(shasum -a 256 build/test-primary-host | cut -d ' ' -f 1)

build/test-whisper-lifecycle-actual: tests/test_whisper_lifecycle.c tests/whisper_fixture.h $(PRIMARY_HOST_HEADERS) build/libsamosa-inference-primary.a | build
	$(CC) $(CPPFLAGS) $(CFLAGS) $(WARN) -pthread -Iinclude -Isrc $< build/libsamosa-inference-primary.a $(LIBS) -o $@
build/test-whisper-lifecycle-actual-sanitize: tests/test_whisper_lifecycle.c tests/whisper_fixture.h $(PRIMARY_HOST_HEADERS) src/whisper_activation.c src/whisper_lifecycle.c src/whisper_process.c src/engine_process.c src/activation.c src/artifact.c src/engine_runtime.c | build
	$(CC) $(CPPFLAGS) -O1 -g $(WARN) -pthread -Iinclude -Isrc -fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer $(filter %.c,$^) $(LIBS) -o $@
