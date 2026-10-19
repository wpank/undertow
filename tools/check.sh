#!/bin/bash
# Local check: build, test and (with --plugin) validate the plugin.
#
#   tools/check.sh           core preset: build + test
#   tools/check.sh --plugin  also the plugin preset, its tests, and pluginval
#
# Stops at the first failure with its exit code.
set -euo pipefail
cd "$(dirname "$0")/.."

cmake --preset core
cmake --build --preset core
ctest --preset core

if [ "${1:-}" = "--plugin" ]; then
    cmake --preset plugin
    cmake --build --preset plugin
    ctest --preset plugin
    tools/pluginval.sh 5
fi

echo "check: OK"
