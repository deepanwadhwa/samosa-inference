#!/bin/sh
# Build the existing VisionPsy engine on the shared pinned MLX source profile.
set -eu
[ "$#" = 5 ] || { echo "usage: $0 MLX_CHECKOUT FMT_CHECKOUT JSON_ARCHIVE METAL_CPP_ARCHIVE NEW_OUTPUT" >&2; exit 2; }
vision_repo=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)
sh "$vision_repo/tools/build_mlx_runtime.sh" "$@"
vision_output=$(CDPATH= cd -- "$5" && pwd)
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
chmod 0500 "$vision_output/bin/samosa-visionpsy"
printf 'Built independent VisionPsy engine and colocated kernels: %s\n' "$vision_output/bin"
