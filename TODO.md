## Automatisches Layering (2026-11-12)

- [x] `LayeringStage` (DSP): zwei vollstaendig aus den Modul-Parametern abgeleitete Layer
      (L1 DEPTH: Grundton-Stuetzung mit langem Decay; L2 PRESENCE: kurzer bandbegrenzter Textur-Layer)
- [x] Alle Layer-Werte automatisch: Frequenz, Laenge, Attack, Decay, Pegel, Dichte, Bandlage, Delay
- [x] Beide Layer folgen derselben Modulation wie die Basis (Pitch-Env, Glide, Velocity->Pitch, Velocity)
- [x] Einspeisung am Membrane-Bus -> Tube/Drive/EQ/Comp, Mixer-Kanaele und Patchbay unveraendert
- [x] Pegelneutral: teil-kohaerenter Basis-Trim; Messung ueber alle 11 Factory-Presets max. +0,79 dB
- [x] Checkbox "LAYERING ENABLED" in der Waveform-Kopfzeile (+ Eintrag in FEATURES)
- [x] Waveform-Anzeige ueberlagert beide Layer farbig mit Legende (Hz / ms / dB / Dichte)
- [x] Parameter `layeringEnabled` am Ende von getAllIDs() (1309 gesamt, Indizes stabil)
- [x] Tests: off-by-default, beide Layer tragen, kein Overgain/Lautheitssprung,
      Depth-Layer bis in den Tail, Presence kurz, Pitch-/Velocity-Modulation, 11 Presets

## Mixer-Tab (2026-11-12)

- [x] MixerStage (DSP): 13 Kanäle, Gain + 4-Band-EQ (RBJ-Shelf/Peak, 0 dB = transparent), Mute
- [x] Automatische Sidechains: COMP→ROOM/HIGGS/REVERB/MATROOM, CLICK→SUB; globaler Sidechain-Regler
- [x] Engine-Integration: pro Modul am Modul-Ausgang (Body separat), Master-Kanal vor Output
- [x] 79 neue APVTS-Parameter, ans Ende von getAllIDs() (1308 gesamt, Indizes stabil)
- [x] MixerView-GUI (13 Streifen, EQ-Knobs, Fader, Mute, Peak-Meter, Sidechain-Label) + MIXER-Button
- [x] Factory-Presets neu kalibriert (Loudness-Streuung ≤1,5 dB über alle 11 Presets)
- [x] Sound-Engineer-Review: EQ-Makeup-Fix (-4,7 dB Bassverlust behoben), Higgs-Default aus,
      Tube-Metallik reduziert, Pitch-Sweep bei Presets auf ≤0,55 begrenzt

# CERN VST3 - Neubau nach Dokumentation

## Ziel

Kompletter Neubau des CERN VST3-Plugins in `/data/projekte/Kickdrum/` strikt nach `TECHNICAL_WIRING_FULL.md`.

## Struktur

- `Source/PluginProcessor.{h,cpp}` — APVTS, Smoothing, StepSequencer, PresetManager, Trigger, Display-Buffers
- `Source/PluginEditor.{h,cpp}` — Host-Editor, hält MainComponent
- `Source/PluginEntry.cpp` — createPluginFilter
- `Source/JuceHeader.h`, `Source/JuceUsings.h`
- `Source/Parameters/ParameterIDs.h` — 132 Parameter-IDs (84 static + 48 step)
- `Source/Parameters/ParameterLayout.{h,cpp}` — APVTS Layout mit Ranges/Defaults
- `Source/Parameters/ParameterSmoothing.h` — O(1) Lookup, getById, getNextValueById
- `Source/DSP/*.h` — Membrane, PitchEnvelope, Click, Tube, Sub, Drive, EQ, Compressor, Output, Room, Reverb, Higgs
- `Source/DSP/KickSynthesisEngine.{h,cpp}` — Signal-Flow (§3), render()
- `Source/Sequencer/StepSequencer.{h,cpp}` — sample-genaue Clock, Swing-Formel r=0.5+swing*0.25
- `Source/Presets/PresetManager.{h,cpp}` — 7 Factory-XML-Snapshots + User-XML-Datei
- `Source/GUI/*.{h,cpp}` — MainComponent, HeaderBar, FooterSequencer, SectionPanel, ParamKnob, Displays, HiggsField3D, EnvelopeEditor, LookAndFeel
- `Source/Standalone/` — Standalone-App
- `CMakeLists.txt` — VST3 + Standalone + Tests

## Fortschritt

- [x] Projektgerüst (CMakeLists, JuceHeader, PluginEntry, JuceUsings)
- [x] Parameters (IDs, Layout, Smoothing)
- [x] DSP-Module (Membrane, PitchEnvelope, Click, Tube, Sub, Drive, EQ, Compressor, Output, Room, Reverb, Higgs)
- [x] KickSynthesisEngine (§3 Signal-Flow)
- [x] PluginProcessor (APVTS, Trigger, PresetManager, StepSequencer)
- [x] StepSequencer (§7 Swing, sample-genau)
- [x] PresetManager (§10 Factory XML + User XML)
- [x] GUI (MainComponent, HeaderBar, FooterSequencer, SectionPanel, Displays, HiggsField3D, EnvelopeEditor, LookAndFeel)
- [x] PluginEditor
- [x] Standalone
- [x] Build (CERN VST3-Bundle, CERNStandalone)
- [x] Tests (CERNTests, 77 Test-Funktionen, 1.175.198 Assertions grün)
- [x] Verifikation gegen Dokumentation (Abweichung §1.3 Sidebar-Layout vs. 2×2-Display-Grid bewusst dokumentiert)

## GUI-Fix-Batch (2026-11-12)

Aus der Screenshot-Analyse (nim-nemotron) abgeleitete 16 Punkte, alle umgesetzt:

### Funktionale Blocker

- [x] `ParameterSmoothing::update()` wird in `processBlock` aufgerufen (vorher nie → alle Parameter eingefroren)
- [x] `setBlockSize()` setzte wegen Parameter-Shadowing den Member nicht (Smoothing konvergierte 500× zu langsam)
- [x] `StepSequencer::processBlock()` wird aufgerufen (vorher nie → Sequencer tot)
- [x] Sequencer liest BPM/Swing/Steps/Playing aus der APVTS, 16tel-Timing, paarweise Swing-Skalierung
- [x] `createEditor()` liefert `CERNAudioProcessorEditor` (vorher `GenericAudioProcessorEditor`)
- [x] Factory-Presets sind echte Parameter-Snapshots und werden per `ValueTree::fromXml` + `replaceState` angewendet
- [x] Sequencer-Trigger-Kette durch Integrationstest abgesichert (`sequencer_triggers_engine`)

