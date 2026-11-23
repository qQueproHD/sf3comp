#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Fork-only. Builds build/collection/SF3comp.app from your ROM set and copies it to /Applications, with the fork's icon: your own picture if
# fork/icon/custom.png (or .jpg) is there (git-ignored: it stays on this machine), else fork/icon/icon.svg.
#   fork/build.sh [ROM]      ROM: path to sfiii3nr1.zip (default: $SF3_ROM, else sfiii3nr1.zip beside the repository)
set -eu
ROM=${1:-${SF3_ROM:-$(cd "$(dirname "$0")/../.." && pwd)/sfiii3nr1.zip}}
[ -f "$ROM" ] || { echo "fork/build.sh: ROM set not found: $ROM (pass its path, or set SF3_ROM)" >&2; exit 1; }
ROM=$(cd "$(dirname "$ROM")" && pwd)/$(basename "$ROM")
cd "$(dirname "$0")/.."

bash cps3recomp/tools/translate.sh "$ROM" 0
sh cps3recomp/tools/collection.sh
FORCE=1 sh cps3recomp/tools/make_app.sh --collection

APP=build/collection/SF3comp.app
PLIST=$APP/Contents/Info.plist
mkdir -p "$APP/Contents/Resources"
ICON=fork/icon/icon.svg
for custom in fork/icon/custom.png fork/icon/custom.jpg fork/icon/custom.jpeg; do
    [ -f "$custom" ] && ICON=$custom && break
done
swift fork/icon/make_icns.swift "$ICON" "$APP/Contents/Resources/AppIcon.icns"
/usr/libexec/PlistBuddy -c "Delete :CFBundleIconFile" "$PLIST" 2>/dev/null || true
/usr/libexec/PlistBuddy -c "Add :CFBundleIconFile string AppIcon" "$PLIST"
codesign --force --deep -s - "$APP"
touch "$APP" # (Finder re-reads the icon)
echo "fork/build.sh: built $APP"

# (the app is self-contained: settings and ROM paths live in ~/Library/Application Support/SF3comp)
if [ -z "${NO_INSTALL:-}" ]; then
    rm -rf /Applications/SF3comp.app
    ditto "$APP" /Applications/SF3comp.app
    echo "fork/build.sh: installed /Applications/SF3comp.app (NO_INSTALL=1 skips this)"
fi
