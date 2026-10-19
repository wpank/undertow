#!/bin/bash
# Fetch JUCE 8.0.12 into the shared dependency cache, check its SHA256, unpack it once and print the
# directory. The URL and hash are the ones Metakosmia pins: JUCE 8.0.12 is a compatibility baseline,
# not the newest release.
#
#   tools/fetch-juce.sh            print the JUCE source directory (downloads and unpacks on first use)
#   tools/fetch-juce.sh --prefix   also build and install JUCE once (its CMake package and juceaide),
#                                  and print the install prefix, for find_package(JUCE CONFIG)
#
# The cache is shared by every checkout and worktree, so a new worktree configures without a download or
# a juceaide build: ${UNDERTOW_DEPS_DIR:-$HOME/Library/Caches/undertow}. Runs are serialized by a lock.
set -euo pipefail

JUCE_VERSION=8.0.12
JUCE_URL=https://codeload.github.com/juce-framework/JUCE/tar.gz/29396c22c93392d6738e021b83196283d6e4d850
JUCE_SHA256=97cc01e0fe15278f6edadc895a3cf0aac2fd98bcd023823384d5ca7025beb91c
DEPS=${UNDERTOW_DEPS_DIR:-$HOME/Library/Caches/undertow}
SRC="$DEPS/JUCE-$JUCE_VERSION"
PREFIX="$DEPS/JUCE-$JUCE_VERSION-install"

mode=source
case "${1:-}" in
  "") ;;
  --prefix) mode=prefix ;;
  *) echo "usage: tools/fetch-juce.sh [--prefix]" >&2; exit 2 ;;
esac

mkdir -p "$DEPS"
if [ -z "${UNDERTOW_FETCH_LOCKED:-}" ]; then
  UNDERTOW_FETCH_LOCKED=1 exec lockf -k "$DEPS/juce.lock" "$0" "$@"
fi

if [ ! -f "$SRC/.undertow-sha256" ] || [ "$(cat "$SRC/.undertow-sha256")" != "$JUCE_SHA256" ]; then
  tmp=$(mktemp -d "$DEPS/juce-download.XXXXXX")
  trap 'rm -rf "$tmp"' EXIT
  echo "fetch-juce: downloading JUCE $JUCE_VERSION" >&2
  curl -fsSL --retry 3 -o "$tmp/juce.tar.gz" "$JUCE_URL"
  actual=$(shasum -a 256 "$tmp/juce.tar.gz" | awk '{print $1}')
  if [ "$actual" != "$JUCE_SHA256" ]; then
    echo "fetch-juce: SHA256 mismatch: expected $JUCE_SHA256, got $actual" >&2
    exit 1
  fi
  mkdir "$tmp/unpacked"
  tar -xzf "$tmp/juce.tar.gz" -C "$tmp/unpacked"
  unpacked=$(find "$tmp/unpacked" -mindepth 1 -maxdepth 1 -type d | head -1)
  if [ ! -f "$unpacked/CMakeLists.txt" ]; then
    echo "fetch-juce: the archive holds no JUCE source tree" >&2
    exit 1
  fi
  echo "$JUCE_SHA256" > "$unpacked/.undertow-sha256"
  rm -rf "$SRC" "$PREFIX"
  mv "$unpacked" "$SRC"
fi

if [ "$mode" = source ]; then
  echo "$SRC"
  exit 0
fi

if [ ! -f "$PREFIX/.undertow-sha256" ] || [ "$(cat "$PREFIX/.undertow-sha256")" != "$JUCE_SHA256" ]; then
  echo "fetch-juce: building and installing JUCE $JUCE_VERSION (juceaide) into $PREFIX" >&2
  build="$DEPS/JUCE-$JUCE_VERSION-build"
  rm -rf "$build" "$PREFIX"
  cmake -S "$SRC" -B "$build" -G "Unix Makefiles" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
    -DCMAKE_INSTALL_PREFIX="$PREFIX" >&2
  cmake --build "$build" --target install >&2
  echo "$JUCE_SHA256" > "$PREFIX/.undertow-sha256"
  rm -rf "$build"
fi
echo "$PREFIX"
