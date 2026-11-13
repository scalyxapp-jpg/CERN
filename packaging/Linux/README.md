# Linux Packaging – C.E.R.N

## Aktueller Stand

- **Formate:** VST3 (`CERN.vst3`) und Standalone (`C.E.R.N Standalone`)
- **Build-System:** CMake + JUCE (FetchContent, Branch `develop`)
- **Audio-Backends:** ALSA (Standard), optional JACK (`-DCERN_WITH_JACK=ON`, weak-linked zur Laufzeit)
- **GUI:** X11/GTK (auf Linux erforderlich)

## Abhängigkeiten (Build)

Typische Pakete (Debian/Ubuntu):

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake git pkg-config \
  libasound2-dev libjack-jackd2-dev \
  libfreetype6-dev libfontconfig1-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
  libxinerama-dev libxrandr-dev libxrender-dev \
  libglu1-mesa-dev mesa-common-dev \
  libgtk-3-dev libwebkit2gtk-4.1-dev || true
```

> Hinweis: `libwebkit2gtk` ist im Projekt standardmäßig deaktiviert (`JUCE_WEB_BROWSER=0`), wird aber von manchen JUCE-Setups erwartet – Installation schadet nicht.

## Build

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j"$(nproc)" --target CERN_VST3
cmake --build build -j"$(nproc)" --target CERNStandalone
```

Artefakte:

- `build/Source/CERN_artefacts/Release/VST3/CERN.vst3/`
- `build/Source/CERNStandalone_artefacts/Release/Standalone/C.E.R.N Standalone`

## Installation (manuell)

- VST3 (User): `~/.vst3/`
- VST3 (System): `/usr/lib/vst3/` oder `/usr/local/lib/vst3/`
- Standalone: beliebiges Verzeichnis im `$PATH` oder `~/.local/bin/`

```bash
cmake --install build --component VST3
cmake --install build --component Standalone
```

> Die Installationspfade sind in `Source/CMakeLists.txt` als Cache-Variablen konfigurierbar (`CERN_VST3_INSTALL_DIR`, `CERN_STANDALONE_INSTALL_DIR`).

## Paketierung

- **Archiv:** `.tar.gz` oder `.zip` mit VST3-Bundle und Standalone-Binary
- **Distribution:** optional `.deb`/`.rpm` via CPack (`CPACK_GENERATOR` in Root-CMakeLists anpassen)
- **AppImage/Flatpak:** nicht enthalten, kann später ergänzt werden

Siehe `scripts/package-vst3.sh` und `scripts/package-standalone.sh` für plattformübergreifende Packaging-Helfer.

## Hinweise

- Keine Codesignatur/Notarisierung erforderlich (Linux).
- Headless-Builds/Tests: `xvfb-run` kann für GUI-Tests nötig sein (aktuell nicht Teil des Standard-CI-Pfads).
- ASIO ist Windows-spezifisch und unter Linux nicht verfügbar.
