#!/usr/bin/env bash
set -euo pipefail

# Package C.E.R.N Standalone artefact for the current platform.
# Usage: scripts/package-standalone.sh [BUILD_DIR] [CONFIG] [DIST_DIR]
# Defaults: BUILD_DIR=build, CONFIG=Release, DIST_DIR=dist

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="${1:-$ROOT_DIR/build}"
CONFIG="${2:-Release}"
DIST_DIR="${3:-$ROOT_DIR/dist}"
PLUGIN_NAME="C.E.R.N Standalone"

UNAME_S="$(uname -s 2>/dev/null || echo Unknown)"
case "$UNAME_S" in
Linux*) PLATFORM="linux" ;;
Darwin*) PLATFORM="macos" ;;
MINGW* | MSYS* | CYGWIN*) PLATFORM="windows" ;;
*) PLATFORM="unknown" ;;
esac

OUT_DIR="$DIST_DIR/$PLATFORM"
mkdir -p "$OUT_DIR"

# JUCE legt die GUI-App NICHT in einen "Standalone/"-Unterordner (nur Plugin-Formate
# bekommen einen solchen). Das Artefakt liegt direkt in <config>/ - der exakte
# Name variiert je nach PRODUCT_NAME. Deshalb: erst den erwarteten Pfad, sonst
# per find im ganzen CERNStandalone_artefacts-Ordner suchen.
ARTIFACT_ROOT="$BUILD_DIR/Source/CERNStandalone_artefacts/$CONFIG"

case "$PLATFORM" in
	macos)   PATTERN="*.app" ;;
	windows) PATTERN="*.exe" ;;
	linux)   PATTERN="C.E.R.N Standalone" ;;
	*)       PATTERN="" ;;
esac

if [[ "$PLATFORM" == "macos" ]]; then
	EXPECTED="$ARTIFACT_ROOT/Standalone/${PLUGIN_NAME}.app"
elif [[ "$PLATFORM" == "windows" ]]; then
	EXPECTED="$ARTIFACT_ROOT/Standalone/${PLUGIN_NAME}.exe"
else
	EXPECTED="$ARTIFACT_ROOT/${PLUGIN_NAME}"
fi

if [[ -e "$EXPECTED" ]]; then
	APP_SRC="$EXPECTED"
else
	APP_SRC="$(find "$ARTIFACT_ROOT" -name "$PATTERN" -print -quit 2>/dev/null || true)"
fi

if [[ -z "$APP_SRC" || ! -e "$APP_SRC" ]]; then
	echo "ERROR: Standalone artefact not found under: $ARTIFACT_ROOT" >&2
	echo "Build first, e.g.: cmake --build \"$BUILD_DIR\" --config $CONFIG --target CERNStandalone" >&2
	exit 1
fi

STAMP="$(date +%Y%m%d-%H%M%S)"
ARCHIVE_BASE="CERN-standalone-${PLATFORM}-${CONFIG}-${STAMP}"

echo "Packaging Standalone from: $APP_SRC"
echo "Platform: $PLATFORM  Config: $CONFIG"

case "$PLATFORM" in
macos)
	ARCHIVE="$OUT_DIR/${ARCHIVE_BASE}.tar.gz"
	tar -C "$(dirname "$APP_SRC")" -czf "$ARCHIVE" "$(basename "$APP_SRC")"
	echo "Created: $ARCHIVE"
	echo "For .dmg/.pkg distribution, sign and notarize on macOS. See packaging/macOS/README.md."
	;;
linux)
	ARCHIVE="$OUT_DIR/${ARCHIVE_BASE}.tar.gz"
	tar -C "$(dirname "$APP_SRC")" -czf "$ARCHIVE" "$(basename "$APP_SRC")"
	echo "Created: $ARCHIVE"
	;;
windows)
	ARCHIVE="$OUT_DIR/${ARCHIVE_BASE}.zip"
	if command -v zip >/dev/null 2>&1; then
		(cd "$(dirname "$APP_SRC")" && zip -r "$ARCHIVE" "$(basename "$APP_SRC")")
	elif command -v powershell.exe >/dev/null 2>&1; then
		powershell.exe -NoProfile -Command "Compress-Archive -Path '$APP_SRC' -DestinationPath '$ARCHIVE' -Force"
	else
		echo "ERROR: neither 'zip' nor 'powershell.exe' found to create archive" >&2
		exit 1
	fi
	echo "Created: $ARCHIVE"
	echo "For signed installer, use packaging/Windows/CERN.iss on Windows. See packaging/Windows/README.md."
	;;
*)
	echo "ERROR: unsupported platform '$UNAME_S'" >&2
	exit 1
	;;
esac

echo "NOTE: Unsigned artefact. See packaging/$PLATFORM/README.md for signing steps."
