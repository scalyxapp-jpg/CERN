# C.E.R.N — Physical Modeling Techno Kick Drum VST3

## Übersicht

**C.E.R.N** ist ein physikalisch modellierter Techno-Kick-Drum-Synthesizer als VST3-Plugin (C++ + JUCE). Statt Samples wird eine kreisförmige Trommelfell-Membran über **modale Synthese** simuliert: Die Eigenfrequenzen der Membran folgen der Bessel-Funktionen-Nullstellentabelle (J₀, J₁, …) und werden auf die gewählte Grundfrequenz gestimmt. Jeder Modus schwingt als exponentiell abklingender Sinus mit individuellem Decay — genau wie eine echte Membran.

## Forschung: Was einen Techno-Kick ausmacht

Die folgenden Erkenntnisse aus der Schwingungslehre, klassischer Drum-Maschinen (TR-808 / TR-909) und modernem Industrial-Techno-Design sind direkt in den Algorithmus eingeflossen:

### Frequenz-Grundlagen (recherchiert)

| Bereich                   | Frequenz                                                   | Funktion                                        |
| ------------------------- | ---------------------------------------------------------- | ----------------------------------------------- |
| **Sub-Fundamental**       | **40–60 Hz** (Sweet Spot 40–50 Hz = E0 41,2 Hz – A0 55 Hz) | Tiefenfundament, körperlich spürbar             |
| **Körper / Percussion**   | 80–100 Hz                                                  | Punch, „Fleisch“ des Kicks                      |
| **Beater / Snap / Click** | 1,5–3,5 kHz                                                | Anschlag-Definition                             |
| **Attack-Höhe**           | 2–5 kHz                                                    | Durchsetzungsvermögen auf kleinen Lautsprechern |
| **Rumble-Cut**            | < 20–40 Hz (High-Pass)                                     | Unhörbarer Tiefbass entfernen                   |

### Material-Frequenzbilder (hohle Körper)

| Material   | Frequenzbild                   | Charakter                           | Decay  |
| ---------- | ------------------------------ | ----------------------------------- | ------ |
| **Holz**   | Hollowness 200–400 Hz          | Warm, Mahagoni dunkel, Ahorn heller | Mittel |
| **Metall** | 800–2000 Hz Klingen            | Weniger Resonanz, mehr Brightness   | Kurz   |
| **Acryl**  | Kaum Resonanz, direkter Attack | Lauter, punchiger als Holz          | Mittel |
| **Beton**  | 100–200 Hz, sehr dumpf         | Starkes High-Cut                    | Lang   |
| **Glas**   | 2000–5000 Hz, klingelnd        | Hell, metallisch                    | Kurz   |
| **Stein**  | 300–600 Hz, dumpfig            | Mittlerer Charakter                 | Mittel |

### Reverb / Modulation Stage

Ein **FDN (Feedback Delay Network)**-basierter Reverb mit material-spezifischer Farbierung:

- Jedes Material hat einen eigenen Material-EQ vor dem Reverb (Hollowness bei Holz, Klingen bei Metall, etc.)
- Unterschiedliche RT60-Decay-Kurven pro Material
- Pre-Delay, Size, Damping, Mix, Low/High-Cut frei einstellbar
- Techno-typisch: Low-Cut ~120 Hz, High-Cut ~800 Hz (um Mud im Mix zu vermeiden)
- Sidechain-kompatibel durch klare Trennung von Trocken- und Nasssignal

### Klassische Maschinen als Referenz

- **TR-909** (Pitch-Envelope, 2026-11-13 korrigiert): Einzelner Sinus-Oszillator + Pitch-Envelope (**Tonfall von oben auf die Zielfrequenz**) + Amplituden-Envelope. `Pitch Depth` ist die **Anfangstonhöhe in Oktaven** (0…3 ⇒ 0…+36 st, Default 1,5 ⇒ +18 st), `Pitch Decay` die Dauer des exponentiellen Abfalls (`exp(-4.605·u·2^curve)`), `Pitch Attack` die Haltezeit auf dem Maximum vor dem Abfall. Vor der Korrektur lief der Tonfall nach **oben** — die Ursache der Nutzerbeschwerde „zu hochgepatcht".
- **TR-808**: Reiner Sinus ohne Filterung, schneller Pitch-Decay (20–70 ms) für den typischen Click, langer Decay für den „Jeep Drone“.

### Hollow Steel / Industrial Kick

- **Wave-Folding** statt einfaches Clipping erzeugt komplexere Obertöne und den metallischen Charakter.
- Verzerrter Low-End, **Hohlkörper-Resonanz** (Luftkavity mit schmalem Resonanz-Pick, f ~ 1/D), spätes Raum-Rumble.
- Mehrstufige Verarbeitung verändert das Envelope und die Harmonische Struktur.

### Physische Modellierung der Membran

- Grundfrequenz nach Schwingungslehre: **f₁ ≈ 0,766 · T / (σ · D)**  
  (T = Spannung, σ = Flächenmasse, D = Durchmesser)
- **Nichtlineare Spannungs-Modulation** (nach von Kármán / Berger): Bei hoher Anregungsamplitude dehnt sich die Membran → effektive Spannung steigt → alle Teillast-Frequenzen gleiten nach oben (Pitch-Glide), beim Ausklingen zurück.
- **Hohlkörper-Kopplung**: Eine Luftkavity mit schmalem Resonanz-Pick wird über einen Kopplungsfaktor mit der Membran vermischt → typisch hohler, metallischer Sound.

### Spatial / Club-Tauglichkeit

- Club-Sound-Systeme sind überwiegend **mono** (insbesondere unter 150 Hz).
- Mono-Kompatibilität ist essenziell; Low-End sollte unter einer einstellbaren Grenzfrequenz (Default 120 Hz) auf Mono gemischt werden (Mid/Side-Verarbeitung).
- Stereobreite nur im Obertönbereich, nicht im Sub.

---

## Higgs-Feld: Materiephysik als Klangmodulation

### Die Physik (Fakten)

Das Higgs-Feld Φ ist ein **Skalarfeld**, das den gesamten Raum durchdringt. Sein
Potential lautet

```
V(Φ) = μ²|Φ|² + λ|Φ|⁴        mit μ² < 0
```

Dadurch hat V(Φ) die Form eines **Mexican Hat** (Sombrero): in der Mitte
(Φ = 0) liegt ein lokaler **Potentialberg**, auf einem Ring bei

```
|Φ| = v = 246,22 GeV        (Vakuumerwartungswert, VEV)
```

das **Minimum**. Der Grundzustand des Universums ist nicht Φ = 0, sondern
Φ = v — die **spontane Symmetriebrechung**. Teilchen erhalten ihre **Masse**
durch die Kopplung an dieses Feld, quantifiziert über die **Yukawa-Kopplung**:

```
m_f = y_f · v / √2
```

Die Kopplungsstärken sind extrem hierarchisch (Top-Quark y ≈ 1,0,
Elektron y ≈ 2,9·10⁻⁶). Die Anregung des Feldes um sein Minimum ist das
**Higgs-Boson** mit m_H = 125,35 GeV und einer Zerfallsbreite Γ ≈ 4,1 MeV
(Lebensdauer ~1,6·10⁻²² s). Der Selbstkopplungs-Term λ folgt aus
λ = m_H² / (2v²) ≈ 0,129.

Der Schlüssel für die musikalische Übertragung ist die **Äquivalenz von
Energie und Masse**:

```
E = m·c²      ↔      E = h·f
```

**Frequenz ist Energie ist Masse.** Ein tiefer Ton trägt physikalisch mehr
Energie pro Quant — und klingt in unserer Wahrnehmung „schwer", träge,
lang nachklingend. Genau diese Hierarchie bildet das Plugin ab.

### Die musikalische Übertragung (Materie → Klang)

| Physik                             | Klang-Entsprechung im Plugin                                                      |
| ---------------------------------- | --------------------------------------------------------------------------------- |
| Higgs-Feld Φ durchdringt den Raum  | Der Effekt liegt **hinter** der Klangerzeugung und formt den gesamten Kick        |
| Φ/v = Feldwert                     | **„Sein" des Kicks**: wie massiv, substanziell und sustainreich er ist            |
| Φ → 0 (symmetrische Phase)         | **Masselos**: Kick ist dünn, transient, „durchscheinend" (Bänder stark gedämpft)  |
| Φ ≈ v (Potentialminimum)           | **Gebrochene Phase**: volle Masse/Präsenz, maximale Trägheit                      |
| V(Φ) Potententialberg in der Mitte | Sättigung/Nichtlinearität (Selbstkopplung λ) — der Kick „kollabiert" in die Mitte |
| Yukawa-Hierarchie y_f              | Frequenzbänder koppeln unterschiedlich stark ans Feld                             |
| m_H = 125,35 GeV                   | **125,35 Hz** als Boson-Resonanz-Anker (Peak mit wählbarer Q)                     |
| Zerfallsbreite Γ                   | Resonanzbreite Q ∝ 1/Γ und „Ausklingen" des Effekts                               |
| λ Selbstkopplung                   | Nichtlineare Rückkopplung der Bänder — Oberton-Reichtum/Substanz                  |
| Spontane Symmetriebrechung         | Der Kick „wählt" einen Grundzustand: mit oder ohne Masse                          |

### Die fünf „Teilchen" des Kicks

Die Frequenzbänder sind als Teilchen mit absteigender Yukawa-Kopplung angeordnet
(abgeleitet aus E = h·f ↔ E = m·c²):

