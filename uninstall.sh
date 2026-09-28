#!/usr/bin/env bash
set -euo pipefail

PLUGIN="kwin/effects/plugins/nxglow.so"
QTPATHS="/usr/lib/qt6/bin/qtpaths"
for cmd in sudo kwriteconfig6 qdbus6; do
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
sudo rm -f -- "$PLUGIN_DIR/$PLUGIN"
printf 'Disabled and removed nx glow from %s\n' "$PLUGIN_DIR/$PLUGIN"
