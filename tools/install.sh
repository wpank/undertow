#!/bin/bash
# Install Undertow bundles either into the user's standard locations or a test destination tree.
#
#   tools/install.sh --dest <dir>   stage signed bundles into <dir>/{VST3,Components,Applications}
#   tools/install.sh --uninstall --dest <dir>   remove staged bundles from that destination tree
#   tools/install.sh                install into ~/Library/Audio/Plug-Ins and ~/Applications, then rescan Audio Units
set -euo pipefail
cd "$(dirname "$0")/.."

usage() {
  cat <<'EOF'
Usage: tools/install.sh [--dest DIR] [--uninstall]

Options:
  --dest DIR     Install into DIR/VST3, DIR/Components, and DIR/Applications instead of system locations
  --uninstall    Remove the staged bundles from the destination tree
EOF
}

DEST=""
UNINSTALL=0
while [ "$#" -gt 0 ]; do
  case "$1" in
    --dest)
      [ "$#" -ge 2 ] || { echo "install: missing value for --dest" >&2; usage >&2; exit 2; }
      DEST=$2
      shift 2
      ;;
    --uninstall)
      UNINSTALL=1
      shift
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "install: unknown argument: $1" >&2
      usage >&2
      exit 2
      ;;
  esac
done

if [ -z "$DEST" ]; then
  VST3_DIR="$HOME/Library/Audio/Plug-Ins/VST3"
  COMPONENTS_DIR="$HOME/Library/Audio/Plug-Ins/Components"
  APPLICATIONS_DIR="$HOME/Applications"
  RESCAN=1
else
  VST3_DIR="$DEST/VST3"
  COMPONENTS_DIR="$DEST/Components"
  APPLICATIONS_DIR="$DEST/Applications"
  RESCAN=0
fi

VST3_SRC="build/plugin/plugin/Undertow_artefacts/RelWithDebInfo/VST3/Metakosmia Undertow 2.vst3"
COMPONENT_SRC="build/plugin/plugin/Undertow_artefacts/RelWithDebInfo/AU/Metakosmia Undertow 2.component"
APP_SRC="build/plugin/plugin/Undertow_artefacts/RelWithDebInfo/Standalone/Metakosmia Undertow 2.app"

ensure_build() {
  cmake --build --preset plugin --target Undertow_All
}

install_bundle() {
  src=$1
  dst=$2
  mkdir -p "$(dirname "$dst")"
  rm -rf "$dst"
  cp -R "$src" "$dst"
  codesign --force --deep --sign - "$dst" >/dev/null 2>&1 || {
    echo "install: failed to ad hoc sign $dst" >&2
    exit 1
  }
  printf 'installed %s\n' "$dst"
}

uninstall_bundle() {
  dst=$1
  rm -rf "$dst"
  printf 'removed %s\n' "$dst"
}

if [ "$UNINSTALL" -eq 1 ]; then
  uninstall_bundle "$VST3_DIR/Metakosmia Undertow 2.vst3"
  uninstall_bundle "$COMPONENTS_DIR/Metakosmia Undertow 2.component"
  uninstall_bundle "$APPLICATIONS_DIR/Metakosmia Undertow 2.app"
  exit 0
fi

ensure_build

for path in "$VST3_SRC" "$COMPONENT_SRC" "$APP_SRC"; do
  [ -e "$path" ] || { echo "install: missing bundle: $path" >&2; exit 1; }
done

install_bundle "$VST3_SRC" "$VST3_DIR/Metakosmia Undertow 2.vst3"
install_bundle "$COMPONENT_SRC" "$COMPONENTS_DIR/Metakosmia Undertow 2.component"
install_bundle "$APP_SRC" "$APPLICATIONS_DIR/Metakosmia Undertow 2.app"

if [ "$RESCAN" -eq 1 ]; then
  killall -9 AudioComponentRegistrar >/dev/null 2>&1 || true
fi
