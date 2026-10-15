#!/bin/sh
# Build the existing encoder/decoder T5 sidecar with a closed CPU-only runtime.
# Build-time tooling only; installed inference packages need neither CMake nor C++.
set -eu
[ "$#" = 2 ] || { echo "usage: $0 PINNED_PRISM_CHECKOUT PRIVATE_OUTPUT" >&2; exit 2; }
t5_source=$(CDPATH= cd -- "$1" && pwd)
t5_revision=9fcaed763ccda38ea81068ad9d7f991aaddca451
t5_module=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
[ "$(git -C "$t5_source" rev-parse HEAD)" = "$t5_revision" ] || { echo 'unexpected Prism revision' >&2; exit 2; }
git -C "$t5_source" diff --quiet HEAD -- . || { echo 'modified Prism source refuses' >&2; exit 2; }
# Never modify an existing tree. The caller retains this build/output directory.
(umask 077; mkdir "$2")
t5_output=$(CDPATH= cd -- "$2" && pwd)
mkdir "$t5_output/source" "$t5_output/bin"
git -C "$t5_source" archive --format=tar --output="$t5_output/source.tar" "$t5_revision"
tar -xf "$t5_output/source.tar" -C "$t5_output/source"
t5_deployment=
if [ "$(uname -s)" = Darwin ]; then t5_deployment=-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0; fi
# SummaryConfig already requests CPU inference. Excluding Metal avoids its cold
# compiler/cache side effects and keeps the existing process file cap intact.
cmake -S "$t5_output/source" -B "$t5_output/build" \
    -DCMAKE_BUILD_TYPE=Release ${t5_deployment:+"$t5_deployment"} \
    -DBUILD_SHARED_LIBS=OFF -DGGML_BACKEND_DL=OFF -DGGML_NATIVE=OFF \
    -DGGML_METAL=OFF -DGGML_BLAS=OFF -DGGML_OPENMP=OFF \
    -DLLAMA_OPENSSL=OFF -DLLAMA_BUILD_TESTS=OFF -DLLAMA_BUILD_EXAMPLES=OFF \
    -DLLAMA_BUILD_TOOLS=OFF -DLLAMA_BUILD_SERVER=OFF
cmake --build "$t5_output/build" --target llama --parallel 4
t5_compiler=${CXX:-c++}
if [ "$(uname -s)" = Darwin ]; then
    "$t5_compiler" -O3 -std=c++17 -mmacosx-version-min=14.0 \
        -I"$t5_output/source/include" -I"$t5_output/source/ggml/include" \
        "$t5_module/engines/t5_summary.cpp" \
        "$t5_output/build/src/libllama.a" "$t5_output/build/ggml/src/libggml.a" \
        "$t5_output/build/ggml/src/libggml-cpu.a" "$t5_output/build/ggml/src/libggml-base.a" \
        -framework Accelerate -pthread -o "$t5_output/bin/samosa-summarizer"
else
    "$t5_compiler" -O3 -std=c++17 \
        -I"$t5_output/source/include" -I"$t5_output/source/ggml/include" \
        "$t5_module/engines/t5_summary.cpp" \
        "$t5_output/build/src/libllama.a" "$t5_output/build/ggml/src/libggml.a" \
        "$t5_output/build/ggml/src/libggml-cpu.a" "$t5_output/build/ggml/src/libggml-base.a" \
        -pthread -lm -ldl -o "$t5_output/bin/samosa-summarizer"
fi
printf '%s\n' "$t5_revision" > "$t5_output/prism-revision"
chmod 0500 "$t5_output/bin/samosa-summarizer"
printf 'Built CPU-only T5 runtime: %s\n' "$t5_output/bin/samosa-summarizer"