| Band      | Frequenz    | Teilchen-Analogie | Yukawa (normiert) | Klang-Charakter                       |
| --------- | ----------- | ----------------- | ----------------- | ------------------------------------- |
| **Sub**   | 20–80 Hz    | Top-Quark         | 1,00              | Schwerste Trägheit, maximaler Sustain |
| **Body**  | 80–250 Hz   | Bottom-Quark      | 0,55              | Gewicht, Wärme, „Fleisch"             |
| **Punch** | 250–1500 Hz | Charm-Quark       | 0,32              | Attack-Schärfe, Durchsetzung          |
| **Click** | 1,5–5 kHz   | Tau-Lepton        | 0,14              | Anschlag, Beater-Textur               |
| **Air**   | 5–20 kHz    | Elektron          | 0,05              | Fast masselos, nur Luft/Glanz         |

### Die interaktive 3D-Fläche (Mexican Hat)

Die GUI zeigt das Higgs-Potential als 3D-Oberfläche (Mexican Hat):

- **Der Cursor kann mit der Maus an beliebige Positionen gezogen werden.**
  Beim Ziehen werden **alle Higgs-Parameter live aus der Cursorposition abgeleitet**
  und in die APVTS geschrieben (Φ/Θ direkt, alle anderen Werte als Funktion davon):
  Field Strength (1-φ), VEV (radiale Position → 0,2 … 0,95), λ-Self-Coupling,
  Inertia (sin(π·φ)), Yukawa-Boost, Boson-Frequenz (100–140 Hz) und Boson-Q,
  Symmetry (sin(π·Θ)), Decay (0,5…1) und Mix (|sin(2π·φ)|).
- Alle **13 Parameter sind im Display selbst sichtbar** (Wireframe + numerische
  Spalte rechts) und ändern sich in Echtzeit, sobald sie geändert werden:
  VEV → Radius des gestrichelten Minimums-Rings, λ → Wandsteilheit,
  Strength → Kuppelhöhe, Symmetry → Verkippung der Fläche, Yukawa →
  Partikel-Dichte, Decay → Geister-Ringe, Mix → gefülltes Wet/Dry-Volumen,
  Boson Hz/Q → Resonanz-Ring, Inertia → Glow des Cursors.
- **HIGGS FIELD**-Knob in der Sektion oder per Checkbox in der GUI → Effekt an/aus.
- Der **VEV-Ring** (Minimum) leuchtet als gestrichelter Kreis; die Kuppel in der Mitte
  glüht; außen fällt die Fläche wieder ab.
- Die fünf Teilchen sitzen als Punkte am Rand; ihre Helligkeit zeigt die
  aktuelle Yukawa-Kopplung.

**Bedienlogik in einem Satz:** Ziehe den Cursor auf den VEV-Ring → der Kick
bekommt maximale Masse/Substanz. Ziehe ihn ins Zentrum (Φ→0, oder SYMM auf 1)
→ der Kick entmaterialisiert zum transienten, dünnen Impuls. Der Winkel θ
bestimmt, _welches_ Frequenzband gerade am stärksten an das Feld andockt —
also ob der Sub, der Punch oder die Luft „die Masse trägt".

### Parameter

| Parameter        | Bereich             | Bedeutung                                 |
| ---------------- | ------------------- | ----------------------------------------- |
| Higgs Field On   | an/aus              | Effekt aktiv                              |
| Field Φ/v        | 0–1                 | Cursor radial (auch direkt per 3D-Fläche) |
| Yukawa Angle θ   | 0–360°              | Frequenzfokus (welches Band koppelt)      |
| Field Strength   | 0–1                 | Globale Kopplungsstärke                   |
| VEV (v)          | 0,2–0,95 (×246 GeV) | Lage des Minimumrings (Anzeige in GeV)    |
| Self-Coupling λ  | 0–1 (≈0–0,129)      | Nichtlineare Selbstkopplung/Sättigung     |
| Inertia          | 0–1                 | Trägheit → Sustain pro Band               |
| Yukawa Boost     | 0–1                 | Verstärkung der θ-Fokus-Kopplung          |
| Boson m_H        | 100–140 Hz          | Resonanzfrequenz (Default 125 Hz)         |
| Boson Q          | 1–30                | Resonanzgüte (∝ 1/Γ)                      |
| Symmetry Restore | 0–1                 | 1 = symmetrische Phase (masselos)         |
| Decay Rate Γ     | 0–1                 | Zerfallsrate → Resonanzbreite             |
| Wet/Dry Mix      | 0–1                 | Effektanteil                              |

### Factory-Presets mit Higgs-Feld-Setup

Drei Factory-Presets enthalten vollständige Higgs-Feld-Einstellungen und Parameter-CV-Kabel:

| Preset                   | Higgs-Setup                                   | Klang / Modulation                                            |
| ------------------------ | --------------------------------------------- | ------------------------------------------------------------- |
| **CV Pulse Kick**        | Higgs Mix 0.12, Feld 0.60, Boson 103,8 Hz/Q 8 | Body-Level → Drive-Mix; Higgs aktuell aus (bis SND-117)       |
| **Higgs Mod Kick**       | Higgs Mix 0.15, Feld 0.80, Boson 92,5 Hz/Q 8  | Vier CV-Kabel auf Higgs-Stärke, Higgs-Mix, Room und Symmetrie |
| **Parameter Patch Kick** | Higgs Mix 0.12, Feld 0.55, Boson 116,5 Hz/Q 8 | Higgs-Stärke → Boson-Q; Diode-Drive mit Pre-HP                |

Hinweis: SND-117 (Higgs liefert nur Wet) ist umgesetzt. `higgsEnabled` ist deshalb in
fuenf der sieben Factory-Presets **1** (Deep Techno Kick, Tight Techno, CV Pulse Kick,
Higgs Mod Kick, Parameter Patch Kick); `Hard Acid Kick` und `808 Boomy` halten es bewusst
auf 0. Die Higgs-Settings und CV-Kabel sind in allen CV-Presets enthalten.

---

## Tech Stack

- **C++20**, JUCE Framework (via `FetchContent` aus GitHub `develop`)
- **VST3** (nur dieses Format; CLAP/AU/Standalone leicht erweiterbar)
- CMake Build, optionale AddressSanitizer/UBSan-Laeufe (`-DCERN_SANITIZE=ON`)
- Projekt-eigene Unit-Test-Harness (`Tests/test_main.cpp`, ~1,17 Mio. Assertions)

## Projektstruktur

```
CERN/
├── CMakeLists.txt              # Root-CMake, JUCE-FetchContent, CPack
├── Source/
│   ├── CMakeLists.txt          # Plugin-Target + Quellen + BinaryData
│   ├── PluginProcessor.{h,cpp} # AudioProcessor (APVTS, Prozessblock, Zustand)
│   ├── PluginEditor.{h,cpp}    # Editor-Fenster
│   ├── PluginEntry.cpp         # createPluginFilter() Entry-Point
│   ├── DSP/
│   │   ├── MembraneModel.h     # Kreisförmige Membran: Bessel-Modalbank, Nonlinearität, f1≈0.766·T/(σ·D)
│   │   ├── PitchEnvelope.h     # Pitch-Glide-Envelope (TR-909/808-Style)
│   │   ├── ClickGenerator.h    # Beater-Impact (Noise+Sine, 3 Materialien)
│   │   ├── TubeResonator.h     # Hohlkörper-/Röhren-Resonanz (Hollow Steel)
│   │   └── KickSynthesisEngine.{h,cpp} # Vollsignalfluss
│   ├── Parameters/
│   │   ├── ParameterIDs.h      # 1229 Parameter-IDs (Reihenfolge = APVTS-Index)
│   │   ├── ParameterLayout.h   # Default-Werte
│   │   ├── ParameterLayout.cpp # APVTS Parameter-Layout
│   │   └── ParameterSmoothing.h # Click-free exponentielle Glättung
│   ├── GUI/
│   │   ├── MainComponent.{h,cpp}     # Sektionen (Membrane…Global/Sequencer) + Visualisierungen, HeaderBar mit Preset-Auswahl
│   │   ├── SectionPanel.{h,cpp}      # Titelierte Panel-Container (3D)
│   │   ├── ParamKnob.{h,cpp}         # 3D-Drehregler + Wert-Chip
│   │   ├── CERNFader.{h,cpp}         # 3D-Fader (Mix-/Breiten-Parameter)
│   │   ├── LogoComponent.{h,cpp}     # Emblem-Asset + Vektor-Wortmarke
│   │   ├── WaveformDisplay.h         # Letzte Attack-Wellenform
│   │   ├── SpectrumAnalyzer.h        # Log-spacing Spektralbalken 20 Hz–18 kHz
│   │   ├── EnvelopeEditor.h          # Pitch-Envelope Kurve
│   │   ├── PatchView.{h,cpp}         # Rückseite: Audio-Patch
│   │   ├── ParameterPatchView.{h,cpp}# Parameter-CV-Patch
│   │   ├── HiggsField3D.{h,cpp}      # Interaktive Mexican-Hat-Fläche
│   │   ├── Theme/CERNLookAndFeel.{h,cpp} # Theme, 3D-Rotary/Linear/Toggle/Panels
│   │   └── Preview/PreviewMain.cpp   # Headless-Render der GUI nach PNG
│   ├── Assets/                       # Logo + Texturen (BinaryData)
│   └── Presets/
│       └── PresetManager.{h,cpp}     # 7 Factory-Presets (Code-Snapshots) + User-Save
├── Tests/
│   ├── CMakeLists.txt
│   └── test_main.cpp                 # Unit-Tests (DSP, Patches, Presets, Engine-Integration)
└── build/                            # CMake Build-Dir (gitignoren)
```

