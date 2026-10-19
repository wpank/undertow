#!/bin/bash
# Build the plugin (the `plugin` preset) and validate its VST3 with pluginval 1.0.4, seed 42.
#
#   tools/pluginval.sh 5     phase 0: strictness 5, without the GUI tests (they open windows on the desktop)
#   tools/pluginval.sh 10    phase 5: strictness 10, with the GUI tests
#
# The log is written to build/pluginval/. Exits with pluginval's exit code (0 = every test passed).
set -uo pipefail
cd "$(dirname "$0")/.."

strictness=${1:-5}
case "$strictness" in
  [1-9] | 10) ;;
  *) echo "usage: tools/pluginval.sh <strictness 1-10>" >&2; exit 2 ;;
esac
if [ ! -f CMakePresets.json ]; then
  echo "pluginval: FAIL: there is no CMakePresets.json; phase 0 (docs/specs/phase-0-foundation.md) creates it" >&2
  exit 1
fi

mkdir -p build/pluginval
if ! cmake --preset plugin >build/pluginval/configure.log 2>&1; then
  echo "pluginval: FAIL: \`cmake --preset plugin\` failed (full log: build/pluginval/configure.log):" >&2
  grep -E -A3 'CMake Error|error' build/pluginval/configure.log | head -n 40 >&2
  exit 1
fi
# Show the build's progress lines as they happen; keep the full log.
cmake --build --preset plugin --target Undertow_VST3 2>&1 | tee build/pluginval/build.log |
  grep --line-buffered -E '^\[ *[0-9]+%\]'
if [ "${PIPESTATUS[0]}" -ne 0 ]; then
  echo "pluginval: FAIL: building Undertow_VST3 failed (full log: build/pluginval/build.log):" >&2
  grep -E -A2 'error:|Error:|undefined symbol|ld: ' build/pluginval/build.log | head -n 40 >&2
  exit 1
fi
vst3=$(find build/plugin -type d -name 'Metakosmia Undertow.vst3' -prune 2>/dev/null | head -n 1)
if [ -z "$vst3" ]; then
  echo "pluginval: FAIL: no 'Metakosmia Undertow.vst3' bundle under build/plugin" >&2
  exit 1
fi
pluginval=$(tools/fetch-pluginval.sh) || exit 1

args=(--strictness-level "$strictness" --random-seed 42 --timeout-ms 120000
  --output-dir build/pluginval --output-filename "strictness-$strictness.log")
if [ "$strictness" -lt 10 ]; then
  args+=(--skip-gui-tests)
fi
echo "pluginval: validating $vst3 at strictness $strictness" >&2
"$pluginval" "${args[@]}" --validate "$PWD/$vst3" >build/pluginval/run.log 2>&1
status=$?
if [ "$status" -ne 0 ]; then
  echo "pluginval: FAIL (exit $status); the failing tests (full log: build/pluginval/run.log):" >&2
  grep -E -B2 -A4 '\*\*\* FAILED|!!! Test|FAILED|Error|error' build/pluginval/run.log | head -n 60 >&2
  exit "$status"
fi
grep -E 'SUCCESS|ALL TESTS PASSED' build/pluginval/run.log | tail -n 2
echo "pluginval: OK, strictness $strictness"
