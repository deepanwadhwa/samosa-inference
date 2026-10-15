#!/bin/sh
# Build the existing Molmo2 engine on the shared pinned MLX source profile.
set -eu
[ "$#" = 5 ] || { echo "usage: $0 MLX_CHECKOUT FMT_CHECKOUT JSON_ARCHIVE METAL_CPP_ARCHIVE NEW_OUTPUT" >&2; exit 2; }
molmo_repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sh "$molmo_repo/tools/build_mlx_runtime.sh" "$@"
molmo_output=$(CDPATH= cd -- "$5" && pwd)
# Older SDKs use MLX's no_jaccl implementation instead of a companion archive.
set --
if [ -f "$molmo_output/build/jaccl/libjaccl.a" ]; then set -- "$molmo_output/build/jaccl/libjaccl.a"; fi
"${CXX:-c++}" -std=c++20 -O2 -Wall -Wextra \
    -mmacosx-version-min="$(sw_vers -productVersion)" \
    -I"$molmo_output/mlx" -I"$molmo_output/build" \
    "$molmo_repo/engines/molmo2/samosa_molmo2.cpp" \
    "$molmo_repo/engines/molmo2/molmo2_model.cpp" \
    "$molmo_repo/engines/molmo2/molmo2_processor.cpp" \
    "$molmo_repo/engines/molmo2/molmo2_contract.cpp" \
    "$molmo_repo/engines/molmo2/molmo2_media.mm" \
    "$molmo_output/build/libmlx.a" "$@" \
    -framework Foundation -framework Metal -framework Accelerate \
    -framework AVFoundation -framework CoreMedia -framework CoreGraphics -lc++ \
    -o "$molmo_output/bin/samosa-molmo2"
chmod 0500 "$molmo_output/bin/samosa-molmo2"
printf 'Built independent Molmo2 engine and colocated kernels: %s\n' "$molmo_output/bin"