## Commands

### Erstmaliges Konfigurieren & Bauen (Release)

```bash
cd /data/projekte/Kickdrum
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc) --target CERN_VST3
```

Ergebnis:

```
build/Source/CERN_artefacts/Release/VST3/CERN.vst3/
  Contents/x86_64-linux/CERN.so
```

### Debug-Build (mit AddressSanitizer + UBSan)

```bash
cmake -B build-debug -DCMAKE_BUILD_TYPE=Debug -DCERN_SANITIZE=ON
cmake --build build-debug -j$(nproc) --target CERNTests
./build-debug/Source/CERNTests
```

`CERN_WERROR` ist standardmäßig **aus**; die Projekt-Warnflags (`-Wall -Wextra -Wpedantic -Wshadow -Wconversion`)
gelten nur für die eigenen Targets, nie für JUCE.

### Unit-Tests

```bash
cmake --build build -j$(nproc) --target CERNTests
./build/Source/CERNTests
# oder via ctest:
cd build && ctest --output-on-failure
```

### VST3 laden & smoke-test (ohne Host)

```bash
g++ /tmp/user/1000/opencode/test_vst3_load.cpp -o /tmp/user/1000/opencode/test_vst3_load -ldl
/tmp/user/1000/opencode/test_vst3_load
# Erwartet: loaded=0x… GetPluginFactory=0x…
```

## Parameter (automatisch generiert, 543)

Diese Tabellen werden **aus dem Quelltext erzeugt** (Review 2026-11-13, M11) und
können nicht mehr veralten:

```
python3 Scripts/gen_param_docs.py          # Tabellen auf stdout
python3 Scripts/update_readme_params.py    # README aktualisieren
```

Quelle der Defaults: `DefaultValues` in `Source/Parameters/ParameterLayout.h`;
Bereiche und Schrittweiten aus dem APVTS-Layout in
`Source/Parameters/ParameterLayout.cpp` (Stand 2026-11-13).

### Membrane / Physical Model

| Parameter | Werte | Default | Bedeutung |
| --------- | ----- | ------- | --------- |
| `membraneTension` | 0–1 | 0.5 | 0.0–1.0 |
| `membraneRadius` | 0.05–0.5 | 0.22 | 0.05–0.5 m |
| `membraneMass` | 0.1–2 | 0.8 | 0.1–2.0 kg/m² |
| `membraneDamping` | 0–1 | 0.45 | 0.0–1.0 |
| `membraneModes` | 1–12 | 4.0 | 1–24 |
| `fundamentalHz` | 25–180 | 46.25 | 30–120 Hz |
| `membraneNonlinearity` | 0–2 | 0.50 | 0.0–1.0 |
| `membraneStiffness` | 0–1 | 0.10 | 0.0–1.0 |

### Hollow Steel / Tube Resonance

| Parameter | Werte | Default | Bedeutung |
| --------- | ----- | ------- | --------- |
| `tubeResonanceFreq` | 40–400 | 70.0 | 40–300 Hz |
| `tubeResonanceQ` | 0.5–20 | 2.5 | 0.5–20 |
| `tubeResonanceLevel` | 0–1 | 0.15 | 0.0–1.0 |
| `tubeCoupling` | 0–1 | 0.30 | 0.0–1.0 |
| `hollowMetallic` | 0–1 | 0.08 | 0.0–1.0 |

### Pitch Envelope (TR-909/808)

| Parameter | Werte | Default | Bedeutung |
| --------- | ----- | ------- | --------- |
| `pitchEnvAttack` | 0.1–50 | 0.2 | 0.1–50 ms |
| `pitchEnvDecay` | 20–1500 | 45.0 | 20–1000 ms |
| `pitchEnvDepth` | 0–3 | 1.50 | 0.0–1.0 |
| `pitchEnvCurve` | -1–1 | 0.0 | -1.0–1.0 |

**Tonfall nach unten** (TR-909/808): Der Oszillator startet bei `2^PitchDepth × f0`
und fällt exponentiell auf `f0`. `Pitch Attack` hält den Startton kurz, `Pitch Decay`
bestimmt die Abfalldauer, `Pitch Curve` biegt die Abfallrate (`2^curve`).

### Click / Beater

| Parameter | Werte | Default | Bedeutung |
| --------- | ----- | ------- | --------- |
| `clickLevel` | 0–2 | 0.38 | 0.0–1.0 |
| `clickDecay` | 0.5–30 | 4.0 | 0.5–20 ms |
| `clickTone` | 0–1 | 0.22 | 0.0–1.0 |
| `clickDistortion` | 0–1 | 0.2 | 0.0–1.0 |
| `clickBeaterMaterial` |  |  | Choice: Soft/Medium/Hard |

### Body

| Parameter | Werte | Default | Bedeutung |
| --------- | ----- | ------- | --------- |
| `bodyLevel` | 0–1 | 0.35 | 0.0–1.0 |
| `bodyAttack` | 0.5–10 | 0.8 | 0.5–10 ms |
| `bodyDecay` | 50–4000 | 500.0 | 50–2000 ms |
| `bodyTuning` | 0.5–2 | 2.0 | 0.5–2.0 |

### Sub Oscillator (40–60 Hz Sweet Spot)

| Parameter | Werte | Default | Bedeutung |
| --------- | ----- | ------- | --------- |
| `subLevel` | 0–1 | 0.46 | 0.0–1.0 |
| `subDecay` | 50–3000 | 520.0 | 50–3000 ms |
| `subDetune` | -50–50 | 0.0 | -50–+50 cents |
| `subShape` |  |  | Choice: Sine/Triangle/Square |
| `subRatio` |  |  | Choice: 0.5 / 1.0 (SND-056/049) |

### Drive / Saturation (inkl. Wave-Folding)

| Parameter | Werte | Default | Bedeutung |
| --------- | ----- | ------- | --------- |
| `driveAmount` | 0–1 | 0.4 | 0.0–1.0 |
| `driveMix` | 0–1 | 0.45 | 0.0–1.0 |
| `driveCharacter` |  |  | Choice: Tube/Diode/Fuzz/Wave Fold/Alias |
| `drivePreHP` | 0–1 | 0.0 | 0.0–1.0 |

### EQ (Sub 40–60, Punch 80–100 & 2–5 kHz, Air)

| Parameter | Werte | Default | Bedeutung |
| --------- | ----- | ------- | --------- |
| `subLevel` | 0–1 | 0.46 | 0.0–1.0 |
| `subDecay` | 50–3000 | 520.0 | 50–3000 ms |
| `subDetune` | -50–50 | 0.0 | -50–+50 cents |
| `subShape` |  |  | Choice: Sine/Triangle/Square |
| `subRatio` |  |  | Choice: 0.5 / 1.0 (SND-056/049) |

### Compressor

| Parameter | Werte | Default | Bedeutung |
| --------- | ----- | ------- | --------- |
| `compThreshold` | -60–0 | -10.0 | -60–0 dB |
| `compRatio` | 1–20 | 2.5 | 1–20 :1 |
| `compAttack` | 0.1–100 | 6.0 | 0.1–100 ms |
| `compRelease` | 10–1000 | 240.0 | 10–1000 ms |
| `compKnee` | 0–12 | 6.0 | 0–12 dB |
| `compMakeup` | -12–24 | 0.0 | -12–+24 dB |
| `compMix` | 0–1 | 1.0 | 0–1 (SND-090/180) |
| `compAutoMakeup` | Bool | aus | Auto gain reduction makeup |

### Output / Spatial

| Parameter | Werte | Default | Bedeutung |
| --------- | ----- | ------- | --------- |
| `outputGain` | -60–12 | 0.0 | -60–+12 dB |
| `outputPan` | -1–1 | 0.0 | -1–+1 |
| `outputWidth` | 0–1 | 1.0 | 0–1 |
| `outputMonoBelow` | 40–200 | 150.0 | 40–200 Hz |
| `outputClip` | 0–1 | 0.0 | 0–1 |
| `outputLimiter` | 0–1 | 0.15 | 0–1 |

### Room / Late Rumble (Hollow Industrial)

| Parameter | Werte | Default | Bedeutung |
| --------- | ----- | ------- | --------- |
| `roomLevel` | 0–1 | 0.0 | 0.0–1.0 |
| `roomDecay` | 50–2000 | 350.0 | 100–1000 ms |
| `roomRumble` | 0–1 | 0.3 | 0.0–1.0 |

### Reverb / Modulation Stage (Hollow Material Verbs)

| Parameter | Werte | Default | Bedeutung |
| --------- | ----- | ------- | --------- |
| `reverbMaterial` |  |  | Choice 0–6 |
| `reverbPreDelay` | 0–150 | 30.0 | 0–150 ms |
| `reverbDecay` | 200–5000 | 1200.0 | 200–5000 ms (RT60) |
| `reverbSize` | 0–1 | 0.5 | 0–1 |
| `reverbDamping` | 0–1 | 0.3 | 0–1 |
| `reverbMix` | 0–1 | 0.15 | 0–1 |
| `reverbLowCut` | 20–500 | 150.0 | 20–500 Hz |
| `reverbHighCut` | 1000–16000 | 6000.0 | 1000–16000 Hz |
| `reverbFreeze` | Bool | aus | Freeze reverb tail |
| `reverbGate` | 0–500 | 0.0 | 0–500 ms (0=off, aktiv 10–500) |

### Material Room + Sub Grumble (Wet-Only, kein Druckverlust)

