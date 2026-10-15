#!/bin/sh
# Reuse Samosa's pinned whisper.cpp source with a closed CPU-only build profile.
# Build-time tooling only; the installed engine has no compiler/Python dependency.
set -eu
[ "$#" = 2 ] || { echo "usage: $0 PINNED_SOURCE_ARCHIVE NEW_PRIVATE_OUTPUT" >&2; exit 2; }
case "$1" in /*) ;; *) echo 'source archive must be absolute' >&2; exit 2;; esac
case "$2" in /*) ;; *) echo 'output must be absolute' >&2; exit 2;; esac
whisper_source_pin=d8cd961352377b1cc612224016a9ebdfe0ae508dc2b2f9ef514b341d672e3fdc
whisper_observed=$(cmake -E sha256sum "$1")
[ "${whisper_observed%% *}" = "$whisper_source_pin" ] || { echo 'Whisper source checksum mismatch' >&2; exit 2; }
# Exclusive output ownership prevents overwriting an existing runtime/build tree.
(umask 077; mkdir "$2")
whisper_output=$(CDPATH= cd -- "$2" && pwd)
mkdir "$whisper_output/source" "$whisper_output/bin"
tar -xzf "$1" -C "$whisper_output/source" --strip-components 1
whisper_deployment=
if [ "$(uname -s)" = Darwin ]; then whisper_deployment=-DCMAKE_OSX_DEPLOYMENT_TARGET=14.0; fi
# The legacy GPU-capable engine hits the existing file cap while initializing
# embedded Metal code on a cold install. Qualify an explicit CPU profile instead.
cmake -S "$whisper_output/source" -B "$whisper_output/build" \
    -DCMAKE_BUILD_TYPE=Release ${whisper_deployment:+"$whisper_deployment"} \
    -DBUILD_SHARED_LIBS=OFF -DGGML_BACKEND_DL=OFF -DGGML_NATIVE=OFF \
    -DGGML_METAL=OFF -DGGML_BLAS=OFF -DGGML_OPENMP=OFF \
    -DWHISPER_COREML=OFF -DWHISPER_OPENVINO=OFF \
    -DWHISPER_BUILD_EXAMPLES=ON -DWHISPER_BUILD_TESTS=OFF -DWHISPER_BUILD_SERVER=OFF
cmake --build "$whisper_output/build" --target whisper-cli --parallel 4
cp "$whisper_output/build/bin/whisper-cli" "$whisper_output/bin/whisper-cli"
chmod 0500 "$whisper_output/bin/whisper-cli"
printf '%s\n' f049fff95a089aa9969deb009cdd4892b3e74916 > "$whisper_output/whisper-revision"
printf '%s\n' "$whisper_source_pin" > "$whisper_output/source-sha256"
cp "$whisper_output/source/LICENSE" "$whisper_output/LICENSE.whisper.cpp"
printf 'Built CPU-only Whisper runtime: %s\n' "$whisper_output/bin/whisper-cli"
