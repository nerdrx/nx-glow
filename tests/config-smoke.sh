#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p test-output
config_dir=$(mktemp -d)
trap 'rm -rf -- "$config_dir"' EXIT
cmake -S . -B build -DNX_GLOW_BUILD_TESTS=ON
cmake --build build
timeout -k 3 20 gamescope --backend headless -W 800 -H 600 -- \
    dbus-run-session -- env QT_QPA_PLATFORM=xcb QT_FORCE_STDERR_LOGGING=1 \
    XDG_CONFIG_HOME="$config_dir" ./build/config_smoke \
    "$PWD/build/plugins/kwin/effects/configs/nxglow_config.so" \
    "$PWD/test-output/native-settings.png" > test-output/kcm-test.log 2>&1
grep 'PASS: native plugin' test-output/kcm-test.log
