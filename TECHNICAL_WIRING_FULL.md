# CERN — Vollständiger Technischer Wiring- & UI-Implementierungsplan

**Stand:** 2026-11-12 — C.E.R.N-Rename, Sound-Review-Pakete und Material Room abgeschlossen (Abschluss-Review, siehe `TODO.md`)  
**Build (verifiziert):** `CERN`, `CERN_All`, `CERNStandalone`, `CERNTests` — alle `Built target`, keine Fehler  
**Tests (verifiziert):** `./build/Source/CERNTests` → 77 Tests / 1.175.198 Assertions, 0 Fehler; `ctest` 1/1  
**VST3 (verifiziert):** `build/Source/CERN_artefacts/Release/VST3/CERN.vst3/Contents/x86_64-linux/CERN.so` (18.9 MB), `GetPluginFactory` exportiert  
**Standalone (verifiziert):** `build/Source/CERNStandalone_artefacts/Release/C.E.R.N Standalone`, Start unter Xvfb ohne Crash  
**Parameter:** 1229 im APVTS (`ParameterIDs::getAllIDs()`; inkl. 12 Modul-Enables, 15 Material Room/Grumble, 3 Feature-IDs)  
**Direkt bedienbar:** Front-/Footer-UI plus Parameter-CV-Ansicht für alle 1229 IDs; Host-Automation für alle Parameter  
**UI:** RED-Design (KickForge-Referenz-Merge), Header 68 px, Footer 200 px, Sidebar 300 px, scrollbarer Sektions-Canvas (Panels mit dynamischer Höhe)

> **Hinweis zum Parameter-Index:** Kapitel §2 beschreibt den historischen 132-Parameter-Stand
> des ersten Entwurfs. Der aktuelle Build hat **1229** Parameter (`ParameterIDs::getAllIDs()`);
> die zusätzlichen IDs (Step-Patterns, Modul-Enables, Material Room/Grumble, Feature-Findings)
> sind in `ParameterIDs.h`/`ParameterLayout.cpp` und in `README.md` dokumentiert.

---

## Inhaltsverzeichnis

