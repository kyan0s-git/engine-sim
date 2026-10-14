#!/bin/sh
# Builds a disk image with the app bundle and a link to /Applications.
#
#   make_dmg.sh <staged dir containing Engine Simulator.app> <output.dmg>
set -eu

STAGE=$1
OUTPUT=$2

WORK=$(mktemp -d)
trap 'rm -rf "$WORK"' EXIT

ditto "$STAGE/Engine Simulator.app" "$WORK/Engine Simulator.app"
for f in LICENSE README.md CHANGELOG.md; do
    if [ -f "$STAGE/$f" ]; then cp "$STAGE/$f" "$WORK/"; fi
done
if [ -d "$STAGE/docs" ]; then ditto "$STAGE/docs" "$WORK/docs"; fi
ln -s /Applications "$WORK/Applications"

hdiutil create -volname "Engine Simulator" -srcfolder "$WORK" -fs HFS+ -format UDZO -ov "$OUTPUT"
