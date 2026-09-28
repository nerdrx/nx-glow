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
ELEVATE=(sudo)
if [[ ${1:-} == --gui ]]; then ELEVATE=(pkexec); fi
command -v "${ELEVATE[0]}" >/dev/null || { echo "Missing ${ELEVATE[0]}" >&2; exit 1; }
# Arch/CachyOS can resolve the native build dependencies during setup.
if command -v pacman >/dev/null; then
    missing=()
    for package in cmake ninja gcc pkgconf kwin qt6-base qt6-declarative extra-cmake-modules vulkan-headers pyside6; do
        pacman -Q "$package" >/dev/null 2>&1 || missing+=("$package")
    done
    if ((${#missing[@]})); then
        printf 'Installing build dependencies: %s\n' "${missing[*]}"
        "${ELEVATE[@]}" pacman -S --needed --noconfirm "${missing[@]}"
    fi
fi

for cmd in cmake kwriteconfig6 qdbus6 python3; do
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
# Qt keeps loaded plugin libraries cached. A unique real path lets updates
# load immediately without restarting the user's compositor/session.
BUILD_ID=$(sha256sum "$BINARY")
BUILD_ID=${BUILD_ID%% *}
VERSIONED="$PLUGIN_DIR/kwin/effects/nxglow/$BUILD_ID/nxglow.so"
"${ELEVATE[@]}" install -Dm755 "$BINARY" "$VERSIONED"
"${ELEVATE[@]}" mkdir -p "$PLUGIN_DIR/kwin/effects/plugins"
"${ELEVATE[@]}" ln -sfn "$VERSIONED" "$PLUGIN_DIR/$PLUGIN"
qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.unloadEffect nxglow >/dev/null || true
kwriteconfig6 --file kwinrc --group Plugins --key nxglowEnabled true
LOAD_RESULT="$(qdbus6 org.kde.KWin /Effects org.kde.kwin.Effects.loadEffect nxglow)"
[[ "$LOAD_RESULT" == true ]] || { printf 'KWin did not load nx glow (result: %s)\n' "$LOAD_RESULT" >&2; exit 1; }
SHARE="${XDG_DATA_HOME:-$HOME/.local/share}/nx-glow"
if [[ "$ROOT" != "$SHARE" ]]; then
    for file in CMakeLists.txt settings.py install.sh uninstall.sh launch.sh nx-glow-settings.desktop src/glow.cpp src/glow.json; do
        install -Dm644 "$ROOT/$file" "$SHARE/$file"
    done
fi
chmod +x "$SHARE/install.sh" "$SHARE/uninstall.sh"
install -Dm755 "$ROOT/launch.sh" "$HOME/.local/bin/nx-glow-settings"
install -Dm644 "$ROOT/nx-glow-settings.desktop" "${XDG_DATA_HOME:-$HOME/.local/share}/applications/nx-glow-settings.desktop"
printf 'Installed and enabled nx glow at %s\n' "$PLUGIN_DIR/$PLUGIN"
