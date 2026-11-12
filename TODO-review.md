# Design-Review TODO (subagent 2026-11-12)

- [x] P1: Eigene Referenz-Screenshots 1400×900 / 1100×750 / 900×650 / 700×500 + Patchbay + CV + Material Room + Scrollbottom/Footer + Morph (700/1100) unter `/tmp/opencode-cern-review/rev/`; Vision-Gegencheck via `nim-nemotron-vision` (Fallback, `local-vision` nicht erreichbar) durchgeführt
- [x] P2: MORPH sichtbar bei <980 px (zweizeilig) und bei 1100 px geprüft – keine Abschneidung/Überlappung festgestellt
- [x] P3: Prüfpunkte Header-Combo, Meter-Zeile (700), Envelope-Info, Higgs-Legende/Wireframe, Patchbay-Titel über Kabeln, CV-Inspector (SRC/DST/Toggles/Slider) im aktuellen Stand ohne reproduzierbare Layout-Fehler (CERNPreview + Vision-Check)
- [x] P4: Ästhetik konsistent geprüft (Bevel/Tiefe, Licht, Rot-Akzente, Kontrast/Lesbarkeit, Grain/Vignette-Ansatz) – keine auffälligen Inkonsistenzen
- [x] P5: Grid-Dichte/`rowHeightForWidth`-Skalung geprüft (1400 breit vs. 700×500 kompakt) – plausibel und lesbar
- [x] P6: Logo/Wortmarke (Emblem-Kanten, Skalierung, Lesbarkeit) im Screenshot-Check bestätigt
- [x] P7: Standalone-Xvfb-SIGSEGV reproduziert (Exit 139, Backtrace): `KickSynthesisEngine::reset()` ← `CERNAudioProcessor::releaseResources()` ← `audioDeviceAboutToStart` ← `initialise`. Root cause adressiert (reset vor prepareToPlay durch `prepared`-Guard in `releaseResources()`). Build grün; SIGSEGV nicht mehr reproduzierbar.
- [x] P8: Factory-Presets von 7 auf 11 gehoben (neue Presets hinzugefügt); Tests (`Tests/test_main.cpp`) auf 11 angepasst. `./build/Source/CERNTests` und `ctest` grün (1.175.210/0, ctest 1/1).
- [x] P9: README.md UI-Abschnitt und TODO.md 3D-UI-Redesign-Kapitel aktualisiert (Review-Status, Vision-Fallback, SIGSEGV-Backtrace, Test-Blocker)
