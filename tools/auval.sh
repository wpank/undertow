#!/bin/bash
# Validate the installed AU with Apple's auval (instrument aumu, subtype MkUt, manufacturer Mtks) and keep its log as
# a review artifact.
#
#   tools/auval.sh       writes artifacts/reports/auval.txt; exits with auval's code (0 = validated)
#
# auval finds only registered components: install the AU first (tools/install.sh puts it in
# ~/Library/Audio/Plug-Ins/Components, where macOS registers it).
set -uo pipefail
cd "$(dirname "$0")/.."

mkdir -p artifacts/reports
auval -v aumu MkUt Mtks > artifacts/reports/auval.txt 2>&1
status=$?
if [ "$status" -ne 0 ] || ! grep -q 'AU VALIDATION SUCCEEDED' artifacts/reports/auval.txt; then
  echo "auval: FAIL (exit $status); from the log (artifacts/reports/auval.txt):" >&2
  grep -E -i 'error|fail|not found|cannot' artifacts/reports/auval.txt | head -n 20 >&2
  tail -n 10 artifacts/reports/auval.txt >&2
  exit 1
fi
grep 'AU VALIDATION SUCCEEDED' artifacts/reports/auval.txt
echo "auval: OK, log kept in artifacts/reports/auval.txt"
