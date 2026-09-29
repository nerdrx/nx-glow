#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
version=${1:-0.1.3}
[[ "$version" =~ ^[0-9]+\.[0-9]+\.[0-9]+$ ]] || { echo 'Expected a numeric x.y.z version' >&2; exit 1; }
stage=$(mktemp -d)
trap 'rm -rf -- "$stage"' EXIT
share="$stage/usr/share/nx-glow"
mkdir -p "$share/src" "$stage/usr/bin" "$stage/usr/share/applications" dist
for file in CMakeLists.txt settings.py install.sh uninstall.sh launch.sh nx-glow-settings.desktop README.md LICENSE; do
    cp "$file" "$share/$file"
done
cp src/glow.cpp src/glow.json src/config.cpp "$share/src/"
install -m755 launch.sh "$stage/usr/bin/nx-glow-settings"
install -m644 nx-glow-settings.desktop "$stage/usr/share/applications/"
chmod +x "$share/install.sh" "$share/uninstall.sh"
tar -czf "dist/nx-glow-$version-linux.tar.gz" -C "$stage" usr
cp nx-app.json dist/nx-app.json
(cd dist && sha256sum "nx-glow-$version-linux.tar.gz" > SHA256SUMS)
echo "Created dist/nx-glow-$version-linux.tar.gz"
