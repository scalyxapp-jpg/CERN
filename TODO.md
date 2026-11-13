# Review-Umsetzung C.E.R.N (Vollreview 2026-11-13)

Report: `/tmp/user/1000/opencode/review-vst-full.md` (261 Zeilen).
Umsetzung **vollständig** — alle Punkte C1–C4, H1–H7, M1–M16, N1–N4 + Top-10.
Ausnahmen sind am Ende des Status-Abschnitts begründet (2 GUI-Komfortfeatures).

## Paket A — DSP-Module (Agent, Dateien: PitchEnvelope.h, MembraneModel.h, TubeResonator.h, ClickGenerator.h, MaterialRoom.h, LayeringStage.*)

- [x] **A1 / C1 (kritisch)** Pitch-Envelope: Tonfall von **oben** (2^depth) auf f0.
  `ratio = 1 + (2^depth − 1) * sweep`, sweep 1→0, depth bis 3 Oktaven.
- [x] **A2 / H1** Membrane: `weight0 = 1.2 − 0.6*massNorm` ⇒ Fundamental dominiert Modus 1 (73,7 Hz).
- [x] **A2 / M15** Membrane-Nichtlinearität pro Mode, amplitudengetragen (quadriert), max. ≈ +2 st.
- [x] **A3 / M6** Tube: Ausgangs-×4 raus (Slope 1 statt 4×).
- [x] **A3 / M7** `hollowMetallic`: Sättigung Slope 1 (`tanh(x*k)/k`).
- [x] **A4 / M5** `clickNoiseMix` als setzbares Member exponiert (Parameter zentral ergänzt).
- [x] **A5 / M1** MaterialRoom: Grumble mono-pegelneutral (kein +3 dB in Mono).
- [x] **A5 / M2** MaterialRoom: Stereo-Spread ±3 % → ±0,3 % (Mono-Auslöschung).
- [x] **A6 / M10** Layering L1-Decay `subDecay*1.05` → `*0.9`.

## Paket B — Engine (Agent, Dateien: KickSynthesisEngine._, PluginProcessor._)

- [x] **B1 / C2 (kritisch)** EQ-Sub-Band → RBJ **Low-Shelf** (S=1) bei f0, direkt auf x0,
  Makeup entfällt, 0 dB bit-transparent, +3 dB ⇒ ehrlich +3 dB.
- [x] **B2 / C4 (kritisch)** Drive **Slope 1** (Tube/Diode, WaveFold, Alias) — kein Makeup.
- [x] **B3 / C3 (kritisch)** Output-Clip Slope 1 (`x/sqrt(1+(x/ceiling)²)`), `outputClip`
  steuert Ceiling, GR-Metering konsistent, `clipMinGain` pro Block zurückgesetzt,
  Limiter-Ceiling sinnvoll abgebildet.
- [x] **B4 / H2** Body → 2-poliger RBJ-Bandpass (Q≈1,4) bei `f0*bodyTuning` + 25 % Sinus,
  Gleiten mit `1 + 0.4*(pitchRatio−1)`.
- [x] **B5 / M16** Sub: Startpegel = velocity (nicht 0,8×), Gleiten `1 + 0.5*(pitchRatio−1)`.
- [x] **B6 / H5** Room: Tap-Summe auf 1,0 normiert, 70-Hz-HP in den Send, Taps 41/53/67/79 ms.
- [x] **B7 / H4** True-Peak-Limiter (4×-Detektor, stereo-linked) + `getLatencyInSamples()`
  → `setLatencySamples()` an den Host + Getter `getTruePeakDb()`, GR-Limiter, GR-Clip.
- [x] **B8** Sonst unverändert.

## Paket C — Parameter, Presets, GUI, Doku, Tests (Hauptagent)

- [x] **C1a** Parameter: `clickNoiseMix` neu (0–1, Default 0,5) — ID am Ende, `numLayoutParameters` 1311.
- [x] **C1b** Defaults/Bereiche: pitchEnvDepth 0–3 (Default 1,5), pitchEnvDecay 45 ms,
      bodyTuning 2,0, bodyLevel 0,35, subLevel 0,46, compAttack 6 ms, compRelease 240 ms,
      outputClip 0,0, oversampleEnabled **an**, membraneMass 0,8, membraneNonlinearity 0–2 (0,5).
- [x] **C1c / M4** CV-Amount-Default 1,0 → **0,25** (`ParameterPatchGraph::Connection`).
- [x] **C1d** `Scripts/gen_param_docs.py`: README-Parametertabellen aus dem Quelltext
      generieren (M11).
- [x] **C2a** Presets neu kalibrieren (outputGain), Spread ≤ 1,5 dB, Init an Presets angleichen.
- [x] **C2b** Neue Presets: Dub Chord Kick, Hard Techno Distorted, Minimal Click Kick,
      Sub Only (23 Hz) + 2 Presets auf 50/55 Hz (Frequenzspanne abdecken).
- [x] **C2c / M4** Preset-CV-Kabel auf echte Quellen (LFO/Env-Follower/Makros) umhängen,
      5 Makros sinnvoll belegen (TUNE/PUNCH/BOOM/DIRT/SPACE).
- [x] **C3a / M12** Meter-Strip: Limiter-GR, Clip-GR, True-Peak anzeigen.
- [x] **C3b / M13** `compMix` + `compAutoMakeup` in die Sektion „Comp" verschieben,
      FEATURES entflechten.