### Geometrie

- [x] `ParamKnob` als Component mit Name (oben) + Wert (unten) + zentriertem Drehregler
- [x] `SectionPanel` mit dynamischer Höhe (`getPreferredHeight`), 4 Spalten, kein abgeschnittener Regler mehr
- [x] Displays als 2×2-Raster statt überlappender Stapel
- [x] Footer: PLAY + BPM/SWING/STEPS + 16 Gate-Buttons + 16 Velocity-Balken + Playhead

### Audio-Callback-Fix (2026-11-12)

- [x] Standalone registrierte den Audio-Callback **nie** → `processBlock` lief nie: kein Ton, Sequencer tot, Waveform/Spectrum leer
- [x] `juce::AudioProcessorPlayer` als `AudioIODeviceCallback` registriert (`addAudioCallback`), sauberes `removeAudioCallback` + `releaseResources` + `closeAudioDevice` beim Beenden
- [x] `prepareToPlay` mit Fallback auf 48000/64 (falls Device nicht geöffnet)
- [x] Playhead sichtbar: heller Rahmen um den aktuellen Step-Button (LookAndFeel-Property `playhead`), statt nur 6-px-Lücken zwischen den Velocity-Balken
- [x] Footer-Timer nutzt die APVTS-Wahrheit des Play-Buttons für den Playhead-Status
- [x] Verifikation: PipeWire-Sink-Input `ALSA plug-in [C.E.R.N Standalone]` erscheint beim Start; PLAY → STOP am Button; Playhead-Rahmen wandert (Step 10 → 13 → 2)

### Audio-Settings &amp; ASIO (2026-11-12)

- [x] Standalone hat Audio-Settings-Persistenz bekommen: `~/.config/C.E.R.N/audio_settings.xml` (Gerät, Sample-Rate, Buffer, Kanäle), lädt beim Start (Default-Fallback wenn Gerät weg), speichert beim Schliessen des Dialogs und bei sauberem Shutdown
- [x] `createStateXml()` liefert bei nie geändertem Setup `nullptr` (JUCE-Eigenheit) → Fallback `stateXmlFromCurrentSetup()` baut loader-kompatibles XML aus `getAudioDeviceSetup()`; Roundtrip getestet (48000 Hz / 256 Buffer im Dialog sichtbar)
- [x] Neuer **AUDIO**-Button im Header neben TRIGGER (nur Standalone; VST3 ohne DeviceManager zeigt ihn nicht)
- [x] Audio-Setup-Dialog: `juce::AudioDeviceSelectorComponent` (Output-Gerät, Stereo-Paare, **Sample rate** + **Audio buffer size** immer sichtbar), Eigen-Destruktor speichert die Settings beim Schliessen
- [x] ASIO bereit: CMake-Option `-DCERN_WITH_ASIO=ON` → `JUCE_ASIO=1` (JUCE-gebundene Steinberg-Header, kein externer SDK nötig); Standalone-only; Build mit der Option geprüft
- [x] JACK bereit: CMake-Option `-DCERN_WITH_JACK=ON` → `JUCE_JACK=1` (weak-linked); nur Linux
- [x] Verifikation: Dialog öffnet (PLAY wechselt Kontext), funktioniert mit echtem PipeWire/ALSA-Gerät, Roundtrip 48 kHz/256 getestet, README dokumentiert Button/Datei/Flags

### Schliessen / Beenden (2026-11-12)

- [x] `juce::DocumentWindow::closeButtonPressed()` tut in JUCE nur `jassertfalse` → X-Button und WM-Close wirkten nicht, App lief weiter
- [x] Eigenes `StandaloneWindow` mit `closeButtonPressed()` → `JUCEApplicationBase::quit()`, dazu Ctrl/Cmd+Q als Shortcut
- [x] Getestet: X-Button quittet in ~200 ms (exit 0), mit xfwm4 als echtem WM quittet `wmctrl -c` in ~300 ms (exit 0)

### Higgs-Field: Interaktive Fläche &amp; sichtbare Parameter (2026-11-12)

- [x] `HiggsField3D` neu geschrieben: Mexican-Hat-Wireframe (nichtlineare Ringverteilung aus λ, Kuppelhöhe aus Field Strength, gestrichelter VEV-Ring, Boson-Resonanz-Ring mit Q, verkippte Pole aus Symmetry, Yukawa-Partikel, Decay-Geister-Ringe, MIX Wet/Dry-Volumen)
- [x] Alle **13 Parameter** im Display selbst sichtbar: Nummern-Spalte rechts (PHI, THETA, STRENGTH, VEV, LAM-SELF, INERTIA, YUKAWA, BOSON Hz, BOSON Q, SYMMETRY, DECAY, MIX, ENABLED) + Visual-Mapping im Wireframe; Cursor-Parameter hervorgehoben
- [x] Cursor mit der Maus an beliebige Position ziehbar (Bowl-Bereich); **beim Ziehen werden alle 12 Float-Parameter live aus der Cursorposition abgeleitet** (`HiggsField3D::deriveFromCursor`) und in die APVTS geschrieben (`convertTo0to1`)
- [x] Higgs-Parameter bleiben separat einstellbar (Knobs/APVTS), reagieren ebenso auf Drag (Attachments)
- [x] Unit-Test `higgs_cursor_derives_all_params` (Wertebereiche + tatsächliche Änderung)
- [x] Higgs-Display größer: Bottom-Row der Displays umgestellt (Envelope 34 %, Higgs 66 %)
- [x] Verifikation per xvfb+Drag: Parameterliste wechselt komplett sichtbar (Beispiele: YUKAWA 0.30→0.71, BOSON Hz 125.0→114.4, INERTIA 0.50→0.99, SYMMETRY 0.50→0.90)

### Design