Material-physikalisch relativiertes Raummodell (Delay = L/c, Feedback aus Absorption α,
Damping aus α und Dichte). Das Modul liefert **ausschließlich Wet**; der Dry-Bus bleibt
unangetastet, der Wet-Return hat einen 100-Hz-Highpass 2. Ordnung (keine Sub-Antilphase). Layer:
Kick / Rumble / beides.

| Parameter | Werte | Default | Bedeutung |
| --------- | ----- | ------- | --------- |
| `materialRoomMaterial` |  |  | Choice: Water/Wood/Plastic/Steel/Concrete/Fabric |
| `materialRoomSize` | 1–30 | 8.0 | 1–30 m |
| `materialRoomMix` | 0–1 | 0.0 | 0–1 |
| `materialRoomLayer` |  |  | Choice: Kick/Rumble/Both |
| `materialRoomWidth` | 0–1 | 0.5 | 0–1 |
| `grumbleLevel` | 0–1 | 0.0 | 0–1 |
| `grumbleFreq` | 20–120 | 45.0 | 20–120 Hz |
| `grumbleRate` | 0.05–8 | 0.5 | 0.05–8 Hz |
| `grumbleDepth` | 0–1 | 0.5 | 0–1 |
| `grumblePanA` | -1–1 | -0.5 | -1–+1 |
| `grumblePanB` | -1–1 | 0.5 | -1–+1 |
| `grumbleWave` |  |  | Choice: Sine/Triangle/Square |
| `grumblePanWave` |  |  | Choice: Sine/Triangle/Square/Saw |
| `grumbleDecay` | 0.1–2 | 0.5 | 0.1–2 s |

Hinweis (MR-003/MR-004/MR-006): Steel ist ein **metallischer Resonator**, kein Hall. Der Wet-Pfad ist mit einem LowCut bei 100 Hz (2. Ordnung) geschuetzt, damit er den 20–100-Hz-Druck des Dry-Signals nicht ausloescht oder aufblaeht; Sub-Druck kommt ausschliesslich aus dem Dry-Signal. Bei hohem Mix bleibt trotzdem Gain-Staging sinnvoll. α unverändert. Modell ist **phänomenologisch** (Material-Charakter als Klangmetapher), keine raumakustische Simulation. **MR-013:** Width 0.5 = neutral, Seitenfaktor gekappt auf max 1.75 bei Width 1.0. **MR-020/056:** `IN KICK = CompOut` (Pre-Output), `IN RUMBLE = RoomOut` (Room-Tail; bei ROOM-Off stumm). **MR-030:** Grumble-Pan nutzt konstante Pan-Leistung (cos/sin). **MR-031:** Grumble-Pan geslewtes (~5 ms). **MR-033:** `grumbleLevel > 0.7` kann stark im Sub auftragen — Gain-Staging beachten. **MR-035:** starker Kick-Transient randomisiert Grumble-Phase deterministisch per LCG. **MR-043:** Size-Sweep erzeugt metallischen Comb-Sweep/Wub. **MR-050/053:** verifiziert (Verhalten entspricht Spezifikation). **MR-051:** **CV:** Velocity → `grumbleLevel` (über Parameter-CV/Modulation). **MR-052:** Pan A/B Endpunkte, Depth 0 = Mitte zwischen A und B. **MR-095:** Layer Rumble hängt von RoomOut/Room ab; bei ausgeschaltetem ROOM ist der Rumble-Eingang stumm. **MR-040/041/042/005:** bewusst unverändert (Materialtabelle/α, kein Dry-Eingriff, kein Fan-in-Normalisierung, keine impliziten Claims).

### Mixer (13 × 6 + 1 = 79 Parameter)

Ein Kanal je klang-erzeugendem Modul. Reihenfolge und IDs (`mixer0Gain` … `mixer12Mute`,
`mixerSidechainDepth`) stehen **am Ende** von `ParameterIDs::getAllIDs()`, damit
bestehende Preset-/State-Indizes stabil bleiben (Gesamtzahl: **1310**).

| Kanal | Modul           | Parameter                                                                          |
| ----- | --------------- | ---------------------------------------------------------------------------------- |
| 0     | MEMBRANE        | `mixer0Gain`, `mixer0Low`, `mixer0MidLow`, `mixer0Mid`, `mixer0High`, `mixer0Mute` |
| 1     | BODY            | `mixer1…`                                                                          |
| 2     | TUBE            | `mixer2…`                                                                          |
| 3     | SUB             | `mixer3…`                                                                          |
| 4     | CLICK           | `mixer4…`                                                                          |
| 5     | DRIVE           | `mixer5…`                                                                          |
| 6     | EQ              | `mixer6…`                                                                          |
| 7     | COMP            | `mixer7…`                                                                          |
| 8     | ROOM            | `mixer8…`                                                                          |
| 9     | HIGGS           | `mixer9…`                                                                          |
| 10    | REVERB          | `mixer10…`                                                                         |
| 11    | MAT ROOM        | `mixer11…`                                                                         |
| 12    | OUTPUT (Master) | `mixer12…`                                                                         |

- **Gain**: 0–2 (1,0 = neutral); **Bänder**: −24…+24 dB. Bei 0 dB laufen alle vier Bänder über einen Bypass (bit-transparent, keine Filter-Artefakte); beim Zurückschalten werden die Filterzustände genullt, Koeffizienten nur bei Wert-/Samplerate-Änderung neu berechnet.
- **`mixerSidechainDepth`**: 0–1, skaliert alle automatischen Ducking-Tiefen (Default 1,0).
- **Sidechain-Beziehungen** (fest): COMP → ROOM (max. 5,0 dB), HIGGS (4,0 dB), REVERB (5,0 dB), MAT ROOM (4,5 dB). Kein CLICK → SUB mehr (erzeugte ein hörbares Wobbeln des Grundtons und nahm dem Attack Low-End).
- **Ducking-Kennlinie**: `env / (env + 0,10)` – proportional und monoton, ein lauter Kick duckt tiefer, ein leiser nur wenig (früher: geklemmtes `env/0,35`, dadurch binär). Hüllkurve post-fader, Attack ~1 ms, Release ~90 ms; gemutete Kanäle ducken nicht.
- **Comp-Bypass**: Der COMP-Kanal führt auch im Bypass das Dry-Signal; das Ducking folgt dem Kick-Bus, nicht dem Kompressor-Gain-Reduction. Ducking bleibt daher bewusst aktiv, wenn der Compressor aus ist.
- **Anzeige**: Jeder FX-Streifen zeigt im Kopf die Quelle und die aktuelle Tiefe live (`SC COMP -3.2 dB`, 20 Hz). Peak-Meter fallen mit 20 dB/s.

### Layering (2 Parameter)

| Parameter | Werte | Default | Bedeutung |
| --------- | ----- | ------- | --------- |
| `layeringEnabled` | Bool | aus | Blendet zwei automatisch abgeleitete Zusatz-Layer ein (L1 Depth + L2 Presence) |
| `layeringAmount`  | 0–200 % | 100 % | Skaliert beide Layer-Pegel; die Pegelkompensation wird mitgezogen (0 % = exakt neutral) |

Alle Layer-Werte sind **abgeleitet**; der einzige Regler ist der Amount:

- **L1 DEPTH** – Sinus auf `fundamentalHz` (bzw. eine Oktave darunter, wenn f0 ≥ 55 Hz),
  Attack aus `bodyAttack` (×0,8), Decay aus `subDecay` (×1,05), Pegel aus `subLevel`
  + `driveAmount`, plus kleiner 2. Oberton (TV-Hörbarkeit).
- **L2 PRESENCE** – bandbegrenztes Rausch-/Ton-Gemisch, Bandlage aus `clickTone`
  (1,4–7,2 kHz), Attack 0,6–4 ms, Decay aus `clickDecay` (×2,4, 14–110 ms) und
  0,6–9 ms Delay (Definition statt Doppelklick); Dichte aus `clickDistortion` +
  `driveAmount` steuert Rausch-/Ton-Anteil.
