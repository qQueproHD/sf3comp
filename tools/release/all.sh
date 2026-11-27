#!/bin/sh
# SPDX-License-Identifier: GPL-3.0-only
# Copyright (C) 2026 Tommy McLeroy. Part of sf3comp; from cps2recomp.
#
# All release builds on a Mac with Docker: macOS and the Android APK here, Linux x86_64 and Windows in cps2recomp's
# x86_64 build image (cps2recomp-build: cps2recomp's tools/release/Dockerfile). Writes build/release/ with the four
# downloads and SHA256SUMS.txt. Needs the translated code, made from the builder's own ROM sets (docs/BUILDING.md); the full
# translated code takes a long time to compile in the x86_64 image on an ARM Mac (use nice, and few jobs).
set -eu
ROOT="$(cd "$(dirname "$0")/../.." && pwd)"
cd "$ROOT"
tools/audit.sh
docker image inspect cps2recomp-build >/dev/null 2>&1 || {
    echo "error: no cps2recomp-build image: docker build --platform linux/amd64 -t cps2recomp-build <cps2recomp>/tools/release" >&2
    exit 1
}
tools/release/macos.sh
docker run --rm --platform linux/amd64 -e CMAKE_BUILD_PARALLEL_LEVEL="${JOBS:-2}" -e UPDATE_REPO="${UPDATE_REPO-tmcleroy/sf3comp}" -v "$ROOT":"$ROOT" -w "$ROOT" cps2recomp-build tools/release/linux.sh
# (Windows' signing, docs/RELEASING.md: the certificate's folder read-only, its password passed on from this
# environment and never written on the command line)
set --
if [ -n "${WINDOWS_SIGN_PFX:-}" ]; then
    set -- -v "$(cd "$(dirname "$WINDOWS_SIGN_PFX")" && pwd)":/sign:ro -e WINDOWS_SIGN_PFX="/sign/$(basename "$WINDOWS_SIGN_PFX")" \
        -e WINDOWS_SIGN_PASSWORD -e WINDOWS_SIGN_TIMESTAMP
fi
[ -z "${WINDOWS_SIGN:-}" ] || set -- "$@" -e WINDOWS_SIGN
docker run --rm --platform linux/amd64 -e CMAKE_BUILD_PARALLEL_LEVEL="${JOBS:-2}" -e UPDATE_REPO="${UPDATE_REPO-tmcleroy/sf3comp}" "$@" -v "$ROOT":"$ROOT" -w "$ROOT" cps2recomp-build tools/release/windows.sh
# the Android app, signed with the release key (~/.config/sf3comp/android-signing.env; the script refuses without it)
RELEASE=1 tools/release/android.sh
cd build/release
shasum -a 256 SF3comp-macOS.zip SF3comp-linux-x86_64.tar.gz SF3comp-windows-x64.zip SF3comp-android.apk > SHA256SUMS.txt
cat SHA256SUMS.txt
echo "version $(cat "$ROOT/VERSION"): release files in $ROOT/build/release"