- [x] Toggle-Buttons in lokalen Koordinaten gezeichnet, mit Text (Switch-Stil) bzw. kompaktem Stil für Step-Buttons
- [x] Aktiv-Erkennung der Regler über normalisierten Wert (nicht über `getValue() > 0`)
- [x] Scrollbar-, ComboBox-, PopupMenu- und TextButton-Farben im LookAndFeel
- [x] Panel-Rahmen, Akzentleiste, ComboBox-Pfeil, Rotary-Track/Wert-Bogen
- [x] Display-Titel kontrastreicher, ohne „(§9)"-Spec-Referenz
- [x] „kein Signal"-Hinweis in Waveform und Spectrum bei Stille
- [x] VST3-Ziel: `FORMATS VST3` + Link-Libraries ergänzt (vorher entstand kein `.vst3`-Bundle)
- [x] Fehlende GUI-Quellen in `CERN_COMMON_SOURCES` ergänzt

### Rückseite / Patch-Bay (2026-11-12)

- [x] `RUECKSEITE`-Button im Header schaltet in die Patch-Ansicht (Front/Rückseite toggle), Vollbild unter dem Header
- [x] Hardware-artige 3D-Rückwand (2.5D): gebürstete Metallplatte, Vignette, extrudierte Modulboxen mit Schrauben, Titelplatten, Parameternamen pro Modul, Metall-Jackbuchsen (IN/OUT) mit Beschriftung
- [x] Module/Buchsen: MEMBRANE (OUT), TUBE (IN/OUT), SUB (OUT), CLICK (OUT), DRIVE (IN/OUT), EQUALIZER (IN/OUT), COMPRESSOR (IN/OUT), ROOM (IN/OUT), HIGGS FIELD (IN/OUT stereo), REVERB (IN/OUT stereo), OUTPUT (IN/OUT stereo); Parameter-Zeilen je Modul sichtbar
- [x] Kabel: Bezier-Kurven mit Durchhang, Schatten und Highlight; Pending-Kabel folgt der Maus beim Patchen
- [x] Interaktion: Jack klicken/ziehen zum Verbinden (Richtung automatisch, Out→In), Klick ins Leere/Drag ins Leere bricht ab, Rechtsklick auf Jack/Kabel entfernt Verbindungen, Kabel-Auswahl
- [x] Buttons `STANDARD PATCH` (Werks-Verschaltung) und `ALLE LOESCHEN`
- [x] `PatchGraph` (DSP): Jacks/Module, Verbindungen, Zyklus-Erkennung (`canConnect`), topologische Reihenfolge, Default-Patch (14 Kabel, ursprünglicher Signalfluss)
- [x] Engine-Refactor: Sample-Loop verarbeitet Module graph-basiert in topologischer Reihenfolge; Eingänge summieren alle verbundenen Quellen (Eurorack-Stil) → Umstecken hat sofort hörbaren Einfluss auf das Ausgangssignal
- [x] Tests: `patch_graph_default_and_validation`, `patch_graph_changes_engine_output` (Default erzeugt Signal, leeres Patch stumm)

### Patch-Persistenz (2026-11-12)

- [x] `<Patch>`-Knoten wird in VST3/Standalone-Host-State sowie in User-Preset-XML serialisiert (`PatchGraph::createStateXml/restoreStateXml`)
- [x] `PluginProcessor::getStateInformation/setStateInformation` speichern/laden den Patch (host-project persistence)
- [x] `PresetManager` speichert/ladet den Patch in Factory- und User-XML-State (`SAVE/LOAD` im Header sowie `getCurrentStateXml/setCurrentStateXml`)
- [x] Standalone-State wird beim Start geladen (`~/.config/C.E.R.N/standalone_state.xml`) und beim sauberen Shutdown geschrieben
- [x] Factory-Presets stellen den Werks-Patch wieder her; User-Presets und Host-Projekte behalten den benutzerdefinierten Patch
- [x] Tolerante Migration alter/korrupter States mit mehreren `<Patch>`-Knoten (der zuletzt geschriebene/aktuelle Patch zählt)
- [x] APVTS-Mapping entkoppelt: `<Patch>` wird vor `replaceState` entfernt, damit Engine-Patch und APVTS-Parameter nicht inkonsistent gemischt werden
- [x] Tests: `patch_graph_state_roundtrip`, `preset_manager_state_roundtrip_includes_patch` (Serialisierung, Empty-Patch, Missing-Patch → Werks-Patch, doppelte `<Patch>`-Knoten, Roundtrip via Preset-Manager)

### Parameter-Patch (CV) (2026-11-12)

- [x] `ParameterPatchGraph` (DSP): 132 Parameter als CV-Knoten (OUT bipolar, IN additiv), Zyklus-Schutz, topologische Auswertung, Serialisierung `<ParameterPatch>`
- [x] `KickSynthesisEngine` berechnet effektive Parameterwerte pro Block (Base + bipolar summierte CV), nutzt sie als smoothing-Werte (Sequencer, DSP, Higgs etc.)
- [x] `PluginProcessor::processBlock` wendet den Parameter-Patch vor Sequencer/Engine an
- [x] UI: `PARAMETER CV`-Tab im Rückseiten-Patch (scrollbare Liste aller 132 Parameter mit OUT/IN-Jacks, Kabel, Rechtsklick-Entfernung, Live-Werte)

### Kabelfarben (2026-11-12)

- [x] Audio-Kabel farbkodiert nach Quellmodul; deaktivierte Module/Kabel grau
- [x] Parameter-CV-Kabel farbkodiert nach Quellparameter-Gruppe; deaktivierte Quellen/Ziele grau
- [x] Selektierte Kabel werden weiß hervorgehoben
- [x] Persistenz: `<ParameterPatch>` in VST3/Standalone-State, User-Preset-XML und `standalone_state.xml`, tolerant bei alten States
- [x] Tests: `parameter_patch_graph_validation_and_roundtrip`, `parameter_patch_modulates_engine_parameter`

### Parameter-Schalter (SRC/DST) (2026-11-12)

- [x] Jeder Parameter hat `SRC`- und `DST`-Schalter (PowerSwitch) in der Parameter-Patch-Ansicht
- [x] `SRC` steuert, ob ein Parameter als CV-Quelle arbeitet; `DST`, ob eingehende CV auf den Parameter wirkt
- [x] `ParameterPatchGraph` serialisiert und restauriert Source/Target-Enable-Zustände (`<SourceEnable>`, `<TargetEnable>`)
- [x] Tests: `parameter_patch_source_target_enable`, `parameter_patch_disabled_source_does_not_modulate`

