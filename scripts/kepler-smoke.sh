#!/usr/bin/env bash
set -euo pipefail
if [[ $# != 4 ]]; then
    echo 'usage: kepler-smoke.sh <build-dir> <VulkanN> <model.gguf> <fresh-record-dir>' >&2
    exit 2
fi
repo_root=$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/.." && pwd)
build_dir=$(realpath -- "$1")
device=$2
model=$(realpath -- "$3")
record_dir=$4
[[ "$device" =~ ^Vulkan[0-9]+$ && -f "$model" ]] || exit 2
[[ -x "$build_dir/bin/llama-bench" && -x "$build_dir/bin/test-backend-ops" ]] || exit 2
[[ ! -e "$record_dir" ]] || { echo 'record directory already exists' >&2; exit 2; }
mkdir -p -- "$record_dir"
record_dir=$(realpath -- "$record_dir")
{
    git -C "$repo_root" rev-parse HEAD
    git -C "$repo_root" status --short
    uname -r
    printf 'VK_DRIVER_FILES=%s\ndevice=%s\n' "${VK_DRIVER_FILES:-unavailable}" "$device"
    sha256sum -- "$model" "$build_dir/bin/llama-bench" "$build_dir/bin/test-backend-ops"
} > "$record_dir/environment.txt"
run() {
    local name=$1
    shift
    { printf '%q ' "$@"; printf '\n'; } > "$record_dir/$name.command"
    local status=0
    timeout --signal=TERM --kill-after=10s 600s "$@" > "$record_dir/$name.stdout" 2> "$record_dir/$name.stderr" || status=$?
    printf '%s\n' "$status" > "$record_dir/$name.exit"
    if [[ "$status" != 0 ]]; then
        tail -30 "$record_dir/$name.stderr" >&2
        echo "$name failed; records: $record_dir" >&2
        exit "$status"
    fi
    echo "$name: OK"
}
run devices "$build_dir/bin/llama-bench" --list-devices
run correctness env MESA_SHADER_CACHE_DISABLE=true GGML_VK_KEPLER=1 GGML_VK_KEPLER_TRACE=1 \
    "$build_dir/bin/test-backend-ops" test -b "$device" -o MUL_MAT --test-file "$repo_root/docs/backend/kepler-smoke.ops"
if ! grep -q 'Kepler tuning on' "$record_dir/correctness.stderr"; then
    echo 'Selected device did not enable Kepler tuning; check GPU and NVK identification.' >&2
    exit 2
fi
index=0
for enabled in 0 1 1 0; do
    index=$((index + 1))
    run "decode-$index-$enabled" env -u GGML_VK_KEPLER_TRACE -u GGML_VK_PERF_LOGGER -u GGML_VK_PIPELINE_STATS \
        MESA_SHADER_CACHE_DISABLE=true GGML_VK_KEPLER="$enabled" GGML_VK_KEPLER_WIDE=1 \
        "$build_dir/bin/llama-bench" -m "$model" -dev "$device" -ngl 99 -t 8 -b 32 -ub 32 \
        -ctk f16 -ctv f16 -fa off -r 1 -p 0 -n 128 -o json -v
done
echo "Results: $record_dir"
