# MATERIAL ROOM + SUB GRUMBLE — Spezifikation (2026-11-12)

## 1. Physikalische Materialtabelle

Quellen: Engineering Toolbox (Schallgeschwindigkeit Festkörper), Wikipedia Speed of Sound, Omni Calculator Acoustic Impedance, NASA Acoustic Absorption in Porous Materials, ScienceDirect Sound Absorption, Acoustic.ua Absorption Coefficients.
Für Faser/Stoff existiert keine einheitliche Longitudinalgeschwindigkeit; verwendet wird ein effektives poröses Medium (c_eff, Bulk-Dichte) und ein gemessener Absorptionsgrad.

| Material                  | c (m/s) | ρ (kg/m³) | Z = ρ·c (MRayl) | R = (Z−Z_air)/(Z+Z_air) | α = 1−R² | NRC/α_mid (Raum) | delay @ 8 m (ms) |
| ------------------------- | ------: | --------: | --------------: | ----------------------: | -------: | ---------------: | ---------------: |
| Water                     |    1480 |       998 |           1.477 |                 0.99944 |   0.0011 |             0.01 |             5.41 |
| Wood (hard, longitudinal) |    3960 |       700 |           2.772 |                 0.99970 |   0.0006 |             0.10 |             2.02 |
| Plastic (acrylic)         |    2750 |      1180 |           3.245 |                 0.99975 |   0.0005 |             0.05 |             2.91 |
| Steel                     |    5940 |      7850 |           46.64 |                 0.99998 |  0.00004 |             0.02 |             1.35 |
| Concrete                  |    3700 |      2400 |            8.88 |                 0.99991 |  0.00018 |             0.09 |             2.16 |
| Fabric (effective porous) |     250 |       100 |           0.025 |                  0.9675 |    0.064 |             0.60 |             32.0 |

Formeln:

- `Z = ρ * c` [Rayl = Pa·s/m]
- `R = (Z_m - Z_air)/(Z_m + Z_air)`, `Z_air ≈ 413 Rayl`
- `α_normal = 1 - R²`
- Raum-Dämpfung verwendet `α_room` (NRC/α_mid), nicht `α_normal`, weil poröse Materialien über viskose Reibung absorbieren.
- Moden/Delay: `t_delay = L / c`; Raumgröße L in Metern.
- Hinweis (MR-006): Das Modell ist **phänomenologisch** (Material-Charakter als Klangmetapher), keine raumakustische Simulation; α_room ist ein Sound-Design-Parameter, keine Messung.
- **MR-004 (Water/Absorption, korrigiert):** α_room bleibt laut Materialtabelle unverändert. Der frühere Hinweis „bis +17 dB im 20–100-Hz-Bereich“ gilt **nicht mehr**: Der Wet-Return passiert einen LowCut bei 100 Hz (2. Ordnung, §2), damit er den Sub-Druck des Dry-Signals weder auslöscht noch aufbläht. Wasser bleibt das „raumhafteste“ Material (längster Tail, siehe MR-092-Test), der Sub-Bereich kommt aber ausschließlich aus dem Dry-Signal. Für hohe Mix-Werte bleibt Gain-Staging trotzdem sinnvoll (Wet ist additiv).
- **MR-003 (Steel):** Steel wirkt als **metallischer Resonator**, nicht als „Hall/Raum“ im klassischen Sinn; Fabric und Water sind die raumhaftigeren Materialien.
- **MR-040/041/042/005 (bewusst unverändert, dokumentiert):** keine Änderung an der Materialtabelle/α-Werten, kein zusätzlicher Physik-Claim, kein Dry-Eingriff, keine implizite Normalisierung des Fan-ins; Verhalten bleibt wie spezifiziert.

## 2. DSP-Mapping

- **Reflexions-/Feedback-Gain**: `feedback = clamp(0.55 + 0.44*(1 - α_room), 0.55, 0.995)`.
  - Water 0.985, Wood 0.946, Plastic 0.968, Steel 0.981, Concrete 0.950, Fabric 0.726.
- **Tap-Delays**: 4 Taps mit Verhältnissen `[1.0, 1.37, 1.79, 2.21] * (L / c)`, L = `materialRoomSize` (1..30 m, Default 8).
- **Stereo**: L-Taps und R-Taps mit ±3 % Versatz; `materialRoomWidth` mischt Mid/Side des Wet-Returns.
  - **MR-013 (Width-Cap):** `width <= 0.5` → Side-Gain `2*width` (bei 0.5 neutral = 1.0). `width > 0.5` → Side-Gain `1.0 + (width - 0.5)*1.5`, gekappt bei **max 1.75** (`width = 1.0`). Effektiv: 0.5 = neutral, 1.0 = breiter, aber begrenzt.