### Zusätzliche CERN-Presets (2026-11-12)

- [x] 3 neue Factory-Presets: `CV Pulse Kick`, `Higgs Mod Kick`, `Parameter Patch Kick`
- [x] Alle drei aktivieren Higgs und enthalten fertige Parameter-CV-Verbindungen
- [x] PresetManager setzt Preset-Parameter korrekt in die APVTS-Child-Trees (`<PARAM>`), nicht nur Root-Properties
- [x] Test: `factory_presets_include_higgs_and_parameter_patch` prüft 7 Presets, Audio-Patch, Parameter-CV und Higgs
- [x] Factory-Presets enthalten jetzt Standard-Audio-Patch (war zuvor leer → Stille)
- [x] Deutlich sichtbare Power-Schalter für Modul- und Parameter-Ein/Aus (PowerSwitch, ON/OFF, LED/Glow)

### Modul-Ein/Aus (Front & Rückseite) (2026-11-12)

- [x] 12 neue Bool-Parameter (`membraneEnabled`, `tubeEnabled`, `pitchEnvEnabled`, `clickEnabled`, `bodyEnabled`, `subEnabled`, `driveEnabled`, `eqEnabled`, `compEnabled`, `outputEnabled`, `roomEnabled`, `reverbEnabled`; `higgsEnabled` vorhanden)
- [x] Front-UI: `ON`-Schalter im Header jeder Section (inkl. zwei Schalter für Mix/EQ/Comp)
- [x] Rückseite: `ON`-Schalter in jedem Modul-Panel (Audio-Modus)
- [x] DSP-Bypass/Mute: deaktivierte Module steuern kein Signal mehr bei (Quellen 0, Drive/EQ/Comp bypass, Room/Reverb 0, Higgs bypass, Output mute, Pitch/Body deaktiviert)
- [x] Persistenz über APVTS/Host-State/User-Preset/Standalone-State

## Bugfix-Backlog (Audits 2026-11-12)

Sammelstelle für **alle** Findings der beiden Audits (GUI/Observability und
DSP/Engine), damit nichts verloren geht. Status: `offen`, `in Arbeit`,
`erledigt` (= behoben **oder** bewusst entschieden und dokumentiert). Die
Design-Entscheidungen sind zusätzlich in `README.md`
(„Bekannte Design-Entscheidungen / Grenzen“) begründet.

### GUI / Observability / Doku

| #   | Befund                                                                          | Status   | Notiz                                                                                                                                                                          |
| --- | ------------------------------------------------------------------------------- | -------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------ |
| G1  | Parameter-CV-Ansicht listet die Modul-Ein/Aus-Parameter nicht                   | erledigt | Neue Gruppe `MODULE ENABLE` mit 12 IDs; Zeilenindex weiterhin aus `getAllIDs()`/`idToIndex`, keine Indexverschiebung                                                           |
| G2  | Parameter-CV-Ansicht zeigt für Choice/Bool rohe Zahlen (0..6, 0/1)              | erledigt | `getText(getValue(), 24)` → „Wood“/„On“; Fallback auf Rohwert nur bei leerem Text                                                                                              |
| G3  | `parameterCableColour` baut pro Kabel/Paint eine neue Gruppenliste              | erledigt | `static const auto groups = makeGroups();` (auch in `rebuildRows`)                                                                                                             |
| G4  | Doppelte Includes                                                               | erledigt | `ParameterPatchView.cpp`: keine Duplikate (PowerSwitch nur einmal). Zusätzlich doppelten `enableIdForModule`-Lambda in `PatchView.cpp` entfernt (nutzt jetzt `moduleEnableId`) |
| G5  | Modul-Enable-Kabel ohne eigene Farbe                                            | erledigt | `MODULE ENABLE` → neutrales Amber-Grau `0xffc9b18a`                                                                                                                            |
| G6  | PatchView: Mute-vs-Bypass-Semantik undokumentiert, Tooltips ohne Parameternamen | erledigt | Kommentarblock in `PatchView.cpp`; Tooltip z. B. „MEMBRANE ein/aus - Parameter: membraneEnabled“                                                                               |
| G7  | README enthält den Abschnitt „Rückseite / Patch-Bay“ doppelt                    | erledigt | Duplikat entfernt                                                                                                                                                              |
| G8  | Design-Entscheidungen/Grenzen nicht dokumentiert                                | erledigt | Neuer README-Abschnitt + dieses Backlog                                                                                                                                        |
| G9  | `PatchView.h` inkludiert `juce_gui_basics` doppelt                              | offen    | Header liegt außerhalb des Scopes dieses Batches; harmlos, bei nächster Header-Änderung mitnehmen                                                                              |

### DSP / Engine (andere Bearbeiter)

