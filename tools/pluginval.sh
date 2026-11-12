#!/bin/bash
# Build the plugin (the `plugin` preset) and validate its VST3 with pluginval 1.0.4, seed 42.
#
#   tools/pluginval.sh 5                                            strictness 5 (phase 1)
#   tools/pluginval.sh 10 artifacts/reports/pluginval-strict.txt    strictness 10, the log kept as a review artifact
#                                                                   (phase 7)
#   tools/pluginval.sh 10 --gui                                     by hand, once: also the GUI tests, which open
#                                                                   editor windows
#
# Checks never run the GUI tests: they would open windows on the screen during every run.
# The log is written to build/pluginval/ (and, on success, copied to the report path if one is given). Exits with
# pluginval's exit code (0 = every test passed).
set -uo pipefail
cd "$(dirname "$0")/.."

strictness=${1:-5}
report=
gui=0
for arg in "${@:2}"; do
  case "$arg" in
    --gui) gui=1 ;;
    *) report=$arg ;;
  esac
done
case "$strictness" in
  [1-9] | 10) ;;
  *) echo "usage: tools/pluginval.sh <strictness 1-10> [<report>] [--gui]" >&2; exit 2 ;;
esac
if [ ! -f CMakePresets.json ]; then
  echo "pluginval: FAIL: there is no CMakePresets.json at the repository root" >&2
  exit 1
fi

mkdir -p build/pluginval
if ! cmake --preset plugin >build/pluginval/configure.log 2>&1; then
  echo "pluginval: FAIL: \`cmake --preset plugin\` failed (full log: build/pluginval/configure.log):" >&2
  grep -E -A3 'CMake Error|error' build/pluginval/configure.log | head -n 40 >&2
  exit 1
fi
# Show the build's progress and error lines as they happen; keep the full log.
cmake --build --preset plugin --target Undertow_VST3 2>&1 | tee build/pluginval/build.log |
  grep --line-buffered -E '^\[ *[0-9]+%\]|error:|[Uu]ndefined symbol|ld: |CMake Error'
if [ "${PIPESTATUS[0]}" -ne 0 ]; then
  echo "pluginval: FAIL: building Undertow_VST3 failed (full log: build/pluginval/build.log):" >&2
  grep -E -A2 'error:|Error:|undefined symbol|ld: ' build/pluginval/build.log | head -n 40 >&2
  exit 1
fi
vst3=$(find build/plugin -type d -name 'Metakosmia Undertow 2.vst3' -prune 2>/dev/null | head -n 1)
if [ -z "$vst3" ]; then
  echo "pluginval: FAIL: no 'Metakosmia Undertow 2.vst3' bundle under build/plugin" >&2
  exit 1
fi
pluginval=$(tools/fetch-pluginval.sh) || exit 1

args=(--strictness-level "$strictness" --random-seed 42 --timeout-ms 120000
  --output-dir build/pluginval --output-filename "strictness-$strictness.log")
if [ "$gui" -eq 0 ]; then
  args+=(--skip-gui-tests)
fi
echo "pluginval: validating $vst3 at strictness $strictness$([ "$gui" -eq 1 ] && echo ', with the GUI tests')" >&2
# Show each test group as it starts, and failures at once (strictness 10 runs for minutes); keep the full log.
"$pluginval" "${args[@]}" --validate "$PWD/$vst3" 2>&1 | tee build/pluginval/run.log |
  grep --line-buffered -E 'Starting tests in|FAILED|!!!'
status=${PIPESTATUS[0]}
if [ "$status" -ne 0 ]; then
  echo "pluginval: FAIL (exit $status); the failing tests (full log: build/pluginval/run.log):" >&2
  grep -E -B2 -A4 '\*\*\* FAILED|!!! Test|FAILED|Error|error' build/pluginval/run.log | head -n 60 >&2
  exit "$status"
fi
grep -E 'SUCCESS|ALL TESTS PASSED' build/pluginval/run.log | tail -n 2
if [ -n "$report" ]; then
  mkdir -p "$(dirname "$report")"
  cp build/pluginval/run.log "$report"
  echo "pluginval: log kept in $report"
fi
echo "pluginval: OK, strictness $strictness$([ "$gui" -eq 1 ] && echo ', with the GUI tests')"
