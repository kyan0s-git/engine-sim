#!/bin/sh
# Builds an AppImage from a staged installation (cmake --install).
#
#   make_appimage.sh <staged dir> <output.AppImage> [appimagetool]
#
# The AppDir keeps the installation's layout (bin/, assets/, es/, delta/) so
# that the application finds its data next to the executable.
set -eu

STAGE=$1
OUTPUT=$2
TOOL=${3:-appimagetool}
HERE=$(cd "$(dirname "$0")" && pwd)

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT
APPDIR="$WORK/EngineSimulator.AppDir"

mkdir -p "$APPDIR"
cp -a "$STAGE/." "$APPDIR/"

cat > "$APPDIR/AppRun" <<'APPRUN'
#!/bin/sh
HERE="$(dirname "$(readlink -f "$0")")"
exec "$HERE/bin/engine-sim-app" "$@"
APPRUN
chmod +x "$APPDIR/AppRun"

cp "$HERE/engine-sim.desktop" "$APPDIR/engine-sim.desktop"
cp "$HERE/../icons/engine-sim.png" "$APPDIR/engine-sim.png"
cp "$HERE/../icons/engine-sim.png" "$APPDIR/.DirIcon"

# The runners have no FUSE; let appimagetool extract itself instead
ARCH=x86_64 APPIMAGE_EXTRACT_AND_RUN=1 "$TOOL" --no-appstream "$APPDIR" "$OUTPUT"