- **Damping**: One-Pole-LP im Feedback, `fc = clamp(12000 - 9000*α_room, 1500, 18000) * (1 + 0.35*ρ/7850)` Hz (Dichte hellt dichte/metallische Materialien auf; Fabric bleibt dunkel), begrenzt auf 0.45*fs.
- **LowCut/DC**: 10 Hz DC-Blocker plus 2nd-Order-Highpass (100 Hz) im Wet-Pfad (kein Sub-Miss, kein Dry-Cut). Der Cut liegt bewusst bei 100 Hz statt 28 Hz: er schützt den 20–100-Hz-Druck des Dry-Signals vor Antiphase (MR-090).
- **Layer**: choice Kick / Rumble / Both. Zwei Inputs; nur die gewählten Layer werden in den Wet-Tap gespeist.
  - **MR-095:** Layer **Rumble** hängt vom Eingang `RoomOut`/der ROOM-Stufe ab; bei ausgeschaltetem ROOM ist der Rumble-Eingang stumm (kein Tail → kein MaterialRoom-Rumble-Wet aus diesem Pfad).
- **No pressure loss**: Modul liefert **nur Wet**; Dry bleibt im Bus (CompOut/RoomOut→OutputIn). Kein Dry-Abzug, keine Phaseninversion, kein LowCut auf dem Dry.
- **Grumble**: Sub-Oszillator 20–120 Hz, Waveform Sine/Triangle/Square; Envelope folgt Kick+Rumble-Peak **unabhängig vom Layer-Modus** (Grumble reagiert auch bei `layer=Rumble` auf den Kick), Attack ~5 ms, Decay `grumbleDecay` 0.1–2 s; Pan-LFO um die Mitte A↔B mit `grumbleRate` 0.05–8 Hz, Tiefe `grumbleDepth` (**0 = Mitte A/B**, 1 = voller Schwung), Pan-Waveform Sine/Triangle/Square/Saw, Pan geslewtes (**≈5 ms**, MR-031) gegen Klicks bei Square/Saw; Pegel `grumbleLevel`. Der Grumble ist vom Room-Mix unabhängig und wird additiv dem Modul-Output zugemischt, kein Dry-Eingriff.
  - **MR-030:** konstante Pan-Leistung (Equal-Power: L/R via cos/sin des Pan-Winkels).
  - **MR-033:** **Safety-Hinweis:** `grumbleLevel > 0.7` kann im Sub-Bereich stark auftragen — Gain-Staging/Output-Limiter beachten.
  - **MR-035:** bei starkem Kick-Transienten (Env-Retrigger von < 20 % auf ≥ 80 %) wird die Sub-Oszillator-Phase deterministisch per LCG neu gezogen (fester Seed, nicht pro Sample).
  - **MR-043:** **Size-Sweep** (Modulation/Automation von `materialRoomSize`) erzeugt einen metallischen Comb-Sweep/Wub-Charakter (Tap-Delays ~ L/c wandern).
  - **MR-052:** Pan **A/B** sind die Endpunkte; **Depth 0** = Mitte zwischen A und B (kein Schwung).

## 3. Parameter (alle ans Ende, Indizes erhalten)

| ID                     | Typ                                             | Range             | Default  |
| ---------------------- | ----------------------------------------------- | ----------------- | -------- |
| `materialRoomEnabled`  | Bool                                            | –                 | true     |
| `materialRoomMaterial` | Choice Water/Wood/Plastic/Steel/Concrete/Fabric | 6                 | 1 (Wood) |
| `materialRoomSize`     | Float                                           | 1..30 m, step 0.1 | 8.0      |
| `materialRoomMix`      | Float                                           | 0..1              | 0.0      |
| `materialRoomLayer`    | Choice Kick/Rumble/Both                         | 3                 | 2 (Both) |
| `materialRoomWidth`    | Float                                           | 0..1              | 0.5      |
| `grumbleLevel`         | Float                                           | 0..1              | 0.0      |
| `grumbleFreq`          | Float                                           | 20..120 Hz        | 45.0     |
| `grumbleRate`          | Float                                           | 0.05..8 Hz        | 0.5      |
| `grumbleDepth`         | Float                                           | 0..1              | 0.5      |
| `grumblePanA`          | Float                                           | -1..1             | -0.5     |
| `grumblePanB`          | Float                                           | -1..1             | 0.5      |
| `grumbleWave`          | Choice Sine/Triangle/Square                     | 3                 | 0        |
| `grumblePanWave`       | Choice Sine/Triangle/Square/Saw                 | 4                 | 0        |
| `grumbleDecay`         | Float                                           | 0.1..2 s          | 0.5      |

