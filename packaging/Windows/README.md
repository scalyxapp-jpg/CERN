# Windows Packaging – C.E.R.N

## Ziele

- **VST3:** `CERN.vst3` (Ordner/Bundle) für `C:\Program Files\Common Files\VST3`
- **Standalone:** `C.E.R.N Standalone.exe` (optional mit Installer)
- **Distribution:** signierter Installer (Inno Setup `.exe` oder WiX `.msi`)

> **Wichtig:** Authenticode-Signierung und reale Windows-Builds können **nicht** auf Linux durchgeführt werden. Diese Dateien liefern Vorlagen und Checklisten für die Ausführung auf Windows.

## Build-Voraussetzungen (Windows)

- Visual Studio 2022 (MSVC, C++20) oder kompatibler Toolchain
- CMake ≥ 3.22
- Git (für JUCE FetchContent)

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release --target CERN_VST3
cmake --build build --config Release --target CERNStandalone
```

Optional ASIO (nur Windows, Standalone):

```powershell
cmake -B build -G "Visual Studio 17 2022" -A x64 -DCERN_WITH_ASIO=ON
```

> Bei ASIO gelten die Steinberg-Lizenzbedingungen. ASIO-Header sind über JUCE verfügbar; ein externer ASIO-SDK ist in der Regel nicht erforderlich.

## Artefakte (typisch)

- `build\Source\CERN_artefacts\Release\VST3\CERN.vst3\Contents\x86_64-win\CERN.vst3`
- `build\Source\CERNStandalone_artefacts\Release\Standalone\C.E.R.N Standalone.exe`

## Installer-Vorlagen

- **Inno Setup:** `packaging/Windows/CERN.iss` (Vorlage, Pfade/Version vor Build prüfen)
- **WiX:** optional; für MSIX/MSI-Enterprise-Deployment geeignet (hier nicht als vollständiges Projekt enthalten)

## Code Signing (Authenticode, auf Windows)

### Voraussetzungen

- Code-Signing-Zertifikat (EV empfohlen für SmartScreen-Reputation)
- `signtool.exe` (Windows SDK)

### Schritte

1. [ ] VST3-Binary und Standalone-EXE signieren
2. [ ] Installer signieren (nach Erstellung)
3. [ ] Timestamp-Server verwenden (`/tr` + `/td sha256`)
4. [ ] Signatur prüfen (`signtool verify /pa /v <datei>`)

```powershell
# Beispiel (Platzhalter)
signtool sign /fd sha256 /tr http://timestamp.digicert.com /td sha256 ^
  /sha1 <CERT_THUMBPRINT> "path\to\CERN.vst3"
```

## Manuelle Schritte (offen auf Windows)

- [ ] Release-Build mit MSVC (x64, ggf. ARM64 separat evaluieren)
- [ ] ASIO-Build testen (falls benötigt, Lizenzhinweise beachten)
- [ ] VST3 in gängigen Hosts testen (Reaper, Ableton Live, Cubase, FL Studio etc.)
- [ ] Installer bauen (Inno Setup/WiX) und signieren
- [ ] SmartScreen/Defender-Verhalten prüfen
- [ ] DPI-/Skalierungsverhalten auf Windows validieren
