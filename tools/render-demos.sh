#!/usr/bin/env bash
set -euo pipefail

root_dir=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
build_dir="$root_dir/build"
demos_dir="$build_dir/demos"
audio_dir="$root_dir/artifacts/audio"
renderer_bin="$build_dir/core/tools/render/undertow_render"

cmake --build "$build_dir/core" --target undertow_render

rm -rf "$demos_dir"
mkdir -p "$demos_dir" "$audio_dir"

render() {
  local preset=$1
  local out=$2
  "$renderer_bin" --preset "$preset" --bars 8 --bpm 128 --note 45 --sample-rate 48000 --out "$out"
}

presets=(
  init
  sub-pure-foundation
  sub-low-pressure
  sub-kick-companion
  bass-hypnotic-roller
  bass-acid-undertow
  bass-dub-pulse
  bass-warehouse-drone
  rumble-dark-room
  rumble-tunnel-floor
  rumble-sidechain-fog
  seq-evolving-minor
  seq-phrygian-pulse
)

for preset in "${presets[@]}"; do
  render "$preset" "$demos_dir/$preset.wav"
done

for showcase in init sub-pure-foundation bass-hypnotic-roller rumble-dark-room; do
  render "$showcase" "$audio_dir/$showcase.wav"
done
