# Build & Package – Cross-Platform (macOS / Windows / Linux)

Dieses Dokument beschreibt den plattformübergreifenden Build- und Packaging-Prozess für **C.E.R.N** (JUCE 7/8, CMake, VST3 + Standalone).

## Übersicht

| Plattform |                   VST3 |                     Standalone | Audio-Backend (typisch) | Besonderheiten                                         |
| --------- | ---------------------: | -----------------------------: | ----------------------- | ------------------------------------------------------ |
| Linux     |                     ✅ |                             ✅ | ALSA, optional JACK     | X11/GTK für GUI; Headless-CI mit `xvfb-run` möglich    |
| macOS     |   ✅ (auf macOS bauen) |   ✅ (`.app`, auf macOS bauen) | CoreAudio/CoreMIDI      | Codesign + Notarisierung erforderlich für Distribution |
| Windows   | ✅ (auf Windows bauen) | ✅ (`.exe`, auf Windows bauen) | WASAPI, optional ASIO   | Authenticode-Signierung empfohlen; ASIO nur Windows    |

> **Hinweis:** Auf Linux können CMake-Konfiguration und Linux-Builds vorbereitet/geprüft werden. Echte macOS-/Windows-Builds, Codesignatur und Notarisierung müssen auf den jeweiligen Zielplattformen erfolgen.

## Gemeinsame Build-Voraussetzungen

- CMake ≥ 3.22
- C++20-Compiler (GCC/Clang/MSVC je Plattform)
- Git (für JUCE FetchContent, `JUCE_FETCH_FROM_GIT=ON`)
- Internetzugang beim ersten Configure (JUCE wird aus GitHub geladen)

## Linux

### Dependencies (Debian/Ubuntu)

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential cmake ninja-build git pkg-config \
  libasound2-dev libjack-jackd2-dev \
  libfreetype6-dev libfontconfig1-dev \
  libx11-dev libxcomposite-dev libxcursor-dev libxext-dev \
  libxinerama-dev libxrandr-dev libxrender-dev \
  libglu1-mesa-dev mesa-common-dev libgtk-3-dev
```

### Configure & Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --target CERN_VST3 --parallel
cmake --build build --target CERNStandalone --parallel
```

Optionale Backends:

- JACK (Standalone): `-DCERN_WITH_JACK=ON`

Artefakte:

- `build/Source/CERN_artefacts/Release/VST3/CERN.vst3/`
- `build/Source/CERNStandalone_artefacts/Release/Standalone/C.E.R.N Standalone`

### Tests

```bash
cmake --build build --target CERNTests --parallel
ctest --test-dir build --output-on-failure
# ggf. headless: xvfb-run -a ctest --test-dir build --output-on-failure
```

## macOS

### Dependencies

- Xcode Command Line Tools (oder Xcode)
- CMake ≥ 3.22, Ninja optional

### Configure & Build

```bash
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release --target CERN_VST3 --parallel
cmake --build build --config Release --target CERNStandalone --parallel
```

Artefakte:

- `build/Source/CERN_artefacts/Release/VST3/CERN.vst3`
- `build/Source/CERNStandalone_artefacts/Release/Standalone/C.E.R.N Standalone.app`

### Distribution (auf macOS)

- Hardened Runtime signieren (`codesign --options runtime --timestamp ...`)
- Entitlements: `packaging/macOS/entitlements.plist` (Vorlage)
- Notarisierung via `xcrun notarytool`, danach `xcrun stapler staple`
- Installer: `.pkg` (VST3 nach `/Library/Audio/Plug-Ins/VST3`) und/oder `.dmg` (Standalone)

Details: `packaging/macOS/README.md`

## Windows

### Dependencies

- Visual Studio 2022 (MSVC, Desktop development with C++)
- CMake ≥ 3.22
- Ninja (optional, empfohlen)

### Configure & Build (PowerShell)

```powershell
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target CERN_VST3 --parallel
cmake --build build --config Release --target CERNStandalone --parallel
```

Optional ASIO (nur Standalone, nur Windows):

