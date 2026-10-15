#!/bin/sh
# Build the existing native VisionPsy engine without a Samosa Chat checkout.
# All external sources are pinned and materialized into a new owned directory.
set -eu
[ "$#" = 5 ] || { echo "usage: $0 MLX_CHECKOUT FMT_CHECKOUT JSON_ARCHIVE METAL_CPP_ARCHIVE NEW_OUTPUT" >&2; exit 2; }
[ "$(uname -s):$(uname -m)" = Darwin:arm64 ] || { echo 'requires macOS arm64' >&2; exit 2; }
for vision_arg in "$@"; do case "$vision_arg" in /*) ;; *) echo 'all paths must be absolute' >&2; exit 2;; esac; done
vision_mlx_revision=7a1d4f5c12ac82f4b4d0a6e71538d89ca0605247
vision_fmt_revision=407c905e45ad75fc29bf0f9bb7c5c2fd3475976f
[ "$(git -C "$1" rev-parse HEAD)" = "$vision_mlx_revision" ]
[ "$(git -C "$2" rev-parse HEAD)" = "$vision_fmt_revision" ]
[ -z "$(git -C "$1" status --porcelain --untracked-files=no)" ]
[ -z "$(git -C "$2" status --porcelain --untracked-files=no)" ]
vision_json_pin=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
vision_metal_pin=4df3c078b9aadcb516212e9cb03004cbc5ce9a3e9c068fa3144d021db585a3a4
vision_observed=$(cmake -E sha256sum "$3"); [ "${vision_observed%% *}" = "$vision_json_pin" ]
vision_observed=$(cmake -E sha256sum "$4"); [ "${vision_observed%% *}" = "$vision_metal_pin" ]
vision_repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
(umask 077; mkdir "$5")
vision_output=$(CDPATH= cd -- "$5" && pwd)
mkdir "$vision_output/mlx" "$vision_output/fmt" "$vision_output/deps" "$vision_output/bin"
git -C "$1" archive "$vision_mlx_revision" | tar -xf - -C "$vision_output/mlx"
git -C "$2" archive "$vision_fmt_revision" | tar -xf - -C "$vision_output/fmt"
tar -xf "$3" -C "$vision_output/deps"
unzip -q "$4" -d "$vision_output/deps"
# Change only the compiled fallback, not MLX_METAL_PATH: upstream also uses
# that CMake variable as the kernel compiler's output destination.
vision_metal_cmake="$vision_output/mlx/mlx/backend/metal/CMakeLists.txt"
sed 's@PRIVATE METAL_PATH="${MLX_METAL_PATH}/mlx.metallib"@PRIVATE METAL_PATH="/samosa-inference-no-fallback/mlx.metallib"@' "$vision_metal_cmake" > "$vision_metal_cmake.package"
if cmp -s "$vision_metal_cmake" "$vision_metal_cmake.package"; then
    echo 'pinned MLX fallback definition missing' >&2; exit 2
fi
mv "$vision_metal_cmake.package" "$vision_metal_cmake"
# Colocated mlx.metallib is the package-owned runtime dependency. Disable the
# upstream build-directory fallback so a missing package kernel cannot borrow it.
cmake -S "$vision_output/mlx" -B "$vision_output/build" \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
    -DMLX_BUILD_PYTHON_BINDINGS=OFF -DMLX_BUILD_METAL=ON \
    -DMLX_BUILD_SAFETENSORS=ON -DMLX_BUILD_GGUF=OFF \
    -DMLX_BUILD_TESTS=OFF -DMLX_BUILD_EXAMPLES=OFF \
    -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
    -DFETCHCONTENT_SOURCE_DIR_JSON="$vision_output/deps/json" \
    -DFETCHCONTENT_SOURCE_DIR_METAL_CPP="$vision_output/deps/metal-cpp" \
    -DFETCHCONTENT_SOURCE_DIR_FMT="$vision_output/fmt"
cmake --build "$vision_output/build" --target mlx --parallel 4
# Older SDKs use MLX's no_jaccl implementation instead of a companion archive.
set --
if [ -f "$vision_output/build/jaccl/libjaccl.a" ]; then set -- "$vision_output/build/jaccl/libjaccl.a"; fi
"${CXX:-c++}" -std=c++20 -O2 -Wall -Wextra \
    -mmacosx-version-min="$(sw_vers -productVersion)" \
    -I"$vision_output/mlx" -I"$vision_output/build" \
    "$vision_repo/engines/visionpsy/samosa_visionpsy.cpp" \
    "$vision_repo/engines/visionpsy/visionpsy_model.cpp" \
    "$vision_output/build/libmlx.a" "$@" \
    -framework Foundation -framework Metal -framework Accelerate -lc++ \
    -o "$vision_output/bin/samosa-visionpsy"
cp "$vision_output/build/mlx/backend/metal/kernels/mlx.metallib" "$vision_output/bin/mlx.metallib"
chmod 0500 "$vision_output/bin/samosa-visionpsy"
chmod 0400 "$vision_output/bin/mlx.metallib"
cp "$vision_output/mlx/LICENSE" "$vision_output/LICENSE.MLX"
cp "$vision_output/fmt/LICENSE" "$vision_output/LICENSE.fmt"
cp "$vision_output/deps/json/LICENSE.MIT" "$vision_output/LICENSE.nlohmann-json"
cp "$vision_output/deps/metal-cpp/LICENSE.txt" "$vision_output/LICENSE.metal-cpp"
printf '%s\n' "$vision_mlx_revision" > "$vision_output/mlx-revision"
printf '%s\n' "$vision_fmt_revision" > "$vision_output/fmt-revision"
printf 'Built independent VisionPsy engine and colocated kernels: %s\n' "$vision_output/bin"