- **Amount**: 0–200 % skaliert `k1`/`k2` vor der Kompensation; 0 % setzt Trim und Layer
  exakt auf neutral (bit-identisch zu „aus"), 200 % verdoppelt die Layer-Gewichte bei
  weiterhin mitgezogener Kompensation. Der Regler sitzt im Waveform-Panel unter der
  Checkbox (und in der FEATURES-Sektion); er ist ausgegraut, solange Layering aus ist.
  Die Legende zeigt Dichte und – sobald der Amount von 100 % abweicht – den Faktor
  (z. B. `D 47% · AMOUNT 150 %`).
- **Pegelneutral**: `baseTrim = 0.985 / sqrt(1 + k1² + k2² + k1)` (kohärente Addition
  des Depth-Layers auf dem Grundton) auf Membrane+Body; die Layer-Gewichte werden
  mit demselben Faktor skaliert.

### Step Sequencer (16 Schritte)

| Parameter | Werte | Default | Bedeutung |
| --------- | ----- | ------- | --------- |
| `sequencerSyncMode` |  |  | Choice: Internal/Host (SND-136) |
| `sequencerGrid` |  |  | Choice: 1/16, 1/8T, 1/16T (SND-144) |
| `sequencerPattern` |  |  | Choice: A–D (SND-146) |
| `sequencerChain` | 0–8 | 0.0 | 0–8 (SND-146) |
| `sequencerClockOut` | Bool | aus | Send MIDI clock/start/stop |

### Global

| Parameter | Werte | Default | Bedeutung |
| --------- | ----- | ------- | --------- |
| `rootNote` |  |  | 0–127 note |
| `velocitySensitivity` | 0–1 | 0.6 | 0.0–1.0 |
| `keyRoot` | 0–11 | 0.0 | 0–11 pitch class (SND-132) |
| `scale` |  |  | Choice: Chromatic/Minor (SND-132) |
| `glideMs` | 0–200 | 0.0 | 0–200 ms (SND-135) |
| `velToPitch` | 0–1 | 0.0 | 0–1 (SND-030) |
| `tubeTrack` | Bool | an | Tube resonance tracks f0 |

## GUI-Features

- **18 Sektionen**: Membrane, Tube, Pitch Envelope, Click, Body, Sub, Drive, EQ, Comp, Output, Room, Reverb, Material Room, Higgs Field, FEATURES, SIDECHAIN, Sequencer, Global
- **MIXER-Tab** (Header-Button `MIXER`): 13 Kanalstreifen – einer pro klang-erzeugendem Modul (MEMBRANE, BODY, TUBE, SUB, CLICK, DRIVE, EQ, COMP, ROOM, HIGGS, REVERB, MAT ROOM, OUTPUT). Jeder Streifen hat **Gain** (0–2), **4-Band-EQ** (LOW 120 Hz Shelf, MID-LOW 400 Hz Peak, MID 1.6 kHz Peak, HIGH 6 kHz Shelf, je ±24 dB), **MUTE** und einen Peak-Meter. Der Master-Kanal (OUTPUT) sitzt vor der Output-Stufe; alle Modul-Kanäle sitzen am Modul-Ausgang (Patchbay-Routing bleibt unangetastet, Fan-Out wird nicht doppelt verstärkt).
- **Automatische Sidechains im Mixer**: `COMP` duckt ROOM/HIGGS/REVERB/MAT ROOM (max. 5,0/4,0/5,0/4,5 dB, proportional zur Quell-Lautstärke); Tiefe global über den **SIDE-CHAIN**-Regler im Mixer-Kopf (0 = aus). Die Hüllkurve ist post-fader (Attack ~1 ms, Release ~90 ms), gemutete Kanäle ducken nicht. Die aktuelle Duck-Tiefe wird pro Streifen live angezeigt (`SC COMP -3.2 dB`).
- **Higgs-3D-Fläche**: Mexican-Hat-Potential, Cursor per Maus an beliebige Positionen ziehbar; beim Ziehen ändern sich **alle 13 Parameter live**; alle Parameter im Display sichtbar
- **Waveform-Display**: letzte Attack-Wellenform (Echtzeit, 30 Hz Refresh)
- **Spectrum-Analyzer**: log-spaced Balken 20 Hz–18 kHz, farbig nach Band
- **Envelope-Editor**: Pitch-Envelope Kurve visualisiert
- **Preset-Auswahl**: Factory-Presets als Dropdown, `SAVE` oeffnet einen Save-As-Dialog (Name + Zielordner unter `~/.config/C.E.R.N/Presets`), `LOAD` laedt eine User-XML von dort (SND-164). Der Preset-Browser listet Init, Factory- und User-Presets (inkl. Unterordner/Kategorien); eine Thumbnail-Waveform ist **geplant** (siehe Roadmap).
- **Step-Sequencer**: 16 sichtbare Schritte mit Velocity/Gate/Pitch, BPM, Swing, Lauflicht, manueller Schritt-Trigger; Ratchet/Flam/Probability/Length, Grid, Pattern-Kette und MIDI-Clock-Out sind als Parameter/Engine-Feature vorhanden (Step-Editor-UI für Ratchet & Co. noch nicht).
- **3D-Interface Schwarz-Weiss-Rot** (Details unten), Doppelklick = Reset auf Default
- **MIXER-Tab** (Header-Button `MIXER`): 13 Kanalstreifen (Membrane, Body, Tube, Sub, Click, Drive, EQ, Comp, Room, Higgs, Reverb, Mat Room, Output) mit je Gain, 4-Band-EQ (Low/Mid-Low/Mid/High, ±24 dB, 0 dB = bit-transparenter Bypass), MUTE und Peak-Meter; horizontales Scrollen, wenn nicht alle Streifen passen. Automatische Sidechains (Comp → Room/Higgs/Reverb/Mat-Room, max. 5,0/4,0/5,0/4,5 dB), Tiefe global über `SIDE-CHAIN` regelbar; Live-Anzeige der Duck-Tiefe im Streifenkopf.
- **MODUL-OVERLAY (Button `MOD` in der Waveform-Kopfzeile)**: blendet die vier
  Einzelbeiträge **Membrane / Body / Sub / Click** als eigene Kurven in die Waveform
  ein (oben steht die Summe, unten zeigt die Legende die Farbenzuordnung). Damit ist
  sichtbar, *was* den Kick trägt – der Review-Punkt „Modulbeiträge wie der Layer-Overlay".
  Die Beiträge werden vor dem Mixer abgegriffen und sind ungemischt dargestellt
  (gleiche absolute Skala wie das Hauptsignal).
- **PRESET-VORSCHAU (64-Punkt-Wellenform neben der Preset-Auswahl)**: jedes Factory-
  Preset wird beim Plugin-Start einmal offline gerendert (0,6 s, Low-Priority-Thread)
  und als Wellenform angezeigt – so sieht man vor dem Laden grob, wie der Preset klingt.
  `Init` (Index 0) bekommt dasselbe Thumbnail; User-Presets zeigen keinen Ausschnitt.
- **LAYERING (Checkbox "LAYERING ENABLED" in der Waveform-Anzeige)**: legt zwei vollständig automatisch berechnete Zusatz-Layer unter/über die Basis-Wave – **L1 DEPTH** (Grundton-Stützung, langer Decay, trägt bis in Rumble/Subbass) und **L2 PRESENCE** (kurzer bandbegrenzter Textur-Layer für Definition in Attack/Click). Frequenz, Länge, Attack, Decay, Pegel, Dichte und Bandlage werden aus fundamentalHz, Sub-, Body-, Click- und Drive-Parametern abgeleitet; beide Layer folgen exakt derselben Modulation (Pitch-Envelope, Glide, Velocity->Pitch, Velocity) wie die Basis. Die Waveform-Anzeige überlagert beide Layer farbig und zeigt die errechneten Werte (Hz · ms · dB · Dichte) als Legende. Die Pegel werden teil-kohärent kompensiert (Basis-Trim), damit das Layering voller statt lauter klingt: gemessen über alle 11 Factory-Presets bleibt die Lautheit innerhalb **+0,8 dB** (Worst Case bei extremen Modul-Einstellungen: **+0,81 dB**).
- **Resizable Fenster** (min. 700x500, Standard 1100x750), mono-kompatibler Low-End

### 3D-Interface & Assets (2026-11-12)

- **Farbpalette:** Background `#08080a`, Panel `#111216`, erhaben `#1b1d22`,
  Text `#f4f4f6`, Akzent `#e10600` / Highlight `#ff3b3b`, Metallkante `#3a3d45`.
  Rot ausschliesslich als Akzent (Bögen, LEDs, Zeiger, Glow) – keine roten Flaechen.
- **Panels/Displays:** `drawBevel`/`drawWell` erzeugen Verlauf, Metall-Textur,
  Licht-/Schattenkanten, Schrauben; Sektionen mit Titelband + roter LED.
- **Knobs:** Metallkappe aus `Assets/knob-cap.png`, prozeduraler Kranz, Track,
  roter Wert-Bogen mit Glow, Zeiger, Hover-Ring; Rotation/Kanten vektoriell.
- **Fader:** `CERNFader` (APVTS-gebunden) + `drawLinearSlider` mit versenktem
  Schacht (`Assets/fader-track.png`), Metallgriff, roter Rille und Fuellung.
  Verwendet fuer `driveMix`, `compMix`, `reverbMix`, `materialRoomMix`,
  `higgsMix`, `outputWidth`.
- **Header:** grafisches Logo statt Text – Emblem-Asset
  (`Assets/logo-emblem.png`, freigestellt) + Vektor-Wortmarke „C.E.R.N“ mit roten
  Punkten; Accessibility-Name `C.E.R.N`. Bei < 980 px zweizeilig, damit nichts
  abgeschnitten wird.
- **Assets:** `Assets/logo-emblem.png`, `knob-cap.png`, `metal-brushed.png`,
  `fader-track.png`, `grain.png` (erzeugt mit dem `generate-image` MCP,
  Nachbearbeitung/Freistellen via `Assets/build_assets.py`); eingebunden ueber
  `juce_add_binary_data(CERNAssets …)`.
- **Visuelle Pruefung:** `CERNPreview <out.png> <w> <h> [patch|patchcv|morph|material|scrollbottom]`
  rendert die echte `MainComponent` headless. Design-Review (2026-11-12, fortgesetzt):
  Screenshots 1400×900, 1100×750, 900×650, 700×500, 940×700 (<980 px, zweizeilig) sowie
  Patchbay, Parameter-CV, Material-Room, Footer-Crop/Scrollbottom und MORPH (A/B belegt)
  geprüft (Vision: `local-vision`/`nim-nemotron-vision`, je nach Verfügbarkeit). UI-Fixes:
  dichtere Sektionen bei ≥1400 px, lesbare Envelope-/Higgs-/Meter-/Footer-Bereiche,
  Preset-Combo ohne Pfeil-Overlap, Footer VEL/PITCH-Labels, nachbearbeitete Metall/Grain-Assets.
  Standalone unter Xvfb weiterhin nicht startbar (SIGSEGV, Exit 139): Backtrace
  `#0 KickSynthesisEngine::reset()` ← `CERNAudioProcessor::releaseResources()` ←
  `juce::AudioProcessorPlayer::audioDeviceAboutToStart` ←
  `juce::AudioDeviceManager::addAudioCallback` ← `CERNStandaloneApp::initialise`
  (Ursache außerhalb GUI-Scope).

## Factory Presets (16)

| Preset                  | Charakter                                                                                  |
| ----------------------- | ------------------------------------------------------------------------------------------ |
| Deep Techno Kick        | Trockener Deep-Techno-Kick (46,25 Hz): Pitch-Snap von +15,6 st, Concrete-Raum, moderate Sättigung |
| Hard Acid Kick          | Harter Acid-Kick (46,25 Hz): harter Beater, Diode-Drive mit Pre-HP, Punch bei 3 kHz        |
| 808 Boomy               | Boomy 808 (46,25 Hz): langer Shell-Body (92,5 Hz), Wood-Reverb, Room-Rumble                |
| Tight Techno            | Kurz und trocken: schneller Pitch-Abfall, kein Reverb, straffe Kompression                 |
| CV Pulse Kick           | Modulierter Kick: Body-Level → Drive-Mix plus Makro-Kabel                                  |
| Higgs Mod Kick          | Massiver Higgs-Kick: langer Pitch-Abfall, großes Shell-/Sub-Fundament, 4 CV-Kabel          |
| Parameter Patch Kick    | Diode-Kick: Pre-HP-Drive, Punch bei 3 kHz, Higgs-Stärke → Boson-Q                          |
| Underground Techno      | Sub-lastiger, dichter Club-Kick (Analyse eines Referenztracks)                             |
| Darker Sub Roller       | Sehr dunkel, viel Sub, wenig Click                                                         |
| Punch-roller            | Maximaler Punch: kurzer Sweep, harter Beater, Transient +3 dB                              |
| Low-end-roller          | Schwerer Roller mit langem Sub/Tube-Anteil                                                 |
| **Detroit 909 Classic** | **50 Hz** (Original-909-Lage), +21,6 st Starttonfall, harter Beater, kein Rumble            |
| **Dub Chord Kick**      | Langer Shell-Body (900 ms), Tube bei 92,5 Hz, Concrete-Reverb 1,8 s                        |
| **Hard Techno Distorted** | Wave-Fold-Sättigung, Drive-Pre-HP 0,35, trockener Raum                                    |
| **Minimal Click Kick**  | Nur Click + kurzer Sub, kein Tube, kein Room – maximale Definition                          |
| **Sub Only (23 Hz)**    | Sub-Oktave (Ratio 0,5), 700 ms Sub-Decay, Rumble-HP 18 Hz – maximaler Club-Sub             |

Alle 16 Presets sind Code-Snapshots in `Source/Presets/PresetManager.cpp`; es gibt
keine separate `factory_presets.json`. Jedes Preset enthält die 5 Makro-Kabel
(TUNE → fundamentalHz, PUNCH → clickLevel, BOOM → subLevel, DIRT → driveAmount,
SPACE → roomLevel), damit die Makro-Regler im Front-Panel wirklich wirken.
Preset-Slot 0 im Browser ist `Init` (SND-166); es zählt nicht zu den Factory-Presets.

**Lautheit:** alle 16 Presets sind auf **−13 LUFS** kalibriert (Messung 4 Takte
@120 BPM, 48 kHz, BS.1770); Streuung **0,31 dB** (Grenze 1,5 dB). Die Kalibrierung
ist reproduzierbar über `Scripts/calibrate_presets.py` (misst mit `CERNLab` und
korrigiert `outputGain`).

## Build Targets

| Target           | Beschreibung                      | Artefakt                                                                              |
| ---------------- | --------------------------------- | ------------------------------------------------------------------------------------- |
| `CERN_VST3`      | VST3-Plugin                       | `build/.../VST3/CERN.vst3/...`                                                        |
| `CERNStandalone` | Standalone-App                    | `build/Source/CERNStandalone_artefacts/Release/C.E.R.N Standalone` (benötigt GTK/X11) |
| `CERNTests`      | Unit-Tests (eigene Harness)       | `build/Source/CERNTests`                                                              |
| `CERNAssets`     | BinaryData-Target (Logo/Texturen) | statische Lib, von CERN/CERNStandalone/CERNTests/CERNPreview gelinkt                  |
| `CERNPreview`    | Headless-GUI-Render (PNG)         | `build/Source/CERNPreview_artefacts/Release/CERNPreview`                              |

### Standalone-Bauen (benötigt System-GUI-Libs)

```bash
cmake -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc) --target CERNStandalone
```

Hinweis: Unter Linux benötigt das Standalone-Target GTK und X11-Entwicklungsbibliotheken (`libgtk-3-dev`, `libx11-dev` etc.). Das VST3-Target baut ohne diese Abhängigkeiten.

## Audio Setup (Standalone)

Die Standalone-App speichert ihre Audio-Einstellungen unter
`~/.config/C.E.R.N/audio_settings.xml` und lädt sie beim nächsten Start
wieder. Wird das gespeicherte Gerät nicht gefunden, fällt die App automatisch
auf das Default-Gerät zurück.

Im Header (neben **TRIGGER**) sitzt der **AUDIO**-Button. Er öffnet den
Audio-Setup-Dialog (JUCE `AudioDeviceSelectorComponent`):

- **Output**: Soundkarte / Ausgabegerät manuell auswählen
- **Active output channels**: Stereo-Paare aktivieren/deaktivieren
- **Sample rate** und **Audio buffer size**: immer sichtbar

Gespeichert wird beim Schliessen des Dialogs und beim Beenden der App.
Der Button ist im VST3-Plugin nicht vorhanden (der Host stellt das Gerät).

### ASIO (Windows)

JUCE bringt die Steinberg-ASIO-Header bereits mit, ein externer SDK ist nicht
nötig. Aktivieren mit:

```bash
cmake -B build -DCERN_WITH_ASIO=ON -DCMAKE_BUILD_TYPE=Release
cmake --build build -j$(nproc) --target CERNStandalone
```

Danach erscheint im Dialog **Audio device type: ASIO**. Es gelten die
Lizenzbedingungen von Steinberg. Standard ist `OFF`.

### JACK (Linux)

Optional (weak-linked, keine JACK-dev-Libraries nötig):

```bash
cmake -B build -DCERN_WITH_JACK=ON -DCMAKE_BUILD_TYPE=Release
```

Dann steht **Audio device type: ALSA** und **JACK** zur Auswahl.

## DSP-Signalfluss

```
Trigger
  → [Pitch-Envelope: hoch→tief Glissando]
  → [Membrane-Modalbank (Bessel, f1≈0.766·T/(σ·D))]
        → Nonlinear Tension Modulation (Pitch-Glide)
        → [Tube/Hollow-Resonator (f ~ 1/D, Hohlkörper-Kopplung)]
  → [Sub-Oszillator (Sine/Triangle/Square, 40–60 Hz)]
  → [Click/Beater-Transient (Noise+Sine, Beater-Material)]
  → [Drive: Tube/Diode/Fuzz/WaveFold/Alias]
  → [3-Band EQ: Sub / Punch / Air]
  → [Rumble High-Pass (20–40 Hz Cut)]
  → [Compressor + Makeup]
  → [Room / Late Rumble (Hollow Industrial)]
  → [HIGGS FIELD: 5-Band Yukawa-Kopplung + Boson-Peak 125.35 Hz + λ-Sättigung]
  → [Reverb / Material (FDN, Material-EQ, Pre-Delay, RT60)]
  → [Output: Gain/Pan/Width/Mono-Below/Clip/Limiter] → Stereo Out
```

## Bekannte Design-Entscheidungen / Grenzen

Diese Punkte sind **bewusst so gebaut** (oder bewusst noch offen) und keine Bugs.
Sie stehen hier, damit sie bei Audits nicht erneut als Fehler gemeldet werden.
Der Bearbeitungsstand der offenen Punkte steht in `TODO.md` (Abschnitt
„Bugfix-Backlog“).

- **Fan-in ist gepuffert, nicht ladend.** Jeder Eingang summiert alle
  verbundenen Quellen (`sumTo` in `KickSynthesisEngine`) mit reiner Addition,
  ohne Pegelkompensation. Es gibt **kein Summing-Headroom** und keine
  Normierung: Mehr Kabel an einem Eingang erhöhen den Pegel (Eurorack-Prinzip,
  Clipping erst in Clip/Limiter der Output-Stufe). Zwei Kabel an `OutputIn`
  ergeben also nicht den halben, sondern den doppelten Pegel.
- **Drive ist der Pflicht-Summing-Knoten für Membrane/Sub/Click.** Der
  Default-Patch führt Membrane, Tube, Sub und Click in `DriveIn` zusammen;
  ohne Drive-Kabel kommt am Rest der Kette nichts an. Das ist die
  Werksverschaltung, kein Zwang: Der Graph erlaubt jedes andere Routing.
- **Tube ist ein Parallelzweig.** Der Default-Patch verbindet
  `MembraneOut → TubeIn` **und** `MembraneOut → DriveIn`; Tube wird auf Drive
  summiert, nicht in Serie dahinter gehängt. Tube kann daher nur additiv
  färben, nicht das trockene Membransignal ersetzen.
- **Room- und Reverb-Returns sind wet-only.** Beide Module geben ausschließlich
  ihr Nasssignal aus (`RoomOut = delayed * level`, `ReverbOut = Wet`). Das
  Trockensignal muss im Patch separat geführt werden; klassische
  Insert-Verschaltung (Dry+Wet im Modul) gibt es nicht. Higgs gibt nach dem Fix
  ebenfalls nur den Wet-Anteil aus (Dry läuft am Higgs vorbei).
- **Room: 4 Taps statt einer festen 80-ms-Verzögerung.** Die Stufe nutzt vier
  Taps (61/79/97/113 ms) mit R-Taps +8,3 % für ein dekorelliertes Stereobild und
  einen DC-Blocker (~2 Hz) im Feedback. `Room Decay` skaliert das Feedback
  (RT60-korrekt). `roomSync` ist als Engine-Enum vorhanden, aber noch nicht als
  APVTS-Parameter/UI freigeschaltet (SND-093, siehe TODO.md).
- **Kein DSP-Reset beim Preset-/State-Laden.** `setStateInformation` und der
  Preset-Manager schreiben nur Parameter und Patches; `engine.reset()` läuft
  ausschließlich in `prepareToPlay`. Laufende Tails (Room/Reverb/Higgs) klingen
  beim Preset-Wechsel also weiter. Das ist bewusst so: ein Reset mitten im
  Betrieb würde Tails abschneiden und Klicks erzeugen. Wer einen harten
  Preset-Wechsel braucht, muss vorher stoppen/neu triggern.
- **CompOut ist konstruktionsbedingt mono.** Der Compressor summiert L/R zu
  einem Mono-Signal und legt dieses auf beide Kanäle von `CompOut`. Nach dem
  Compressor ist die Stereobreite weg; Breite entsteht erst wieder in der
  Output-Stufe (`Output Width`, `Mono Below`). Das ist für einen Kick gewollt,
  macht den Compressor aber als Stereo-Insert unbrauchbar.
- **`reverbMaterial` ist klanglich differenziert.** Material 0 („None“) umgeht
  den Reverb; Wood/Metal/Plastic/Concrete/Glass/Stone haben eigene
  RT60-Multiplikatoren, Dämpfungs-/LowCut-/HighCut-Kurven und einen
  materialabhängigen Pre-FDN-Tonfilter. Die Unterschiede sind durch
  `reverb_material_distinctness_tail_rt60_filter` und
  `material_reverb_materials_differ_rt60_and_tone` abgesichert.
- **Sequencer-Patterns B–D und Steps 17–64 sind derzeit nicht spielbar.**
  `stepVel`, `stepGate` und `stepPitch` existieren nur für die 16 sichtbaren
  Steps (Pattern A). Ratchet/Flam/Probability/Length sind dagegen für
  4×64 Slots vorhanden. `sequencerSteps` erlaubt 1–64, aber Steps 17–64 haben
  kein Gate und bleiben stumm; Pattern B–D ebenfalls. Das ist eine bekannte
  Grenze (SND-145/146), kein stiller Fehler — siehe `TODO.md`.
- **Parameter-CV-Persistenz ist index- und ID-basiert.** `<ParameterPatch>`
  speichert Verbindungen als `source`/`target`-Indizes plus die Parameter-IDs;
  beim Laden wird über die ID auf den aktuellen Index aufgelöst. Das erlaubt
  Migration, wenn sich die Reihenfolge in `ParameterIDs::getAllIDs()` ändert,
  setzt aber voraus, dass IDs stabil bleiben (sie werden nie umbenannt).
  Unbekannte IDs werden verworfen, nicht auf einen anderen Parameter gemappt.
- **CV-Ziele sind alle 1229 APVTS-Parameter.** Die Parameter-CV-Ansicht listet
  jede ID aus `getAllIDs()`; die 12 Modul-Ein/Aus-Bool-Parameter stehen in der
  Gruppe `MODULE ENABLE` (plus `higgsEnabled` in `HIGGS FIELD`). Sie sind gültige
  CV-Quellen/-Ziele, werden aber als Bool auf 0/1 geklemmt — CV auf einem
  Enable-Parameter schaltet also hart, es gibt keine Hysterese.
- **Werte-Anzeige in der Parameter-CV-Liste** nutzt den JUCE-Text-Formatter
  (`getText`), also Choice-Namen („Wood“, „Metal“, …) und „On“/„Off“. Nur wenn
  ein Parameter keinen Formatter liefert, erscheint der rohe Zahlenwert.
- **Doppelte Kabel / Mehrfach-Verbindungen** sind pro Source→Target-Paar
  dedupliziert; dasselbe Paar kann nicht zweimal gesteckt werden. Es gibt keine
  Kabel-Multiplikation (kein „2× stecken = 2× Pegel“).

## Roadmap (offen)

- CLAP + AU (macOS) + Standalone-Export
- Sample-basierte Click-Layer (Hybrid)
- Thumbnail-Waveform im Preset-Browser
- Step-Editor-UI für Ratchet/Flam/Probability/Length sowie Steps 17–64/Patterns B–D
- ARA-Unterstützung
- MPE / Polyphonie (aktuell monofon)

## Sound-Review 2026-11-12

Die vollständige, priorisierte Sound-Review (DSP, Presets, GUI, Doku) liegt in
[`SOUND_REVIEW.md`](SOUND_REVIEW.md): **327 Findings** (SND-001…SND-327).
Alle 327 Findings sind in `TODO.md` im Abschnitt „Sound-Review 2026-11-12“
getrackt; die fünf Dokumentations-Findings SND-186…SND-190 sind mit diesem
Stand erledigt.

## Sound-Engineer-Review 2026-11-13 (Vollreview, umgesetzt)

Reviewer-Rolle: Tontechnik · PA-Technik · Techno-Produzent · Modular-Synth.
Vollständiger Report: `/tmp/user/1000/opencode/review-vst-full.md`.
Es wurden **alle** Befunde umgesetzt (C1–C4, H1–H7, M1–M16, N1–N4).

### Kritisch – Grundton & Gain-Staging

| Befund | Problem | Umsetzung |
| ------ | ------- | --------- |
| **C1** Pitch-Envelope lief **nach oben** | `exp2(depth·env)` ⇒ Tonfall **auf** +24 st, „zu hochgepatcht" | Tonfall **nach unten**: `ratio = 1 + (2^depth − 1)·sweep`, `sweep` 1→0 exponentiell, `depth` 0–3 Oktaven (Default 1,5 ⇒ +18 st Starttonfall) |
| **C2** EQ-Sub war funktional tot | 2-poliger LP auf f0, **parallel** addiert (−90° Phase) + Makeup ⇒ +3 dB ⇒ **−0,69 dB** | RBJ-**Low-Shelf** (S = 1,0) bei f0, direkt auf das Direktsignal, Makeup entfällt, 0 dB **bit-transparent**, +3 dB ⇒ ehrlich **+3 dB** |
| **C3** Output-Clip war ein Verstärker | `tanh(k·x)·1/tanh(k)` ⇒ Kleinsignal **+7,7 dB** (`outputClip 0,35`) | Slope-1-Softclip `y = x/√(1+(x/ceiling)²)`, `outputClip` steuert nur noch das **Ceiling** (1,0 → 0,5), Default **0** |
| **C4** Drive machte lauter | gesättigtes Band mit Slope **4,2** ⇒ +12,5 dB (gesamt +7,75 dB) | Alle Charaktere (Tube/Diode, WaveFold, Alias) auf **Slope 1** (`tanh(k·x)/k`), Drive färbt, verstärkt nicht |

### Hoch

| Befund | Umsetzung |
| ------ | --------- |
| **H1** Membrane: Fundamental war der **leiseste** Modus | `weight0 = 1.2 − 0.6·massNorm` ⇒ Fundamental dominiert Modus 1 (73,7 Hz); `membraneMass` Default 0,8 |
| **H2** Body war ein **zweiter Sub-Sinus** auf f0 | Body = 2-poliger RBJ-Bandpass (Q ≈ 1,4) bei `f0·bodyTuning` + 25 % Sinus ⇒ **Shell-Band 92,5 Hz** (`bodyTuning` 2,0, `bodyLevel` 0,35) |
| **H3** Oversampling aus ⇒ Aliasing im Anschlag-Band | `oversampleEnabled` Default **an**; Latenz wird dem Host gemeldet (`setLatencySamples`) |
| **H4** Limiter sah keine Inter-Sample-Peaks | True-Peak-Detektor mit **4× Oversampling**, stereo-linked; `getTruePeakDb()` im GUI |
| **H5** Room-Wet enthielt den **vollen Sub** (Kammfilter-Nullen bei 27,8/55,6/83,3 Hz) | Tap-Summe auf 1,0 normiert, **70-Hz-HP** im Send, Taps 41/53/67/79 ms |
| **H6** Patch ohne Weg zum Output = Stille | PatchView meldet „NO PATH TO OUTPUT" |
| **H7** Preset-Lautheit handkalibriert (7,1 dB Spanne), Init ≠ Presets | einheitlicher Output-Pfad, automatische Kalibrierung, Init an Presets angeglichen |

### Mittel / Niedrig

**M1** Grumble mono-pegelneutral (Pan-LFO moduliert die Mono-Summe nicht mehr) ·
**M2** MaterialRoom-Spread ±3 % → ±0,3 % · **M3** globales `duckDepth` wirkt nur noch
auf die **FX** (Room/Higgs/Reverb/MaterialRoom) – der trockene Kick duckt sich nicht
mehr selbst · **M4** CV-Amount-Default 1,0 → **0,25**, Preset-Kabel auf echte Quellen,
5 Makro-Kabel pro Preset · **M5** `clickNoiseMix` als Parameter, Tick folgt dem
Pitch-Sweep (30 %) · **M6** Tube-Ausgang ohne ×4 (Slope 1) · **M7** `hollowMetallic`
Slope 1 · **M8** Kompressor-Attack 15 → 6 ms, Release 120 → 240 ms · **M9**
Modus-Tabelle auf **16 Bessel-Modi** erweitert (Membraninhalt bis 5,83·f0 ≈ 270 Hz) ·
**M10** Layering-L1 kürzer (`subDecay × 0,9`) · **M11** README-Parametertabellen werden
aus dem Quelltext **generiert** (`Scripts/gen_param_docs.py`,
`Scripts/update_readme_params.py`) · **M12** True-Peak, Limiter-GR und Clip-GR im
Meter-Strip · **M13** `compMix`/`compAutoMakeup` in der Sektion „Comp" ·
**M14** Sequencer-Slots 64 → **16** (768 zuvor tote Parameter entfernt, 1311 → 543) ·
**M15** Nichtlinearität pro Mode, amplitudengetragen · **M16** Sub startet mit
`velocity` (nicht 0,8×) · **N3** Clip-Meter wird pro Block zurückgesetzt ·
**N4** dokumentiert.

### Abweichungen von der Review-Empfehlung (bewusst)

- **C2:** Der RBJ-Low-Shelf liefert am Eckpunkt f0 konstruktiv nur die Hälfte des
  asymptotischen Gains. Der interne Gain wird deshalb verdoppelt, damit `eqSubGain`
  **bei f0** ehrlich ist (gemessen: +2,99 dB bei +3 dB, 0 dB bit-transparent).
- **C3:** Das Limiter-Ceiling ist *relativ* zum Clip-Ceiling definiert
  (`clipCeiling · 10^(−3·amount/20)`), sonst könnte der Limiter bei aktivem Clipper
  nie eingreifen.
- **N1:** `OversamplingHelper.h` ist **nicht** tot – dort liegt `StreamingTwoX`, das
  der Drive-/Clip-Pfad nutzt. Die Datei bleibt.
- **N2:** `roomSync` ist öffentliche Engine-API (`setRoomSync`), aktuell fest auf
  „frei"; nicht entfernt, sondern dokumentiert.
- **H4:** Der True-Peak-Detektor arbeitet mit kubischer Interpolation (4 Punkte je
  Sample) ohne Lookahead – kein zusätzlicher Sample Latenz für alle Hosts.

### Messtechnik

`Source/Lab/LabRenderer.cpp` (Target `CERNLab`) rendert alle Factory-Presets als WAV und
misst LUFS (stereo), LUFS (Mono-Summe), Peak, Crest-Faktor sowie die Loudness-Differenz
mit/ohne Layering:

```
./build/Source/CERNLab /tmp/lab 100 8     # Ordner, Layering-Amount %, Dauer (s)
```

## Lizenz

Interne Entwicklung, ScalyxSoft 2026.

## Build & Package (macOS/Windows/Linux)

Plattformübergreifende Build-/Packaging-Vorbereitung (CMake, CI, Skripte, Installer-Vorlagen) ist im Repo enthalten. **Reale macOS-/Windows-Builds, Codesignatur und Notarisierung** müssen auf den jeweiligen Zielplattformen ausgeführt werden.

- Detaillierte Anleitung: [`docs/BUILD_CROSS_PLATFORM.md`](docs/BUILD_CROSS_PLATFORM.md)
- Packaging: [`packaging/`](packaging/) (`macOS/`, `Windows/`, `Linux/`)
- Skripte: [`scripts/package-vst3.sh`](scripts/package-vst3.sh), [`scripts/package-standalone.sh`](scripts/package-standalone.sh)
- CI: [`.github/workflows/build-cross-platform.yml`](.github/workflows/build-cross-platform.yml)

### Kurzüberblick

```bash
# Configure (Release)
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release

# Linux: VST3 + Standalone
cmake --build build --target CERN_VST3 --parallel
cmake --build build --target CERNStandalone --parallel

# macOS (auf macOS): Universal-Binary + Deployment Target
cmake -S . -B build-mac -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_OSX_DEPLOYMENT_TARGET=11.0 -DCMAKE_OSX_ARCHITECTURES="arm64;x86_64"

# Windows (auf Windows, MSVC/Ninja): optional ASIO für Standalone
cmake -S . -B build-win -G Ninja -DCMAKE_BUILD_TYPE=Release -DCERN_WITH_ASIO=ON
```

Optionen: `CERN_WITH_JACK=ON` (Linux, Standalone), `CERN_WITH_ASIO=ON` (Windows, Standalone). Siehe `docs/BUILD_CROSS_PLATFORM.md` für Dependencies, Install-Pfade, Signierung/Notarisierung, Installer und Troubleshooting.

## Rückseite / Patch-Bay (modulares Re-Patching)

Über den Header-Button **RUECKSEITE** erreichst du die hardware-artige Rückwand des Synths (FRONT schaltet zurück). Die Ansicht zeigt alle Module als 3D/2.5D-Panele mit Schrauben, Parameternamen pro Modul und beschrifteten IN/OUT-Buchsen:

- **MEMBRANE** (OUT), **TUBE** (IN/OUT), **SUB** (OUT), **CLICK** (OUT)
- **DRIVE** (IN/OUT), **EQUALIZER** (IN/OUT), **COMPRESSOR** (IN/OUT)
- **ROOM** (IN/OUT), **HIGGS FIELD** (IN/OUT, Stereo), **REVERB** (IN/OUT, Stereo),
  **MATERIAL ROOM** (IN KICK/IN RUMBLE/OUT, Stereo), **OUTPUT** (IN/OUT, Stereo)

**Bedienung:** Jack klicken oder ziehen, um eine Verbindung zu stecken (Out→In, Richtung wird automatisch erkannt). Mehrere Kabel an einem Eingang werden summiert (Eurorack-Stil). Rechtsklick auf eine Buchse oder ein Kabel entfernt Verbindungen. **STANDARD PATCH** stellt die Werks-Verschaltung wieder her, **ALLE LOESCHEN** entfernt alle Kabel.

**Audio-Impact:** Das Routing läuft graph-basiert in der DSP-Engine (`PatchGraph`, topologische Reihenfolge, Zyklus-Schutz). Jedes Umstecken verändert den Signalfluss sofort hörbar (z. B. keine Verbindung zum OUTPUT → Stille, Quellen direkt in den Ausgang, FX parallel/seriell neu verdrahtet).

**Parameter-Patch (CV):** Die Tabs oben (AUDIO ROUTING / PARAMETER CV) schalten zwischen Audio-Routing und parameterbezogener CV-Verkabelung. Die Parameter-Ansicht listet alle 1229 APVTS-Parameter als scrollbare Zeilen mit `OUT` und `IN` (inkl. der 12 Modul-Ein/Aus-Schalter in `MODULE ENABLE`; `higgsEnabled` steht in `HIGGS FIELD`). Choice-/Bool-Parameter zeigen Text („Wood“, „On“/„Off“), keine rohen Indizes. Ein Kabel von einem `OUT` zu einem `IN` summiert bipolar auf den Basis-Parameter. Daraus resultierende effektive Parameterwerte werden pro Block in der Engine ausgewertet.

**Presets:** Es gibt 7 Factory-Presets. Die drei Presets `CV Pulse Kick`, `Higgs Mod Kick` und `Parameter Patch Kick` enthalten vollständige Higgs-Feld-Einstellungen und fertige Parameter-CV-Verbindungen (z. B. Body-Level → Drive-Mix, Sub-Level → Room-Rumble, Higgs-Stärke → Boson-Q).

Zusätzlich kann für jeden Parameter separat `SRC` und `DST` ein- oder ausgeschaltet werden: `SRC` steuert, ob ein Parameter als CV-Quelle arbeitet, `DST`, ob eingehende CV auf den Parameter wirkt. Schalter und Kabel sind persistent im State/Preset.

**Persistenz:** Audio-Patch und Parameter-Patch werden automatisch zusammen mit Parametern und Host-State gespeichert und geladen: als `<Patch>`- bzw. `<ParameterPatch>`-Knoten im VST3/Standalone-State sowie in User-Preset-XML. Im Standalone liegt der Gesamt-State zusätzlich unter `~/.config/C.E.R.N/standalone_state.xml` und wird beim Start geladen bzw. beim sauberen Beenden geschrieben. Factory-Presets stellen den Werks-Patch wieder her; User-Presets und Host-Projekte behalten den benutzerdefinierten Patch. Alte/korrupte States mit mehreren Patch-Knoten werden tolerant migriert (der letzte/aktuelle Patch zählt).

**Modul-Ein/Aus:** Jedes Modul hat auf Front- und Rückseite einen `ON`-Schalter (AudioParameterBool, default on). Ist ein Modul aus, steuert es kein Signal mehr bei: Quellen liefern 0, serielle Stufen werden bypassed, Room/Reverb liefern 0, Higgs/Reverb bypassen, Output muted. Der Schalter ist automatisierbar und wird mit Preset/State persistiert.

**Kabelfarben:** Audio-Kabel sind nach Quellmodul farbkodiert (Membrane rot, Tube orange, Sub gelb, Click magenta, Drive dunkelrot, EQ amber, Compressor dunkelrot, Room teal, Higgs violett, Reverb blau, Output weiß); deaktivierte Module/Kabel werden grau. Parameter-CV-Kabel erhalten Farben nach Quellparameter-Gruppe (z. B. Membrane rot, Tube orange, Sub gelb, Drive orange, EQ amber, Compressor dunkelrot, Room teal, Reverb blau, Higgs violett, Sequencer grün/cyan, Modul-Ein/Aus neutral amber-grau). Selektierte Kabel werden weiß hervorgehoben, deaktivierte Quellen/Ziele grau.
