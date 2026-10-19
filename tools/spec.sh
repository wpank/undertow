#!/bin/bash
# Build and run one spec test executable: the acceptance tests of docs/specs/ (tests/spec/pN/<name>_test.cpp).
#
#   tools/spec.sh <phase> <name> [<test-case filter>]
#   tools/spec.sh 1 oscillator 'P1.03*'      runs the P1.03 test cases of tests/spec/p1/oscillator_test.cpp
#
# Core tests build with the `core` preset (no JUCE); a test file marked "// spec-preset: plugin" builds with
# the `plugin` preset. Output is kept short for the person or model fixing a failure: the build shows only its
# progress lines, a configure or build failure ends with only its errors (full logs in build/spec-logs/), and a
# run shows only the failed checks.
# Exits 0 only when every selected test case passes, and fails when the filter selects none.
set -uo pipefail
cd "$(dirname "$0")/.."

if [ $# -lt 2 ] || [ $# -gt 3 ]; then
  echo "usage: tools/spec.sh <phase> <name> [<test-case filter>]" >&2
  exit 2
fi
phase=$1
name=$2
filter=${3:-}
source="tests/spec/p$phase/${name}_test.cpp"
target="spec_p${phase}_${name}"
if [ ! -f "$source" ]; then
  echo "spec: no spec test $source" >&2
  exit 2
fi
preset=core
if grep -q '^// spec-preset: plugin' "$source"; then
  preset=plugin
fi
if [ ! -f CMakePresets.json ]; then
  echo "spec: FAIL $target: there is no CMakePresets.json at the repository root; phase 0" \
    "(docs/specs/phase-0-foundation.md) creates the build system and its core and plugin presets" >&2
  exit 1
fi

logs="build/spec-logs"
mkdir -p "$logs"
if ! cmake --preset "$preset" >"$logs/$preset-configure.log" 2>&1; then
  echo "spec: FAIL $target: \`cmake --preset $preset\` failed (full log: $logs/$preset-configure.log):" >&2
  grep -E -A3 'CMake Error|error' "$logs/$preset-configure.log" | head -n 40 >&2
  exit 1
fi
# The build's progress lines (`[ 42%] Building CXX object ...`) are shown as they happen, so a long plugin build
# shows its progress; the full log is kept, and a failure ends with only its errors.
cmake --build --preset "$preset" --target "$target" 2>&1 | tee "$logs/$target-build.log" |
  grep --line-buffered -E '^\[ *[0-9]+%\]'
build_status=${PIPESTATUS[0]}
if [ "$build_status" -ne 0 ]; then
  echo "spec: FAIL $target: \`cmake --build --preset $preset --target $target\` failed (full log: $logs/$target-build.log):" >&2
  if grep -q -E 'error:|Error:|undefined symbol|ld: ' "$logs/$target-build.log"; then
    grep -E -A2 'error:|Error:|undefined symbol|ld: ' "$logs/$target-build.log" | grep -v -E '^make(\[[0-9]+\])?: \*\*\*' | head -n 40 >&2
  else
    tail -n 30 "$logs/$target-build.log" >&2
  fi
  exit 1
fi

binary=$(find "build/$preset" -type f -name "$target" -perm -u+x 2>/dev/null | head -n 1)
if [ -z "$binary" ]; then
  echo "spec: FAIL $target: built, but no executable named $target under build/$preset" >&2
  exit 1
fi
args=(--no-version)
if [ -n "$filter" ]; then
  args+=("--test-case=$filter")
fi
count=$("$binary" "${args[@]}" --count 2>/dev/null | sed -n 's/.*unskipped test cases passing the current filters: *\([0-9][0-9]*\).*/\1/p')
if [ "${count:-0}" -eq 0 ]; then
  echo "spec: FAIL $target: no test case matches '${filter:-*}' in $source" >&2
  exit 1
fi
"$binary" "${args[@]}"