```powershell
cmake -S . -B build -G "Ninja" -DCMAKE_BUILD_TYPE=Release -DCERN_WITH_ASIO=ON
cmake --build build --config Release --target CERNStandalone --parallel
```

> ASIO unterliegt den Steinberg-Lizenzbedingungen. Die JUCE-gebundenen ASIO-Header werden verwendet.

Artefakte:

- `build\Source\CERN_artefacts\Release\VST3\CERN.vst3\`
- `build\Source\CERNStandalone_artefacts\Release\Standalone\C.E.R.N Standalone.exe`

### Distribution (auf Windows)

- Authenticode signieren (`signtool sign /fd sha256 /tr ... /td sha256`)
- Installer: Inno Setup Vorlage `packaging/Windows/CERN.iss` (oder WiX für MSI)
- Signatur von Binaries und Installer prüfen

Details: `packaging/Windows/README.md`

## Packaging-Skripte

Portabel (Bash, Plattform-Erkennung via `uname`):

```bash
# VST3 paketieren -> dist/<platform>/CERN-vst3-*.tar.gz|zip
./scripts/package-vst3.sh [BUILD_DIR] [CONFIG] [DIST_DIR]

# Standalone paketieren -> dist/<platform>/CERN-standalone-*.tar.gz|zip
./scripts/package-standalone.sh [BUILD_DIR] [CONFIG] [DIST_DIR]
```

Standard: `build`, `Release`, `dist`.

## CMake-Install

Installationsregeln sind in `Source/CMakeLists.txt` definiert (komponentenbasiert, `OPTIONAL`).
Pfade sind als Cache-Variablen überschreibbar:

- `CERN_VST3_INSTALL_DIR`
- `CERN_STANDALONE_INSTALL_DIR`

Beispiel:

```bash
cmake --install build --config Release --component VST3
cmake --install build --config Release --component Standalone
```

## CI/CD

Workflow: `.github/workflows/build-cross-platform.yml`

- Matrix: `ubuntu-latest`, `macos-latest`, `windows-latest`
- Schritte: Dependencies installieren, Configure, Build (VST3/Standalone/Tests), Tests (best effort), Artefakte paketieren und hochladen
- Keine Secrets, keine Signaturen

## Troubleshooting

- **JUCE FetchContent schlägt fehl:** Netzwerk/Git-Zugriff prüfen; alternativ lokales JUCE mit `-DJUCE_FETCH_FROM_GIT=OFF -DJUCE_DIR=/pfad/zu/JUCE`.
- **Linux GUI-Build fehlt X11/GTK:** Dev-Pakete installieren (`libx11-dev`, `libgtk-3-dev` etc.). Für reine Nicht-GUI-Setups nicht vorgesehen.
- **`CERN.vst3` nicht gefunden:** Artefaktname/Config prüfen (`Release` vs `Debug`), Pfad unter `build/Source/CERN_artefacts/<CONFIG>/VST3/`.
- **ASIO auf Nicht-Windows:** `CERN_WITH_ASIO` wird ignoriert/gewarnt; nur unter Windows aktivieren.
- **JACK auf Nicht-Linux:** `CERN_WITH_JACK` ist nur unter Linux vorgesehen.
- **macOS Gatekeeper blockiert:** App/Plugin muss signiert und notariert sein (auf macOS durchführen).
- **Windows SmartScreen:** Authenticode-Signierung (idealerweise EV-Zertifikat) verbessert Reputation.
- **Universal Binary (macOS):** Beide Architekturen (`arm64;x86_64`) erfordern kompatible Toolchains/Dependencies; bei Problemen separat bauen.

## Offene manuelle Schritte

- [ ] macOS: Codesign, Notarisierung, Stapling, DAW-Tests
- [ ] Windows: MSVC-Release, ASIO-Validierung (falls genutzt), Signierung, Installer, DAW-Tests
- [ ] Linux: Distribution-Pakete (`.deb`/`.rpm`/AppImage) bei Bedarf ergänzen
- [ ] Finale QA in Ziel-DAWs auf allen Plattformen