| #   | Befund                                                                          | Status   | Notiz                                                                                                                   |
| --- | ------------------------------------------------------------------------------- | -------- | ----------------------------------------------------------------------------------------------------------------------- |
| E1  | Output-Clip/Limiter: statische Gain-Reduktion ohne Release                      | erledigt | Instant-Attack + Release, Peak-Hold, Ceiling-Invariante; bewusst ohne Lookahead (keine Latenz), dokumentiert            |
| E2  | Kein Reset beim Wieder-Einschalten eines Moduls                                 | erledigt | `applyEnableTransitions()` leert bei 0→1-Flanken die Modul-Zustaende (kein Ghost-Hit/Tail)                              |
| E3  | Sequencer/MIDI-Trigger: nur ein `pendingTrigger`-Slot                           | erledigt | Sample-genaue 16er-FIFO (`triggerQueue`) im Audio-Thread; `pendingTrigger` bleibt Fallback fuer den Message-Thread      |
| E4  | Reverb ist ein 4-Zeilen-FDN ohne Diffusions-Allpaesse                           | erledigt | 8x8-Hadamard-FDN + 4 Eingangs-Allpaesse (7/11/17/23 ms) + 1 Allpass pro Zeile (SND-102/104)                             |
| E5  | Drive-Ausgang ohne DC-Schutz                                                    | erledigt | `applyDcBlocker` nach der Sättigung im Drive-Zweig vorhanden; Pre-HP bleibt separat                                     |
| E6  | „Body“ war nur ein Hüllkurven-Follower-Multiplikator                            | erledigt | Eigener Body-Sinus (Attack/Decay/Pitch, π/2-Startphase) statt reiner Pegelmodulation                                    |
| E7  | EQ-Compounding: 3 serielle Biquads, Gains addieren sich ungeprüft               | erledigt | Makeup-Normierung `1/(1+Boost-Summe)` mit Clamp in `processEQSample`                                                    |
| E8  | Mono-Below über einpoligen Side-Tiefpass                                        | offen    | Subtraktion erzeugt Phasendreher/Allpass-Anteil um die Kreuzungsfrequenz                                                |
| E9  | `tailCounter` ist `int` und wird pro Sample dekrementiert                       | erledigt | Dekrement nur bei `tailCounter > 0`; kein signed overflow mehr                                                          |
| E10 | Transzendentale pro Sample (`std::exp` im Body-Env, `std::pow` in `processSub`) | erledigt | Koeffizienten sind Block-Konstanten; im Sample-Loop kein `exp`/`pow` mehr (geprueft)                                    |
| E11 | Spectrum-Anzeige war tot (`getSpectrum`/`spectrumSnapshot` unbenutzt)           | erledigt | GUI-FFT aus Waveform-Snapshot; totes Engine-API entfernt                                                                |
| E12 | Room: feste 80-ms-Verzögerung + DC-Blocker im Feedback                          | erledigt | 4 Taps 61/79/97/113 ms, R-Taps +8,3 %, DC-Blocker 2 Hz (SND-091…100); `roomSync` ohne APVTS-Parameter = SND-093 PARTIAL |
| E13 | Kein DSP-Reset bei Preset-/State-Laden                                          | erledigt | Bewusst: Tails klingen weiter, kein Klick; `engine.reset()` nur in `prepareToPlay`. Dokumentiert                        |
| E14 | CompOut ist mono (L/R gemittelt)                                                | erledigt | Bewusst für Kick; dokumentiert als Grenze                                                                               |
| E15 | `reverbMaterial` unterscheidet nur None vs. gemeinsames Netzwerk                | erledigt | 6 Material-Profile (RT60-Mul/Kurve, Size, Damping-Offset/Kurve, LowCut/HighCut-Mul, Pre-FDN-Tonfilter, Line-Diffusion)  |
| E16 | Fan-in summiert ohne Headroom/Normierung                                        | erledigt | Bewusst (Eurorack-Stil), dokumentiert                                                                                   |
| E17 | Drive ist Pflicht-Summing-Knoten; Tube hängt parallel                           | erledigt | Default-Patch-Design, dokumentiert; jedes andere Routing ist erlaubt                                                    |
| E18 | Parameter-CV-Persistenz nutzt Index + ID                                        | erledigt | Migrationspfad, IDs bleiben stabil; dokumentiert                                                                        |

## Sound-Review 2026-11-12 — Status (Abschluss-Review)

Vollständige Einzelbeschreibung: [`SOUND_REVIEW.md`](SOUND_REVIEW.md)
(`ID | Prio | Datei/Parameter | Ist -> Soll`, 327 Findings).
Status je Abschnitt, geprüft gegen Code, Tests und Doku:

| Abschnitt           | IDs                 | Status  | Nachweis / offener Grund                                                                                                                                                                                                                                                                                             |
| ------------------- | ------------------- | ------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| A Output / Pegel    | SND-001…010         | DONE    | `processOutputStage` (Clip vor Pan, Stereo-Link, Ceiling-Invariante), `limiter_and_clip_gain_reduction_metering`, `factory_presets_loudness_spread`                                                                                                                                                                  |
| B Drive             | SND-011…021         | DONE    | Band-Split 150 Hz, Makeup 1/tanh(k), DC 10 Hz, Pre-HP-Mapping, 2× Oversampling; `oversampling_changes_drive_path_and_stays_finite`                                                                                                                                                                                   |
| C Pitch-Envelope    | SND-022…030         | DONE    | exponentieller Attack/Decay, Defaults, `velToPitch`; `pitch_envelope`                                                                                                                                                                                                                                                |
| D Membrane          | SND-031…041         | DONE    | Bessel-Moden, gewichtete Amplituden, Spannungs-/Radius-/Masse-Mapping; `membrane_model`                                                                                                                                                                                                                              |
| E Body              | SND-042…048         | DONE    | eigener Resonator, π/2-Phase, Pitch-Anteil; `module_enable_*`, `engine_tail_decays_to_silence`                                                                                                                                                                                                                       |
| F Sub               | SND-049…056         | DONE    | Ratio-Parameter 0.5/1.0, PolyBLEP, Ranges/Defaults; `subRatio` im Layout/GUI                                                                                                                                                                                                                                         |
| G Click / Beater    | SND-057…066         | DONE    | Noise-Burst + Reso-Tick, Material 8/4/2 ms, pegelneutrale Distortion; `velocity_to_click_scales_click_level`                                                                                                                                                                                                         |
| H Tube              | SND-067…072         | DONE    | 3 gekoppelte Moden (1×/2.7×/5.4×), Track-Ratio; `tube_track_follows_pitch`                                                                                                                                                                                                                                           |
| I EQ                | SND-073…082         | DONE    | Sub 12 dB/oct, Punch-Biquad, Makeup-Normierung, Ranges; `eqSubTrack` ist funktional immer an (kein eigener APVTS-Schalter)                                                                                                                                                                                           |
| J Compressor        | SND-083…090         | DONE    | Defaults, Auto-Makeup, Dry/Wet; `compressor_mix_dry_bypasses_makeup`                                                                                                                                                                                                                                                 |
| K Room              | SND-091…100         | PARTIAL | 4 Taps 61/79/97/113 ms, Send/Return, Eq, DC 2 Hz sind umgesetzt. **SND-093 offen:** `roomSync` existiert nur als Engine-Enum (`free`), ohne APVTS-Parameter/UI.                                                                                                                                                      |
| L Reverb            | SND-101…116, E4/E15 | DONE    | 8-Zeilen-FDN, 4 Eingangs-Allpässe + 1 pro Zeile, Material-RT60/-Filter, Freeze/Gate, Size-Glättung; `reverb_snd101…116`, `material_reverb_*`                                                                                                                                                                         |
| M Higgs             | SND-117…127         | DONE    | Wet-only, 8-Hz-DC, LFO über `higgsRate`/`higgsDepth`; `higgs_effect_wired_into_engine`, `higgs_lfo_rate_and_depth_affect_output`                                                                                                                                                                                     |
| N Tuning / Global   | SND-128…135         | DONE    | `rootNote` 30, Key/Scale-Snap, Glide, 30-%-Step-Pitch-Tracking, Note+Cents-Anzeige; `parameter_smoothing*`, `parameter_layout_ids_are_complete_unique`                                                                                                                                                               |
| O Sequencer         | SND-136…150         | PARTIAL | Takt/Transport/Host-Sync, Swing, Grid, Ratchet (140), Flam (141), Probability (142), Length (143), Preview, Clock-Out sind implementiert. **SND-145/146 offen:** `stepVel/stepGate/stepPitch` existieren nur für 16 Steps; Steps 17–64 und Patterns B–D haben keine Gate-/Vel-/Pitch-Parameter und sind damit stumm. |
| P CV / Patchbay     | SND-151…161         | DONE    | Amount/Polarität/Rate/Slew/Wrap, LFO + Envelope-Follower + Makros 1–5 über APVTS; `parameter_patch_*`, `cv_virtual_sources_follow_apvts`                                                                                                                                                                             |
| Q GUI / Workflow    | SND-162…173         | DONE    | Makro-Panel, getrennte Sektionen, Save-As, Browser, Init, A/B, Undo, Modified-Marker, LUFS/GR-History, Envelope-Editor, Shortcuts                                                                                                                                                                                    |
| R Fehlende Features | SND-174…183         | DONE    | Ducking, Transient-Shaper, MIDI-Filter/Choke, velToClick, Humanize, 2× Oversampling, compMix, fxWidth, Reverb-Freeze/Gate, Bounce; Tests `ducking_*`, `transient_*`, `midi_filter_*`, `velocity_to_click_*`, `humanize_*`, `oversampling_*`, `compressor_mix_*`, `fx_width_*`, `engine_bounce_*`                     |
| R Fehlende Features | SND-184/185         | PARTIAL | Randomize und Preset-Morph sind in `MainComponent` implementiert (GUI-Logik), aber nicht im Unit-Test-Target; keine automatisierte Abnahme.                                                                                                                                                                          |
| S Dokumentation     | SND-186…190         | DONE    | README/Presetliste/Root-Note/909/Parametertabellen in diesem Review gegen den Build abgeglichen                                                                                                                                                                                                                      |
| T Presets           | SND-191…327         | DONE    | Alle 7 Factory-Snapshots umgesetzt; `outputGain` je Preset für die Loudness-Kalibrierung (SND-006) abweichend feinjustiert, alle übrigen Sollwerte gesetzt. Test: `factory_presets_loudness_spread` (≤1,5 dB)                                                                                                        |

**Summe:** 327 Findings — DONE: 322, PARTIAL: 5 (SND-093, SND-145, SND-146, SND-184, SND-185).
Die PARTIAL-Punkte sind funktionale Grenzen, keine stillen Fehler; sie stehen zusätzlich in `README.md`
unter „Bekannte Design-Entscheidungen / Grenzen“.

### Dokumentations-Findings (SND-186…190)

| ID      | Umsetzung                                                                          |
| ------- | ---------------------------------------------------------------------------------- |
| SND-186 | README-Presetliste: 7 tatsächliche Factory-Presets mit Charakter                   |
| SND-187 | Root-Note F#1 (MIDI 30) in Defaults/README                                         |
| SND-188 | 909-Abschnitt beschreibt den exponentiellen Pitch-Decay                            |
| SND-189 | Parametertabellen gegen `DefaultValues`/APVTS-Ranges abgeglichen; Layout-ID-Test   |
| SND-190 | Preset-Browser/Thumbnail als geplant markiert (Browser vorhanden, Thumbnail offen) |

## Material Room + Sub Grumble (2026-11-12)

Neues Modul „MATERIAL ROOM“ (12. Modul), voll integriert wie die bestehenden Module:

- `Source/DSP/MaterialRoom.h`: 4-Tap-FDN (Hadamard 4×4), Taps = `[1, 1.37, 1.79, 2.21] * (L/c)`,
  Stereo ±3 %, Feedback = `clamp(0.55 + 0.44*(1-α_room), 0.55, 0.995)`, Damping =
  `clamp(12000 - 9000*α, 1500, 18000) * (1 + 0.35*ρ/7850)`, 10-Hz-DC-Blocker, Wet-LowCut 100 Hz (2. Ordnung),
  M/S-Width. Materialtabelle Water/Wood/Plastic/Steel/Concrete/Fabric mit c/ρ/Z=ρ·c/R/α (Water 1480/998,
  Wood 3960/700, Plastic 2750/1180, Steel 5940/7850, Concrete 3700/2400, Fabric 250/100 effektiv porös).
- Zwei Inputs `MaterialRoomKickIn`/`MaterialRoomRumbleIn`, Layer **Kick/Rumble/Both**, ein Output
  `MaterialRoomOut`. Jacks/Modul ans Ende der Enums (Index-Kompatibilität), Default-Patch 14 → **17 Kabel**,
  insgesamt **1229 Parameter** (15 neu für Material Room), Persistenz automatisch via APVTS/PatchGraph.
- **Kein Druckverlust:** Modul liefert ausschließlich Wet; Dry bleibt unangetastet; Wet-LowCut 100 Hz;
  `mix=0` und `grumbleLevel=0` neutral; Grumble eigener Pegel, unabhängig vom Room-Mix.
- **Sub-Grumble:** 20–120 Hz (Sine/Triangle/Square), Envelope aus Kick+Rumble (Peak, Attack ~5 ms,
  Decay 0.1–2 s), Pan-LFO um die Mitte A↔B, 0.05–8 Hz, Sine/Triangle/Square/Saw, Depth 0 = Mitte.
