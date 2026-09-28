#!/usr/bin/env bash
set -euo pipefail

if [[ ${EUID:-0} -eq 0 ]]; then
    echo 'Run this script as your user; it invokes sudo only for the system plugin file.' >&2
    exit 1
fi

ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")" && pwd)"
BUILD_DIR="$ROOT/build"
PLUGIN="kwin/effects/plugins/nxglow.so"
QTPATHS="/usr/lib/qt6/bin/qtpaths"

for cmd in cmake sudo kwriteconfig6 qdbus6; do
    command -v "$cmd" >/dev/null 2>&1 || { printf 'Missing required command: %s\n' "$cmd" >&2; exit 1; }
done
if [[ ! -x "$QTPATHS" ]]; then
    QTPATHS="$(command -v qtpaths6 || command -v qtpaths || true)"
fi
[[ -n "$QTPATHS" && -x "$QTPATHS" ]] || { echo 'Could not find Qt 6 qtpaths.' >&2; exit 1; }

cmake -S "$ROOT" -B "$BUILD_DIR"
cmake --build "$BUILD_DIR" --parallel
BINARY="$BUILD_DIR/plugins/$PLUGIN"
[[ -f "$BINARY" ]] || { printf 'Build did not produce expected plugin: %s\n' "$BINARY" >&2; exit 1; }
PLUGIN_DIR="$("$QTPATHS" --plugin-dir)"
[[ -n "$PLUGIN_DIR" && "$PLUGIN_DIR" = /* ]] || { printf 'Invalid Qt plugin directory: %s\n' "$PLUGIN_DIR" >&2; exit 1; }
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect nxglow >/dev/null || true
sudo install -Dm755 "$BINARY" "$PLUGIN_DIR/$PLUGIN"
kwriteconfig6 --file kwinrc --group Plugins --key nxglowEnabled true
LOAD_RESULT="$(qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect nxglow)"
[[ "$LOAD_RESULT" == true ]] || { printf 'KWin did not load nx glow (result: %s)\n' "$LOAD_RESULT" >&2; exit 1; }
printf 'Installed and enabled nx glow at %s\n' "$PLUGIN_DIR/$PLUGIN"
