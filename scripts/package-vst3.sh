#!/usr/bin/env bash
set -euo pipefail

# Package C.E.R.N VST3 artefact for the current platform.
# Usage: scripts/package-vst3.sh [BUILD_DIR] [CONFIG] [DIST_DIR]
# Defaults: BUILD_DIR=build, CONFIG=Release, DIST_DIR=dist

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-$ROOT_DIR/build}"
CONFIG="${2:-Release}"
DIST_DIR="${3:-$ROOT_DIR/dist}"
PLUGIN_NAME="CERN"

UNAME_S="$(uname -s 2>/dev/null || echo Unknown)"
case "$UNAME_S" in
Linux*) PLATFORM="linux" ;;
Darwin*) PLATFORM="macos" ;;
MINGW* | MSYS* | CYGWIN*) PLATFORM="windows" ;;
*) PLATFORM="unknown" ;;
esac

VST3_SRC="$BUILD_DIR/Source/CERN_artefacts/$CONFIG/VST3/${PLUGIN_NAME}.vst3"
OUT_DIR="$DIST_DIR/$PLATFORM"
mkdir -p "$OUT_DIR"

if [[ ! -d "$VST3_SRC" ]]; then
	echo "ERROR: VST3 artefact not found: $VST3_SRC" >&2
	echo "Build first, e.g.: cmake --build \"$BUILD_DIR\" --config $CONFIG --target CERN_VST3" >&2
	exit 1
fi

STAMP="$(date +%Y%m%d-%H%M%S)"
ARCHIVE_BASE="CERN-vst3-${PLATFORM}-${CONFIG}-${STAMP}"

echo "Packaging VST3 from: $VST3_SRC"
echo "Platform: $PLATFORM  Config: $CONFIG"

case "$PLATFORM" in
linux | macos)
	ARCHIVE="$OUT_DIR/${ARCHIVE_BASE}.tar.gz"
	tar -C "$(dirname "$VST3_SRC")" -czf "$ARCHIVE" "$(basename "$VST3_SRC")"
	echo "Created: $ARCHIVE"
	;;
windows)
	ARCHIVE="$OUT_DIR/${ARCHIVE_BASE}.zip"
	if command -v zip >/dev/null 2>&1; then
		(cd "$(dirname "$VST3_SRC")" && zip -r "$ARCHIVE" "$(basename "$VST3_SRC")")
	elif command -v powershell.exe >/dev/null 2>&1; then
		powershell.exe -NoProfile -Command "Compress-Archive -Path '$VST3_SRC' -DestinationPath '$ARCHIVE' -Force"
	else
		echo "ERROR: neither 'zip' nor 'powershell.exe' found to create archive" >&2
		exit 1
	fi
	echo "Created: $ARCHIVE"
	;;
*)
	echo "ERROR: unsupported platform '$UNAME_S'" >&2
	exit 1
	;;
esac

echo "NOTE: Unsigned/unnotarized artefact. See packaging/$PLATFORM/README.md for signing steps."