## 4. PatchGraph-Integration

Neue Jacks **ans Ende** (Indizes erhalten):

- `MaterialRoomOut` (OUT, stereo)
- `MaterialRoomKickIn` (IN, stereo)
- `MaterialRoomRumbleIn` (IN, stereo)

Neues Modul `ModuleIndex::MaterialRoom` ans Ende (vor ModuleCount):

- id `materialroom`, display `MATERIAL ROOM`, params `MAT SIZE MIX LAYER WIDTH GRUMB P-RATE P-DEPTH WAVE`, row 2, column 3.
- Enable: `materialRoomEnabled`.

Default-Patch (3 neue Kabel, gesamt 17):

- `CompOut -> MaterialRoomKickIn`
- `RoomOut -> MaterialRoomRumbleIn`
- `MaterialRoomOut -> OutputIn`

Engine:

- `case ModuleIndex::MaterialRoom`: `kick = sumTo(KickIn)`, `rumble = sumTo(RumbleIn)`, `wet = materialRoom.process(kick, rumble)`, `jacks[MaterialRoomOut] = wet`.
- Bei `materialRoomEnabled == false`: `{0,0}` (Wet mute, Dry bleibt).
- Reset-on-enable.

UI:

- Neue Sektion `MATERIAL ROOM` in `MainComponent` mit Material/Size/Mix/Layer/Width + Grumble-Parametern.
- `PatchView`: Modulpanel + PowerSwitch + Kabel; `moduleEnableId`-Case; Kabel-Farbe für MaterialRoom.
- `ParameterPatchView`: Gruppen `MATERIAL ROOM` und `SUB GRUMBLE`.
- `ParamKnob`/Display: Materialname, Layer-Name, Pan-Werte.

## 5. Akzeptanz

- Materialwahl ändert Delay-Zeiten, Feedback/Decay, Damping und Stereo-Taps messbar gemäß Tabelle.
- `materialRoomMix=0` und `grumbleLevel=0` = exakt keine Änderung; Dry-Pegel unverändert. `grumbleLevel>0` bei `mix=0` erzeugt ausschließlich Grumble (Room determiniert stumm).
- Layer Kick/Rumble/Both routet nur die gewählten Inputs; Dry bleibt voll.
- Grumble: hörbarer Sub 20–120 Hz, Pan-Schwung von A nach B (Depth skaliert den Schwung), Waveform wirkt, Level 0 = aus, unabhängig vom Room-Mix.
- Patch-Kabel, PowerSwitch, Persistenz (Host/User/Standalone) funktionieren; Default-Patch 17 Kabel.
- Kein Druckverlust: A/B mit/ohne Modul bei gleichem Dry-Pegel; Wet additiv.
- Build + Tests grün; danach Sound-Engineer/Techno-Review.

## 6. Review-Traceability (MR-001…MR-096, dokumentierter Stand)

- **MR-001/004/013/031/035 (Code/Doku):** Materialtabelle Concrete/Wood/Water gemäß §1; Pan-Slew (~5 ms) gegen Klicks bei Square/Saw; Width-Cap/Neutralität (Mid/Side, 0.5 neutral, Seitenfaktor max 1.75); Grumble-Phase-Retrigger deterministisch per LCG bei starkem Transient (Env <20 % → ≥80 %).
- **MR-002/003/006/020/030/033/043/050/051/052/053/054/056/095 (Doku/Tooltips/Farben):** Rack/Parameter-Gruppen in Teal `0xff0ca678` (Audio-Modul, CV-Gruppen Material Room/Sub Grumble); Hover-Hints in der Patch-Ansicht (IN KICK = CompOut Pre-Output, IN RUMBLE = RoomOut/bei ROOM-Off stumm, OUT = Wet-only+Grumble, Material-Hinweise Steel/Water/SIZE).
- **MR-040/041/042/005:** unverändert belassen, Verhalten wie in dieser Spezifikation/Implementierung dokumentiert (keine API-/Patch-/Index-Änderung).
- **MR-060/061:** nur in `MATERIAL_ROOM_PRESET_PLAN.md` dokumentiert (Werte + Messprotokoll-Vorgabe); `PresetManager.cpp` unverändert.
- **MR-090–MR-094, MR-096:** automatisierte Tests in `Tests/test_main.cpp` (Low-End-RMS 20–100 Hz, mix=0/grumble=0 neutral/bit-identisch im Wet-Pfad, RT60-/Decay-Ordering, Pan-Swing, Additivität/Mix-Skalierung, Default 17 Kabel).
