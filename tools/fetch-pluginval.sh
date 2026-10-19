#!/bin/bash
# Fetch pluginval 1.0.4 (Tracktion's plugin validator, the version Metakosmia validates with) into the
# shared dependency cache, check its SHA256, unpack it once and print the path of the executable.
#
#   tools/fetch-pluginval.sh       print .../pluginval.app/Contents/MacOS/pluginval
#
# The cache is shared by every checkout and worktree: ${UNDERTOW_DEPS_DIR:-$HOME/Library/Caches/undertow}.
set -euo pipefail

PLUGINVAL_VERSION=1.0.4
PLUGINVAL_URL=https://github.com/Tracktion/pluginval/releases/download/v1.0.4/pluginval_macOS.zip
PLUGINVAL_SHA256=3c4c533bda0c5059eea3ddaea752d757ee2025041f0f47e6bcb0e87f6082b29f
DEPS=${UNDERTOW_DEPS_DIR:-$HOME/Library/Caches/undertow}
DIR="$DEPS/pluginval-$PLUGINVAL_VERSION"
BIN="$DIR/pluginval.app/Contents/MacOS/pluginval"

if [ $# -ne 0 ]; then
  echo "usage: tools/fetch-pluginval.sh" >&2
  exit 2
fi

mkdir -p "$DEPS"
if [ -z "${UNDERTOW_FETCH_LOCKED:-}" ]; then
  UNDERTOW_FETCH_LOCKED=1 exec lockf -k "$DEPS/pluginval.lock" "$0" "$@"
fi

if [ ! -x "$BIN" ] || [ "$(cat "$DIR/.undertow-sha256" 2>/dev/null)" != "$PLUGINVAL_SHA256" ]; then
  tmp=$(mktemp -d "$DEPS/pluginval-download.XXXXXX")
  trap 'rm -rf "$tmp"' EXIT
  echo "fetch-pluginval: downloading pluginval $PLUGINVAL_VERSION" >&2
  curl -fsSL --retry 3 -o "$tmp/pluginval.zip" "$PLUGINVAL_URL"
  actual=$(shasum -a 256 "$tmp/pluginval.zip" | awk '{print $1}')
  if [ "$actual" != "$PLUGINVAL_SHA256" ]; then
    echo "fetch-pluginval: SHA256 mismatch: expected $PLUGINVAL_SHA256, got $actual" >&2
    exit 1
  fi
  mkdir "$tmp/unpacked"
  unzip -q "$tmp/pluginval.zip" -d "$tmp/unpacked"
  if [ ! -x "$tmp/unpacked/pluginval.app/Contents/MacOS/pluginval" ]; then
    echo "fetch-pluginval: the archive holds no pluginval.app" >&2
    exit 1
  fi
  echo "$PLUGINVAL_SHA256" > "$tmp/unpacked/.undertow-sha256"
  rm -rf "$DIR"
  mv "$tmp/unpacked" "$DIR"
fi
echo "$BIN"
