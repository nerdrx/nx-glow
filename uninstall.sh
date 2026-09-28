#!/usr/bin/env bash
set -euo pipefail

if [[ ${EUID:-0} -eq 0 ]]; then
    echo 'Run this script as your user; it invokes sudo only for the system plugin file.' >&2
    exit 1
fi

PLUGIN="kwin/effects/plugins/nxglow.so"
QTPATHS="/usr/lib/qt6/bin/qtpaths"
ELEVATE=(sudo)
if [[ ${1:-} == --gui ]]; then ELEVATE=(pkexec); fi
for cmd in "${ELEVATE[0]}" kwriteconfig6 qdbus6; do
    command -v "$cmd" >/dev/null 2>&1 || { printf 'Missing required command: %s\n' "$cmd" >&2; exit 1; }
done
if [[ ! -x "$QTPATHS" ]]; then
    QTPATHS="$(command -v qtpaths6 || command -v qtpaths || true)"
fi
[[ -n "$QTPATHS" && -x "$QTPATHS" ]] || { echo 'Could not find Qt 6 qtpaths.' >&2; exit 1; }
PLUGIN_DIR="$("$QTPATHS" --plugin-dir)"
[[ -n "$PLUGIN_DIR" && "$PLUGIN_DIR" = /* ]] || { printf 'Invalid Qt plugin directory: %s\n' "$PLUGIN_DIR" >&2; exit 1; }

kwriteconfig6 --file kwinrc --group Plugins --key nxglowEnabled false
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect nxglow || true
"${ELEVATE[@]}" rm -f -- "$PLUGIN_DIR/$PLUGIN"
"${ELEVATE[@]}" rm -rf -- "$PLUGIN_DIR/kwin/effects/nxglow"
rm -f -- "$HOME/.local/bin/nx-glow-settings" "${XDG_DATA_HOME:-$HOME/.local/share}/applications/nx-glow-settings.desktop"
printf 'Disabled and removed nx glow from %s\n' "$PLUGIN_DIR/$PLUGIN"
