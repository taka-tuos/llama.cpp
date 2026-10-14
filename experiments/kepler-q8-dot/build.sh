#!/usr/bin/env bash
set -euo pipefail
source_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)
build_dir=$(realpath "$1")
mkdir -p "$build_dir"
cc -O2 -std=c11 -D_POSIX_C_SOURCE=200809L -Wall -Wextra -Werror "$source_root/experiments/kepler-q8-dot/q8-dot.c" -lvulkan -lm -o "$build_dir/q8-dot"
glslc -fshader-stage=compute --target-env=vulkan1.2 -O -DKEPLER_WIDE_RECHECK=1 -DDATA_A_Q8_0=1 -DFLOAT_TYPE=float -DFLOAT_TYPEV2=vec2 -DB_TYPE=float -DB_TYPEV2=vec2 -DB_TYPEV4=vec4 -DD_TYPE=float -DUSE_SUBGROUP_ADD=1 "$source_root/ggml/src/ggml-vulkan/vulkan-shaders/mul_mat_vec.comp" -o "$build_dir/q8-dot.spv"
spirv-val --target-env vulkan1.2 "$build_dir/q8-dot.spv"