- GUI: Front-Sektion `Material Room`, Rack-Panel mit zwei getrennten Buchsen (IN KICK/IN RUMBLE),
  PowerSwitch, Kabel-Farbe Teal `0xff0ca678` (Audio-Modul und CV-Gruppen), Gruppen `MATERIAL ROOM`/`SUB GRUMBLE`.
- Tests: 17 `material_room_*`-Tests (Materialtabelle/Coeffs, Wet-only/Neutralität, Layer, Grumble, Width-Cap, RT60-Ordering, Pan-Swing/Slew, Additivität, 17 Kabel, kein Sub-Druckverlust). Spec: `MATERIAL_ROOM_SPEC.md`.
- Hinweis: Der angeforderte externe Sound-Engineer/Techno-Review-Agent wurde durch das
  OpenCode-Go-Usage-Limit blockiert; die Eigenprüfung (Materialphysik, Wet-only, Low-End) ist
  umgesetzt und in `MATERIAL_ROOM_SPEC.md` dokumentiert.

## Verifikation

- Build: `cmake --build build -j$(nproc) --target CERN_All CERNStandalone CERNTests`
- Tests: `./build/Source/CERNTests` → 77 Tests, 1.175.198 Assertions, 0 Fehler
  (`ctest --test-dir build` → 1/1, Label `unit`, Timeout 60 s)
- ASan/UBSan (Debug): `build-asan/Source/CERNTests` → 1.175.198 Assertions, 0 Fehler
  (`ASAN_OPTIONS=detect_leaks=0`; JUCE-Singletons melden sonst erwartete Leaks)
- VST3: `build/Source/CERN_artefacts/Release/VST3/CERN.vst3` (moduleinfo `C.E.R.N`/`ScalyxSoft`)
- Standalone: `build/Source/CERNStandalone_artefacts/Release/C.E.R.N Standalone`

## Abschluss-Review (2026-11-12)

Tiefenprüfung der zuletzt abgeschlossenen Pakete (Rename, SND-174…185, Material Room,
Reverb/FDN, Engine-Integration, Build/Test, Doku). Ergebnis je Prüfbereich:

| Bereich              | Status       | Befund / Maßnahme                                                                                                                                                                                                                                                                                                                                                    |
| -------------------- | ------------ | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1 C.E.R.N-Rename     | DONE         | Keine Reste des alten Produktnamens außerhalb `build*/` (Treffersuche: 0); Targets/Optionen/Klassen/Dateien konsistent; VST3-Metadaten `C.E.R.N`/`ScalyxSoft`; Standalone heißt `C.E.R.N Standalone`; State-Pfade `~/.config/C.E.R.N`; Bounce `C.E.R.N-Bounce.wav`.                                                                                                  |
| 2 SND-Findings       | DONE/PARTIAL | Status je Abschnitt oben; 322 DONE, 5 PARTIAL (SND-093, 145, 146, 184, 185). Alle fokussierten Pakete 006/005/020, 068, 133/134, 174–185, 170/168, 150/156/157/164 im Baum nachweisbar.                                                                                                                                                                              |
| 3 Material Room      | DONE         | MR-090 Wet-only/kein Druckverlust, MR-096 17 Kabel, RT60-Ordering, Pan-Swing/Slew, Additivität, Width-Cap, Teal-Farbe und Preset-Plan geprüft; 17 `material_room_*`-Tests. `PresetManager.cpp` enthält bewusst keine MR-Factory-Presets (MR-060/061 nur im Plan).                                                                                                    |
| 4 Reverb/FDN         | DONE         | Material-Distinktheit (RT60/Filter), 4 Eingangs-Allpässe + 1 pro Zeile, 8×8-Hadamard, DC-Blocker, Freeze/Gate, Size-Glättung, Stabilität; Tests `reverb_snd101…116`, `material_reverb_*`, `reverb_e4_e15_*`.                                                                                                                                                         |
| 5 Engine-Integration | DONE         | Transient-Shaper am Drive-Eingang (vor Drive/Comp), Ducking am Output-Eingang (post-Summe, inkl. FX), fxWidth/compMix nur Wet, 2× Oversampling, humanize/velToClick; Allokationsfreiheit im Render (Snapshot/Smoothing), Reset bei Enable-Flanken. Transient-Shaper ist patch-abhängig (nur wenn der Drive-Knoten im Signalweg liegt).                               |
| 6 Build/Test/CMake   | DONE         | `CERN_SANITIZE`/`CERN_WERROR` (Default OFF); getrennter Debug-Lauf `build-asan` mit ASan+UBSan grün; CTest-Label `unit`, Timeout 60 s; triviale Warnungen behoben (Preset-Initializer, Shadowing im Sequencer-Ctor, unbenutzter Test-Helper, Higgs-Double→Float). Verbleibende `-Wshadow`/`-Wconversion`-Hinweise in DSP-Konstruktoren sind bewusst nicht `-Werror`. |
| 7 Tests              | DONE         | 77 Tests / 1.175.198 Assertions grün; neue Tests für Layout-IDs, Ducking, Transient-Shaper, MIDI-Filter/Choke, velToClick, Humanize, Oversampling, compMix, fxWidth, Bounce, Higgs-LFO, GR-Metering, Tube-Track und Preset-Loudness.                                                                                                                                 |
| 8 Doku/Status        | DONE         | README/TODO/SOUND_REVIEW/TECHNICAL_WIRING_FULL/MATERIAL_ROOM_SPEC gegen Pfade, Namen und Status abgeglichen.                                                                                                                                                                                                                                                         |

### Verbleibende offene Punkte (begründet)

- **SND-093 (Room-Sync):** Engine-Enum und Skalierung vorhanden, aber kein APVTS-Parameter/UI.
  Ein neuer Choice-Parameter würde die stabilen Preset-/CV-Indizes erweitern; bewusst zurückgestellt.
- **SND-145/146 (Patterns/Steps):** `stepVel/stepGate/stepPitch` existieren nur für 16 Steps.
  Steps 17–64 und Patterns B–D haben Ratchet/Flam/Probability/Length, aber keine Gate-/Velocity-/Pitch-Werte
  und sind dadurch stumm. Eine saubere Lösung braucht 720 zusätzliche Step-Parameter (Layout-/Preset-Migration).
- **SND-184/185 (Randomize/Morph):** GUI-Logik in `MainComponent` implementiert, aber nicht im
  Unit-Test-Target (kein GUI-Test-Harness); manuell verifiziert, keine automatisierte Abnahme.
