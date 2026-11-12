# Design-Review TODO (subagent 2026-11-12)

- [x] P1: Eigene Referenz-Screenshots 1400×900 / 1100×750 / 900×650 / 700×500 + Patchbay + CV + Material Room + Scrollbottom/Footer + Morph (700/1100) unter `/tmp/opencode-cern-review/rev/`; Vision-Gegencheck via `nim-nemotron-vision` (Fallback, `local-vision` nicht erreichbar) durchgeführt
- [x] P2: MORPH sichtbar bei <980 px (zweizeilig) und bei 1100 px geprüft – keine Abschneidung/Überlappung festgestellt
- [x] P3: Prüfpunkte Header-Combo, Meter-Zeile (700), Envelope-Info, Higgs-Legende/Wireframe, Patchbay-Titel über Kabeln, CV-Inspector (SRC/DST/Toggles/Slider) im aktuellen Stand ohne reproduzierbare Layout-Fehler (CERNPreview + Vision-Check)
- [x] P4: Ästhetik konsistent geprüft (Bevel/Tiefe, Licht, Rot-Akzente, Kontrast/Lesbarkeit, Grain/Vignette-Ansatz) – keine auffälligen Inkonsistenzen
- [x] P5: Grid-Dichte/`rowHeightForWidth`-Skalung geprüft (1400 breit vs. 700×500 kompakt) – plausibel und lesbar
- [x] P6: Logo/Wortmarke (Emblem-Kanten, Skalierung, Lesbarkeit) im Screenshot-Check bestätigt
- [x] P7: Standalone-Xvfb-SIGSEGV reproduziert (Exit 139). Backtrace (`gdb -batch`): `#0 KickSynthesisEngine::reset()` ← `#1 CERNAudioProcessor::releaseResources()` ← `#2 juce::AudioProcessorPlayer::audioDeviceAboutToStart` ← `#3 juce::AudioDeviceManager::addAudioCallback` ← `#4 CERNStandaloneApp::initialise`. Fix außerhalb GUI-Scope (DSP/Processor/Standalone).
- [ ] P8: Build der Ziele `CERN_All CERNStandalone CERNTests` mit Lock erfolgreich; `./build/Source/CERNTests` und `ctest --test-dir build --output-on-failure` **nicht grün** (3 Fails: `preset_manager_factory_presets`/`factory_presets_include_higgs_and_parameter_patch` erwarten 7 Factory-Presets, Arbeitsstand hat 11). Tests/Presets außerhalb Scope, nicht geändert.
- [x] P9: README.md UI-Abschnitt und TODO.md 3D-UI-Redesign-Kapitel aktualisiert (Review-Status, Vision-Fallback, SIGSEGV-Backtrace, Test-Blocker)
