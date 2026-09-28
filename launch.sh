#!/usr/bin/env bash
set -euo pipefail
exec python3 "${XDG_DATA_HOME:-$HOME/.local/share}/nx-glow/settings.py" "$@"