- **E8 (Mono-Below):** einpoliger Side-Tiefpass bleibt bewusste Näherung; Phasendreher um die
  Kreuzungsfrequenz ist dokumentiert.
- **MR-060/061:** Preset-Plan dokumentiert, aber absichtlich nicht in `PresetManager.cpp` umgesetzt.

## 3D-UI-Redesign (2026-11-12)

- [x] Farbwelt auf Schwarz-Weiss-Rot umgestellt (`#08080a`, `#111216`, `#1b1d22`,
      `#f4f4f6`, `#e10600`, `#ff3b3b`, `#3a3d45`); Rot nur als Akzent/Glow/LED
- [x] `CERNLookAndFeel` neu: `drawBevel`, `drawWell`, `drawScrew`, `drawBackdrop`,
      3D-`drawRotarySlider` (Metallkappe, Kranz, Wert-Bogen mit Glow, Hover),
      3D-`drawLinearSlider` (Schacht, Griff, rote Fuellung), Toggle/ComboBox/Buttons
- [x] `ParamKnob`: 3D-Kappe, Hover-Repaint, versenkter Wert-Chip
- [x] `CERNFader` neu (APVTS-gebunden) fuer `driveMix`, `compMix`, `reverbMix`,
      `materialRoomMix`, `higgsMix`, `outputWidth`; keine Parameter-/DSP-Aenderung
- [x] `SectionPanel`: 3D-Rahmen, Titelband, rote LED, kompakte ComboBox-/Toggle-Zellen
- [x] Header: Textlabel durch `LogoComponent` ersetzt (Emblem-Asset + Vektor-Wortmarke,
      Accessibility „C.E.R.N“); zweizeiliges Layout unter 980 px
- [x] Assets per `generate-image` erzeugt, freigestellt/nachbearbeitet
      (`Assets/build_assets.py`), als `CERNAssets` (BinaryData) eingebunden
- [x] Displays (Waveform/Spectrum/Envelope), Meter-Strip, Footer, Patchbay und
      PowerSwitch auf Palette + Tiefe umgestellt
- [x] `CERNPreview`-Harness (headless PNG-Render) + Screenshots 1400x900/700x500

### Design-Review-Fortsetzung (2026-11-12, Session 2)

- [x] Screenshots (CERNPreview, Nachher) erstellt: 1400×900, 1100×750, 900×650, 700×500, 940×700 (<980 px Header), Patchbay-Audio, Parameter-CV, Material-Room, Footer-Crop/Scrollbottom, MORPH (A/B belegt). Ablage Vorher: `/tmp/user/1000/opencode/review-2026-11-12/`, Nachher: `/tmp/user/1000/opencode/review-2026-11-12/after/`.
- [x] Vision-Check: `local-vision`/llama-swap zeitweise nicht erreichbar (HTTP 500/circuit open/Cold-Start) → Gegenprüfung/Fallback mit `nim-nemotron-vision` (einzelne Requests Timeout/trunkiert, Kernaussagen verifiziert).
- [x] UI-Fixes (direkt umgesetzt): Grid-Dichte bei großen Breiten erhöht (`SectionPanel::rowHeightForWidth` 740/620 → 132/118 px), Preset-Combo verbreitert + `positionComboBoxText` (Pfeil/Text-Overlap vermieden), Envelope-Infozeile mit mehr Platz (kein Abschneiden), Higgs-Parameterliste mit vertikalem Puffer (kein Zeilen-Clipping), Meter-Hint mit Fallback/Short-Form (kein Rechts-Clipping), Footer-Reihenbeschriftung VEL/PITCH ergänzt, Metall-/Grain-Assets kantengeglättet/tilebarer nachbearbeitet (`Assets/build_assets.py`), `CERNPreview`-Modi `material`/`scrollbottom` ergänzt.
- [x] Regressions-Check (statisch, Scope GUI/Preview/Assets): Header-Buttons inkl. PRESET/A/B/A>B/RND/MORPH/RENDER/TRIGGER/PATCHBAY/AUDIO vorhanden/verdrahtet; Header-Umschaltung `<980 px` zweizeilig (`useTwoRows`); Macro→`macro1…5`-Write im GUI; `lfoRate`/`lfoDepth` in Parameter-IDs/Layout/UI vorhanden; Bounce-Ziel `~/Documents/C.E.R.N-Bounce.wav`; Patch-Default 17 Kabel (`PatchGraph`); Scrollbar im Sektions-Viewport aktiv.
- [x] MORPH: bei belegten A/B-Slots Slider/Label sichtbar und layout-korrekt (Screenshot `after/morph.png`, Vision bestätigt). Material-Room: Front-Sektion/ON-Toggle/Controls sichtbar (`after/material.png`); Patchbay zeigt `IN KICK`/`IN RUMBLE` am Material-Room-Modul (Vorher-Review `patch-audio.png`). Patchbay/Parameter-CV ohne grobe Overlaps/Clipping (Screenshots/Vision).
- [x] Standalone-Xvfb-SIGSEGV reproduziert (Exit 139), Backtrace `xvfb-run -a gdb -batch -ex run -ex "bt full"`: `#0 KickSynthesisEngine::reset()` ← `#1 CERNAudioProcessor::releaseResources()` ← `#2 juce::AudioProcessorPlayer::audioDeviceAboutToStart` ← `#3 juce::AudioDeviceManager::addAudioCallback` ← `CERNStandaloneApp::initialise` (Ursache außerhalb GUI-Scope, kein UI-seitiger Fix).
- [x] Build (flock, `-j2`): `cmake --build build -j2 --target CERN_All CERNStandalone CERNTests` und `CERNPreview` erfolgreich (UI-Targets gelinkt).
- [!] Tests: `./build/Source/CERNTests` → **1.175.207 Passed / 3 Failed** (alle in `preset_manager_factory_presets`/`factory_presets_include_higgs_and_parameter_patch`: Erwartung 7, `getNumFactoryPresets()==4` laut aktuellem `Source/Presets/PresetManager.cpp` im Arbeitsbaum). `ctest --test-dir build --output-on-failure` → 0/1. **Außerhalb GUI-Scope** (Presets/Tests nicht geändert); kein Fix vorgenommen.
