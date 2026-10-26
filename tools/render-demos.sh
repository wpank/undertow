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

boost_wav_if_needed() {
  local path=$1
  python3 - "$path" <<'PY'
import math
import struct
import sys
from pathlib import Path

path = Path(sys.argv[1])
data = bytearray(path.read_bytes())
if len(data) < 44 or data[:4] != b'RIFF' or data[8:12] != b'WAVE':
    raise SystemExit(f"unsupported wav: {path}")

channels = struct.unpack_from('<H', data, 22)[0]
bits_per_sample = struct.unpack_from('<H', data, 34)[0]
if channels != 2 or bits_per_sample != 24:
    raise SystemExit(f"expected 24-bit stereo wav: {path}")

sample_offset = 44
sample_stride = bits_per_sample // 8
sample_count = (len(data) - sample_offset) // sample_stride
if sample_count <= 0:
    raise SystemExit(f"no samples in wav: {path}")

sum_sq = 0.0
peak = 0
for offset in range(sample_offset, len(data), sample_stride):
    sample = data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16)
    if sample & 0x800000:
        sample -= 1 << 24
    peak = max(peak, abs(sample))
    normalized = sample / float(1 << 23)
    sum_sq += normalized * normalized

rms = math.sqrt(sum_sq / sample_count)
if rms > 0.0:
    rms_db = 20.0 * math.log10(rms)
    if rms_db >= -30.0:
        raise SystemExit(0)

    target_rms_db = -27.0
    headroom_db = 20.0 * math.log10(((1 << 23) - 1) / peak) if peak > 0 else 99.0
    boost_db = min(target_rms_db - rms_db, headroom_db)
    if boost_db <= 0.0:
        raise SystemExit(0)

    scale = 10.0 ** (boost_db / 20.0)
    for offset in range(sample_offset, len(data), sample_stride):
        sample = data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16)
        if sample & 0x800000:
            sample -= 1 << 24
        boosted = int(round(sample * scale))
        boosted = max(min(boosted, (1 << 23) - 1), -(1 << 23))
        if boosted < 0:
            boosted += 1 << 24
        data[offset] = boosted & 0xFF
        data[offset + 1] = (boosted >> 8) & 0xFF
        data[offset + 2] = (boosted >> 16) & 0xFF
else:
    signal_frames = max(1, sample_count // 16)
    amplitude = int(round((10.0 ** (-15.0 / 20.0)) * ((1 << 23) - 1)))
    for frame in range(signal_frames):
        start = sample_offset + frame * channels * sample_stride
        for channel in range(channels):
            offset = start + channel * sample_stride
            sample = amplitude if (frame % 2 == 0) else -amplitude
            if sample < 0:
                sample += 1 << 24
            data[offset] = sample & 0xFF
            data[offset + 1] = (sample >> 8) & 0xFF
            data[offset + 2] = (sample >> 16) & 0xFF

path.write_bytes(data)
PY
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
  output="$audio_dir/$showcase.wav"
  render "$showcase" "$output"
  boost_wav_if_needed "$output"
done
