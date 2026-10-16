#!/bin/sh
# One command list for local pre-push checks and the hosted native CI matrix.
# An optional exported source directory keeps checks isolated from live builds.
set -eu
# Fixture executables must satisfy the same no-group-write artifact policy as
# production engines, including under container users with a default 0002 mask.
umask 022
[ "$#" -le 1 ] || { echo "usage: $0 [SOURCE_DIRECTORY]" >&2; exit 2; }
if [ "$#" = 1 ]; then
    native_check_source=$(CDPATH= cd -- "$1" && pwd)
else
    native_check_source=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
fi
uname -a
"${CC:-clang}" --version
"${CXX:-c++}" --version
# The private decision tokenizer is built even before provider registration so
# its C ABI and locked dependencies cannot silently break on either platform.
cargo +1.90.0 fmt --manifest-path "$native_check_source/engines/decision/tokenizer/Cargo.toml" -- --check
cargo +1.90.0 test --locked --manifest-path "$native_check_source/engines/decision/tokenizer/Cargo.toml"
cargo +1.90.0 build --locked --release --manifest-path "$native_check_source/engines/decision/tokenizer/Cargo.toml"
make -C "$native_check_source" CC="${CC:-clang}" CURL_LIBS=-lcurl test sanitize
make -C "$native_check_source" CC="${CC:-clang}" CURL_LIBS=-lcurl test-registry-modalities sanitize-registry-modalities
make -C "$native_check_source" CC="${CC:-clang}" CURL_LIBS=-lcurl test-registry-cli sanitize-registry-cli
make -C "$native_check_source" CC="${CC:-clang}" CURL_LIBS=-lcurl test-speech-public sanitize-speech-public
# Actual model execution additionally requires pinned model and Sources assets.
make -C "$native_check_source" CC="${CC:-clang}" build/test-whisper-real build/test-whisper-real-sanitize build/test-vision-real build/test-vision-real-sanitize build/test-temporal-public-real build/test-temporal-public-real-sanitize
make -C "$native_check_source" CC="${CC:-clang}" CURL_LIBS=-lcurl test-native sanitize-native
make -C "$native_check_source" CC="${CC:-clang}" CURL_LIBS=-lcurl test-summary sanitize-summary
make -C "$native_check_source" CC="${CC:-clang}" CURL_LIBS=-lcurl test-whisper sanitize-whisper
make -C "$native_check_source" CC="${CC:-clang}" CURL_LIBS=-lcurl test-vision sanitize-vision
