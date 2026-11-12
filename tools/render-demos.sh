#!/usr/bin/env bash
set -euo pipefail

root_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="$root_dir/build"
demos_dir="$build_dir/demos"
renderer_bin="$build_dir/core/tools/render/undertow_render"

cmake --build "$build_dir/core" --target undertow_render

rm -rf "$demos_dir"
mkdir -p "$demos_dir"

while IFS= read -r slug; do
  [ -n "$slug" ] || continue
  "$renderer_bin" --preset "$slug" --bars 8 --bpm 130 --sample-rate 48000 \
    --out "$demos_dir/$slug.wav" --hits "$demos_dir/$slug-hits.json"
done < <("$renderer_bin" --list)
