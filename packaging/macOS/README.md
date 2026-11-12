# macOS Packaging – C.E.R.N

## Ziele

- **VST3:** `C.E.R.N.vst3` (Bundle) für `~/Library/Audio/Plug-Ins/VST3` oder `/Library/Audio/Plug-Ins/VST3`
- **Standalone:** `C.E.R.N Standalone.app` für `/Applications` oder `~/Applications`
- **Distribution:** signierte/notarisierte `.pkg` und/oder `.dmg`

> **Wichtig:** Codesignatur und Notarisierung können **nicht** auf Linux durchgeführt werden. Diese Dateien liefern Struktur, Vorlagen und Checklisten für die Ausführung auf einem macOS-System mit Apple Developer Account.

## Build-Voraussetzungen (macOS)

- Xcode Command Line Tools / Xcode
- CMake ≥ 3.22
- Apple Clang (C++20)
- Optional: JUCE-Dependencies werden über FetchContent gebaut

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 \
  -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"
cmake --build build --config Release --target CERN_VST3
cmake --build build --config Release --target CERNStandalone
```

## Bundle-ID / Metadaten

- VST3 Bundle ID: `com.scalyxsoft.cern` (gesetzt in CMake)
- Standalone Bundle ID: `com.scalyxsoft.cern.standalone`
- Deployment Target: standardmäßig `11.0` (in Root-CMakeLists konfigurierbar)

## Codesigning (auf macOS ausführen)

### Voraussetzungen

- Apple Developer ID Application Zertifikat (für App/VST3 außerhalb App Store)
- Optional: Developer ID Installer Zertifikat (für `.pkg`)
- Team ID und Signing Identity bekannt

### Entitlements

Beispiel: `packaging/macOS/entitlements.plist` (Hardened Runtime). Für Audio-Plugins ggf. anpassen (z. B. `com.apple.security.cs.disable-library-validation` nur wenn erforderlich und begründet).

### Signatur-Reihenfolge (typisch)

1. Alle eingebetteten Frameworks/Binaries im Bundle signieren (innen nach außen)
2. Hauptbundle signieren
3. Signatur prüfen

```bash
# Beispiel (Platzhalter – Identity/Team anpassen)
codesign --force --deep --options runtime --timestamp \
  --entitlements packaging/macOS/entitlements.plist \
  --sign "Developer ID Application: YOUR NAME (TEAMID)" \
  "build/Source/CERN_artefacts/Release/VST3/C.E.R.N.vst3"

codesign --verify --deep --strict --verbose=2 "build/Source/CERN_artefacts/Release/VST3/C.E.R.N.vst3"
spctl --assess --type exec --verbose "build/Source/CERNStandalone_artefacts/Release/Standalone/C.E.R.N Standalone.app" || true
```

> `--deep` ist praktisch, aber für produktive Releases besser explizit jedes Binary signieren.

## Notarisierung (Checkliste)

1. [ ] Artefakte mit Hardened Runtime signieren
2. [ ] `.zip`, `.dmg` oder `.pkg` für Upload erstellen
3. [ ] Mit `xcrun notarytool submit ... --wait` einreichen (App Store Connect API Key oder Apple ID)
4. [ ] Bei Fehlern Log prüfen: `xcrun notarytool log <submission-id> ...`
5. [ ] Ticket staple: `xcrun stapler staple <artefakt>`
6. [ ] Gatekeeper prüfen: `spctl --assess -vv --type install <pkg>` / `--type exec <app>`
7. [ ] Auf frischem System (ohne Dev-Tools-Zertifikate) testen

## Installer / Distribution

### DMG (empfohlen für Standalone)

- Hintergrund/Drag-to-Applications optional
- Enthält signierte `.app`
- DMG selbst signieren/notarisieren und stapeln

### PKG (empfohlen für VST3, ggf. kombiniert)

- Installiert VST3 nach `/Library/Audio/Plug-Ins/VST3` und/oder Standalone nach `/Applications`
- Mit `pkgbuild`/`productbuild` erstellen, Installer signieren (`Developer ID Installer`)
- Notarisieren und stapeln

```bash
# Beispiel-Struktur (auf macOS)
pkgbuild --root payload-vst3 --identifier com.scalyxsoft.cern.vst3 \
  --version 1.0.0 --install-location /Library/Audio/Plug-Ins/VST3 \
  build/pkg/CERN-vst3.pkg
```

## Manuelle Schritte (offen auf macOS)

- [ ] Apple Developer Zertifikate bereitstellen
- [ ] Entitlements projektbezogen prüfen (Audio Units/JIT/Sandbox nicht relevant für VST3-only, aber Hardened Runtime beachten)
- [ ] Universal Binary testen (arm64 + x86_64)
- [ ] Signatur, Notarisierung, Stapling durchführen
- [ ] DAW-Kompatibilitätstest (Logic, Ableton, Reaper etc.) auf macOS
- [ ] Gatekeeper/Quarantine-Test auf Clean System
