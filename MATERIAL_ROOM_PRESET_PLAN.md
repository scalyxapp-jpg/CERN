# Material Room Preset Plan (MR-060/061)

Nur Plan/Dokumentation — **keine** Änderungen an `PresetManager.cpp`.

## MR-060: 808 Boomy (Material Room + Grumble)

Ziel: Boomy 808-Charakter mit Wood-Raum und tiefem, langsamem Grumble; Low-End betonen ohne Dry-Druckverlust (Wet-only).

| Parameter | Wert | Begründung |
|---|---:|---|
| `materialRoomEnabled` | true | Modul aktiv |
| `materialRoomMaterial` | 1 (Wood) | Warmer, mittlerer Hall (c 3960 m/s, α_room 0.10, Feedback ~0.946) |
| `materialRoomSize` | 10.0 m | Längere Tap-Delays (L/c) für boomy Tail |
| `materialRoomMix` | 0.18 | Deutlicher Wet-Anteil, Dry bleibt voll |
| `materialRoomLayer` | 2 (Both) | Kick + Rumble speisen den Raum |
| `materialRoomWidth` | 0.45 | Knapp unter neutral, fokussiertes Low-End |
| `grumbleLevel` | 0.35 | Hörbares Sub-Grummen |
| `grumbleFreq` | 38.0 Hz | Tiefer 808-Sub |
| `grumbleRate` | 0.35 Hz | Langsamer Pan-Schwung |
| `grumbleDepth` | 0.40 | Moderater A↔B-Schwung |
| `grumblePanA` | -0.40 | |
| `grumblePanB` | 0.40 | |
| `grumbleWave` | 0 (Sine) | Sauberer Sub |
| `grumblePanWave` | 0 (Sine) | Weicher Pan |
| `grumbleDecay` | 0.90 s | Langer Grumble-Tail |

### Messprotokoll-Vorgabe (MR-060)
- **A/B:** Preset mit `materialRoomMix=0, grumbleLevel=0` vs. obigen Werten, identischer Trigger, gleicher Dry-Pegel.
- **Low-End:** RMS 20–100 Hz (Wet-Anteil) messen; bei Wood/Mix 0.18 moderater Anstieg erwartet, Dry unverändert (kein Druckverlust).
- **Subjektiv:** Boomy Tail, kein Mud im Dry, Grumble mittig schwingend wahrnehmbar.
- **Dokumentation:** Sample-Rate, Blocksize, Trigger-Note/Velocity, RMS-Vorher/Nachher (dB) und Render-Pfad festhalten.

## MR-061: Deep Techno Kick (Material Room + Grumble)

Ziel: Tiefer, kontrollierter Techno-Kick mit Concrete-Charakter (kurz/trocken im Vergleich zu Water/Fabric) und subtiler Grumble-Bewegung.

| Parameter | Wert | Begründung |
|---|---:|---|
| `materialRoomEnabled` | true | Modul aktiv |
| `materialRoomMaterial` | 4 (Concrete) | c 3700 m/s, α_room 0.09, Feedback ~0.950, dicht/trocken |
| `materialRoomSize` | 6.0 m | Kürzere Delays, straffer Raum |
| `materialRoomMix` | 0.12 | Subtiler Wet-Anteil |
| `materialRoomLayer` | 2 (Both) | Kick + Rumble |
| `materialRoomWidth` | 0.50 | Neutral (MR-013: 1.0 Seitenfaktor) |
| `grumbleLevel` | 0.22 | Dezentes Sub-Grummen |
| `grumbleFreq` | 45.0 Hz | Techno-Sub-Grundton |
| `grumbleRate` | 0.60 Hz | Leichte Bewegung |
| `grumbleDepth` | 0.30 | Kleiner Pan-Schwung |
| `grumblePanA` | -0.35 | |
| `grumblePanB` | 0.35 | |
| `grumbleWave` | 0 (Sine) | Sauber |
| `grumblePanWave` | 0 (Sine) | Sauber |
| `grumbleDecay` | 0.55 s | Mittlerer Tail |

### Messprotokoll-Vorgabe (MR-061)
- **A/B:** wie MR-060, Referenz `mix=0, grumbleLevel=0`.
- **RT60/Charakter:** Concrete schneller/straffer als Wood (gleiche Size) — per Impulsantwort und RMS-Abklingzeit vergleichen.
- **Low-End:** RMS 20–100 Hz kontrolliert; kein übermäßiger Water-artiger Boost.
- **Pan:** Grumble-L/R-Energie differiert bei Depth>0 (Pan-Swing messbar).
- **Dokumentation:** Sample-Rate, Blocksize, Trigger, RMS- und Abklingwerte, Render-Pfad.

## Status
- MR-060: dokumentiert (PresetManager.cpp bewusst nicht geändert).
- MR-061: dokumentiert (PresetManager.cpp bewusst nicht geändert).
