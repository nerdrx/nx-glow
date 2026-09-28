#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
root=$PWD
mkdir -p test-output/config
export NX_GLOW_TEST_OUTPUT="$root/test-output"
export XDG_CONFIG_HOME="$root/test-output/config"
export QT_PLUGIN_PATH="$root/build/plugins${QT_PLUGIN_PATH:+:$QT_PLUGIN_PATH}"
export KWIN_COMPOSE=O2
export XDG_CURRENT_DESKTOP=KDE
export KWIN_SCREENSHOT_NO_PERMISSION_CHECKS=1
export QT_LOGGING_RULES='kwin*.debug=true;org.kde.kwin*.debug=true'
export QT_FORCE_STDERR_LOGGING=1
cat > "$XDG_CONFIG_HOME/kwinrc" <<'EOF'
[Compositing]
Backend=OpenGL
[Plugins]
nxglowEnabled=false
blurEnabled=false
contrastEnabled=false
scaleEnabled=false
slideEnabled=false
EOF
cat > test-output/session.sh <<EOF
#!/usr/bin/env bash
export QT_QPA_PLATFORM=xcb
exec python "$root/tests/scene.py"
EOF
chmod +x test-output/session.sh
timeout -k 3 45 gamescope --backend headless -W 1440 -H 900 -- \
    dbus-run-session -- kwin_wayland --virtual \
    --socket nx-glow-test --width 1440 --height 900 --xwayland \
    --no-lockscreen --no-kactivities --exit-with-session "$root/test-output/session.sh" \
    > test-output/smoke.log 2>&1
grep -q NX_GLOW_SCENE_OK test-output/smoke.log
python tests/check.py
