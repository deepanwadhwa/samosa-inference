#!/bin/sh
# Build pinned native MLX libraries/kernels for independent inference engines.
# All external sources are pinned and materialized into a new owned directory.
set -eu
[ "$#" = 5 ] || { echo "usage: $0 MLX_CHECKOUT FMT_CHECKOUT JSON_ARCHIVE METAL_CPP_ARCHIVE NEW_OUTPUT" >&2; exit 2; }
[ "$(uname -s):$(uname -m)" = Darwin:arm64 ] || { echo 'requires macOS arm64' >&2; exit 2; }
for mlx_arg in "$@"; do case "$mlx_arg" in /*) ;; *) echo 'all paths must be absolute' >&2; exit 2;; esac; done
mlx_revision=7a1d4f5c12ac82f4b4d0a6e71538d89ca0605247
mlx_fmt_revision=407c905e45ad75fc29bf0f9bb7c5c2fd3475976f
[ "$(git -C "$1" rev-parse HEAD)" = "$mlx_revision" ]
[ "$(git -C "$2" rev-parse HEAD)" = "$mlx_fmt_revision" ]
[ -z "$(git -C "$1" status --porcelain --untracked-files=no)" ]
[ -z "$(git -C "$2" status --porcelain --untracked-files=no)" ]
mlx_json_pin=d6c65aca6b1ed68e7a182f4757257b107ae403032760ed6ef121c9d55e81757d
mlx_metal_pin=4df3c078b9aadcb516212e9cb03004cbc5ce9a3e9c068fa3144d021db585a3a4
mlx_observed=$(cmake -E sha256sum "$3"); [ "${mlx_observed%% *}" = "$mlx_json_pin" ]
mlx_observed=$(cmake -E sha256sum "$4"); [ "${mlx_observed%% *}" = "$mlx_metal_pin" ]
mlx_repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
(umask 077; mkdir "$5")
mlx_output=$(CDPATH= cd -- "$5" && pwd)
mkdir "$mlx_output/mlx" "$mlx_output/fmt" "$mlx_output/deps" "$mlx_output/bin"
git -C "$1" archive "$mlx_revision" | tar -xf - -C "$mlx_output/mlx"
git -C "$2" archive "$mlx_fmt_revision" | tar -xf - -C "$mlx_output/fmt"
tar -xf "$3" -C "$mlx_output/deps"
unzip -q "$4" -d "$mlx_output/deps"
# Change only the compiled fallback, not MLX_METAL_PATH: upstream also uses
# that CMake variable as the kernel compiler's output destination.
mlx_metal_cmake="$mlx_output/mlx/mlx/backend/metal/CMakeLists.txt"
sed 's@PRIVATE METAL_PATH="${MLX_METAL_PATH}/mlx.metallib"@PRIVATE METAL_PATH="/samosa-inference-no-fallback/mlx.metallib"@' "$mlx_metal_cmake" > "$mlx_metal_cmake.package"
if cmp -s "$mlx_metal_cmake" "$mlx_metal_cmake.package"; then
    echo 'pinned MLX fallback definition missing' >&2; exit 2
fi
mv "$mlx_metal_cmake.package" "$mlx_metal_cmake"
# Colocated mlx.metallib is the package-owned runtime dependency. Disable the
# upstream build-directory fallback so a missing package kernel cannot borrow it.
cmake -S "$mlx_output/mlx" -B "$mlx_output/build" \
    -DCMAKE_BUILD_TYPE=Release -DBUILD_SHARED_LIBS=OFF \
    -DMLX_BUILD_PYTHON_BINDINGS=OFF -DMLX_BUILD_METAL=ON \
    -DMLX_BUILD_SAFETENSORS=ON -DMLX_BUILD_GGUF=OFF \
    -DMLX_BUILD_TESTS=OFF -DMLX_BUILD_EXAMPLES=OFF \
    -DFETCHCONTENT_FULLY_DISCONNECTED=ON \
    -DFETCHCONTENT_SOURCE_DIR_JSON="$mlx_output/deps/json" \
    -DFETCHCONTENT_SOURCE_DIR_METAL_CPP="$mlx_output/deps/metal-cpp" \
    -DFETCHCONTENT_SOURCE_DIR_FMT="$mlx_output/fmt"
cmake --build "$mlx_output/build" --target mlx --parallel 4
cp "$mlx_output/build/mlx/backend/metal/kernels/mlx.metallib" "$mlx_output/bin/mlx.metallib"
chmod 0400 "$mlx_output/bin/mlx.metallib"
cp "$mlx_output/mlx/LICENSE" "$mlx_output/LICENSE.MLX"
cp "$mlx_output/fmt/LICENSE" "$mlx_output/LICENSE.fmt"
cp "$mlx_output/deps/json/LICENSE.MIT" "$mlx_output/LICENSE.nlohmann-json"
cp "$mlx_output/deps/metal-cpp/LICENSE.txt" "$mlx_output/LICENSE.metal-cpp"
printf '%s\n' "$mlx_revision" > "$mlx_output/mlx-revision"
printf '%s\n' "$mlx_fmt_revision" > "$mlx_output/fmt-revision"
printf 'Built pinned native MLX library and kernels: %s\n' "$mlx_output"
