#!/bin/sh
# Builds a Debian package from a staged installation (cmake --install).
#
#   make_deb.sh <staged dir> <version> <output.deb>
#
# Installs into /opt/engine-sim with launchers in /usr/bin and a menu entry.
set -eu

STAGE=$1
VERSION=$2
OUTPUT=$3
HERE=$(cd "$(dirname "$0")" && pwd)

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
ROOT="$WORK/root"

mkdir -p "$ROOT/opt/engine-sim" "$ROOT/usr/bin" "$ROOT/DEBIAN" \
    "$ROOT/usr/share/applications" "$ROOT/usr/share/icons/hicolor/512x512/apps"
cp -a "$STAGE/." "$ROOT/opt/engine-sim/"
rm -f "$ROOT/opt/engine-sim/engine-sim.desktop" "$ROOT/opt/engine-sim/engine-sim.png"

ln -s /opt/engine-sim/bin/engine-sim-app "$ROOT/usr/bin/engine-sim"
if [ -f "$ROOT/opt/engine-sim/bin/engine-sim-dyno" ]; then
    ln -s /opt/engine-sim/bin/engine-sim-dyno "$ROOT/usr/bin/engine-sim-dyno"
fi

sed 's|^Exec=.*|Exec=/opt/engine-sim/bin/engine-sim-app|' "$HERE/engine-sim.desktop" \
    > "$ROOT/usr/share/applications/engine-sim.desktop"
cp "$HERE/../icons/engine-sim.png" "$ROOT/usr/share/icons/hicolor/512x512/apps/engine-sim.png"

SIZE=$(du -sk "$ROOT" | cut -f1)
cat > "$ROOT/DEBIAN/control" <<CONTROL
Package: engine-sim
Version: $VERSION
Section: games
Priority: optional
Architecture: amd64
Installed-Size: $SIZE
Depends: libc6 (>= 2.35), libstdc++6, libgl1
Recommends: libasound2 | libpulse0 | libpipewire-0.3-0
Maintainer: Engine Simulator contributors <noreply@github.com>
Homepage: https://github.com/kyan0s-git/engine-sim
Description: Real-time internal combustion engine simulator
 Simulates piston and rotary engines in real time and synthesizes their
 sound. Includes an engineering model with per-cycle outputs, a
 dynamometer, an engine designer and a command line dyno tool.
CONTROL

dpkg-deb --root-owner-group --build "$ROOT" "$OUTPUT"