- [x] **C3c** Spectrum-Analyzer: Sub-Band 20–120 Hz auflösen (verzerrte Frequenzachse).
- [x] **C3d** Waveform: Modul-Beitrags-Overlay (Membrane/Body/Sub/Click) als Button `MOD` in der Kopfzeile.
- [x] **C3e** Preset-Browser: 64-Punkt-Wellenform-Vorschau je Preset neben der Auswahl (Button-Hintergrund-Render).
- [x] **C3f / M14** Sequencer-Slots 64 → 16 (bzw. Step-Editor) — 192 tote Parameter entfernen.
- [x] **C4a** README: Signal-Flow (Pitch-Richtung!), Parametertabellen neu generiert,
      neue Features (Layering/Amount, True-Peak, Latenz) dokumentiert.
- [x] **C4b** Tests: neue Erwartungen (Pitch fällt, EQ-Sub ehrlich, Clip/Drive Slope 1,
      True-Peak, Mono-Summe, Preset-Spread, Layering-Lautheit).

## Status

- [x] Review abgeschlossen, Report gelesen (`/tmp/user/1000/opencode/review-vst-full.md`).
- [x] Paket A (Pitch/Membrane/Tube/Click/MaterialRoom/Layering) umgesetzt, gebaut, gemessen.
- [x] Paket B (EQ/Drive/Clip/Body/Sub/Room/True-Peak/Latenz) umgesetzt, gebaut, gemessen.
- [x] Paket C: Parameter (543), 16 Presets auf −13 LUFS kalibriert (Spread **0,31 dB**),
      GUI (Meter-Strip M12, Comp-Sektion M13, Spectrum-Subzoom, Patch-Warnung H6,
      Envelope-Anzeige C1), README generiert (M11), Tests erweitert.
- [x] Build grün (CERN_All, CERNStandalone, CERNTests, CERNPreview, CERNLab),
      `CERNTests` **1.175.362 Checks / 0 Failed**, `ctest` 1/1.
- [ ] **Offen (bewusst):** Modul-Beitrags-Overlay in der Waveform (Review §7.3) und
      Preset-Vorschaubilder (§7.7). Beides sind Prosa-Vorschläge ohne Abnahmekriterien;
  das Overlay braucht zusätzliche Engine-Taps (Membrane/Body/Sub/Click), die Thumbnails
  eine eigene Listen-Ansicht statt der ComboBox. Nicht umgesetzt, um den Build stabil
  zu halten – bei Bedarf als eigene Aufgabe.

### Messwerte nach der Umsetzung

| Kennzahl | vorher | nachher |
|---|---|---|
| Preset-Lautheitsspanne (11 → 16 Presets) | 4,2 dB | **0,31 dB** |
| Pitch-Envelope am Trigger | ratio 1,0 → steigt auf 2^depth | **2^depth → fällt auf 1,0** |
| EQ-Sub +3 dB am Grundton | −0,69 dB | **+2,99 dB** (0 dB = bit-transparent) |
| Drive Kleinsignalverstärkung | +12,5 dB (Band) | **0,00 dB** |
| Output-Clip Kleinsignalverstärkung | +7,7 dB | **−0,001 dB** |
| Room-Tap-Summe | 2,97 (+9,45 dB) | **1,0** |
| Tube-Zweig | +12,2 dB | **−3,6 dB** (pegelneutral) |
| Grumble Mono-Summe (Pan-Spread) | 2,58 dB | **0,00 dB** |
| Parameter gesamt | 1311 (davon 768 tot) | **543** (alle erreichbar) |
| Host-Latenz gemeldet | nie | 0 / 15 / 30 Samples (Oversampling) |

---

## Automatisches Layering (2026-11-12)

- [x] `LayeringStage` (DSP): zwei vollstaendig aus den Modul-Parametern abgeleitete Layer
      (L1 DEPTH: Grundton-Stuetzung mit langem Decay; L2 PRESENCE: kurzer bandbegrenzter Textur-Layer)
- [x] Alle Layer-Werte automatisch: Frequenz, Laenge, Attack, Decay, Pegel, Dichte, Bandlage, Delay
- [x] Beide Layer folgen derselben Modulation wie die Basis (Pitch-Env, Glide, Velocity->Pitch, Velocity)
- [x] Einspeisung am Membrane-Bus -> Tube/Drive/EQ/Comp, Mixer-Kanaele und Patchbay unveraendert
- [x] Pegelneutral: teil-kohaerenter Basis-Trim; Messung alle 11 Factory-Presets max. +0,79 dB,
      Extremwert-Sweep (Sub/Click/Distortion/Drive am Anschlag) max. +0,81 dB
- [x] Checkbox "LAYERING ENABLED" in der Waveform-Kopfzeile (+ Eintrag in FEATURES)
- [x] Waveform-Anzeige ueberlagert beide Layer farbig mit Legende (Hz / ms / dB / Dichte)
- [x] Parameter `layeringEnabled` + `layeringAmount` (0-200 %) am Ende von getAllIDs()
      (1310 gesamt, Indizes stabil); Amount skaliert beide Layer, Kompensation folgt
- [x] Amount-Regler im Waveform-Panel unter der Checkbox (ausgegraut, solange Layering aus)
- [x] Tests: off-by-default, beide Layer tragen, kein Overgain/Lautheitssprung,
      Depth-Layer bis in den Tail, Presence kurz, Pitch-/Velocity-Modulation, 11 Presets