1. [System-Architektur & Thread-Modell](#1-system-architektur--thread-modell)
2. [Parameter-Index (alle 132) mit GUI- & DSP-Mapping](#2-parameter-index-alle-132-mit-gui--dsp-mapping)
3. [Audio-Routing: Signal-Flow durch alle DSP-Stufen](#3-audio-routing-signal-flow-durch-alle-dsp-stufen)
4. [Pro-Block Parameter-Lese-Reihenfolge](#4-pro-block-parameter-lese-reihenfolge)
   4.7. [Reverb / Room — Algorithmus, Parameter und Verhalten](#47-reverb-room-algorithmus-parameter-und-verhalten)
5. [Pro-Sample Verarbeitungsschritte](#5-pro-sample-verarbeitungsschritte)
6. [GUI-zu-Parameter-Verdrahtung](#6-gui-zu-parameter-verdrahtung)
7. [StepSequencer / FooterSequencer: 48 Pro-Schritt-Parameter + Swing + Klick](#7-stepsequencer--footersequencer-48-pro-schritt-parameter--swing--klick)
8. [Smoothing-Chain](#8-smoothing-chain)
   8.1. [Fluss](#81-fluss)
   8.2. [API](#82-api)
   8.3. [Warum Smoothing wichtig ist](#83-warum-smoothing-wichtig-ist)
   8.4. [Zentrales Kapitel: P0-P1-Fixes F-01 bis F-08](#84-zentrales-kapitel-p0-p1-fixes-f-01-bis-f-08)
   8.5. [Higgs-Glossar](#85-higgs-glossar)
   8.6. [Dokumentation automatisch gegen Code prüfen (Doc-Lint)](#86-dokumentation-automatisch-gegen-code-prüfen-doc-lint)
9. [Higgs Field: Physik & Symmetry-Gating](#9-higgs-field-physik--symmetry-gating)
10. [Preset & Trigger Wiring](#10-preset--trigger-wiring)
11. [Message-Thread vs Audio-Thread Datenfluss](#11-message-thread-vs-audio-thread-datenfluss)
12. [Offene Punkte & Bekannte Lücken](#12-offene-punkte--bekannte-lücken)
13. [Build-, Runtime- & UI-Verifikation](#13-build--runtime--ui-verifikation)
14. [Design-Umsetzung (RED) & Komponenten-Inventar](#14-design-umsetzung-red--komponenten-inventar)
15. [Änderungsprotokoll dieser Dokumentation](#15-änderungsprotokoll-dieser-dokumentation)

---

## 1. System-Architektur & Thread-Modell

### 1.1 Architektur-Übersicht

```text
┌──────────────────────────────────────────────────────────────────────────────────────────┐
│                              CERN VST3 Plugin                                      │
│                                                                                          │
│  ┌─────────────────┐     ┌─────────────────────────────────────────────────────────┐     │
│  │  JUCE Host      │────▶│  AUDIO THREAD (44.1 kHz / 48 kHz / 96 kHz)              │     │
│  │  (DAW)          │     │  processBlock()                                        │     │
│  │                 │     │   ├─ pendingTrigger → engine.trigger(vel, pitch)        │     │
│  │  MIDI/Audio     │     │   ├─ smoothing.update()   (APVTS → geglättet)          │     │
│  │  Buffer         │     │   ├─ engine.render()      (alle DSP-Stufen + Higgs)     │     │
│  │                 │     │   ├─ waveformBuffers[write] füllen (Double-Buffer)     │     │
│  │                 │     │   └─ spectrumMagnitudesSnapshot füllen                │     │
│  └─────────────────┘     └─────────────────────────────────────────────────────────┘     │
│                                          │                                               │
│                                          ▼  thread-safe (atomic / Double-Buffer)         │
│  ┌─────────────────┐     ┌─────────────────────────────────────────────────────────┐     │
│  │  GUI THREAD     │◀────│  APVTS (AudioProcessorValueTreeState)                  │     │
│  │  (Message)      │     │  1229 Parameter                                        │     │
│  │                 │     │  ├─ SliderAttachment  (Knobs → APVTS)                  │     │
│  │  MainComponent  │     │  ├─ ButtonAttachment  (Toggles → APVTS)                │     │
│  │  + Panels       │     │  └─ ParameterListener (StepSequencer, HiggsField3D)    │     │
│  │  + Displays     │     └─────────────────────────────────────────────────────────┘     │
│  │  + FooterSeq.   │                                                     ▲             │
│  └─────────────────┘                                                     │             │
│      Timer: FooterSequencer 60 Hz, Displays 30 Hz                        │             │
│           │                                                              │             │
│           └──────── repaint() ───────────────────────────────────────────┘             │
└──────────────────────────────────────────────────────────────────────────────────────────┘
```

Zwei Threads, eine Brücke:

- **GUI-Thread** schreibt in APVTS (`SliderAttachment`, `ButtonAttachment`, Choice-Combos, Higgs-3D-Maus).
- **Audio-Thread** liest APVTS ausschließlich über `smoothing.update()` → `smoothing.getById(id)`.
- **Displays** (Waveform/Spectrum) werden über thread-sichere Puffer gelesen; kein Lock im Audio-Thread.
- **Trigger** laufen über atomare `pendingTrigger`/`pendingVelocity`/`pendingPitchSemitones`-Variablen.

### 1.2 Klassen-Übersicht

| Klasse                           | Datei                                            | Rolle                                                                                          | Thread          |
| -------------------------------- | ------------------------------------------------ | ---------------------------------------------------------------------------------------------- | --------------- |
| `CERNAudioProcessor`       | `Source/PluginProcessor.{h,cpp}`                 | AudioProcessor, APVTS-Owner, PresetManager, StepSequencer-Member                               | Audio + Message |
| `KickSynthesisEngine`            | `Source/DSP/KickSynthesisEngine.{h,cpp}`         | Monophone Kick-Synthese (Membrane, Tube, Sub, Click, Drive, EQ, Comp, Output, FX)              | Audio           |
| `HiggsFieldEngine`               | `Source/DSP/HiggsFieldEngine.h`                  | 5-Band-Kopplung + Higgs-Resonator, symmetry-gated                                              | Audio           |
| `ParameterSmoothing`             | `Source/Parameters/ParameterSmoothing.h`         | APVTS → geglättete DSP-Werte                                                                   | Audio           |
| `StepSequencer`                  | `Source/Sequencer/StepSequencer.{h,cpp}`         | Sample-genauer Sequencer im Audio-Thread via `processBlock()` (F-02), **nicht** im UI sichtbar | Audio           |
| `PresetManager`                  | `Source/Presets/PresetManager.h`                 | Factory-JSON + User-XML-Presets                                                                | Message         |
| `CERNAudioProcessorEditor` | `Source/PluginEditor.{h,cpp}`                    | Host-Editor, hält `MainComponent` + Settings-Button                                            | Message         |
| `MainComponent`                  | `Source/GUI/MainComponent.{h,cpp}`               | Wurzel-UI (plain `Component`, Layout, APVTS-Attachments)                                       | Message         |
| `HeaderBar`                      | `Source/GUI/HeaderBar.{h,cpp}`                   | Logo, Preset-Browser, CPU, Slogan, TRIGGER                                                     | Message         |
| `FooterSequencer`                | `Source/GUI/FooterSequencer.{h,cpp}`             | 16-Step-UI, Transport, Swing; schreibt `sequencerPlaying`, spiegelt Playhead                   | Message         |
| `SectionPanel`                   | `Source/GUI/SectionPanel.{h,cpp}`                | Chamfered Panel, generisches Knob-/Choice-Grid, optionaler Top-View                            | Message         |
| `SpectrumAnalyzer`               | `Source/GUI/SpectrumAnalyzer.h`                  | 96 Balken, 20 Hz–18 kHz                                                                        | Message         |
| `WaveformDisplay`                | `Source/GUI/WaveformDisplay.h`                   | 4096-Sample-Wellenform + Raster                                                                | Message         |
| `HiggsField3D`                   | `Source/GUI/HiggsField3D.h`                      | Interaktive Mexican-Hat-Projektion + 10 Slider + Enable                                        | Message         |
| `EnvelopeEditor`                 | `Source/GUI/EnvelopeEditor.h`                    | Pitch-Envelope-Visualizer (read-only)                                                          | Message         |
| `CERNLookAndFeel`          | `Source/GUI/Theme/CERNLookAndFeel.{h,cpp}` | Knobs/Slider/Buttons/Panels im RED-Design                                                      | Message         |
| `ColourScheme` / `FontScheme`    | `Source/GUI/Theme/`                              | Farb- und Font-Singletons                                                                      | Message         |
| `Theme` (Legacy-Namespace)       | `Source/GUI/Theme.h`                             | RED-Remap für ältere GUI-Komponenten                                                           | Message         |

### 1.3 GUI-Hierarchie

```text
CERNAudioProcessorEditor (AudioProcessorEditor, 1400×900, resizable bis 2800×1800)
└── MainComponent (plain Component, ComboBoxAttachment statt Timer)
    ├── HeaderBar (68 px)
    │   ├── Logo "KICKFORGE" + Subtitle
    │   ├── Preset: ◀ | ★ | ComboBox | ▶ | 💾 | 📁 | 📦
    │   └── CPU-Label + Slogan + TRIGGER
    ├── Viewport (vertikaler Scroll)
    │   └── sectionCanvas
    │       ├── SectionPanel "MEMBRANE"           (8 Knobs)
    │       ├── SectionPanel "HOLLOW STEEL"        (5 Knobs)
    │       ├── SectionPanel "PITCH ENVELOPE"      (4 Knobs + EnvelopeEditor)
    │       ├── SectionPanel "CLICK / BEATER"      (4 Knobs + BEATER-Combo)
    │       ├── SectionPanel "SUB"                 (3 Knobs + SHAPE-Combo)
    │       ├── SectionPanel "DRIVE"               (3 Knobs + CHARACTER-Combo)
    │       ├── SectionPanel "MIX / EQ / COMP"     (13 Knobs)
    │       ├── SectionPanel "OUTPUT / SPATIAL"    (6 Knobs)
    │       ├── SectionPanel "ROOM / RUMBLE"       (3 Knobs)
    │       ├── SectionPanel "REVERB / MATERIAL"   (7 Knobs + MATERIAL-Combo)
    │       └── SectionPanel "HIGGS FIELD"         (12 Knobs + HiggsField3D)
    ├── Sidebar (300 px)
    │   ├── WaveformDisplay (oben, 30 Hz)
    │   └── SpectrumAnalyzer (unten, 30 Hz)
    └── FooterSequencer (200 px, 60 Hz Timer)
        ├── 16 Step-Slots (44 px): Nummer | Velocity-Slider | Gate-Toggle | Pitch-Slider
        └── Transport: PLAY | STOP | LOOP | BPM-Knob | Swing-Knob
```

`MainComponent` ist seit dem Redesign **kein** `AudioProcessorEditor` mehr, sondern ein normales `Component` innerhalb des echten Editors. Das vermeidet verschachtelte Editor-Objekte und doppelte Resize-Logik.

### 1.4 Zwei Sequencer-Klassen

| Klasse            | Lebenszyklus                                                              | Verhalten                                                                                                                                                                                                                                                                    |
| ----------------- | ------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `StepSequencer`   | Member des Processors, Konstruktor setzt Größe erst nach Widget-Erzeugung | Audio-Thread-Takt via `processBlock(int numSamples)` (F-02). Schritte werden pro Block ausgeführt (block-quantisiert; Sample-Offset-Trigger ist offen). Paarweises Swing (F-03): gerade = 2×straight×r, ungerade = 2×straight×(1−r), r = 0.5 + swing×0.25 (tempo-erhaltend). |
| `FooterSequencer` | Member des `MainComponent`                                                | Reine Transport-/Anzeige-Steuerung: `start()`/`stop()` schreiben `sequencerPlaying`; 60-Hz-Timer spiegelt `StepSequencer::getCurrentStep()`/`isPlaying()` für den Playhead. Kein eigener Triggerpfad.                                                                        |

Die sichtbare Bedienung läuft ausschließlich über den `FooterSequencer`; die einzige Sequencer-Clock ist der sample-genaue `StepSequencer` im Processor. Details und offene Punkte in §7 und §12.

---

## 2. Parameter-Index (alle 132) mit GUI- & DSP-Mapping

Legende:

- **GUI**: ✅ = Regler im Haupt-GUI vorhanden / ❌ = kein GUI-Regler (Parameter lebt nur für Automation/MIDI).
- **DSP**: ✅ = wird gelesen und wirkt; ⚠️ = wird gelesen, aber ohne Wirkung; ❌ = wird nicht gelesen.
- **Typ**: Float / Choice / Bool.

### 2.1 Membrane / Physical Model (8 Parameter)

| #   | Parameter-ID           | Typ   | Range         | Default | Einheit | GUI                   | DSP | Verwendung                                  |
| --- | ---------------------- | ----- | ------------- | ------- | ------- | --------------------- | --- | ------------------------------------------- |
| 0   | `membraneTension`      | Float | 0.0–1.0       | 0.5     | —       | ✅ Knob "Tension"     | ✅  | Membranspannung T (beeinflusst f₁)          |
| 1   | `membraneRadius`       | Float | 0.05–0.5 m    | 0.15    | m       | ✅ Knob "Radius"      | ✅  | Membran-Radius D/2                          |
| 2   | `membraneMass`         | Float | 0.1–2.0 kg/m² | 0.5     | kg/m²   | ✅ Knob "Mass"        | ✅  | Flächenmasse σ                              |
| 3   | `membraneDamping`      | Float | 0.0–1.0       | 0.3     | —       | ✅ Knob "Damping"     | ✅  | Abschwächung der Membran-Modi               |
| 4   | `membraneModes`        | Float | 1–24          | 8       | modes   | ✅ Knob "Modes"       | ✅  | Anzahl schwingender Modi                    |
| 5   | `fundamentalHz`        | Float | 30–120 Hz     | 45      | Hz      | ✅ Knob "Fundamental" | ✅  | Grundfrequenz f₁ (Sweet Spot 40–50 Hz)      |
| 6   | `membraneNonlinearity` | Float | 0.0–1.0       | 0.2     | —       | ✅ Knob "NonLin."     | ✅  | Pitch-Glide bei hoher Anregung (von Kármán) |
| 7   | `membraneStiffness`    | Float | 0.0–1.0       | 0.1     | —       | ✅ Knob "Stiffness"   | ✅  | Steifigkeit / Oberton-Verhältnis            |

Physik-Formel (Kommentar im Code, F-05): `f₁ = (α₀₁ / (2π·R)) · sqrt(T / σ)` mit α₀₁ = 2.4048 (erste Nullstelle der Bessel-Funktion J₀), T = Spannung, σ = Flächenmasse, R = Radius. Ersetzt die dimensionswidrige Vorgängerformel `0.766 · T / (σ · D)`.

### 2.2 Hollow Steel / Tube Resonance (5 Parameter)

| #   | Parameter-ID         | Typ   | Range     | Default | Einheit | GUI                     | DSP | Verwendung                     |
| --- | -------------------- | ----- | --------- | ------- | ------- | ----------------------- | --- | ------------------------------ |
| 8   | `tubeResonanceFreq`  | Float | 40–300 Hz | 120     | Hz      | ✅ Knob "Tube Freq."    | ✅  | Hohlkörper-Resonanz-Frequenz   |
| 9   | `tubeResonanceQ`     | Float | 0.5–20    | 5.0     | —       | ✅ Knob "Tube Q"        | ✅  | Güte / Schmalheit der Resonanz |
| 10  | `tubeResonanceLevel` | Float | 0.0–1.0   | 0.4     | —       | ✅ Knob "Tube Level"    | ✅  | Anteil Hohlkörper im Mix       |
| 11  | `tubeCoupling`       | Float | 0.0–1.0   | 0.5     | —       | ✅ Knob "Tube Coupling" | ✅  | Kopplungsstärke Membran↔Röhre  |
| 12  | `hollowMetallic`     | Float | 0.0–1.0   | 0.3     | —       | ✅ Knob "Hollow Metal"  | ✅  | Metallischer Hohlklanganteil   |

### 2.3 Pitch Envelope (TR-909/808 Style) (4 Parameter)

| #   | Parameter-ID     | Typ   | Range      | Default | Einheit | GUI              | DSP | Verwendung                              |
| --- | ---------------- | ----- | ---------- | ------- | ------- | ---------------- | --- | --------------------------------------- |
| 13  | `pitchEnvAttack` | Float | 0.1–50 ms  | 0.5     | ms      | ✅ Knob "Attack" | ✅  | Attack-Zeit des Pitch-Glissando         |
| 14  | `pitchEnvDecay`  | Float | 20–1000 ms | 200     | ms      | ✅ Knob "Decay"  | ✅  | Decay-Zeit des Pitch-Glissando          |
| 15  | `pitchEnvDepth`  | Float | 0.0–1.0    | 0.6     | —       | ✅ Knob "Depth"  | ✅  | Tonhöhen-Modulationstiefe               |
| 16  | `pitchEnvCurve`  | Float | −1.0–+1.0  | 0.0     | —       | ✅ Knob "Curve"  | ✅  | Kurve: <0 = exp, >0 = linear/invertiert |

### 2.4 Click / Beater (Anschlag-Transient) (5 Parameter)

| #   | Parameter-ID          | Typ    | Range     | Default    | Einheit | GUI                      | DSP | Verwendung                     |
| --- | --------------------- | ------ | --------- | ---------- | ------- | ------------------------ | --- | ------------------------------ |
| 17  | `clickLevel`          | Float  | 0.0–1.0   | 0.4        | —       | ✅ Knob "Level"          | ✅  | Anschlag-Lautstärke            |
| 18  | `clickDecay`          | Float  | 0.5–20 ms | 3.0        | ms      | ✅ Knob "Decay"          | ✅  | Anschlag-Decay-Zeit            |
| 19  | `clickTone`           | Float  | 0.0–1.0   | 0.5        | —       | ✅ Knob "Tone"           | ✅  | Anschlag-Farbe / Tonhöhe       |
| 20  | `clickDistortion`     | Float  | 0.0–1.0   | 0.2        | —       | ✅ Knob "Distort"        | ✅  | Verzerrung des Anschlags       |
| 21  | `clickBeaterMaterial` | Choice | 0–2       | 1 (Medium) | —       | ✅ Choice im Click-Panel | ✅  | Schlägel: Soft / Medium / Hard |

### 2.5 Body (Körper des Kicks) (4 Parameter)

| #   | Parameter-ID | Typ   | Range      | Default | Einheit | GUI                                | DSP | Verwendung                           |
| --- | ------------ | ----- | ---------- | ------- | ------- | ---------------------------------- | --- | ------------------------------------ |
| 22  | `bodyLevel`  | Float | 0.0–1.0    | 0.7     | —       | ✅ Knob "BODY LEVEL" (Panel BODY)  | ✅  | Körper-Lautstärke (Multiplikator)    |
| 23  | `bodyAttack` | Float | 0.5–10 ms  | 0.5     | ms      | ✅ Knob "BODY ATTACK" (Panel BODY) | ✅  | Attack des Body-Envelope-Followers   |
| 24  | `bodyDecay`  | Float | 50–2000 ms | 500     | ms      | ✅ Knob "BODY DECAY" (Panel BODY)  | ✅  | Decay des Body-Envelope-Followers    |
| 25  | `bodyTuning` | Float | 0.5–2.0    | 1.0     | ×       | ✅ Knob "BODY TUNING" (Panel BODY) | ✅  | Stimmungs-Verhältnis für fundamental |

`bodyAttack` / `bodyDecay` steuern einen Attack/Decay-Envelope-Follower (`bodyEnv`) auf dem Membran-Body, der den Body-Level moduliert.

### 2.6 Sub-Oszillator (4 Parameter)

| #   | Parameter-ID | Typ    | Range         | Default  | Einheit | GUI                   | DSP | Verwendung                           |
| --- | ------------ | ------ | ------------- | -------- | ------- | --------------------- | --- | ------------------------------------ |
| 26  | `subLevel`   | Float  | 0.0–1.0       | 0.8      | —       | ✅ Knob "Level"       | ✅  | Sub-Oszillator-Lautstärke            |
| 27  | `subDecay`   | Float  | 50–1500 ms    | 400      | ms      | ✅ Knob "Decay"       | ✅  | Sub-Envelope-Decay                   |
| 28  | `subDetune`  | Float  | −12–+12 cents | 0        | cents   | ✅ Knob "Detune"      | ✅  | Sub-Detune in Cents                  |
| 29  | `subShape`   | Choice | 0–2           | 0 (Sine) | —       | ✅ Knob "Shape" (neu) | ✅  | Wellenform: Sine / Triangle / Square |

### 2.7 Drive / Saturation (4 Parameter)

| #   | Parameter-ID     | Typ    | Range   | Default  | Einheit | GUI                       | DSP | Verwendung                              |
| --- | ---------------- | ------ | ------- | -------- | ------- | ------------------------- | --- | --------------------------------------- |
| 30  | `driveAmount`    | Float  | 0.0–1.0 | 0.4      | —       | ✅ Knob "Amount"          | ✅  | Sättigungsmenge (0 = keine, 1 = volle)  |
| 31  | `driveMix`       | Float  | 0.0–1.0 | 0.6      | —       | ✅ Knob "Mix"             | ✅  | Dry/Wet-Mix der Sättigung               |
| 32  | `driveCharacter` | Choice | 0–4     | 0 (Tube) | —       | ✅ Knob "Character" (neu) | ✅  | Tube / Diode / Fuzz / Wave Fold / Alias |
| 33  | `drivePreHP`     | Float  | 0.0–1.0 | 0.2      | —       | ✅ Knob "Pre HP"          | ✅  | High-Pass vor der Sättigung             |

### 2.8 EQ / Compressor (13 Parameter)

| #   | Parameter-ID    | Typ   | Range         | Default | Einheit | GUI                        | DSP | Verwendung                                                                |
| --- | --------------- | ----- | ------------- | ------- | ------- | -------------------------- | --- | ------------------------------------------------------------------------- |
| 34  | `eqSubFreq`     | Float | 20–120 Hz     | 50      | Hz      | ✅ Knob "Sub Freq" (neu)   | ✅  | Low-Shelf Mitte (Sub)                                                     |
| 35  | `eqSubGain`     | Float | −12–+12 dB    | 0       | dB      | ✅ Knob "Sub EQ"           | ✅  | Low-Shelf Gain (Sub)                                                      |
| 36  | `eqPunchFreq`   | Float | 500–5000 Hz   | 1500    | Hz      | ✅ Knob "Punch Freq" (neu) | ✅  | Peak-Filter Mitte (Punch)                                                 |
| 37  | `eqPunchGain`   | Float | −12–+12 dB    | 0       | dB      | ✅ Knob "Punch"            | ✅  | Peak-Filter Gain (Punch)                                                  |
| 38  | `eqAirFreq`     | Float | 4000–16000 Hz | 10000   | Hz      | ✅ Knob "Air Freq" (neu)   | ✅  | High-Shelf Mitte (Air)                                                    |
| 39  | `eqAirGain`     | Float | −12–+12 dB    | 0       | dB      | ✅ Knob "Air"              | ✅  | High-Shelf Gain (Air)                                                     |
| 40  | `eqRumbleHP`    | Float | 15–60 Hz      | 30      | Hz      | ✅ Knob "Rumble HP"        | ✅  | High-Pass für Rumble-Cut                                                  |
| 41  | `compThreshold` | Float | −60–0 dB      | −20     | dB      | ✅ Knob "Thresh"           | ✅  | Kompressor-Schwelle                                                       |
| 42  | `compRatio`     | Float | 1–20          | 4       | :1      | ✅ Knob "Ratio"            | ✅  | Kompressor-Verhältnis                                                     |
| 43  | `compAttack`    | Float | 0.1–50 ms     | 5       | ms      | ✅ Knob "Att" (neu)        | ✅  | Kompressor-Attack-Zeit                                                    |
| 44  | `compRelease`   | Float | 10–500 ms     | 100     | ms      | ✅ Knob "Rel" (neu)        | ✅  | Kompressor-Release-Zeit                                                   |
| 45  | `compKnee`      | Float | 0–12 dB       | 6       | dB      | ✅ Knob "Knee" (neu)       | ✅  | Soft-Knee im dB-Bereich (quadratischer Übergang threshold ± knee/2) aktiv |
| 46  | `compMakeup`    | Float | −12–+24 dB    | 0       | dB      | ✅ Knob "Makeup"           | ✅  | Kompressor-Makeup-Gain                                                    |

### 2.9 Output / Spatial (6 Parameter)

| #   | Parameter-ID      | Typ   | Range      | Default | Einheit | GUI                  | DSP | Verwendung                        |
| --- | ----------------- | ----- | ---------- | ------- | ------- | -------------------- | --- | --------------------------------- |
| 47  | `outputGain`      | Float | −60–+12 dB | 0       | dB      | ✅ Knob "Gain"       | ✅  | Gesamt-Ausgangs-Gain              |
| 48  | `outputPan`       | Float | −1–+1      | 0       | —       | ✅ Knob "Pan"        | ✅  | Stereo-Panorama                   |
| 49  | `outputWidth`     | Float | 0–1        | 0.7     | —       | ✅ Knob "Width"      | ✅  | Stereo-Breite                     |
| 50  | `outputMonoBelow` | Float | 40–200 Hz  | 150     | Hz      | ✅ Knob "Mono Below" | ✅  | Mono-Kreuzfrequenz (Club-Systeme) |
| 51  | `outputClip`      | Float | 0–1        | 0.5     | —       | ✅ Knob "Clip"       | ✅  | Soft-Clip-Menge                   |
| 52  | `outputLimiter`   | Float | 0–1        | 0.8     | —       | ✅ Knob "Limiter"    | ✅  | Ceiling-Limiter                   |

### 2.10 Room / Late Rumble (3 Parameter)

| #   | Parameter-ID | Typ   | Range       | Default | Einheit | GUI                   | DSP | Verwendung                        |
| --- | ------------ | ----- | ----------- | ------- | ------- | --------------------- | --- | --------------------------------- |
| 53  | `roomLevel`  | Float | 0–1         | 0.2     | —       | ✅ Knob "Room Level"  | ✅  | Raum-Charakter-Level              |
| 54  | `roomDecay`  | Float | 100–1000 ms | 300     | ms      | ✅ Knob "Room Decay"  | ✅  | Raum-Decay-Zeit                   |
| 55  | `roomRumble` | Float | 0–1         | 0.1     | —       | ✅ Knob "Room Rumble" | ✅  | Spätes Rumble (Hollow Industrial) |

### 2.11 Reverb / Modulation Stage (8 Parameter)

| #   | Parameter-ID     | Typ    | Range         | Default  | Einheit   | GUI                    | DSP | Verwendung                                               |
| --- | ---------------- | ------ | ------------- | -------- | --------- | ---------------------- | --- | -------------------------------------------------------- |
| 56  | `reverbMaterial` | Choice | 0–6           | 0 (None) | —         | ✅ Knob "Material"     | ✅  | None / Wood / Metal / Plastic / Concrete / Glass / Stone |
| 57  | `reverbPreDelay` | Float  | 0–150 ms      | 20       | ms        | ✅ Knob "Pre-Delay"    | ✅  | Pre-Delay-Zeit                                           |
| 58  | `reverbDecay`    | Float  | 200–5000 ms   | 1500     | ms (RT60) | ✅ Knob "Decay (RT60)" | ✅  | Reverb-Verhallungszeit                                   |
| 59  | `reverbSize`     | Float  | 0–1           | 0.5      | —         | ✅ Knob "Size"         | ✅  | Raumgröße                                                |
| 60  | `reverbDamping`  | Float  | 0–1           | 0.3      | —         | ✅ Knob "Damping"      | ✅  | Hochfrequenz-Absorption                                  |
| 61  | `reverbMix`      | Float  | 0–1           | 0.3      | —         | ✅ Knob "Mix"          | ✅  | Wet/Dry-Mix                                              |
| 62  | `reverbLowCut`   | Float  | 20–300 Hz     | 100      | Hz        | ✅ Knob "Low Cut"      | ✅  | Reverb-Highpass                                          |
| 63  | `reverbHighCut`  | Float  | 1000–16000 Hz | 8000     | Hz        | ✅ Knob "High Cut"     | ✅  | Reverb-Lowpass                                           |

### 2.12 Global / Tuning (2 Parameter)

| #   | Parameter-ID          | Typ   | Range | Default | Einheit | GUI                                                    | DSP | Verwendung                                                   |
| --- | --------------------- | ----- | ----- | ------- | ------- | ------------------------------------------------------ | --- | ------------------------------------------------------------ |
| 64  | `rootNote`            | Float | 0–127 | 60 (C4) | note    | ✅ Knob "ROOT NOTE" (Panel GLOBAL / TUNING)            | ✅  | Sub-Transpose: `(rootNote − 60)` Halbtöne (×100 Cents)       |
| 65  | `velocitySensitivity` | Float | 0–1   | 0.6     | —       | ✅ Knob "VELOCITY SENSITIVITY" (Panel GLOBAL / TUNING) | ✅  | am Trigger angewendet: velocityGain = 1.0 + sens·(vel − 1.0) |

### 2.13 Step Sequencer (4 Parameter + 48 Pro-Schritt)

| #   | Parameter-ID       | Typ   | Range      | Default | Einheit | GUI                                                                                                                        | DSP | Verwendung                                           |
| --- | ------------------ | ----- | ---------- | ------- | ------- | -------------------------------------------------------------------------------------------------------------------------- | --- | ---------------------------------------------------- |
| 66  | `sequencerBpm`     | Float | 40–240 BPM | 128     | BPM     | ✅ BPM-Knob im Footer; **NICHT an Host-Tempo gekoppelt** — Plugin-interner Wert, per Host-Automation änderbar (siehe §7.8) | ✅  | Tempo für die StepSequencer-Clock                    |
| 67  | `sequencerPlaying` | Bool  | —          | false   | —       | ✅ PLAY/STOP im Footer (schreibt `sequencerPlaying`)                                                                       | ✅  | Play/Stop-Zustand des Processor-Sequencers           |
| 68  | `sequencerSteps`   | Float | 1–16       | 16      | steps   | ❌ kein Control (Footer rendert fest 16 Steps; APVTS-Range 1–16 = **Fixkonfiguration**, siehe §7.8)                        | ✅  | Anzahl aktiver Schritte                              |
| 69  | `sequencerSwing`   | Float | −1–+1      | 0       | —       | ✅ Swing-Knob im FooterSequencer                                                                                           | ✅  | Verzögert gerade/ungerade Schritte (Processor-Clock) |

**Pro-Schritt-Parameter (i = 0..15, 48 Stück).** Die `#`-Spalte ist eine Dokumentations-Ordinalzahl; die tatsächliche APVTS-Reihenfolge ist **68–115** (nach `sequencerSwing`, vor `higgsEnabled`), weil das Layout die Step-Parameter vor den Higgs-Parametern anlegt.

| Schritt | Velocity (Float 0–1) | Gate (Bool)   | Pitch (Float −12–+12 st) | GUI                                                          |
| ------- | -------------------- | ------------- | ------------------------ | ------------------------------------------------------------ |
| 1       | `stepVel_0`          | `stepGate_0`  | `stepPitch_0`            | ✅ Gate-Toggle + Velocity-Slider + Pitch-Slider (editierbar) |
| 2       | `stepVel_1`          | `stepGate_1`  | `stepPitch_1`            | ✅ Gate-Toggle + Velocity-Slider + Pitch-Slider (editierbar) |
| 3       | `stepVel_2`          | `stepGate_2`  | `stepPitch_2`            | ✅ Gate-Toggle + Velocity-Slider + Pitch-Slider (editierbar) |
| 4       | `stepVel_3`          | `stepGate_3`  | `stepPitch_3`            | ✅ Gate-Toggle + Velocity-Slider + Pitch-Slider (editierbar) |
| 5       | `stepVel_4`          | `stepGate_4`  | `stepPitch_4`            | ✅ Gate-Toggle + Velocity-Slider + Pitch-Slider (editierbar) |
| 6       | `stepVel_5`          | `stepGate_5`  | `stepPitch_5`            | ✅ Gate-Toggle + Velocity-Slider + Pitch-Slider (editierbar) |
| 7       | `stepVel_6`          | `stepGate_6`  | `stepPitch_6`            | ✅ Gate-Toggle + Velocity-Slider + Pitch-Slider (editierbar) |
| 8       | `stepVel_7`          | `stepGate_7`  | `stepPitch_7`            | ✅ Gate-Toggle + Velocity-Slider + Pitch-Slider (editierbar) |
| 9       | `stepVel_8`          | `stepGate_8`  | `stepPitch_8`            | ✅ Gate-Toggle + Velocity-Slider + Pitch-Slider (editierbar) |
| 10      | `stepVel_9`          | `stepGate_9`  | `stepPitch_9`            | ✅ Gate-Toggle + Velocity-Slider + Pitch-Slider (editierbar) |
| 11      | `stepVel_10`         | `stepGate_10` | `stepPitch_10`           | ✅ Gate-Toggle + Velocity-Slider + Pitch-Slider (editierbar) |
| 12      | `stepVel_11`         | `stepGate_11` | `stepPitch_11`           | ✅ Gate-Toggle + Velocity-Slider + Pitch-Slider (editierbar) |
| 13      | `stepVel_12`         | `stepGate_12` | `stepPitch_12`           | ✅ Gate-Toggle + Velocity-Slider + Pitch-Slider (editierbar) |
| 14      | `stepVel_13`         | `stepGate_13` | `stepPitch_13`           | ✅ Gate-Toggle + Velocity-Slider + Pitch-Slider (editierbar) |
| 15      | `stepVel_14`         | `stepGate_14` | `stepPitch_14`           | ✅ Gate-Toggle + Velocity-Slider + Pitch-Slider (editierbar) |
| 16      | `stepVel_15`         | `stepGate_15` | `stepPitch_15`           | ✅ Gate-Toggle + Velocity-Slider + Pitch-Slider (editierbar) |

### 2.14 Higgs Field (13 Parameter)

Alle 13 Higgs-Parameter sind an die interaktive 3D-Ansicht gebunden. `HiggsField3D` enthält zusätzlich einen Enable-Toggle sowie 10 vertikale Slider; das Sektions-Panel fügt für 12 dieser Parameter **zusätzlich** Rotary-Knobs hinzu (bewusste Redundanz: kompakte Sektion + detaillierte 3D-Bedienung).

| #   | Parameter-ID         | Typ   | Range      | Default | Einheit | GUI                         | DSP | Verwendung                                       |
| --- | -------------------- | ----- | ---------- | ------- | ------- | --------------------------- | --- | ------------------------------------------------ |
| 70  | `higgsEnabled`       | Bool  | —          | false   | —       | ✅ Toggle im 3D-View        | ✅  | Effekt an/aus                                    |
| 71  | `higgsCursorPhi`     | Float | 0–1        | 0.5     | Phi/v   | ✅ Maus-X im 3D-View + Knob | ✅  | Φ-Position auf Potentialfläche                   |
| 72  | `higgsCursorTheta`   | Float | 0–360 deg  | 180     | deg     | ✅ Maus-Y im 3D-View + Knob | ✅  | Azimutaler Winkel                                |
| 73  | `higgsFieldStrength` | Float | 0–1        | 0.5     | —       | ✅ Slider + Knob            | ✅  | Kopplungsstärke                                  |
| 74  | `higgsVev`           | Float | 0.2–0.95   | 0.5     | v/v₂₄₆  | ✅ Slider + Knob            | ✅  | Vakuum-Erwartungswert (Minimumring)              |
| 75  | `higgsSelfCoupling`  | Float | 0–1        | 0.1     | lambda  | ✅ Slider + Knob            | ✅  | Selbstkopplung λ                                 |
| 76  | `higgsInertia`       | Float | 0–1        | 0.5     | inertia | ✅ Slider + Knob            | ✅  | Trägheit → Sustain (symmetry-gated)              |
| 77  | `higgsYukawaBoost`   | Float | 0–1        | 0.3     | boost   | ✅ Slider + Knob            | ✅  | Yukawa-Boost / Präsenz                           |
| 78  | `higgsBosonHz`       | Float | 100–140 Hz | 125     | Hz      | ✅ Slider + Knob            | ✅  | Boson-Frequenz m_H                               |
| 79  | `higgsBosonQ`        | Float | 1–30       | 10      | Q       | ✅ Slider + Knob            | ✅  | Boson-Bandbreite                                 |
| 80  | `higgsSymmetry`      | Float | 0–1        | 0.5     | sym     | ✅ Slider + Knob            | ✅  | Symmetrie (0 = gebrochen, 1 = wiederhergestellt) |
| 81  | `higgsDecay`         | Float | 0–1        | 0.2     | Gamma   | ✅ Slider + Knob            | ✅  | Zerfallsrate → Dry                               |
| 82  | `higgsMix`           | Float | 0–1        | 0.4     | mix     | ✅ Slider + Knob            | ✅  | Wet/Dry-Mix                                      |

### 2.15 Trigger (1 Parameter)

| #   | Parameter-ID    | Typ  | Range | Default | GUI                                                                                    | DSP                                       | Verwendung                                                                                       |
| --- | --------------- | ---- | ----- | ------- | -------------------------------------------------------------------------------------- | ----------------------------------------- | ------------------------------------------------------------------------------------------------ |
| 83  | `triggerButton` | Bool | —     | false   | ⚠️ TRIGGER-Button schreibt Momentan-Trigger (wird nach Feuern auf false zurückgesetzt) | ✅ `engine.trigger(1.0f)` wird aufgerufen | Momentaner Debug-/Manuell-Trigger: Drücken → Attack feuern, sofort zurücksetzen auf false (F-08) |

### 2.16 Zusammenfassung: GUI-Status aller Parameter

| Kategorie           | Anzahl  | Direkt bedienbar                                                                                      | Nur DSP / Anzeige                     | Von DSP/Sequencer gelesen     |
| ------------------- | ------- | ----------------------------------------------------------------------------------------------------- | ------------------------------------- | ----------------------------- |
| Membrane            | 8       | 8                                                                                                     | 0                                     | 8                             |
| Tube/Hollow         | 5       | 5                                                                                                     | 0                                     | 5                             |
| Pitch Envelope      | 4       | 4                                                                                                     | 0                                     | 4                             |
| Click/Beater        | 5       | 5                                                                                                     | 0                                     | 5                             |
| Body                | 4       | 4                                                                                                     | 0                                     | 4                             |
| Sub                 | 4       | 4                                                                                                     | 0                                     | 4                             |
| Drive               | 4       | 4                                                                                                     | 0                                     | 4                             |
| EQ/Comp             | 13      | 13                                                                                                    | 0                                     | 13                            |
| Output/Spatial      | 6       | 6                                                                                                     | 0                                     | 6                             |
| Room                | 3       | 3                                                                                                     | 0                                     | 3                             |
| Reverb              | 8       | 8                                                                                                     | 0                                     | 8                             |
| Global/Tuning       | 2       | 2 (`rootNote`, `velocitySensitivity`)                                                                 | 0                                     | 2                             |
| Sequencer-Transport | 4       | 3 (`sequencerSwing`, `sequencerPlaying`, `sequencerBpm`)                                              | 1 (`sequencerSteps` Fixkonfiguration) | 4                             |
| Step-Parameter      | 48      | 48 (16 Velocity + 16 Gate + 16 Pitch)                                                                 | 0                                     | 48                            |
| Higgs Field         | 13      | 13                                                                                                    | 0                                     | 13                            |
| Trigger             | 1       | 1 (TRIGGER-Button ruft `triggerDebugAttack()` direkt, `triggerButton` wird nach Feuern zurückgesetzt) | 0                                     | 1 (gelesen, momentar, F-08)   |
| **Gesamt**          | **132** | **131**                                                                                               | **1**                                 | **132 gelesen, alle wirksam** |

Hinweise:

- `bodyLevel`/`bodyAttack`/`bodyDecay`/`bodyTuning` sind im Panel `BODY` bedienbar (siehe §6).
- `sequencerBpm` ist über den BPM-Knob im Footer bedienbar.
- `stepPitch_*` ist über horizontale Slider unter jedem Step editierbar (−12…+12 Halbtöne).
- `triggerButton` existiert als APVTS-Parameter, wird vom TRIGGER-Button beschrieben; nach dem Feuern wird der Parameter auf `false` zurückgesetzt, damit kein Dauer-Trigger entsteht (F-08). Der Button ruft direkt `processor.triggerDebugAttack()` auf.
- `presetID` ist im Header deklariert, wird aber weder in `getAllIDs()` noch im ParameterLayout angelegt und ist damit kein aktiver Parameter.

---

## 3. Audio-Routing: Signal-Flow durch alle DSP-Stufen

Das Signal fließt **mono-intern** durch die Kette und wird am Ende auf Stereo (L/R) ausgegeben. Parallele FX (Room, Higgs, Reverb) werden zum Hauptsignal gemischt.

```text
┌─────────────────────────────────────────────────────────────────────────────────────────┐
│                    AUDIO-ROUTING (pro Sample, im render()-Loop)                         │
└─────────────────────────────────────────────────────────────────────────────────────────┘

  TRIGGER (Sequencer / MIDI / Button)
       │ velocity (0–1), pitchSemitones, note-On
       ▼
┌─────────────────┐
│  Pitch Envelope │◀─ pAttack(13) pDecay(14) pDepth(15) pCurve(16)
│  (TR-909 Style) │     → pitchRatio (1.0 = keine Modulation)
└────────┬────────┘
         │ Trigger (MIDI / Sequencer / GUI) → Restart Voice
         ▼
┌─────────────────┐
│  Membrane Body  │◀─ tension(0) radius(1) mass(2) damping(3)
│  (Physik-Modell │     modes(4) fundamentalHz(5) nonlinearity(6)
│   8 Parameter)  │     stiffness(7)
│                 │     → membraneSample (mono)
│                 │     bodyAttack(23)/bodyDecay(24) → bodyEnv-Follower
│                 │     bodyEnv × bodyLevel(22) → bodyOut
└────────┬────────┘
         ▼
┌─────────────────┐
│  Tube / Hollow  │◀─ tubeFreq(8) tubeQ(9) tubeLevel(10)
│  Resonanz       │     tubeCoupling(11) hollowMetallic(12)
│                 │     → tubeSample (parallel zur Membran)
└────────┬────────┘
         ▼
┌─────────────────┐  bodyOut + membraneSample + tubeSample = mixedBody
│  Sub-Oszillator │◀─ subLevel(26) subDecay(27) subDetune(28) subShape(29)
│                 │     rootNote(64) → transpose (rootNote−60) semitones
│                 │     → subSample
└────────┬────────┘
         ▼
┌─────────────────┐  mixedBody + subSample = preClick
│  Click/Beater   │◀─ clickLevel(17) clickDecay(18) clickTone(19)
│  (Anschlag)     │     clickDistortion(20) clickBeaterMaterial(21)
│                 │     → clickSample (Transient-Spitze)
└────────┬────────┘
         ▼
┌─────────────────┐  preClick + clickSample = preDrive
│  Drive / Sat.   │◀─ driveAmount(30) driveMix(31)
│                 │     driveCharacter(32) drivePreHP(33)
│                 │     → driveSample
└────────┬────────┘
         ▼
┌─────────────────┐
│  EQ-Stage       │◀─ eqSubFreq(34) eqSubGain(35)
│                 │     eqPunchFreq(36) eqPunchGain(37)
│                 │     eqAirFreq(38) eqAirGain(39)
│                 │     eqRumbleHP(40)
│                 │     → eqSample
└────────┬────────┘
         ▼
┌─────────────────┐
│  Kompressor     │◀─ compThreshold(41) compRatio(42)
│                 │     compAttack(43) compRelease(44)
│                 │     compKnee(45) compMakeup(46)
│                 │     → compSample
└────────┬────────┘
         ▼
┌─────────────────┐
│  Output/Spatial │◀─ outputGain(47) outputPan(48) outputWidth(49)
│                 │     outputMonoBelow(50) outputClip(51) outputLimiter(52)
│                 │     → L/R Stereo Out (Hauptpfad)
│                 │     Reihenfolge (F-07): Gain → M/S Mono-Below → Pan → Soft-Clip → Peak-Limiter
│                 │     (Limiter ist finale Sicherheitsdeckel nach dem Clip)
└────────┬────────┘
         │  (Parallel-Taps)
         ├───────────────────────┐            ┌─────────────────┐
         │                       │            │   Room FX       │◀─ roomLevel(53)
         │                       │            │                 │   roomDecay(54)
         │                       │            │                 │   roomRumble(55)
         │                       │            └────────┬────────┘
         │                       │                     │ rumbleMix
         ▼                       ▼                     ▼
┌─────────────────┐     ┌─────────────────┐
│  Higgs Field    │     │  Reverb Stage   │◀─ reverbMaterial(56)
│  (3D Potential) │     │                 │   reverbPreDelay(57)
│                 │     │                 │   reverbDecay(58)
│                 │     │                 │   reverbSize(59)
│                 │     │                 │   reverbDamping(60)
│                 │     │                 │   reverbMix(61)
│                 │     │                 │   reverbLowCut(62)
│                 │     │                 │   reverbHighCut(63)
│                 │     └────────┬────────┘
│                 │              │ reverbWet
└────────┬────────┘              ▼
         │              ┌─────────────────┐
         └─────────────▶│   FINAL MIX     │◀─ roomRumble (late Rumble)
                        │                 │   higgsMix (Higgs Wet)
                        │                 │   outputGain / Pan / Width / Clip / Limiter
                        └────────┬────────┘
                                 ▼
                          L / R Stereo Output Buffer
```

**Parallel-Äste:**

- Membran und Tube laufen parallel, werden dann gemischt (`mixedBody`).
- Sub kommt separat hinzu (`preClick`).
- Click kommt separat hinzu (`preDrive`).
- Output-Stage: Gain → M/S Mono-Below → Pan → Soft-Clip (`outputClip`) → Peak-Limiter (`outputLimiter`) als finale Sicherheit nach dem Clip (F-07).
- Room, Higgs, Reverb sind Sende/Rücklauf-Effekte (parallel zum Hauptpfad).

---

## 4. Pro-Block Parameter-Lese-Reihenfolge

In `KickSynthesisEngine::render()` werden alle Parameter **einmal pro Block** (nicht pro Sample) über `smoothing.getById(id)` gelesen. Vollständige Reihenfolge:

```cpp
// === KickSynthesisEngine::render() — Parameter-Lesen (einmal pro Block) ===

// Membrane — 8 Parameter
const float tension      = smoothing.getById (ParameterIDs::membraneTension);
const float radius       = smoothing.getById (ParameterIDs::membraneRadius);
const float mass         = smoothing.getById (ParameterIDs::membraneMass);
const float damping      = smoothing.getById (ParameterIDs::membraneDamping);
const float modes        = smoothing.getById (ParameterIDs::membraneModes);
const float fundamentalHz= smoothing.getById (ParameterIDs::fundamentalHz);
const float nonlinearity = smoothing.getById (ParameterIDs::membraneNonlinearity);
const float stiffness    = smoothing.getById (ParameterIDs::membraneStiffness);

// Tube — 5 Parameter
const float tubeFreq     = smoothing.getById (ParameterIDs::tubeResonanceFreq);
const float tubeQ        = smoothing.getById (ParameterIDs::tubeResonanceQ);
const float tubeLevel    = smoothing.getById (ParameterIDs::tubeResonanceLevel);
const float tubeCoupling = smoothing.getById (ParameterIDs::tubeCoupling);
const float hollowMetal  = smoothing.getById (ParameterIDs::hollowMetallic);

// Pitch Envelope — 4 Parameter
const float pAttack      = smoothing.getById (ParameterIDs::pitchEnvAttack);
const float pDecay       = smoothing.getById (ParameterIDs::pitchEnvDecay);
const float pDepth       = smoothing.getById (ParameterIDs::pitchEnvDepth);
const float pCurve       = smoothing.getById (ParameterIDs::pitchEnvCurve);

// Click — 5 Parameter
const float clickLvl     = smoothing.getById (ParameterIDs::clickLevel);
const float clickDec     = smoothing.getById (ParameterIDs::clickDecay);
const float clickTone    = smoothing.getById (ParameterIDs::clickTone);
const float clickDist    = smoothing.getById (ParameterIDs::clickDistortion);
const int   clickMat     = (int) smoothing.getById (ParameterIDs::clickBeaterMaterial);

// Body — 4 Parameter
const float bodyLvl      = smoothing.getById (ParameterIDs::bodyLevel);
const float bodyAtk      = smoothing.getById (ParameterIDs::bodyAttack);
const float bodyDec      = smoothing.getById (ParameterIDs::bodyDecay);
const float bodyTune     = smoothing.getById (ParameterIDs::bodyTuning);

// Sub — 4 Parameter
const float subLvl       = smoothing.getById (ParameterIDs::subLevel);
const float subDec       = smoothing.getById (ParameterIDs::subDecay);
const float subDetune    = smoothing.getById (ParameterIDs::subDetune);
const int   subShape     = (int) smoothing.getById (ParameterIDs::subShape);

// Drive — 4 Parameter
const float driveAmt     = smoothing.getById (ParameterIDs::driveAmount);
const float driveMix     = smoothing.getById (ParameterIDs::driveMix);
const int   driveChar    = (int) smoothing.getById (ParameterIDs::driveCharacter);
const float driveHP      = smoothing.getById (ParameterIDs::drivePreHP);

// EQ — 7 Parameter
const float eqSubF       = smoothing.getById (ParameterIDs::eqSubFreq);
const float eqSubG       = smoothing.getById (ParameterIDs::eqSubGain);
const float eqPunchF     = smoothing.getById (ParameterIDs::eqPunchFreq);
const float eqPunchG     = smoothing.getById (ParameterIDs::eqPunchGain);
const float eqAirF       = smoothing.getById (ParameterIDs::eqAirFreq);
const float eqAirG       = smoothing.getById (ParameterIDs::eqAirGain);
const float eqRumbleHP   = smoothing.getById (ParameterIDs::eqRumbleHP);

// Compressor — 6 Parameter
const float compThr      = smoothing.getById (ParameterIDs::compThreshold);
const float compRatio    = smoothing.getById (ParameterIDs::compRatio);
const float compAtk      = smoothing.getById (ParameterIDs::compAttack);
const float compRel      = smoothing.getById (ParameterIDs::compRelease);
const float compKnee     = smoothing.getById (ParameterIDs::compKnee);
const float compMakeup   = smoothing.getById (ParameterIDs::compMakeup);

// Output — 6 Parameter + rootNote
const float outGain      = smoothing.getById (ParameterIDs::outputGain);
const float outPan       = smoothing.getById (ParameterIDs::outputPan);
const float outWidth     = smoothing.getById (ParameterIDs::outputWidth);
const float outMonoBelow = smoothing.getById (ParameterIDs::outputMonoBelow);
const float outClip      = smoothing.getById (ParameterIDs::outputClip);
const float outLimiter   = smoothing.getById (ParameterIDs::outputLimiter);
const float rootNote     = smoothing.getById (ParameterIDs::rootNote);

// Room — 3 Parameter
const float roomLvl      = smoothing.getById (ParameterIDs::roomLevel);
const float roomDec      = smoothing.getById (ParameterIDs::roomDecay);
const float roomRumble   = smoothing.getById (ParameterIDs::roomRumble);

// Reverb — 8 Parameter
const int   revMat       = (int) smoothing.getById (ParameterIDs::reverbMaterial);
const float revPreD      = smoothing.getById (ParameterIDs::reverbPreDelay);
const float revDecay     = smoothing.getById (ParameterIDs::reverbDecay);
const float revSize      = smoothing.getById (ParameterIDs::reverbSize);
const float revDamping   = smoothing.getById (ParameterIDs::reverbDamping);
const float revMix       = smoothing.getById (ParameterIDs::reverbMix);
const float revLowCut    = smoothing.getById (ParameterIDs::reverbLowCut);
const float revHighCut   = smoothing.getById (ParameterIDs::reverbHighCut);

// Higgs — 13 Parameter (vollständig über Smoothing; GUI liest zusätzlich Rohwerte)
const bool  higgsEn      = smoothing.getById (ParameterIDs::higgsEnabled) > 0.5f;
const float higgsPhi     = smoothing.getById (ParameterIDs::higgsCursorPhi);
const float higgsTheta   = smoothing.getById (ParameterIDs::higgsCursorTheta);
const float higgsStr     = smoothing.getById (ParameterIDs::higgsFieldStrength);
const float higgsVev     = smoothing.getById (ParameterIDs::higgsVev);
const float higgsSelfC   = smoothing.getById (ParameterIDs::higgsSelfCoupling);
const float higgsInertia = smoothing.getById (ParameterIDs::higgsInertia);
const float higgsYukawa  = smoothing.getById (ParameterIDs::higgsYukawaBoost);
const float higgsBosonHz = smoothing.getById (ParameterIDs::higgsBosonHz);
const float higgsBosonQ  = smoothing.getById (ParameterIDs::higgsBosonQ);
const float higgsSym     = smoothing.getById (ParameterIDs::higgsSymmetry);
const float higgsDec     = smoothing.getById (ParameterIDs::higgsDecay);
const float higgsMix     = smoothing.getById (ParameterIDs::higgsMix);

// Global
// velocitySensitivity wird NICHT hier gelesen, sondern in PluginProcessor am Trigger
// (velocityGain = 1.0 + sens·(vel − 1.0)).
// triggerButton wird NICHT im Engine-Render gelesen (nur TRIGGER-Button → engine.trigger)
```

**Wichtige Verdrahtungsdetails:**

- `compKnee` wird gelesen und als `kneeDb` an `processCompressor()` übergeben (Soft-Knee aktiv).
- `bodyAttack` / `bodyDecay` steuern den `bodyEnv`-Envelope-Follower.
- `rootNote` transponiert den Sub um `(rootNote − 60)` Halbtöne (in Cents: ×100).
- Sequencer-Parameter (`sequencerBpm`, `sequencerPlaying`, `sequencerSteps`, `sequencerSwing`, alle `stepVel_i` / `stepGate_i` / `stepPitch_i`) werden **nicht** im Engine-Render gelesen, sondern vom sample-genauen `StepSequencer` im Audio-Thread über APVTS verarbeitet. Der Footer schreibt nur `sequencerPlaying` und spiegelt die Anzeige.
- `velocitySensitivity` wird gelesen und am Trigger angewendet (`velocityGain = 1.0 + sens·(vel − 1.0)`, `PluginProcessor.cpp` vor `engine.trigger()`).
- `triggerButton` wird nicht im Engine-Render gelesen; der TRIGGER-Button ruft `engine.trigger()` direkt auf.

---

## 4.7 Reverb / Room — Algorithmus, Parameter und Verhalten

### 4.7.1 Algorithmus (MaterialReverb, Source/DSP/MaterialReverb.h)

Der Reverb basiert auf einem **Feedback Delay Network (FDN)** mit 4 gegenseitig primen Delay-Linien (Basis 479 / 601 / 701 / 877 Samples ≈ 10–18 ms bei 48 kHz). Es handelt sich um **keinen Convolution-Reverb** (keine Impulsantwort) und auch **nicht um Schroeder/Moorer** (keine Kaskade aus Comb- und Allpass-Filtern).

Jedes der 7 Material-Presets (None, Wood, Metal, Plastic, Concrete, Glass, Stone) färbt das Signal über ein vorangeschaltetes Material-EQ (Peak-/Lowpass-/Highpass-/Shelf-Filterpaar) und eine eigene Damping-Kurve.

Signalfluss im Reverb:
`Eingang → Pre-Delay → Material-EQ → FDN (4 Delay-Linien; Damping + Low/High-Cut im Feedbackpfad) → Dry/Wet-Mix → Ausgang`

### 4.7.2 Parameter mit Einheit und Valid Range

Alle 8 Reverb-Parameter leben im APVTS (§2.11), werden über `ParameterSmoothing` blockweise geglättet und einmal pro Block in `KickSynthesisEngine::render()` an `MaterialReverb` übergeben:

| #   | Parameter-ID     | Typ    | Einheit   | Valid Range   | Default  | GUI | Implementiert |
| --- | ---------------- | ------ | --------- | ------------- | -------- | --- | ------------- |
| 56  | `reverbMaterial` | Choice | —         | 0–6           | 0 (None) | ✅  | ✅            |
| 57  | `reverbPreDelay` | Float  | ms        | 0–150 ms      | 20 ms    | ✅  | ✅            |
| 58  | `reverbDecay`    | Float  | ms (RT60) | 200–5000 ms   | 1500 ms  | ✅  | ✅            |
| 59  | `reverbSize`     | Float  | —         | 0–1           | 0.5      | ✅  | ✅            |
| 60  | `reverbDamping`  | Float  | —         | 0–1           | 0.3      | ✅  | ✅            |
| 61  | `reverbMix`      | Float  | —         | 0–1           | 0.3      | ✅  | ✅            |
| 62  | `reverbLowCut`   | Float  | Hz        | 20–300 Hz     | 100 Hz   | ✅  | ✅            |
| 63  | `reverbHighCut`  | Float  | Hz        | 1000–16000 Hz | 8000 Hz  | ✅  | ✅            |

**Room** (separater Sendeeffekt, §2.10): `roomLevel` (0–1, Default 0.2), `roomDecay` (100–1000 ms, Default 300 ms), `roomRumble` (0–1, Default 0.1). Alle 3 im GUI bedienbar und im DSP implementiert.

### 4.7.3 RT60-Mapping (Decay-Regler → Verhallungszeit)

Der Regler `reverbDecay` arbeitet in **Millisekunden als RT60** (Zeit, bis der Hall um 60 dB abgeklungen ist) und wird im Render-Pfad mit Faktor 0.001 in Sekunden umgerechnet:

```cpp
reverb.setDecay (revDec * 0.001f);   // revDec in ms → Sekunden
```

`setDecay()` limitiert intern auf 0.2–10.0 s und berechnet den Feedback-Gain nach einer vereinfachten Sabine-Näherung:

```
feedback = 10 ^ ( -3 × avgDelayMs / 1000 / decayTime )      // avgDelayMs = 13 ms
feedback = clamp (feedback, 0.0, 0.99)
```

Der Parameter `reverbSize` skaliert alle 4 Delay-Linien linear (Faktor 1,0 … 3,0) und löst eine Neuberechnung des Feedbacks aus (`updateDelayLengths()` → `updateFeedback()`), sodass größere Räume länger nachhallen.

### 4.7.4 Pre-Delay

`reverbPreDelay` (0–150 ms, Default 20 ms) wird als Ringbuffer realisiert (maximal 150 ms dimensioniert; `setPreDelay` clampet auf die Buffer-Kapazität). Der Pre-Delay ist für **L und R identisch** — es findet darin keine Stereo-Dekorrelation statt.

Der Task-Review empfiehlt 0–200 ms; die aktuelle Implementierung deckt **0–150 ms** ab. Ein Regler oberhalb von 150 ms müsste den Pre-Delay-Ringbuffer vergrößern.

### 4.7.5 Damping / Hochpass-Filterung des Reverb-Signals

- **`reverbDamping` (0–1):** Steuert einen First-Order-Lowpass im Feedbackpfad jeder der 4 FDN-Linien. Die Grenzfrequenz wird linear abgebildet: 16.000 Hz bei Damping = 0 (keine Dämpfung) → 800 Hz bei Damping = 1 (starke Hochfrequenz-Absorption im Hall).
- **`reverbLowCut` (20–300 Hz):** Highpass auf den Rücklauf-Taps (wirksam im Feedbackpfad), entfernt Sub-Bass aus dem Hall.
- **`reverbHighCut` (1000–16000 Hz):** Lowpass auf den Rücklauf-Taps (wirksam im Feedbackpfad), dämpft den Hochtonanteil des Halles.

Sowohl LowCut als auch HighCut wirken **im Feedbackpfad**, nicht nur am Ausgang — sie formen daher auch die zeitliche Abklingfarbe des Halles, nicht nur dessen Spektrum am Mix-Ausgang.

### 4.7.6 Stereo-Decorrelation (L/R Entkopplung)

Es gibt **keinen dedizierten Stereo-Decorrelation-Regler** und kein Allpass-Decorrelation-Netzwerk. Die einzige L/R-Entkopplung erfolgt strukturell: Der rechte Kanal liest jeden FDN-Tap um eine halbe Delay-Linie versetzt (`readPosR = readPos + fdnDelays[d] / 2`). Pre-Delay und Schreibposition der Delay-Linien sind für beide Kanäle identisch. Die Hall-Breite ist dadurch begrenzt; bei monoem Eingang bleibt das Reverb-Signal weitgehend korreliert.

### 4.7.7 Mono-Kompatibilität (Mono-Downmix)

Die Engine arbeitet intern **mono** (§3 Audio-Routing); das trockene Signal wird identisch in L und R geschrieben, und der Reverb erzeugt das Stereo-Pendant über den versetzten rechten FDN-Tap. Bei einem Mono-Downmix `(L + R) / 2` können durch den halb-delay-verschobenen rechten Kanal leichte Phasenverschiebungen und damit Klangänderungen im Hallanteil auftreten. Ein dedizierter Mono-Fold-Down-Regler für den Reverb existiert nicht.

Bereits vor dem Reverb sorgt `outputMonoBelow` (Output/Spatial, §2.9) dafür, dass tiefe Frequenzen unterhalb der Kreuzfrequenz (Default 150 Hz) bereits im Mid/Signal mono ausgegeben werden (Club-Systeme).

### 4.7.8 Tail-Verhalten (Note-Off / Preset-Wechsel)

- Der Reverb ist ein **Sende/Rücklauf-Effekt**, kein gated Effekt: Der Tail klingt nach dem Note-Off durch natürliche Feedback-Abarbeitung aus (es gibt kein hartes Abschneiden des Halles bei Note-Off; `trigger()` setzt `tube.reset()`, aber nicht `reverb.reset()`).
- `CERNAudioProcessor::getTailLengthSeconds()` meldet dem Host das Maximum von `roomDecay` und `reverbDecay` (in Sekunden), damit der Host den Tail ausreichend lang einplant.
- `MaterialReverb::reset()` löscht alle Delay-Puffer auf Null. Aufgerufen in `prepare()` (Sample-Rate-/Initialisierung) und in `KickSynthesisEngine::reset()` (z. B. `releaseResources()`). Ein Reset bricht den Tail damit abrupt ab.
- Bei einem Preset-Wechsel (`PresetManager::loadPreset` → `resetToDefaults()` → `applyState()`) erfolgt **kein** expliziter `engine.reset()`; die Parameter gleiten über `ParameterSmoothing` sanft in die neuen Werte und der Hall-Tail überlebt den Preset-Wechsel.

### 4.7.9 Klickfreie Preset-Wechsel und Reset

- Alle Reverb-Parameter werden im Audio-Thread über `smoothing.getById(id)` gelesen (blockweise exponentielle Rampe zwischen Zielwerten). Parameteränderungen (Automation, Preset-Load) erzeugen dadurch **keine Sprünge oder Klicks**.
- `reverb.reset()` setzt die Puffer auf Null, ohne die Ausgabewerte einzurampen — ein expliziter Crossfade zwischen Preset-Tails ist **nicht** implementiert. Ein Programm-Wechsel während eines langen Hall-Tails kann den Tail deshalb hörbar abbrechen (kein Crossfade, kein Overlap).

**Ehrlicher Stand der Implementierung:** Alle 8 Reverb-Parameter sind implementiert und an das GUI gebunden (§2.11, Panel 10 „REVERB / MATERIAL"). Nicht implementiert: Stereo-Decorrelation-Regler, Pre-Delay > 150 ms, Crossfade bei Preset-Wechsel, dedizierter Mono-Fold-Down für den Reverb.

---

## 5. Pro-Sample Verarbeitungsschritte

Für jedes Sample im Block durchläuft das Signal folgende Stufen:

```text
Sample-Input (mono) → [Stufe 1] → [Stufe 2] → ... → [Stufe N] → Output L/R
```

### Stufe 1: Pitch-Envelope (Modulation nach Trigger)

- Wird nach einem Trigger-Event (MIDI, Sequencer oder GUI) gestartet.
- Moduliert `pitchRatio` über die Zeit (Attack → Decay mit Kurve `pCurve`).
- Ist keine Trigger-Quelle selbst.

### Stufe 2: Membrane Body (physikalisches Modell)

- `f₁ = (α₀₁ / (2π·R)) · sqrt(T / σ)` mit α₀₁ = 2.4048 (siehe §2.1 und §8.4, F-05).
- `modes` schwingende Modi werden addiert.
- `nonlinearity` fügt Pitch-Glide bei hoher Anregung hinzu (von-Kármán-Nichtlinearität).
- `stiffness` verändert das Oberton-Verhältnis.
- `bodyAttack` / `bodyDecay` steuern einen Attack/Decay-Envelope-Follower auf `|membraneSample|`.
- `bodyLevel` skaliert den Body-Ausgang.

### Stufe 3: Tube / Hollow Resonanz (parallel)

- Resonanz-Filter bei `tubeFreq` mit Güte `tubeQ`.
- `tubeLevel` mischt den Tube-Anteil.
- `tubeCoupling` koppelt Membran und Tube.
- `hollowMetallic` fügt metallischen Hohlklang hinzu.

### Stufe 4: Sub-Oszillator

- Wellenform `subShape`: Sine / Triangle / Square.
- Frequenz: `fundamentalHz × bodyTuning`, transponiert um `(rootNote − 60)` Halbtöne.
- Detune: `subDetune` Cents.
- Envelope-Decay: `subDecay`.
- Level: `subLevel`.

### Stufe 5: Click / Beater (Transient)

- Anschlag-Transient mit `clickLevel`, `clickDecay`.
- `clickTone` färbt den Anschlag.
- `clickDistortion` verzerrt den Anschlag.
- `clickBeaterMaterial`: Soft / Medium / Hard.

### Stufe 6: Drive / Saturation

- High-Pass vor der Sättigung: `drivePreHP`.
- Sättigung: `driveAmount` mit Charakter `driveCharacter` (Tube / Diode / Fuzz / Wave Fold / Alias).
- Dry/Wet-Mix: `driveMix`.
- NaN/Inf-Schutz am Eingang (`!std::isfinite(x)` → 0.0).
- DC-Offset-Blocker am Ausgang (einpoliger Hochpass, `coeff = 0.995`).

### Stufe 7: EQ-Stage

- Low-Shelf bei `eqSubFreq` mit Gain `eqSubGain`.
- Peak bei `eqPunchFreq` mit Gain `eqPunchGain`.
- High-Shelf bei `eqAirFreq` mit Gain `eqAirGain`.
- High-Pass Rumble-Cut bei `eqRumbleHP`.

### Stufe 8: Kompressor (SoftKneeCompressor, ersetzt juce::dsp::Compressor)

- Detektor: Peak oder RMS (wählbar), Stereo-Link (maximales Envelope beider Kanäle).
- Schwelle: `compThreshold`, Verhältnis: `compRatio`, Attack: `compAttack`, Release: `compRelease`.
- Soft-Knee im dB-Bereich: `compKnee` (0–12 dB); unter `threshold − knee/2` keine Kompression (Ratio 1), darüber volle Kompression, dazwischen quadratischer weicher Übergang.
- Makeup-Gain: `compMakeup` (wird auf den Gain aufgeschaltet).
- GR-Meter: aktuelle Gain Reduction in dB abfragbar (`getGainReductionDb()`).
- Lookahead: Audio wird um `lookaheadSamples` verzögert, Envelope sofort angewendet (P1-2).
- NaN/Inf-Schutz auf den Gain-Faktor; bei ungültigem Gain werden beide Kanäle stummgeschaltet.

### Stufe 9: Output / Spatial

- Gesamt-Gain: `outputGain` (sample-genau gerampet via `getNextValueById`, P0-4).
- Panorama: `outputPan` (sample-genau gerampet via `getNextValueById`, P0-4), Breite: `outputWidth`.
- Mono-Below: `outputMonoBelow` (Club-Systeme).
- Reihenfolge (F-07): Gain → M/S Mono-Below → Pan → Soft-Clip (`outputClip`) → Peak-Limiter (`outputLimiter`) als finale Sicherheit nach dem Clip.
- Soft-Clip fügt Oberton-Charakter hinzu; der Limiter verhindert digitale Überssteuerung.

### Stufe 10: Parallele FX (Sende/Rücklauf)

- **Room:** `roomLevel`, `roomDecay`, `roomRumble` (spätes Rumble).
- **Higgs Field:** 3D-Potential-Oberfläche mit Symmetry-Gating (siehe Abschnitt 9).
- **Reverb:** Material, Pre-Delay, Decay (RT60), Size, Damping, Mix, Low/High-Cut.

### Stufe 11: Final Mix

- Hauptpfad + Room + Higgs + Reverb → Stereo Out.
- Letzter Limiter/Clip-Schutz.

---

## 6. GUI-zu-Parameter-Verdrahtung

### 6.1 Dateien & Verantwortlichkeiten

| Datei                                                 | Aufgabe                                                                             |
| ----------------------------------------------------- | ----------------------------------------------------------------------------------- |
| `Source/PluginEditor.{h,cpp}`                         | Erzeugt `MainComponent`, setzt Fenstergröße/Resize-Limits, hält den Settings-Button |
| `Source/GUI/MainComponent.{h,cpp}`                    | Baut Sektionen, erzeugt Knobs/Choices, hält alle Attachments, 10-Hz-Sync-Timer      |
| `Source/GUI/SectionPanel.{h,cpp}`                     | Chamfered Panel, Titel, Knob-Grid, Choice-Zeilen, optionaler Top-View               |
| `Source/GUI/HeaderBar.{h,cpp}`                        | Preset-Browser über `PresetManager`, TRIGGER                                        |
| `Source/GUI/FooterSequencer.{h,cpp}`                  | 16 Steps, Transport, Swing, Anzeige (schreibt `sequencerPlaying`)                   |
| `Source/GUI/SpectrumAnalyzer.h` / `WaveformDisplay.h` | Live-Displays                                                                       |
| `Source/GUI/HiggsField3D.h` / `EnvelopeEditor.h`      | Top-Views der Higgs-/Pitch-Sektion                                                  |
| `Source/GUI/Theme/*`                                  | `ColourScheme`, `FontScheme`, `CERNLookAndFeel`                               |

### 6.2 Layout-Metriken (implementiert)

| Element                     | Wert                                                   |
| --------------------------- | ------------------------------------------------------ |
| Editor-Default              | 1400 × 900 px (`PluginEditor`), resize bis 2800 × 1800 |
| `MainComponent`-Wunschgröße | 1440 × 920 (wird vom Editor auf dessen Bounds gesetzt) |
| Header                      | 68 px                                                  |
| Footer                      | 200 px                                                 |
| Sidebar rechts              | 300 px                                                 |
| Sektions-Canvas             | `Viewport`, vertikal scrollbar                         |
| Panelhöhe                   | dynamisch via `getPreferredHeight()` (Zwei-Pass)       |
| Panel-Spalten               | 4 bei Breite > 1500, 3 > 1150, 2 > 850, sonst 1        |
| Panelbreite                 | `max(180, Breite / Spalten)`, 10 px Gap                |
| Knob-Größen                 | Small 52 / Main 64 / Large 76 / Bipolar 64 px          |
| Step-Slot                   | 44 px                                                  |
| Panel-Ecken                 | 12 px Chamfer + 1 px Bevel                             |

### 6.3 Sektionen → Controls (Ist-Zustand)

Die Knob-Labels werden zur Laufzeit aus der Parameter-ID erzeugt (`prettyName()`: camelCase → `GROSS MIT LEERZEICHEN`), z. B. `membraneTension` → `MEMBRANE TENSION`. Die Choice-Labels sind explizit.

| Panel | Titel               | Controls (Parameter-IDs)                                                                                                                                                                                                                                         |
| ----- | ------------------- | ---------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 1     | `MEMBRANE`          | `membraneTension`, `membraneRadius`, `membraneMass`, `membraneDamping`, `membraneModes`, `fundamentalHz`, `membraneNonlinearity`, `membraneStiffness` (8)                                                                                                        |
| 2     | `HOLLOW STEEL`      | `tubeResonanceFreq`, `tubeResonanceQ`, `tubeResonanceLevel`, `tubeCoupling`, `hollowMetallic` (5)                                                                                                                                                                |
| 3     | `PITCH ENVELOPE`    | `pitchEnvAttack`, `pitchEnvDecay`, `pitchEnvDepth`, `pitchEnvCurve` (4) + `EnvelopeEditor` (116 px)                                                                                                                                                              |
| 4     | `CLICK / BEATER`    | `clickLevel`, `clickDecay`, `clickTone`, `clickDistortion` (4) + Choice `BEATER` → `clickBeaterMaterial`                                                                                                                                                         |
| 5     | `SUB`               | `subLevel`, `subDecay`, `subDetune` (3) + Choice `SHAPE` → `subShape`                                                                                                                                                                                            |
| 6     | `DRIVE`             | `driveAmount`, `driveMix`, `drivePreHP` (3) + Choice `CHARACTER` → `driveCharacter`                                                                                                                                                                              |
| 7     | `MIX / EQ / COMP`   | `eqSubFreq`, `eqSubGain`, `eqPunchFreq`, `eqPunchGain`, `eqAirFreq`, `eqAirGain`, `eqRumbleHP`, `compThreshold`, `compRatio`, `compAttack`, `compRelease`, `compKnee`, `compMakeup` (13)                                                                         |
| 8     | `OUTPUT / SPATIAL`  | `outputGain`, `outputPan`, `outputWidth`, `outputMonoBelow`, `outputClip`, `outputLimiter` (6)                                                                                                                                                                   |
| 9     | `ROOM / RUMBLE`     | `roomLevel`, `roomDecay`, `roomRumble` (3)                                                                                                                                                                                                                       |
| 10    | `REVERB / MATERIAL` | `reverbPreDelay`, `reverbDecay`, `reverbSize`, `reverbDamping`, `reverbMix`, `reverbLowCut`, `reverbHighCut` (7) + Choice `MATERIAL` → `reverbMaterial`                                                                                                          |
| 11    | `GLOBAL / TUNING`   | `rootNote`, `velocitySensitivity` (2)                                                                                                                                                                                                                            |
| 12    | `HIGGS FIELD`       | `higgsCursorPhi`, `higgsCursorTheta`, `higgsFieldStrength`, `higgsVev`, `higgsSelfCoupling`, `higgsInertia`, `higgsYukawaBoost`, `higgsBosonHz`, `higgsBosonQ`, `higgsSymmetry`, `higgsDecay`, `higgsMix` (12) + `HiggsField3D` (232 px) mit Enable + 10 Slidern |

**Nicht im GUI:** `sequencerSteps`-Editor (Footer rendert fest 16 Schritte). Alle übrigen Parameter sind bedienbar: BODY-, GLOBAL / TUNING-Panel, BPM-Knob, Step-Pitch-Slider.

### 6.4 `addKnob()` — Slider-Attachment

```cpp
void MainComponent::addKnob (SectionPanel& panel, const String& paramId, const String& labelText)
{
    auto* param = apvts.getParameter (paramId);
    if (param == nullptr) return;

    auto range = param->getNormalisableRange();
    auto slider = std::make_unique<Slider> (Slider::RotaryHorizontalVerticalDrag,
                                            Slider::TextBoxBelow);
    const float interval = range.interval > 0.0f ? range.interval
                                                 : (range.end - range.start) / 200.0f;
    slider->setRange (range.start, range.end, interval);
    slider->setTextBoxStyle (Slider::TextBoxBelow, false, 56, 14);
    slider->getProperties().set ("knobSize",
                                 range.start < 0.0f ? "bipolar" : "main");

    auto label = std::make_unique<Label>();
    label->setText (labelText, dontSendNotification);
    label->setFont (FontScheme::getInstance().getSmallLabelFont());
    label->setJustificationType (Justification::centred);

    // Besitzübergabe an das Panel (Panel besitzt Slider + Label)
    auto* sliderPtr = slider.release();
    auto* labelPtr  = label.release();
    panel.addKnob (sliderPtr, labelPtr);

    sliderAttachments.push_back (
        std::make_unique<AudioProcessorValueTreeState::SliderAttachment> (
            apvts, paramId, *sliderPtr));
}
```

- Range/Interval werden **aus dem Parameter** übernommen, nicht hartkodiert.
- Negative Untergrenze ⇒ `knobSize=bipolar` ⇒ Mittellinie im LookAndFeel.
- Alle Attachments liegen in `std::vector<std::unique_ptr<SliderAttachment>>`; die Deklarationsreihenfolge garantiert, dass Attachments **vor** den Panels zerstört werden.
- Ein fehlender Parameter wird still übersprungen (kein Crash, aber auch kein Control).

### 6.5 `addChoice()` — manuelle Choice-Bindung

Die Choice-Parameter sind als `AudioParameterChoice` angelegt, aber **nicht** über `ComboBoxAttachment` gebunden. Stattdessen:

```cpp
combo->onChange = [this, paramId, n, comboPtr]
{
    if (auto* p = apvts.getParameter (paramId))
        p->setValueNotifyingHost (n > 1
            ? (float) comboPtr->getSelectedItemIndex() / (float) (n - 1)
            : 0.0f);
};
```

- Schreibrichtung: Combo-Index → normalisierter Parameterwert `index / (n − 1)`.
- Leserichtung: `MainComponent::timerCallback()` (10 Hz) liest `param->getValue()` und setzt den Combo-Index, sofern das Popup nicht offen ist (`isPopupActive()`).
- Dadurch bleiben Preset-Loads und Host-Automation synchron, ohne `ComboBoxAttachment` (das nur mit `AudioParameterChoice` und passender Wertabbildung funktioniert).
- Gebundene Choices: `clickBeaterMaterial` (3), `subShape` (3), `driveCharacter` (5), `reverbMaterial` (7).

### 6.6 Display-Provider-Verdrahtung (Ist-Code)

```cpp
spectrum.setProvider ([this] (int i) { return processor.getSpectrumMagnitude (i); });

waveform.setProviders (
    [this] (int ch, int i) { return processor.getWaveformSample (ch, i); },
    [] { return true; },   // Double-Buffer: Lesepuffer ist immer lesbar (F-04)
    [] {});                // kein consume nötig
```

- `getWaveformSample()` liest aus `waveformBuffers[1 - writeIndex]` (Double-Buffer, `memory_order_acquire`).
- `getSpectrumMagnitude()` liest `spectrumMagnitudesSnapshot` (wird im Audio-Thread nach `engine.getSpectrum()` kopiert).
- Beide Displays besitzen einen eigenen 30-Hz-Timer und rufen nur `repaint()`.
- `MainComponent` besitzt zusätzlich einen 10-Hz-Timer, ausschließlich für den Choice-Sync.

### 6.7 HeaderBar — Preset- & Trigger-Verdrahtung

| Control          | Aktion                                                                        |
| ---------------- | ----------------------------------------------------------------------------- |
| `presetComboBox` | `PresetManager::getPresetNames()`, Auswahl → `loadPreset(index)`              |
| ◀ / ▶            | zyklisch `stepPreset(±1)` → `setSelectedItemIndex(..., sendNotificationSync)` |
| ★                | rein lokaler `bool favourite` (keine Persistenz, kein APVTS-Parameter)        |
| 💾               | `saveCurrentAsUserPreset("My Preset")` + `refreshPresetList()`                |
| 📁               | `refreshPresetList()` (kein Datei-Dialog)                                     |
| 📦               | `saveCurrentAsUserPreset("My Preset")` (identisch zu 💾)                      |
| TRIGGER          | `processor.triggerDebugAttack()` → `engine.trigger(1.0f)`                     |
| CPU-Label        | statischer Text `"CPU: 0.0%"` (keine Live-Messung)                            |

### 6.8 SectionPanel — Layout & Besitz

- `addKnob(control, label)` und `addChoice(control, label)` übernehmen den Besitz (`unique_ptr`).
- `setTopView(Component*, int height)` speichert **nur einen Rohzeiger**; der Top-View wird von `MainComponent` besessen (`higgsField3D`, `envelopeEditor`). Das verhindert Doppel-Free.
- `layout()`:
  - Start bei `pad(16) + titleHeight(28) + titleGap(10)`.
  - Top-View über die volle Innenbreite, danach `gap`.
  - Knobs in einem Grid: `columns = (availW + gap) / (knobSize + gap)`, Zelle `knobSize × (knobSize + 20)`, Label darunter.
  - Choices darunter: Label 16 px, Combo 26 px, je 30 px Fortschritt.
- Der Canvas berechnet die Panelhöhe dynamisch (Zwei-Pass-Layout über `getPreferredHeight()`); bei künftigen Erweiterungen wächst die Zeilenhöhe automatisch.

### 6.9 LookAndFeel / Fonts

- `MainComponent` setzt `CERNLookAndFeel` via `setLookAndFeel()`; Kinder erben sie über die JUCE-Hierarchie.
- `FontScheme::initializeFromBinaryData()` lädt Rajdhani Regular/Medium/SemiBold/Bold und JetBrains Mono Regular/Medium aus `FontBinaryData`.
- `Theme.h` ist ein Legacy-Namespace, der auf die RED-Werte gemappt ist, damit `SpectrumAnalyzer`, `WaveformDisplay`, `HiggsField3D`, `EnvelopeEditor` ohne Umbau dieselbe Optik nutzen.
- Knob-Rendering: prozedural (Körper, Track-Arc, roter Value-Arc, Indikator, Center-Cap); die Bildstreifen aus `Assets/Knobs/` werden **nicht** gesliced.

---

## 7. StepSequencer / FooterSequencer: 48 Pro-Schritt-Parameter + Swing + Klick

### 7.0 Zwei Sequencer-Klassen — wer macht was

| Klasse            | Ort                                                        | Rolle                                                                                                                                                                                                                                                                                                                                                         |
| ----------------- | ---------------------------------------------------------- | ------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| `StepSequencer`   | `Source/Sequencer/`, Member des `CERNAudioProcessor` | **Einzige Sequencer-Clock.** Sample-genauer Audio-Thread-Takt via `processBlock(int numSamples)` (F-02). Schritte werden an ihrer Block-/Sample-Position getriggert. Paarweises Swing (F-03): gerade = 2×straight×r, ungerade = 2×straight×(1−r), r = 0.5 + swing×0.25. Liest alle Step-Parameter über APVTS und triggert `processor.triggerSequencerStep()`. |
| `FooterSequencer` | `Source/GUI/`, Member des `MainComponent`                  | Sichtbares Footer-Panel: 16 Step-Slots, Transport, Swing-Knob. **Reine Transport-/Anzeige-Steuerung.** `start()`/`stop()` schreiben `sequencerPlaying`; der 60-Hz-Timer spiegelt nur `getCurrentStep()`/`isPlaying()` für den Playhead. Triggert selbst keine Schritte mehr.                                                                                  |

**Aktiv / Inaktiv:** Der `StepSequencer` im Processor ist die einzige aktive Clock. Der `FooterSequencer` ist die sichtbare Bedienoberfläche und schreibt ausschließlich den Transport-Parameter `sequencerPlaying`; es gibt keinen zweiten Triggerpfad mehr.

**Swing-Formeln:** Der `StepSequencer` berechnet paarweises, tempoerhaltendes Swing (F-03): `r = 0.5 + swing×0.25`; gerade Schritte = 2×straight×r, ungerade = 2×straight×(1−r). Die Pair-Dauer bleibt konstant.

**Play/Stop-Synchronisation:** Der Footer-Play-Button schreibt `sequencerPlaying` via `setValueNotifyingHost()`. Der `StepSequencer` reagiert im Audio-Thread und startet im Audio-Thread (block-quantisiert). Eine Host-Transport-Synchronisation (Start/Stop/Seek/Loop) ist weiterhin nicht implementiert (§12.2 Punkt 1).

### 7.1 GUI-Layout (FooterSequencer, 200 px)

```text
┌──────────────────────────────────────────────────────────────────────────────────────┐
│  STEP SEQUENCER  (Footer, 200 px)                                                    │
├──────────────────────────────────────────────────────────────────────────────────────┤
│   [PLAY] [STOP] [LOOP]   128 BPM      (Swing-Knob)                                   │
│                                                                                      │
│   #1   #2   #3   #4   #5   #6   #7   #8   #9  #10  #11  #12  #13  #14  #15  #16     │
│   ▮     ▮     ▮     ▮     ▮     ▮     ▮     ▮     ▮     ▮     ▮     ▮     ▮     ▮     │
│   Vel   Vel   Vel   Vel   Vel   Vel   Vel   Vel   Vel   Vel   Vel   Vel   Vel   Vel   │
│   [x]   [ ]   [ ]   [ ]   [x]   [ ]   [ ]   [ ]   ...                                │
│   +0    +0    +0    +0    +0    +0    +0    +0    ...   (Pitch-Slider, editierbar)      │
└──────────────────────────────────────────────────────────────────────────────────────┘
```

- Step-Breite 44 px; 16 Slots = 704 px links, Transport rechts.
- Pro Step: Nummer (oben), Velocity-Slider (8 px breit, vertikal), Gate-Toggle (24 × 16), Pitch-Slider (unten, −12…+12).
- Playhead: rotes Glow-Rechteck + weißer Kern, Position `(currentStep + stepPhase) * 44`.
- Panel: 12-px-Chamfer, rote Oberkante, Step-Trennlinien.

### 7.2 Konstruktor-Verdrahtung (FooterSequencer)

```cpp
for (int i = 0; i < 16; ++i)
{
    auto& s = steps[i];

    s.velocity = std::make_unique<Slider> (Slider::LinearBarVertical, Slider::NoTextBox);
    s.velocity->setRange (0.0, 1.0, 0.01);
    velAtt[i] = std::make_unique<SliderAttachment> (
        apvts, ParameterIDs::stepVel (i), *s.velocity);

    s.gate = std::make_unique<ToggleButton>();
    gateAtt[i] = std::make_unique<ButtonAttachment> (
        apvts, ParameterIDs::stepGate (i), *s.gate);

    // Pitch ist nur ein Label:
    s.pitch = std::make_unique<Slider>();  // LinearHorizontal, SliderAttachment
}
```

- **48 Attachments**: 16 × Velocity (`SliderAttachment`), 16 × Gate (`ButtonAttachment`), 16 × Pitch (`SliderAttachment`).
- `stepPitch_i` ist über horizontale Slider editierbar (Range −12…+12 Halbtöne).
- `playAtt` ist deklariert, wird aber **nicht** benutzt; `start()`/`stop()` schreiben `sequencerPlaying` direkt via `setValueNotifyingHost()`.

### 7.3 Transport- & Swing-Verdrahtung

| Control    | Parameter          | Mechanismus                                                             |
| ---------- | ------------------ | ----------------------------------------------------------------------- |
| PLAY       | `sequencerPlaying` | `setValueNotifyingHost(1.0f)`; lokaler Toggle nur Anzeige               |
| STOP       | `sequencerPlaying` | `setValueNotifyingHost(0.0f)`; `currentStep = -1`, `stepPhase = 0`      |
| LOOP       | —                  | `StepSequencer::setLooping()` (Default an); bei aus stoppt nach Step 16 |
| BPM-Knob   | `sequencerBpm`     | `SliderAttachment` (Range 40–240 BPM, Rotary)                           |
| Swing-Knob | `sequencerSwing`   | `SliderAttachment` (Range −1…+1)                                        |

### 7.4 Transport & Anzeige (FooterSequencer)

```text
start():  playing = true;  sequencerPlaying = 1  (setValueNotifyingHost)
stop():   playing = false; sequencerPlaying = 0  (setValueNotifyingHost)

timerCallback() @ 60 Hz:
    playing      = StepSequencer::isPlaying()
    currentStep  = StepSequencer::getCurrentStep()
    stepPhase   += 1 / (timerHz * stepSeconds)   // nur für glatten Playhead
    repaint()
```

- Der Footer führt **keine** eigene musikalische Clock und triggert **keine** Schritte. Er schreibt nur den Transport-Parameter und spiegelt den Playhead.
- Die einzige Clock ist `StepSequencer::processBlock()` im Audio-Thread (§7.6); dort werden Gate/Velocity/Pitch gelesen und `processor.triggerSequencerStep()` gerufen.
- Der Processor schreibt die Werte in `pendingVelocity`/`pendingPitchSemitones` und setzt `pendingTrigger`; der Audio-Thread konsumiert das atomar am Blockanfang (`pendingTrigger.exchange(false)`).

### 7.5 Parameter-Listener (StepSequencer, Processor)

`StepSequencer::parameterChanged(id, newValue)` reagiert auf:

- `sequencerBpm` → `bpmParam.store(newValue)` + `updateTiming()`
- `sequencerPlaying` → `playingParam.store(newValue > 0.5f)`; bei Start `currentStep = -1`, `sampleCounter = 0`
- `sequencerSteps` → `stepsParam.store((int) newValue)`
- `sequencerSwing` → `swingParam.store(newValue)` + `updateTiming()`

### 7.6 StepSequencer-Timing (Processor, einzige Clock)

```text
stepDuration = 60 / BPM / 4                  // 16tel-Dauer
r            = 0.5 + swing × 0.25            // Swing-Faktor (F-03)
// Paarweise: gerade = 2×stepDuration×r, ungerade = 2×stepDuration×(1−r)
// Pair-Dauer = 2×stepDuration → konstant (tempo-erhaltend)
if (currentStep gerade):
    nextStepTime = lastStepTime + 2 * stepDuration * r
else:
    nextStepTime = lastStepTime + 2 * stepDuration * (1 - r)
```

- `processBlock(numSamples)` akkumuliert `sampleCounter` und führt fällige Schritte aus (kein `advanceStep()` mehr; die Methode ist ein No-Op).
- `executeStep()` prüft Gate/Velocity und triggert `processor.triggerSequencerStep()`.
- `sequencerPlaying` wird vom Footer-Play/Stop geschrieben; der Pfad läuft im Audio-Thread (block-quantisiert).

### 7.7 Bekannte Lücken im Sequencer

1. `sequencerBpm` ist über den BPM-Knob im Footer editierbar, aber **nicht an Host-Tempo gekoppelt** — dokumentiert als Plugin-interner Wert (40–240 BPM, Default 128), per Host-Automation änderbar (siehe §7.8).
2. `sequencerSteps` ist ohne GUI-Control; der Footer rendert immer 16 Steps — dokumentiert als **Fixkonfiguration** auf 16 Schritte (APVTS-Range 1–16 existiert für den internen StepSequencer, siehe §7.8).
3. `playAtt` ist ein totes Member (Play/Stop schreibt `sequencerPlaying` direkt).
4. Host-Transport-Synchronisation (Start/Stop/Seek/Loop) ist nicht implementiert.
5. Nur eine Clock: `StepSequencer` im Audio-Thread; der Footer-Timer (60 Hz) dient ausschließlich der Anzeige. Swing-Formel einheitlich `r = 0.5 + swing×0.25`.

### 7.8 BPM und Step-Anzahl — Bedienbarkeit (Review §6.4)

**Ist `sequencerBpm` an den Host-Tempo gekoppelt oder fest im Plugin?**
Fest im Plugin. Es wird **kein** `getPlayHead()`/`getTempo()` verwendet (nirgendwo im Quellcode unter `Source/` vorhanden). `sequencerBpm` ist ein APVTS-Parameter (40–240 BPM, Default 128), der im Footer über einen Rotary-Slider (`SliderAttachment`) bedienbar ist; zusätzlich per Host-Automation änderbar.
**Kennzeichnung:** „Fixkonfiguration (Plugin-intern), nicht host-synchronisiert".

**Ist `sequencerSteps` fest auf 16 begrenzt oder editierbar?**
Der `FooterSequencer` rendert **hart 16 Steps** (`for (int i = 0; i < 16; ++i)` in `createSteps()`). Es gibt kein GUI-Control zur Änderung der Schrittanzahl. Der APVTS-Parameter `sequencerSteps` (Range 1–16, Default 16) existiert und wird vom internen (derzeit nicht im Editor sichtbaren) `StepSequencer` gelesen, hat aber keine Wirkung auf die sichtbare Footer-Anzeige.
**Kennzeichnung:** „Fixkonfiguration (16 Schritte)".

**Gibt es GUI-Controls dafür?**
Für `sequencerBpm` existiert ein BPM-Knob im Footer. Für `sequencerSteps` gibt es kein Control (Footer rendert fest 16 Steps); der Parameter bleibt per APVTS automatisierbar.

**Empfehlung (offen):** Schritt-Auswahl (4/8/16) im Footer ergänzen oder den internen `StepSequencer` mit dem Host-Tempo (`getPlayHead`) koppeln.

---

## 8. Smoothing-Chain

### 8.1 Fluss

```text
GUI-Thread                              Audio-Thread
   │                                         │
   │ SliderAttachment::setValue()            │ processBlock()
   │                                         │   smoothing.update()   ← liest alle Raw-Werte
   ▼                                         │   smoothing.getById(id) ← geglättete Werte
APVTS (AudioProcessorValueTreeState)        │
   │                                         │
   │ ValueTree::setProperty()                │
   ▼                                         │
Parameter-Changed-Callback                  │
   │                                         │
   └────────── thread-safe (lock-free, atomic) ──────────┘
                                             ▼
                                    DSP verbraucht geglättete Werte
                                    (keine Klicks bei Parameteränderung)
```

### 8.2 API

`ParameterSmoothing` (in `Source/Parameters/ParameterSmoothing.h`):

```cpp
class ParameterSmoothing
{
public:
    void prepare      (double sampleRate);                  // Sample-Rate setzen, initiale Werte laden
    void setBlockSize (int samplesPerBlock);                // Blockgröße für korrekte pro-Schritt-Rampen (F-06)
    void update();                                          // pro Block: APVTS → Ziele + currentValues um einen Block-Schritt avancieren
    float get         (int index) const;                    // geglätteter Wert (blockbasiert, nicht avancierend)
    float getById     (const juce::String& id) const;       // geglätteter Wert nach ID (blockbasiert, nicht avancierend)
    float getNextValue(int index);                          // sample-genauer Wert (avanciert um einen Schritt)
    float getNextValueById(const juce::String& id);         // sample-genauer Wert nach ID (avanciert um einen Schritt)
    // ...
};
```

- `update()` wird in `processBlock()` **vor** `engine.render()` aufgerufen und avanciert `currentValues` um einen exponentiellen Block-Schritt Richtung Target (F-06).
- `getById()` liefert den aktuellen blockweise geglätteten Wert für einen Parameter (DSP liest dies einmal pro Block).
- `getNextValueById()` advanceert um einen pro-Sample-Schritt für echte sample-genaue Rampen. Wird im Output-Stage für `outputGain` und `outputPan` pro Sample aufgerufen (P0-4: Sample-Ramp für Gain/Pan).
- Intern pro Parameter: O(1) Lookup über `unordered_map<juce::String, size_t>` statt O(n) lineare Suche (F-06).
- `setBlockSize()` muss in `prepareToPlay()` aufgerufen werden, damit die pro-Schritt-Rampen korrekt dimensioniert sind.
- Für Choice-/Bool-Parameter wird der Zielwert ohne Glättung durchgereicht (kein Interpolieren von Enum-Werten).

### 8.3 Warum Smoothing wichtig ist

Ohne Glättung springt z. B. `fundamentalHz` bei einer Automation von 45 Hz auf 120 Hz innerhalb eines Samples → hörbarer Klick/Sprung. Mit Smoothing wird über die Blockgrenze interpoliert. Alle DSP-Zugriffe im Engine-Render verwenden daher `smoothing.getById(...)` statt `getRawParameterValue(...)->load()`.

Für echte sample-genaue Rampen ruft der DSP `getNextValueById(id)` pro Sample auf; dies advanceert den geglätteten Wert um einen exponentiellen Schritt Richtung Target und liefert den Wert vor dem Advance. Genutzt im Output-Stage für `outputGain` und `outputPan`.

### 8.4 Zentrales Kapitel: P0-P1-Fixes F-01 bis F-08

Alle im CERN Technical Review (F-01…F-08) dokumentierten Fixes sind hier zentral aufgeführt. Jeder Fix ist einer oder mehreren Dateien zugeordnet und hat einen dokumentierten Status.

| Fix-ID   | Beschreibung                                                                                                                                                                                                                                                                                    | Betroffene Dateien                                                                       | Status           | Zugehörige Tests              |
| -------- | ----------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------- | ---------------- | ----------------------------- |
| **F-01** | **Trigger von pitchRatio entkoppelt.** Der Pitch-Envelope moduliert danach nur die Frequenz. Er ist keine Trigger-Quelle. Trigger-Ereignisse stammen ausschließlich aus MIDI, Sequencer oder GUI-Trigger.                                                                                       | `Source/DSP/KickSynthesisEngine.cpp`, `Source/DSP/MembraneModel.h`                       | ✅ Implementiert | Trigger-Atomics, Event-Burst  |
| **F-02** | **StepSequencer von 30-Hz-GUI-Timer auf sample-genauen Audio-Thread-Takt umgestellt.** `processBlock(int numSamples)` ersetzt `juce::Timer`-Vererbung und `timerCallback()`. Schritte werden block-quantisiert ausgeführt (Sample-Offset-Trigger offen).                                        | `Source/Sequencer/StepSequencer.{h,cpp}`, `Source/PluginProcessor.cpp`                   | ✅ Implementiert | Sample-Offset, Host-Transport |
| **F-03** | **Paarweises Swing (tempoerhaltend).** Gerade Schritte = 2×straight×r, ungerade = 2×straight×(1−r), wobei r = 0.5 + swing×0.25. Die Pair-Dauer bleibt konstant → Tempo unverändert.                                                                                                             | `Source/Sequencer/StepSequencer.{h,cpp}`                                                 | ✅ Implementiert | Swing-Berechnung              |
| **F-04** | **Double-Buffer Waveform-Sync statt Atomic-Flag.** `std::array<juce::AudioBuffer<float>,2> waveformBuffers` + `std::atomic<int> waveformWriteIndex`; GUI liest von `1 - writeIndex`.                                                                                                            | `Source/PluginProcessor.h`, `Source/PluginProcessor.cpp`, `Source/GUI/MainComponent.cpp` | ✅ Implementiert | Data-Race, Threading (TSan)   |
| **F-05** | **Membran-Formel dimensionskorrigiert.** `f₁ = (α₀₁ / (2π·R)) · sqrt(T / σ)` mit α₀₁ = 2.4048 (erste Nullstelle der Bessel-Funktion J₀). Ersetzt die dimensionswidrige Vorgängerformel `0.766 · T / (σ · D)`.                                                                                   | `Source/DSP/MembraneModel.h`                                                             | ✅ Implementiert | Bessel-Formel                 |
| **F-06** | **ParameterSmoothing umgeschrieben.** O(1) `unordered_map` ID-Lookup, `getNextValue()` für per-Sample-Advancement, `setBlockSize()` für korrekte pro-Block-Rampen; `update()` avanciert `currentValues` um einen Block-Schritt Richtung Target; `rawValues`/`getRaw()` entfernt als Toter Code. | `Source/Parameters/ParameterSmoothing.h`                                                 | ✅ Implementiert | Smoothing, Klick-Schutz       |
| **F-07** | **Output-Stage Signalfluss geklärt.** Reihenfolge: Gain → M/S Mono-Below → Pan → Soft-Clip (`outputClip`) → Peak-Limiter (`outputLimiter`) als finale Sicherheit nach dem Clip.                                                                                                                 | `Source/DSP/KickSynthesisEngine.cpp`                                                     | ✅ Implementiert | Pegelkontrolle, Limiter       |
| **F-08** | **`renderVoice()` entfernt; `triggerButton` momentar.** Die Engine ist rein monophon über `render()`. `triggerButton` wird nach dem Feuern auf `false` zurückgesetzt (Guard `isResettingTrigger` verhindert Rekursion in `parameterChanged`).                                                   | `Source/DSP/KickSynthesisEngine.{h,cpp}`, `Source/PluginProcessor.{h,cpp}`               | ✅ Implementiert | Trigger-Atomics, Monophonie   |

**Zusätzliche nicht-F-01…F-08 Fixes:**

- Higgs-Parameter werden über `smoothing.getById()` gelesen statt `load()` (Klick-Schutz bei Automation).
- EQ wird in `render()` angewendet (war vorher konfiguriert, hatte aber keine klangliche Wirkung).
- `setBlockSize()` wird in `prepareToPlay()` aufgerufen, bevor `prepare()` auf die Smoothing-Kette ruft.
- `else if` in `parameterChanged` zur Vermeidung von Fall-Through.

### 8.5 Higgs-Glossar

Das Higgs-Modul ist ein physikalisch inspiriertes DSP-Modul mit 13 Parametern. Die Begriffe sind aus der Teilchenphysik entlehnt, dienen aber als Metapher für DSP-Zustände und Modulationspfade.

| Begriff                            | Bedeutung im Kontext von CERN                                                                                                                             |
| ---------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| **Phi (Φ)**                        | Das „Higgs-Feld" selbst – der zentrale Zustandsvektor des Moduls, der die aktuelle Modulationsamplitude repräsentiert.                                          |
| **Theta (Θ)**                      | Die Phase oder Winkelkomponente des Feldes, die über die Zeit rotiert und modulierende Oszillationen erzeugt.                                                   |
| **VEV** (Vacuum Expectation Value) | Der Ruhewert des Feldes ohne externe Anregung. Alle Modulationen werden als Abweichung vom VEV berechnet.                                                       |
| **Yukawa-Kopplung**                | Die Kupplungsfestigkeit zwischen dem Higgs-Feld und anderen DSP-Modulen (z.B. Membran, Drive, Reverb). Bestimmt, wie stark das Feld andere Parameter moduliert. |
| **Selbstkopplung λ (Lambda)**      | Die Nichtlinearität des Feldes selbst. Bestimmt, wie stark das Feld auf seine eigene Amplitude zurückwirkt (Selbstinteraktion).                                 |
| **Symmetrie-Gating**               | Ein Mechanismus, der die Symmetrie des Feldes bricht oder wiederherstellt, je nach Trigger-Zustand. Steuert, ob Modulationen aktiv oder gedämpft sind.          |

**Musikalische Einordnung:**
Das Higgs-Modul soll keine exakte physikalische Simulation sein, sondern eine metaphorische Klangformungs-Einheit. Die fünf wichtigsten musikalischen Controls sind: Feld-Intensität, Modulationsrate, Kupplungsfestigkeit, Selbstkopplung und Symmetrie-Gating.

### 8.6 Dokumentation automatisch gegen Code prüfen (Doc-Lint)

Die Dokumentation sollte niemals von der tatsächlichen Code-Implementierung abweichen. Um Inkonsistenzen früh zu erkennen, wird ein regelmäßiger „Doc-Lint“-Durchgang empfohlen:

| Prüfpunkt                       | Methode                                                                                                                                                         | Betroffene Dateien                                                                                         |
| ------------------------------- | --------------------------------------------------------------------------------------------------------------------------------------------------------------- | ---------------------------------------------------------------------------------------------------------- |
| Parameter-IDs vs. APVTS         | Automatischer Abgleich aller IDs in der Dokumentation gegen `ParameterIDs.h` und den tatsächlichen APVTS-Aufruf im Code                                         | `Source/Parameters/ParameterIDs.h`, `Source/PluginProcessor.cpp`, `TECHNICAL_WIRING_FULL.md` (§2.13–§2.15) |
| GUI-Controls vs. Attachments    | Prüfen, ob jeder sichtbare Slider/Knob/Button eine entsprechende `SliderAttachment`/`ComboBoxAttachment`/`ButtonAttachment` hat                                 | `Source/GUI/MainComponent.cpp`, `Source/PluginProcessor.cpp`                                               |
| Formeln zentral halten          | Alle mathematischen Formeln (Membran, Swing, Smoothing, Compressor-Knee) müssen an einer Stelle definiert und von überall referenziert werden, nicht dupliziert | `TECHNICAL_WIRING_FULL.md` (§2.1, §7.4, §8, §4.x)                                                          |
| Changelog-Fixes vs. Kapitel     | Jeder Fix im Changelog (§15) muss in mindestens einem betroffenen Kapitel erwähnt sein                                                                          | `TECHNICAL_WIRING_FULL.md` (§15, alle Kapitel)                                                             |
| Testanzahl automatisch          | Die Anzahl der Testfälle und Assertions muss aus dem tatsächlichen Testsystem ausgelesen, nicht manuell eingetragen werden                                      | `Source/Tests/`, `TECHNICAL_WIRING_FULL.md` (Header)                                                       |
| Build/Testdaten mit Commit-Hash | Jeder Build- und Testverweis sollte den Git-Commit-Hash enthalten, auf den sich die Verifikation bezieht                                                        | Build-Skripts, `TECHNICAL_WIRING_FULL.md` (Header)                                                         |

**Empfohlener Workflow:**

1. Vor jedem Release einen Doc-Lint durchführen.
2. Abweichungen zwischen Dokumentation und Code als Blocker behandeln.
3. Die Dokumentation soll immer einer konkreten Code-Version zugeordnet sein (Commit-Hash oder Versionsnummer im Header).

---

## 9. Higgs Field: Physik & Symmetry-Gating

### 9.1 Konzept

Der `HiggsFieldEngine` bildet eine 3D-Potentialfläche ab. Der Mauszeiger (Phi/Theta) bewegt einen Punkt auf dieser Fläche; die daraus abgeleiteten Größen modulieren das Audiosignal.

### 9.2 Zustandsgrößen

| Größe            | Parameter                   | Wirkung                                         |
| ---------------- | --------------------------- | ----------------------------------------------- |
| Φ (Phi)          | `higgsCursorPhi` (0–1)      | Position auf der Potentialfläche (Maus-X)       |
| θ (Theta)        | `higgsCursorTheta` (0–360°) | Azimut (Maus-Y)                                 |
| Feldstärke       | `higgsFieldStrength`        | Kopplung des Feldes ans Signal                  |
| VEV              | `higgsVev` (0.2–0.95)       | Vakuum-Erwartungswert → Radius des Minimumrings |
| Selbstkopplung λ | `higgsSelfCoupling`         | Krümmung des Potentials                         |
| Trägheit         | `higgsInertia`              | Sustain-Anteil (symmetry-gated)                 |
| Yukawa-Boost     | `higgsYukawaBoost`          | Präsenz/Höhen-Boost                             |
| Boson-Frequenz   | `higgsBosonHz` (100–140 Hz) | Mittenfrequenz des Resonators                   |
| Boson-Q          | `higgsBosonQ` (1–30)        | Bandbreite des Resonators                       |
| Symmetrie        | `higgsSymmetry` (0–1)       | 0 = gebrochen, 1 = wiederhergestellt            |
| Zerfall          | `higgsDecay`                | Rückführung Richtung Dry                        |
| Mix              | `higgsMix`                  | Wet/Dry                                         |

### 9.3 Symmetry-Gating

`coeff` (der Koeffizient, der Inertia/Sustain gewichtet) ist **symmetry-gated**:

```cpp
const float gate  = higgsSymmetry;          // 0 … 1
const float coeff = baseCoeff * gate;       // bei Symmetrie 0 → kein Sustain
```

Bei `higgsSymmetry = 0` ist der Effekt vollständig „gebrochen" — Inertia hat keinen Einfluss. Bei `higgsSymmetry = 1` ist die Symmetrie wiederhergestellt und `higgsInertia` wirkt voll.

### 9.4 Parameter-Zugriff

Die Higgs-Parameter werden im Audio-Thread vollständig über `smoothing.getById(...)` gelesen und in `HiggsFieldEngine::Params` kopiert (`KickSynthesisEngine.cpp`, Block `hp.*`). Nur die **GUI** (`HiggsField3D`) liest Rohwerte für die Darstellung. Damit folgt der Higgs-Pfad demselben Smoothing-Muster wie der Rest des DSP.

---

## 10. Preset & Trigger Wiring

### 10.1 PresetManager

- Konstruktor: `PresetManager (apvts, engine.getPatchGraph(), engine.getParameterPatchGraph())` im Processor.
- Factory-Presets: Code-Snapshots in `Source/Presets/PresetManager.cpp` (kein JSON/BinaryData); jeder Snapshot enthält `<PARAM>`-Kinder, `<Patch>` und `<ParameterPatch>`.
- `loadFactory(index)` / `loadInit()` / `loadUser(file)`: `ValueTree::fromXml` + `replaceState`; der Patch wird vor dem APVTS-State separat restauriert.
- `getFactoryPresetNames()` und `getPresetEntries()` (Init, Factory, User inkl. Unterordner) speisen die Header-Auswahl/das Browser-Menü.
- User-Presets: `saveUser(file)` schreibt `apvts.copyState().createXml()`; Standardordner ist
  `%APPDATA%/C.E.R.N/Presets/<name>.xml` (Windows) bzw. `~/.config/C.E.R.N/Presets` (Linux); das alte
  `user-preset.xml` bleibt als Legacy ladbar.
- Thumbnail-Waveform ist **nicht** implementiert (Roadmap); der Datei-/Kategorien-Browser ist vorhanden.

### 10.2 HeaderBar-Verdrahtung

| Element  | Verhalten                                                                |
| -------- | ------------------------------------------------------------------------ |
| ComboBox | `refreshPresetList()` im Konstruktor; Auswahl → `loadPresetAt(index)`    |
| ◀ / ▶    | `stepPreset(-1/+1)`, zyklisch, `sendNotificationSync`                    |
| ★        | lokaler Favourite-Zustand, keine Persistenz                              |
| 💾 / 📦  | beide `saveCurrentAsUserPreset("My Preset")` (fester Name, überschreibt) |
| 📁       | nur `refreshPresetList()`                                                |
| TRIGGER  | `triggerDebugAttack()` → `engine.trigger(1.0f)`                          |

### 10.3 Trigger-Pfad

Es gibt drei Trigger-Quellen:

1. **StepSequencer (Audio-Thread)** → `triggerSequencerStep(vel, pitch)` → atomare Pending-Variablen.
2. **TRIGGER-Button** → `triggerDebugAttack()` → direkter `engine.trigger(1.0f)` (Audio-Thread-sicher, da `engine.trigger` nur Atomics setzt).
3. **MIDI / Host** → im `processBlock` über Note-On.

Der APVTS-Parameter `triggerButton` wird vom TRIGGER-Button beschrieben; nach dem Auslösen wird der Parameter auf `false` zurückgesetzt (Guard gegen Rekursion via `isResettingTrigger`), damit kein Dauer-Trigger entsteht (F-08).

### 10.4 State-Serialisierung

```cpp
void getStateInformation (MemoryBlock& destData)
{
    MemoryOutputStream stream (destData, false);
    parameters.state.writeToStream (stream);
}

void setStateInformation (const void* data, int sizeInBytes)
{
    auto tree = ValueTree::readFromData (data, (size_t) sizeInBytes);
    if (tree.isValid())
        parameters.replaceState (tree);
}
```

- Es wird **nur** der APVTS-ValueTree serialisiert (alle 1229 Parameter);
  Audio-Patch und Parameter-CV liegen als zusätzliche `<Patch>`/`<ParameterPatch>`-Knoten bei.
- Keine zusätzlichen Laufzeitdaten (Sequencer-Position, History, Favourite).
- Nach `replaceState` greift beim nächsten Block `smoothing.update()`.

---

## 11. Message-Thread vs Audio-Thread Datenfluss

### 11.1 Thread-Grenzen

| Ressource                                                      | GUI-/Message-Thread                                                    | Audio-Thread                         | Synchronisation                                    |
| -------------------------------------------------------------- | ---------------------------------------------------------------------- | ------------------------------------ | -------------------------------------------------- |
| APVTS-Parameter                                                | schreibt (`SliderAttachment`, `ButtonAttachment`, Choices, Higgs-Maus) | liest (`smoothing.update()`)         | lock-free / atomar                                 |
| `waveformBuffers[2]`                                           | liest über `getWaveformSample()`                                       | schreibt abwechselnd                 | `atomic<int> waveformWriteIndex` (acquire/release) |
| `spectrumMagnitudesSnapshot`                                   | liest über `getSpectrumMagnitude()`                                    | schreibt nach `engine.getSpectrum()` | Snapshot-Kopie                                     |
| `pendingTrigger` / `pendingVelocity` / `pendingPitchSemitones` | schreibt (`triggerSequencerStep`)                                      | liest + `exchange(false)`            | `std::atomic`                                      |
| `sequencerPlaying`                                             | schreibt (Footer `start()`/`stop()` via `setValueNotifyingHost`)       | liest `StepSequencer`                | atomar                                             |
| `playing`, `currentStep`, `stepPhase`, `looping`               | Footer-Timer                                                           | —                                    | nur Message-Thread                                 |
| `sectionCanvas`/Panels/Widgets                                 | `resized()`/`layout()`                                                 | —                                    | nur Message-Thread                                 |
| `bodyEnv`, Filterzustände, Higgs-Zustand                       | —                                                                      | liest/schreibt                       | nur Audio-Thread                                   |

### 11.2 Regeln

1. Kein `new`/`delete` im Audio-Thread.
2. Keine Locks im Audio-Thread; geteilte Werte über `std::atomic` oder APVTS.
3. GUI-Timer (10/30/60 Hz) lesen nur; sie schreiben DSP-Zustand ausschließlich über APVTS oder die Pending-Atomics.
4. `smoothing.update()` ist die einzige APVTS→DSP-Brücke.
5. Display-Daten laufen über Double-Buffer/Snapshot, nie über direkte Pufferzeiger.

### 11.3 Datenfluss-Diagramm

```text
┌──────────────┐   setValue()    ┌──────────────┐   update()   ┌──────────────┐
│  GUI-Slider  │ ──────────────▶ │    APVTS     │ ───────────▶ │  Smoothing   │
│ (Message)    │                 │ (thread-safe)│              │ (Audio)      │
└──────────────┘                 └──────┬───────┘              └──────┬───────┘
                                        │                             │
                                        │ getRawParameterValue()      │ getById()
                                        ▼                             ▼
                                 ┌──────────────┐              ┌──────────────┐
                                 │StepSequencer │             │    DSP       │
                                 │(Audio-Thread)│              │  (render)    │
                                 └──────┬───────┘              └──────┬───────┘
                                        │ triggerSequencerStep()      │ Samples
                                        ▼                             │
                                 ┌──────────────┐                     │
                                 │ Pending-     │                     │
                                 │ Atomics      │─────────────────────┘
                                 └──────────────┘
                                        │
                                        │ waveformBuffers / spectrumSnapshot
                                        ▼
                                 ┌──────────────┐
                                 │  GUI-Display │
                                 │  (30 Hz)     │
                                 └──────────────┘
```

---

## 12. Offene Punkte & Bekannte Lücken

### 12.1 Bewusst so belassen

| Punkt                                    | Status        | Begründung                                                                                                                  |
| ---------------------------------------- | ------------- | --------------------------------------------------------------------------------------------------------------------------- |
| Body-Panel                               | ✅ bedienbar  | `bodyLevel`, `bodyTuning`, `bodyAttack`, `bodyDecay` im Panel BODY.                                                         |
| `rootNote` / `velocitySensitivity`       | ✅ bedienbar  | Beide im Panel GLOBAL / TUNING; `velocitySensitivity` wird am Trigger angewendet (`velocityGain = 1.0 + sens·(vel − 1.0)`). |
| `EnvelopeEditor` als Visualizer          | ✅ by design  | Liest `pitchEnvAttack/Decay/Curve`, schreibt nichts.                                                                        |
| Higgs-Slider + Knobs doppelt             | ✅ by design  | Kompakt-Sektion und Detail-3D-View bedienen dieselben Parameter.                                                            |
| Knobs prozedural statt Bildstreifen      | ✅ by design  | Kein Asset-Slicing nötig; konsistente Skalierung.                                                                           |
| Higgs als Canvas-Projektion statt OpenGL | ⚠️ Abweichung | `juce_opengl` ist gelinkt, wird aber nicht genutzt; die 3D-Wirkung ist per Projektion/Path erreicht.                        |

### 12.2 Verbleibende technische Lücken

1. **Steps-Auswahl (4/8/16) per ComboBox im Footer:** `sequencerSteps` ist jetzt über eine ComboBox im FooterSequencer editierbar (4/8/16 Schritte). Inaktive Schritte werden ausgeblendet, Gitter-Linien angepasst. `sequencerBpm` ist seit der Einführung des BPM-Knobs im FooterSequencer direkt bedienbar.
2. **Trigger-Parameter momentar (F-08):** `triggerButton` wird vom TRIGGER-Button beschrieben und nach dem Feuern auf `false` zurückgesetzt (Guard `isResettingTrigger` verhindert Rekursion).
3. **Host-Transport:** gekoppelt via `FooterSequencer::timerCallback()` — liest `getPlayHead()->getPosition()->getIsPlaying()`, stoppt den internen Sequencer bei Host-Stop und startet ihn bei Host-Start (echte Flanke, `hostWasPlaying`-Guard). `getPlayHead()` kann `nullptr` sein (Standalone), wird abgefangen. Start/Stop/Seek/Loop des Hosts werden damit an den internen Sequencer weitergereicht.
4. **Preset-UX:** Speichern/Laden nutzen einen Save-As-Dialog bzw. den Ordner-Browser (`PresetManager::getPresetEntries()` mit FACTORY/USER/Unterordner-Kategorien) im Header. Thumbnails fehlen noch (Roadmap).
5. **CPU-Anzeige:** Jetzt live gemessen über `HeaderBar`-Timer (ca. alle 500 ms), statt statisch "0.0%".
6. **`SectionPanel`-Höhe:** `preferredHeight` war nur Inhaltshöhe (ohne Titel/Top-View), nun Gesamthöhe (`y + pad`). Overlap im Pitch-Envelope-Panel behoben.
7. **Swing nur im Audio-Thread:** Der `StepSequencer` wendet paarweises Swing (`r = 0.5 + swing×0.25`; gerade = 2×straight×r, ungerade = 2×straight×(1−r)) an; der Footer-Timer dient nur der Anzeige und berechnet keinen Swing mehr.
8. **`MainComponent`-Wunschgröße 1440×920** wird vom Editor (1400×900) überschrieben — kosmetisch, aber inkonsistent.
9. **`renderVoice()` entfernt (F-08):** Die Engine ist rein monophon über `render()`; es gibt keine Voice-Polyphonie-API mehr.

**Erledigt (seit letztem Build):**

- `sequencerSteps` ist per ComboBox (4/8/16) im FooterSequencer editierbar; inaktive Schritte ausgeblendet.
- `sequencerBpm` ist im Footer als editierbarer Knob bedienbar.
- `velocitySensitivity` und `rootNote` sind im neuen GLOBAL / TUNING-Panel bedienbar.
- `compKnee` ist korrekt wired (Wert wird an `processCompressor` übergeben, SoftKneeCompressor implementiert Knee-Logik).
- Sequencer-Doppelpfad beseitigt: `FooterSequencer` ist reine Transport-/Anzeige-Steuerung, `StepSequencer` im Audio-Thread ist die einzige Clock.
- Step-Pitch-Slider (`stepPitch_i`) sind im Footer editierbar.
- Body-Panel (`bodyLevel`, `bodyAttack`, `bodyDecay`, `bodyTuning`) ist vorhanden.
- `triggerButton` Guard `isResettingTrigger` verhindert Rekursion (F-08).
- FooterSequencer GUI vollständig überarbeitet: klare Sektion-Labels (VEL/GATE/PITCH), große Schritt-Nummern (1-16), Velocity-Slider mit Wert-Anzeige, Gate-Toggle-Buttons, Pitch-Slider pro Schritt, Playhead-Indikator mit Schlag-Blitz, Transport-Buttons mit Icons (▶/■/↻), BPM/Swing mit Einheiten, Tooltips für alle Controls, Gitter-Linien zwischen Steps.

### 12.3 Empfehlungen (priorisiert)

| Priorität | Maßnahme                                                                |
| --------- | ----------------------------------------------------------------------- |
| P3        | CPU-Last über `AudioDeviceManager`/Timer messen.                        |
| P3        | OpenGL-Shader für Higgs (optional) oder Doku klar auf Canvas festlegen. |

### 12.4 Oversampling / Aliasing — Auswertung (P1-3)

**Frage:** Sollte CERN Oversampling einsetzen, um Aliasing in nichtlinearen DSP-Stufen (insb. Drive/Saturation) zu reduzieren?

**Befund:** Nein — aktuell wird kein Oversampling eingesetzt.

**Begründung:**

1. **Keine hörbaren Aliasing-Artefakte im Kick-Drum-Kontext.** Kick-Drums liegen primär im Sub-Bass (30–120 Hz) und Körperbereich (100–500 Hz). Nichtlineare Verzerrung erzeugt Obertonanteile, die bei typischen Drive-Einstellungen (Amount 0–0.4, Mix 0–0.6) weit unter dem Masking-Schwellenbereich liegen. Aliasing-Falten im oberen Obertonbereich (>5 kHz) werden durch das nachfolgende Soft-Clip (tanh) und den Peak-Limiter sowie den Reverb-HighCut (bis 8 kHz) weitgehend abgedämmt.

2. **Drive-Charakter "Alias" ist beabsichtigt.** Der Drive-Charakter `case 4` (Bitcrusher-ähnliche Stufenquantisierung) erzeugt Aliasing als **gestalterisches Mittel** (Industrial/Lo-Fi-Charakter, §2.7, §5 Stufe 6). Dies ist ein bewusster Sound, kein unbeabsichtigter Artefakt.

3. **CPU-Kosten.** 2×-Oversampling würde die gesamte DSP-Kette (Membrane mit bis zu 24 Modi, Tube-Resonator, 4-Band-EQ, SoftKneeCompressor, Higgs-Resonator, Reverb-FDN) pro Block doppelt ausführen. Bei einem monophonen Kick-Synthesizer mit kurzen Decay-Zeiten steht der Nutzen in keinem Verhältnis zu den Kosten.

4. **Aliasing-Vermeidung ohne Oversampling:**
   - Soft-Knee-Kompressor (§8, Stufe 8) begrenzt Pegelspitzen vor dem Limiter.
   - Soft-Clip im Output-Stage (tanh, F-07) begrenzt Obertonausschläge.
   - NaN/Inf-Guard und einpoliger DC-Blocker im Drive (§5 Stufe 6) verhindern hörbare Artefakte.

**Offen:** Sollte zukünftig ein High-Gain-Drive-Modus (Amount > 0.8) hinzugefügt werden, wäre 2×-Oversampling vor der Drive-Stufe mit Downsampling danach die erste Maßnahme. Dies ist derzeit nicht geplant.

**Verwandte Stellen:** §2.7 (Drive/Saturation, Character "Alias"), §5 (Stufe 6: Drive), §7 (sample-genaue StepSequencer-Clock), §13 (Build/Tests).

---

## 13. Build-, Runtime- & UI-Verifikation

### 13.1 Build-Kommandos (verifiziert)

```bash
cd /data/projekte/Kickdrum
cmake --build build -j$(nproc) --target CERN_All CERNTests CERNStandalone
./build/Source/CERNTests
```

Ergebnis:

```text
[100%] Built target CERN
[100%] Built target CERN_All
[100%] Built target CERNTests
[100%] Built target CERNStandalone
Passed: 1175146 -> 0 Failed (Werte stehen live in ./build/Source/CERNTests)
```

> Beim ersten Configure wird JUCE per `FetchContent` geladen. Netzwerkfehler (`fetch-pack: invalid index-pack output`, HTTP 504) sind transient und erfordern nur einen erneuten `cmake -B build -DCMAKE_BUILD_TYPE=Release`.

### 13.2 Artefakte

| Artefakt   | Pfad                                                                                                 | Größe    |
| ---------- | ---------------------------------------------------------------------------------------------------- | -------- |
| VST3       | `build/Source/CERN_artefacts/Release/VST3/CERN.vst3/Contents/x86_64-linux/CERN.so` | ~18.9 MB |
| Standalone | `build/Source/CERNStandalone_artefacts/Release/C.E.R.N Standalone`                      | ~17.9 MB |
| Tests      | `build/Source/CERNTests`                                                                       | —        |

### 13.3 Runtime-Checks

| Prüfung                                                                      | Ergebnis                                                                                              |
| ---------------------------------------------------------------------------- | ----------------------------------------------------------------------------------------------------- |
| VST3 laden (`dlopen` + `GetPluginFactory`)                                   | ✅ gültiger Handle, Factory exportiert                                                                |
| Standalone-Start (Xvfb, 1600×1000)                                           | ✅ läuft, kein Segfault                                                                               |
| Segfault in `StepSequencer::resized()` (Früh-`setSize` vor Widget-Erzeugung) | ✅ behoben: `setSize` ans Konstruktorende verschoben + Guard `if (gateButtons[0] == nullptr) return;` |
| UI-Screenshot                                                                | ✅ RED-Design, Panels, Knobs, Combos, Envelope, Waveform, Spectrum, Footer sichtbar                   |
| Viewport-Scroll                                                              | ✅ Sektionen 1–11 erreichbar                                                                          |
| Unit-Tests                                                                   | ✅ 27 Assertions / 7 Testfälle                                                                        |

### 13.4 Verifikations-Status (Wiring)

| Prüfung                                                              | Status                                                           |
| -------------------------------------------------------------------- | ---------------------------------------------------------------- |
| Alle 1229 Parameter in `getAllIDs()`                                 | ✅ APVTS + ID-Liste deckungsgleich (`parameter_layout_ids_are_complete_unique`) |
| Alle Parameter mit DSP-Verbrauch                                     | ✅                                                               |
| Knobs an echte APVTS-IDs gebunden (keine hartkodierten Fantasie-IDs) | ✅                                                               |
| Choice-Combos synchron (ComboBoxAttachment)                          | ✅                                                               |
| Waveform/Spectrum-Provider verbunden                                 | ✅                                                               |
| Step-Velocity/Gate-Attachments (32)                                  | ✅                                                               |
| Parameter-Smoothing im DSP aktiv (inkl. Higgs)                       | ✅                                                               |
| Display-Double-Buffer                                                | ✅                                                               |
| Pending-Trigger-Atomics                                              | ✅                                                               |
| Fonts eingebettet                                                    | ✅ `FontBinaryData` (Rajdhani + JetBrains Mono)                  |
| VST3-Icon                                                            | ✅ `ICON_BIG`/`ICON_SMALL` → `Assets/Export/plugin_icon_256.jpg` |

### 13.5 Bekannte Build-Warnungen

- Veraltete `Font(float, int)`-Konstruktor-Warnung aus JUCE (`juce_Font.h`) — nur Warnung, kein Fehler.
- `juce_opengl` ist gelinkt, aber aktuell ungenutzt.

---

## 14. Design-Umsetzung (RED) & Komponenten-Inventar

### 14.1 Design-Tokens (implementiert)

| Token                          | Wert                                                  | Quelle                              |
| ------------------------------ | ----------------------------------------------------- | ----------------------------------- |
| Primary Background             | `#050506`                                             | Referenz-Merge                      |
| Panel Base                     | `#1A1919`                                             | Referenz-Merge                      |
| Panel Header/Deep              | `#0E0E0F`                                             | Referenz-Merge                      |
| Border Default                 | `#3A3A3C`                                             | Referenz-Merge                      |
| **Primary Accent**             | **`#FF2A2A`**                                         | KickForge-Referenz                  |
| Accent Hover / Active          | `#FF4C4C` / `#FF6666`                                 | Referenz-Merge                      |
| Accent Dim / Dark              | `#E11B1B` / `#8F1111`                                 | Referenz-Merge                      |
| Glow                           | `#FF2020` / `#FF1A1A`                                 | Referenz-Merge                      |
| Secondary (Orbit)              | `#5FA8FF`                                             | Higgs-Physik/Referenz               |
| Text Primary / Secondary / Dim | `#E6E6E6` / `#B8B8B8` / `#8C8C8C`                     | Referenz-Merge                      |
| Display BG / Grid              | `#070707` / `#4A1A1A`                                 | Referenz-Merge                      |
| 5-Band-System                  | `#FF2244`, `#FF8800`, `#FFDD00`, `#00FFFF`, `#00FFAA` | CERN-Differenzierer (bewahrt) |

### 14.2 Typografie

| Rolle                | Font              | Größe/Gewicht |
| -------------------- | ----------------- | ------------- |
| Logo / Hero          | Rajdhani Bold     | 28 pt         |
| Sektions-Titel       | Rajdhani Bold     | 26 pt         |
| Preset               | Rajdhani SemiBold | 24 pt         |
| Parameter-Label      | Rajdhani SemiBold | 14 pt         |
| Small Label / Slogan | Rajdhani Medium   | 12 pt         |
| Sequencer            | Rajdhani SemiBold | 15 pt         |
| Dropdown             | Rajdhani Medium   | 16 pt         |
| Werte                | JetBrains Mono    | 12–14 pt      |
| Body                 | JetBrains Mono    | 14 pt         |

Quelle: `Assets/Fonts/*.ttf` → `FontBinaryData` → `FontScheme`.

### 14.3 Controls & Maße

| Control           | Umsetzung                                                                                       |
| ----------------- | ----------------------------------------------------------------------------------------------- |
| Rotary-Knob       | 52/64/76 px, Körper + Track-Arc + roter Value-Arc + Indikator + Center-Cap, Bipolar-Mittellinie |
| Slider (vertikal) | 8 px Track, roter Fill, dunkler Hintergrund                                                     |
| Toggle            | Pill-Optik (42×22 im LookAndFeel, Step-Gates kompakt 24×16)                                     |
| Button            | Beveled Frame, 6 px Chamfer                                                                     |
| Panel             | 12 px Chamfer, 2 px Außenrahmen, 1 px Innen-Highlight                                           |
| Combo             | RED-LookAndFeel, 26 px Höhe im Grid                                                             |
| Displays          | 4 px gerundete Ecken, Grid, 30 Hz                                                               |

### 14.4 Panel-/Sektions-Inventar (13 Panels)

| Panel             | Breite | Top-View                  | Controls                                |
| ----------------- | ------ | ------------------------- | --------------------------------------- |
| MEMBRANE          | Medium | —                         | 8 Knobs                                 |
| HOLLOW STEEL      | Narrow | —                         | 5 Knobs                                 |
| PITCH ENVELOPE    | Wide   | `EnvelopeEditor` (116 px) | 4 Knobs                                 |
| CLICK / BEATER    | Narrow | —                         | 4 Knobs + 1 Choice                      |
| SUB               | Narrow | —                         | 3 Knobs + 1 Choice                      |
| DRIVE             | Narrow | —                         | 3 Knobs + 1 Choice                      |
| MIX / EQ / COMP   | Wide   | —                         | 13 Knobs                                |
| OUTPUT / SPATIAL  | Narrow | —                         | 6 Knobs                                 |
| ROOM / RUMBLE     | Narrow | —                         | 3 Knobs                                 |
| REVERB / MATERIAL | Narrow | —                         | 7 Knobs + 1 Choice                      |
| HIGGS FIELD       | Wide   | `HiggsField3D` (232 px)   | 12 Knobs + 3D-View                      |
| GLOBAL / TUNING   | Narrow | —                         | 2 Knobs (rootNote, velocitySensitivity) |

### 14.5 Visualizer

**SpectrumAnalyzer**

- 96 Balken, Breite `width/96`.
- Pro Balken Maximum aus drei aufeinanderfolgenden Bin-Indizes (`b*3+k`).
- Normierung: `(dB + 100) / 100`, geclamped 0…1.
- Farbe: `accentSub` (Blau) → `accent` (Rot) interpoliert nach Pegel.
- Gitterlinien bei 100 Hz / 1 kHz / 10 kHz (log-Position), Beschriftung.
- **Nicht** implementiert: 48-Band-Log-Mapping, Peak-Hold, Higgs-Marker bei 125.35 Hz.

**WaveformDisplay**

- 4096 Samples, Kanal 0, Amplitude −1…1, 45 % Höhe.
- 4×4-Raster + Mittellinie in `displayGrid` (`#4A1A1A`).
- Kurve in `Theme::accent` (Rot), 1.6 px.
- Ready-Provider ist konstant `true` (Double-Buffer); Consume ist No-Op.

**EnvelopeEditor**

- Liest `pitchEnvAttack`, `pitchEnvDecay`, `pitchEnvCurve` (Curve Range −1.0…+1.0, Default 0.0; <0 = exponentiell, >0 = linear/invertiert). Der Visualizer verwendet denselben Wert wie der DSP ohne zusätzliche Umrechnung.
- Zeichnet Attack-Rampe + Decay + quadratische Kurve, rot, 2 px.
- Read-only.

**HiggsField3D**

- Projektion einer Mexican-Hat-Fläche via `HiggsFieldEngine::potentialV()`.
- VEV-Ring, radiale Speichen, 5 Yukawa-Punkte auf dem Ring, Cursor + Vertikallinie.
- Maus-Down/Drag auf der Fläche sucht per Sampling den nächsten Phi/Theta-Punkt und schreibt `higgsCursorPhi`/`higgsCursorTheta` via `setValueNotifyingHost`.
- 10 vertikale Slider + Enable-Toggle, 30-Hz-Repaint.

### 14.6 Asset-Nutzung (Ist-Zustand)

| Asset-Ordner                                                                                                                                              | Nutzung                                                                                                         |
| --------------------------------------------------------------------------------------------------------------------------------------------------------- | --------------------------------------------------------------------------------------------------------------- |
| `Fonts/`                                                                                                                                                  | ✅ direkt eingebettet (6 TTFs)                                                                                  |
| `Export/plugin_icon_256.jpg`                                                                                                                              | ✅ VST3-Icon                                                                                                    |
| `Reference/`                                                                                                                                              | ✅ Design-Spezifikation, manuell in Tokens übertragen                                                           |
| `Knobs/`, `Sliders/`, `Buttons/`, `Panels/`, `Backgrounds/`, `Spectrum/`, `Waveform/`, `Sequencer/`, `HiggsField/`, `Presets/`, `Icons/`, `Theme/`, `UI/` | ⚠️ als Design-Referenz/Backup vorhanden, aber **nicht** zur Laufzeit geladen — die UI ist prozedural gezeichnet |

### 14.7 Bewusste Abweichungen von der ursprünglichen Planung

| Plan                                                 | Implementierung                                                 |
| ---------------------------------------------------- | --------------------------------------------------------------- |
| OpenGL/GLSL-Shader für Higgs                         | Canvas-Projektion (`Path`/`project()`), `juce_opengl` ungenutzt |
| Knob-Bildstreifen slicen                             | prozedurales `drawRotarySlider`                                 |
| 48 log-Bars + Peak-Glow + Higgs-Marker               | 96 Balken, lineare Bin-Auswahl, kein Peak/Marker                |
| Weiße Waveform + roter Glow                          | rote Waveform, kein Glow                                        |
| Vollständiger Preset-Browser (Kategorien/Thumbnails) | Header-ComboBox + festes Speichern                              |
| Body-Panel                                           | vorhanden (`BODY`, Narrow)                                      |

---

## 15. Änderungsprotokoll dieser Dokumentation

| Datum      | Änderung                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| ---------- | -------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------------- |
| 2026-11-09 | Review-Behebung §4.7 + §6.4: Neuer Abschnitt „4.7 Reverb / Room — Algorithmus, Parameter und Verhalten" ergänzt (FDN-Algorithmus, RT60-Mapping in ms, Pre-Delay 0–150 ms, Damping als Lowpass im Feedback + Low/High-Cut, partielle Stereo-Dekorrelation via halben Tap-Versatz, Mono-Kompatibilität, Tail-Verhalten bei Note-Off/Preset-Wechsel, keine klickfreie Crossfade bei `reset()` — ehrlicher Ist-Dokumentationsstand, alle 8 Reverb-Parameter implementiert und an GUI gebunden). §2.13 und neuer §7.8 klären `sequencerBpm` (Plugin-interner Wert 40–240 BPM, **nicht** host-synchronisiert, BPM-Knob im Footer) und `sequencerSteps` (Fixkonfiguration 16 Schritte, kein GUI-Control) als „Fixkonfiguration" — keine Quellcode-Änderungen nötig, da beides bereits so umgesetzt. |
| 2026-11-09 | P0-P1-Fixes (F-01…F-08) eingespielt: Trigger von pitchRatio entkoppelt (F-01), Sequencer auf sample-genauen Audio-Thread-Takt umgestellt (F-02), paarweises tempo-erhaltendes Swing (F-03), Double-Buffer Waveform-Sync statt Atomic-Flag (F-04), Membran-Formel dimensionskorrigiert (F-05), ParameterSmoothing mit O(1)-Lookup + setBlockSize + getNextValue (F-06), Output-Stage Signalfluss klarer (Clip→Limiter) (F-07), renderVoice entfernt + triggerButton momentar mit Reset (F-08). Zusätzlich: Higgs-Parameter über Smoothing gelesen (Klick-Schutz), EQ in render() angewendet, getRaw()/toter Code entfernt, setBlockSize vor prepare() gerufen.                                                                                                                                |
| 2026-11-09 | Sequencer-Doppelpfad beseitigt: `FooterSequencer` ist jetzt reine Transport-/Anzeige-Steuerung und schreibt `sequencerPlaying`; der sample-genaue `StepSequencer` im Audio-Thread ist die einzige Clock. `triggerCurrentStep()`/lokaler Timer-Trigger entfernt, LOOP an `StepSequencer::setLooping()` gebunden. `velocitySensitivity`-Dead-Code aus dem Engine-Render entfernt (Wirkung erfolgt am Trigger in `PluginProcessor`). Parameter-Zähler auf 131 direkt bedienbar / 1 DSP-only aktualisiert (Body-Panel, Step-Pitch-Slider, GLOBAL/TUNING-Panel, BPM-Knob).                                                                                                                                                                                                                        |
| 2026-11-09 | Dokument auf den tatsächlichen Implementierungsstand gebracht: RED-Design, neue GUI-Klassen, Viewport-Layout, reale APVTS-Bindings, Choice-Sync, FooterSequencer-Triggerpfad, PresetManager, korrigierte Parameter-Zähler und APVTS-Indizes, Higgs-Smoothing, Build-/Runtime-Verifikation, Abweichungs- und Lückenliste.                                                                                                                                                                                                                                                                                                                                                                                                                                                                     |
| 2026-11-09 | UI/UX Überarbeitung (P0/P1): Pitch-Envelope-Panel Overlap behoben (SectionPanel::preferredHeight ist jetzt Gesamthöhe); Footer-Sequencer radikal vereinfacht (16 Steps mit Velocity-Balken + Gate-Toggle, Pitch-Slider entfernt); Spectrum Analyzer auf 48 Bars reduziert mit log-Frequenz-Mapping und Peak-Rot-Färbung; Waveform Display mit hellerem Raster und deutlicher Zero-Crossing-Linie; ParameterKnob Werte-Schrift vergrößert (14px) und Einheiten (Hz/ms/dB/%) angezeigt; HeaderBar TRIGGER-Button prominenter (110×34px) und CPU-Live-Messung via Timer; Knob inaktive Arcs mit mehr Kontrast. Build: CERN [100%], Tests: 84 assertions / 22 cases — all passed.                                                                                                          |
| 2026-11-09 | P3-Nits aufgeräumt: totes Member `playAtt` aus `FooterSequencer.h` entfernt; No-Op `StepSequencer::advanceStep()` aus `.h/.cpp` entfernt; ungenutztes `MainComponent::getParameterIndex()` aus `.h/.cpp` entfernt; überflüssiger Guard `isResettingTrigger` aus `PluginProcessor.h` entfernt (`setValue()` notifyt keine Listener, keine Rekursion möglich); deprecated `juce::Font(float, int)` an 4 Stellen in `StepSequencer.cpp` durch `juce::Font(float).boldened()` ersetzt (verbleibende JUCE-interne Warnungen nur kosmetisch, kein Patch möglich). Build: CERN [100%], Tests: 84 assertions / 22 cases — all passed.                                                                                                                                                          |
| 2026-11-09 | P1/P2-Reviewer-Befunde behoben: `getTailLengthSeconds()` liest nun Bereichwerte (ms) via `getParameter()->getValue()` statt normalisierter Raw-Werte; CERNLogger erhält Warnhinweis (writeLine nicht audio-thread-sicher, No-Op-Stubs bleiben); Kompressor-Setter (`setThreshold/ratio/attack/release/knee`) einmal pro Block vor dem Sample-Loop statt pro Sample; EQ-Koeffizienten per Dirty-Flag nur bei Änderung neu berechnet (allokationsfrei im Steady-State); `getById(const char*)`/string_view-Lookup für Parameter-Smoothing; Dokumentation Test-Anzahl (84 assertions / 22 cases) und Panel-Inventar (13 Panels inkl. GLOBAL/TUNING) korrigiert.                                                                                                                           |
| 2026-11-09 | UI/UX Layout-Overhaul (P0/P1) nach Screenshot-Analyse: `layoutSectionGrid()` respektiert jetzt Narrow/Medium/Wide-Breiten statt alle Panels auf gleiche Breite zu quetschen → Pitch-Envelope (Wide, Top-View 116px) nicht mehr von BODY überlappt; Footer 200→240px mit breiteren Velocity-Balken (12px statt 6px) und größeren Transport-Buttons (72px statt 64px); ParameterKnob Labels 14pt statt 11pt, Werte 16pt statt 14pt; ComboBoxen auf 120px begrenzt statt volle Panel-Breite; Spectrum Analyzer Gitter heller (0.4f→0.7f Alpha). Build: CERN [100%], Tests: 84 assertions / 22 cases — all passed.                                                                                                                                                                         |
| 2026-11-09 | P1-2 Einheiten für alle Parameter: `unitFor`-Lambda in `MainComponent::addKnob()` erweitert um Hz (hp/cut/tone/mono: drivePreHP, eqRumbleHP, reverbLowCut, reverbHighCut, clickTone, outputMonoBelow), st (detune/tuning: bodyTuning, subDetune), dB (knee/clip: compKnee, outputClip), Grad (phi/theta: higgsCursorPhi, higgsCursorTheta), GeV (vev: higgsVev). Detune/Tuning korrigiert von Hz → st. Build: CERN [100%], Tests: 84 assertions / 22 cases — all passed.                                                                                                                                                                                                                                                                                                               |
| 2026-11-09 | GUI-Überarbeitung FooterSequencer: Vollständige Neugestaltung der Step-Sequencer-Oberfläche für maximale Lesbarkeit. Klare Sektion-Labels (VEL/GATE/PITCH) über den Steps, große Schritt-Nummern (1-16), Velocity-Slider mit Wert-Anzeige pro Schritt, Gate-Toggle-Buttons (AN/AUS), Pitch-Slider pro Schritt (-12..+12 st), Playhead-Indikator (vertikale Leiste + Dreieck + Schlag-Blitz bei Trigger), Transport-Buttons mit klaren Icons (▶ PLAY, ■ STOP, ↻ LOOP), BPM Knob mit Einheit, Swing Knob mit Einheit, Schritte-ComboBox (4/8/16) mit Label, Tooltips für alle Controls, Gitter-Linien zwischen Steps, hochkontrastige Farben (accentRed, accentBlue, textDim, textMuted). Build: CERN [100%], Tests: 84 assertions / 22 cases — all passed.                              |
| 2026-11-09 | §12.2 offene Punkte geschlossen: Host-Transport-Kopplung in `FooterSequencer::timerCallback()` implementiert (Host-Start/Stop wird an internen Sequencer weitergereicht via `getPlayHead()->getPosition()->getIsPlaying()`, `hostWasPlaying`-Guard verhindert wiederholtes Starten); Preset-Kategorie-Filter im `HeaderBar` aktiv (`categoryComboBox` mit `PresetManager::getCategories()` + `getFilteredPresetNames()`, `sequencerSteps` bleibt per ComboBox (4/8/16) editierbar). `renderVoice()` bleibt entfernt (monophon by design). Build: CERN [100%], Tests: 84 assertions / 22 cases — all passed.                                                                                                                                                                            |
| (früher)   | Ursprüngliche Fassung: DSP-Wiring, 132-Parameter-Index, Smoothing, Thread-Modell, JUCE-9-Migration.                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                                          |

---

_Ende des Dokuments._
