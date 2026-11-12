# SOUND REVIEW — vollständige abarbeitbare Findings (2026-11-12)

Alle Findings müssen umgesetzt werden. Format:
`ID | Prio | Datei/Parameter | Ist -> Soll`
Presets: `ID | Preset | Parameter | Ist -> Soll`.

**Umsetzungsstatus (2026-11-12, Abschluss-Review):** 322 DONE, 5 PARTIAL.
PARTIAL: SND-093 (Room-Sync ohne APVTS-Parameter), SND-145/146 (Steps 17–64 /
Patterns B–D ohne Gate/Velocity/Pitch-Parameter), SND-184/185 (Randomize/Morph
nur GUI-seitig, kein Unit-Test). Details und Nachweise je Abschnitt:
`TODO.md` → „Sound-Review 2026-11-12 — Status“. Bei den Preset-Findings
(SND-191…327) sind alle Klangwerte gesetzt; `outputGain` wurde je Preset für
die Loudness-Kalibrierung (SND-006, Streuung ≤1,5 dB) feinjustiert.

## A. Output / Pegel

SND-001|P0|KickSynthesisEngine.cpp processOutputStage clipNorm|clipNorm=1/clipDrive, Ceiling=1/(1+4*clip) -> clipNorm=1/tanh(clipDrive), Ceiling=1.0
SND-002|P0|ParameterLayout.h outputClip default|0.6 -> 0.35
SND-003|P0|KickSynthesisEngine.cpp limiterCeiling|dbToGain(-6*limiter) -> dbToGain(-0.3*limiter), 0=Bypass
SND-004|P0|outputLimiter default|0.9 -> 0.15
SND-005|P0|Test/Acceptance|kein Test -> GR>0 bei Default-Preset
SND-006|P1|Test/Acceptance|keine Loudness-Pruefung -> LUFS-Streuung <=1.5 dB ueber 7 Presets
SND-007|P2|processOutputStage Reihenfolge|Clip/Limiter nach Pan -> vor Pan, Pan danach
SND-008|P2|processOutputStage Detektor|Peak pro Kanal -> Stereo-Link max(L,R)
SND-009|P1|outputGain Kalibrierung|0 dB ~ -10.6 dBFS -> 0 dB = -1 dBFS Peak
SND-010|P2|Guard|keiner -> assert(clipCeiling <= limiterCeiling)

## B. Drive

SND-011|P0|processDrive Wet-Gain|tanh(kx)/k, Pegelverlust -> Makeup 1/tanh(k) bzw. RMS-konstant
SND-012|P0|drivePreHP default|0.1 -> 0.0
SND-013|P1|processDrive DC-Blocker|c=0.995 (38 Hz), nur Wet -> HP <20 Hz oder Band-Split
SND-014|P1|processDrive Topologie|Vollband-Saettigung -> Band-Split: HP 120-180 Hz saettigen, Low clean
SND-015|P1|driveMix default|0.6 -> 0.45
SND-016|P2|processDrive case2 Fuzz|Gain 2 (+6 dB) -> /(drive*2) bzw. Trim
SND-017|P2|processDrive case3 Wave Fold|Gain 0.5 (-6 dB) -> Gain 1, amplitudeunabhaengige Schwelle
SND-018|P3|processDrive case4 Alias|kein Pre-Gain -> Pre-Gain + Pegelkompensation
SND-019|P2|drivePreHP Mapping|0..1 -> 20..500 Hz -> 0=aus, dann 60..500 Hz
SND-020|P2|Drive-Metering|keins -> GR/Output-Meter
SND-021|P3|Oversampling|keins -> 2x um processDrive/Clip

## C. Pitch-Envelope

SND-022|P0|PitchEnvelope.h Decay|linear -> exponentiell exp(-(t-a)/(d/4.605))
SND-023|P1|PitchEnvelope.h curve|nur Attack -> auch Decay
SND-024|P1|pitchEnvAttack default|5.0 ms -> 0.2 ms
SND-025|P1|pitchEnvDepth Range|0..1 (+12 st) -> 0..2 (+24 st)
SND-026|P2|pitchEnvDepth default|0.6 -> 0.8
SND-027|P1|pitchEnvDecay default|200 ms -> 60 ms
SND-028|P1|Render Depth|nur Membrane -> Body 0.3-0.5x, Sub 0
SND-029|P3|pitchEnvDecay Range|20..1000 -> 20..1500 ms
SND-030|P2|Velocity->Pitch|indirekt -> expliziter velToPitch 0..1

## D. Membrane

SND-031|P1|MembraneModel.h f1|verworfen -> Tension/Radius/Mass auf Timbre mappen (Radius Modus-Spreizung 0.6-1.6; Mass Modus-0-Gewicht 0.2-0.9 + Damping x1.6..0.6; Tension Damping x1.5..0.7)
SND-032|P1|MembraneModel.h modeFreq|f*(1+0.6*i*stiffness) -> Besselfunktionen 1/1.594/2.136/2.296/2.653
SND-033|P1|MembraneModel.h Summe|alle amp=vel, sum*=1/N -> gewichtete Amplituden 1/0.6/0.35/0.25/0.2, Summe normieren
SND-034|P0|MembraneModel.h Glide|phase += nl*amp*0.05 (fix +382*nl*amp Hz) -> phase += omega*(1+nl*amp*0.5)
SND-035|P1|membraneNonlinearity default|0.3 -> 0.15
SND-036|P2|membraneModes default|8 -> 5
SND-037|P2|membraneDamping default|0.2 -> 0.30
SND-038|P3|membraneStiffness default|0.2 -> 0.15
SND-039|P2|membraneStiffness Mapping|1+0.6*i*s -> bessel[i]*(1+0.15*s*i)
SND-040|P3|membraneModes Range|1..24 -> 1..12
SND-041|P3|MembraneModel.h envelope|berechnet, nie gelesen -> entfernen oder fuer Body nutzen

## E. Body

SND-042|P2|Body Phase/Detune|Phase 0, f0 -> Phase pi/2 oder Body-Detune +7 ct
SND-043|P1|bodyDecay Range|50..2000 -> 50..4000 ms
SND-044|P2|bodyDecay default|300 -> 450 ms
SND-045|P2|bodyAttack default|2.0 -> 0.8 ms
SND-046|P3|bodyLevel default|0.7 -> 0.65
SND-047|P2|trigger bodyPeak|fix vel*0.6 -> entfernen, Level direkt
SND-048|P2|processBody|kein Pitch-Anteil -> bodyFreq*(1+0.4*pitchEnvDepth*env)

## F. Sub

SND-049|P0|updateSubOscillator|f0*0.5 -> f0*1.0 (Ratio-Param, Default 1.0)
SND-050|P1|subLevel default|0.6 -> 0.30
SND-051|P2|trigger subPhase|0 -> 0 bei Ratio 1.0; pi/2 bei 0.5
SND-052|P2|processSub Shape 1/2|ohne Bandlimit -> PolyBLEP oder 2x Oversampling
SND-053|P2|subDecay Range|50..1500 -> 50..3000 ms
SND-054|P2|subDetune Range|-12..+12 -> -50..+50 ct
SND-055|P3|subDecay default|400 -> 350 ms
SND-056|P3|Sub-Ratio|fest 0.5 -> sichtbarer Schalter 0.5/1.0

## G. Click / Beater

SND-057|P1|ClickGenerator Quelle|Sinus 1.5-8 kHz -> 0.5-2 ms Noise-Burst (HP 1.5 kHz) + 2-4 kHz Reso-Tick, clickNoiseMix
SND-058|P1|clickLevel Range|0..1 -> 0..2
SND-059|P1|clickLevel default|0.5 -> 0.8
SND-060|P1|clickTone Mapping|1500+6500*tone -> 1200+5300*tone
SND-061|P1|clickTone default|0.5 -> 0.35
SND-062|P2|clickDecay default|5.0 -> 4.0 ms
SND-063|P2|clickBeaterMaterial|Faktor 0.7/1.0/1.3 -> Soft 8 ms/Noise70%, Medium 4 ms/50%, Hard 2 ms/30%
SND-064|P2|clickDistortion|pegelabhaengig -> pegelneutral normalisieren
SND-065|P3|clickDecay Range|0.5..20 -> 0.5..30 ms
SND-066|P2|Default-Patch ClickOut|nur DriveIn -> optional EqIn, Standard Band-Split Drive

## H. Tube

SND-067|P1|TubeResonator return|Peak 0.07-0.10 -> x4 Verstaerkung
SND-068|P1|tubeResonanceFreq|fix 150 Hz -> Ratio zu f0, Default 2.0, tubeTrack on
SND-069|P2|tubeResonanceQ default|5.0 -> 8.0
SND-070|P2|tubeResonanceFreq Range|40..300 -> 40..400 Hz
SND-071|P3|hollowMetallic|tanh(out*2)*m*0.3 -> 2-3 gekoppelte Moden 1x/2.7x/5.4x
SND-072|P2|Tube-Struktur|1 Bandpass -> 2. BP 2.7x -9 dB, 3. BP 5.4x -15 dB

## I. EQ

SND-073|P2|updateEQ eqSubFreq|fix 50 Hz -> Auto-Track = fundamentalHz, eqSubTrack on
SND-074|P1|eqPunchFreq default|1500 -> 2800 Hz
SND-075|P3|eqPunchGain default|2 -> 3 dB
SND-076|P2|eqSubGain default|3 -> 2 dB
SND-077|P2|eqAirGain default|1 -> 0 dB
SND-078|P3|eqRumbleHP default|30 -> 25 Hz
SND-079|P2|processEQSample|unnormiert -> Makeup 1/(1+g_sub+g_punch+g_air) bzw. Clamp
SND-080|P3|Punch-Band|Differenz One-Poles -> Biquad-Peak Q 0.7-1.2
SND-081|P2|Sub-Band|6 dB/oct -> 12 dB/oct (2 kaskadiert)
SND-082|P3|Ranges|Sub 20-120, Punch 500-5000, Rumble 15-60 -> Sub 20-200, Punch 500-8000, Rumble 15-120

## J. Compressor

SND-083|P1|compMakeup default|3 -> 0 dB
SND-084|P1|compThreshold default|-12 -> -8 dB
SND-085|P1|compRatio default|4:1 -> 3:1
SND-086|P2|compAttack default|5 -> 10 ms
SND-087|P2|compRelease default|100 -> 120 ms
SND-088|P2|SoftKneeCompressor|Makeup fix -> Auto-Makeup = aktuelle GR
SND-089|P3|Ranges|Attack 0.1-50, Release 10-500 -> Attack 0.1-100, Release 10-1000
SND-090|P3|Comp-Mix|kein Dry/Wet -> compMix 0..1

## K. Room

SND-091|P1|processRoomRumble|1 Tap 80 ms -> 4 Taps 61/79/97/113 ms, per-Tap-Gain
SND-092|P0|Room Send/Return|x*rumbleAmt, Return*level (2.5%) -> Send=level allein (dB-kalibriert)
SND-093|P2|Room-Timing|fix 80 ms -> roomSync 1/16, 1/8, dotted, free
SND-094|P2|roomLevel default|0.2 -> 0.0
SND-095|P3|roomDecay default|300 -> 350 ms
SND-096|P2|roomRumble default|0.1 Input-Gain -> 0.3, Neudefinition LP/HP-Tone im Send
SND-097|P1|roomLevel Mapping|linear -> sqrt(level) bzw. dB-Kurve
SND-098|P2|roomDecay Range|100..1000 -> 50..2000 ms
SND-099|P3|Room-DC-Blocker|c=0.995 (38 Hz) -> c=0.9995 (2 Hz)
SND-100|P3|Room-Stereo|mono -> Tap-Paare L/R versetzt

## L. Reverb

SND-101|P2|MaterialReverb Feedback|RT60 0.62-1.87 s bei Label 1.5 -> Kalibrierung +2 dB / RT60 messen
SND-102|P2|Reverb Eingang|keine Diffusion -> 2-4 Allpaesse 7/11/17/23 ms, g 0.5
SND-103|P2|Delay-Laengen|statisch -> +-0.3% Modulation bei 0.2 Hz
SND-104|P3|Leitungen|4 -> 8
SND-105|P2|process Mix|wet*mix, Dry voll -> Dry*(1-mix) bzw. Send/Return
SND-106|P2|reverbMix default|0.3 -> 0.15
SND-107|P3|reverbPreDelay default|20 -> 30 ms
SND-108|P3|reverbDecay default|1500 -> 1200 ms
SND-109|P2|reverbLowCut default|100 -> 150 Hz
SND-110|P2|reverbHighCut default|8000 -> 6000 Hz
SND-111|P2|updateTailLength|reverbDecay ohne Material-Mul -> _profile.decayMul +0.5 s
SND-112|P3|Freeze/Gate|keins -> reverbFreeze + Gate 10-500 ms
SND-113|P3|Injektion|mono 0.5_(pdL+pdR) -> Stereo L->0/2, R->1/3
SND-114|P3|reverbLowCut Range|20..300 -> 20..500 Hz
SND-115|P3|Material-Damping|Offset clamped -> Offset 0..1 normieren
SND-116|P2|Size-Aenderung|springt -> 20 ms glaetten/interpolieren

## M. Higgs

SND-117|P0|HiggsFieldEngine::process Return|dry+wet -> wet-only (Dry im Return entfernen)
SND-118|P1|Higgs Zeitverhalten|statisch -> LFO 0.1-2 Hz auf phi/theta, higgsRate/higgsDepth
SND-119|P1|higgsBosonHz Range|100..140 -> 40..400 Hz
SND-120|P2|higgsBosonHz default|125 -> 98 Hz (2x49)
SND-121|P2|higgsBosonQ default|10 -> 8
SND-122|P1|higgsMix default|0.4 -> 0.10
SND-123|P2|applyDcBlocker Wet|38 Hz HP -> 5-10 Hz
SND-124|P2|yukawa/vev Mapping|1+yuk*dev*0.5, dev -1..+1 -> +-3 dB begrenzen, dev normieren
SND-125|P3|UI-Labels|VEV/Lambda/Yukawa/Boson -> Masse/Kopplung/Praesenz/Resonanz
SND-126|P3|higgsBosonQ Range|1..30 -> 1..40
SND-127|P3|higgsEnabled|Bool in Higgs + Enable -> nur eine Instanz

## N. Tuning / Global

SND-128|P1|rootNote|entkoppelt -> rootNote = Note von f0; MIDI-Offset relativ, +-12 st
SND-129|P1|rootNote default|60 -> 30 (F#1)
SND-130|P2|fundamentalHz Range|30..120 -> 25..180 Hz
SND-131|P2|ParamKnob/Display|Hz-Zahl -> Note + Cents (F#1 -3 ct)
SND-132|P2|Key/Scale|keins -> keyRoot + scale, Snap fuer f0/Step-Pitch
SND-133|P2|Choice/Bool Smoothing|15 ms -> ungesmoothed lesen
SND-134|P3|stepPitch Wirkung|nur Membrane/Body/Sub -> Click/Tube 30% tracken
SND-135|P3|Glide|kein Portamento -> glideMs 0..200 fuer Step-Pitch

## O. Sequencer

SND-136|P0|StepSequencer Taktgeber|freier BPM -> Host-BPM/PPQ via getPlayHead, syncMode
SND-137|P0|Transport|kein Playhead/Start -> Start/Stop + Phase-Reset auf Host-Bar
SND-138|P2|sequencerSwing Range|-1..+1 -> 0..0.75, Anzeige 50-75%
SND-139|P1|FooterSequencer|nur Gates+Velocity -> dritte Reihe Pitch-Slider +-24, stepPitch-Attachment
SND-140|P2|Ratchets|keine -> stepRatchet 1/2/3/4, Step teilen, n Trigger
SND-141|P2|Flam|keiner -> stepFlam 0..50 ms, zweiter Trigger
SND-142|P2|Probability|keine -> stepProbability 0..1, deterministischer Seed
SND-143|P3|Step-Laenge|fix 1/16 -> stepLength 0.25..4 Steps
SND-144|P2|Triolen|kein Grid -> gridMode 1/16, 1/8T, 1/16T
SND-145|P2|Pattern-Laenge|fix 16 -> sequencerSteps 1..64
SND-146|P3|Pattern-Chaining|keine -> Pattern A-D + chain 1..8
SND-147|P3|Gate/Velocity Smoothing|15 ms -> unsmoothed
SND-148|P3|Step-Preview|keins -> Klick auf Step triggert Note
SND-149|P3|stepVel Defaults|1.0 nur i%4 in {0,3} -> alle 1.0
SND-150|P3|MIDI-Clock-Out|keins -> Clock/Start/Stop senden

## P. CV / Patchbay

SND-151|P1|ParameterPatchGraph::Connection|nur source/target -> amount -1..+1 pro Kabel, UI-Knob
SND-152|P1|prepareParameterModulation|sum += 2*norm-1 -> sum += amount*(2*norm-1)
SND-153|P2|CV-Polaritaet|bipolar -> unipolar-Flag pro Kabel
SND-154|P2|CV-Rate|pro Block -> optional sample-genau / 1/64-Note
SND-155|P2|CV-Smoothing|keins -> 5-20 ms Slew pro Kabel
SND-156|P2|CV-Quellen|nur Parameter -> LFO + Envelope-Follower
SND-157|P2|CV-Quellen|nur Parameter -> Makro 1-5
SND-158|P2|MainComponent|Patchbay hinter RUECKSEITE -> sichtbarer PATCH-Tab + Kabel-LED
SND-159|P3|UI-Sprache|DE/EN gemischt -> einheitlich
SND-160|P3|Modulations-Feedback|keins -> Knob-Ring zeigt CV-Anteil
SND-161|P3|CV-Clamp|0..1 -> 0..1 + Wrap-Option

## Q. GUI / Workflow

SND-162|P1|Makros|keine -> 5 Makros TUNE/PUNCH/BOOM/DIRT/SPACE
SND-163|P2|Mix/EQ/Comp Sektion|13 Regler -> EQ/Comp/Output trennen
SND-164|P1|HeaderBar savePreset|fix user-preset.xml -> Save-As Name/Ordner/Liste
SND-165|P2|Preset-Browser|nur Factory-Combo -> Ordner-Browser + Kategorien
SND-166|P2|Init-Preset|fehlt -> Init als Preset 0
SND-167|P2|A/B|fehlt -> 2 States + Toggle
SND-168|P2|Undo|fehlt -> UndoManager am APVTS + Ctrl+Z
SND-169|P2|Modified-Marker|fehlt -> * im Preset-Namen
SND-170|P2|Metering|RMS/Peak/GR -> LUFS short-term + GR-History
SND-171|P3|EnvelopeEditor|lineare Rampe -> tatsaechliche exp+Curve
SND-172|P3|Preset-Combo|setSelectedId(0) -> aktuelles Preset nach Load
SND-173|P3|Shortcuts|keine -> Space Play, T Trigger, <-/-> Preset

## R. Fehlende Features

SND-174|P1|Sidechain/Ducking|keins -> Key-Input/interner Duck, duckDepth 0..24 dB, duckRelease 20..500 ms
SND-175|P2|Transient-Shaper|keiner -> DSP/TransientShaper.h, attack/sustain 0..+12 dB
SND-176|P2|MIDI-Filter/Choke|alle Kanaele -> midiChannel 1-16/Omni, Notenbereich, Choke
SND-177|P2|Velocity->Pitch/Click|nur Level -> velToPitch 0..12 st, velToClick 0..1
SND-178|P3|Humanize|deterministisch -> +-3 ct, +-1 dB, +-1 ms
SND-179|P3|Oversampling|keins -> 2x Drive+Clip
SND-180|P3|Comp-Mix|keiner -> 0..100% parallel
SND-181|P3|FX-Stereo-Width|nur global -> Width nur Room/Reverb-Tails
SND-182|P3|Reverb-Freeze/Gate|keins -> Freeze + Gate 10..500 ms
SND-183|P3|Render/Bounce|keiner -> 1 Takt offline WAV
SND-184|P3|Randomize|keins -> sektionsweise, tuning-safe
SND-185|P3|Preset-Morph|keiner -> A/B interpolieren 0..100%

## S. Dokumentation

SND-186|P2|README Preset-Liste|10 nicht existierende -> 7 tatsaechliche + Charakter
SND-187|P3|README Root Note|C3 -> F#1 (MIDI 30)
SND-188|P2|README 909|linear dokumentiert -> nach SND-022 korrigieren
SND-189|P3|README Parametertabelle|weicht ab -> aus DefaultValues generieren
SND-190|P3|README Features|Preset-Browser/Thumbnail -> als geplant markieren oder entfernen

## T. Presets

### T1 Deep Techno Kick

SND-191|Deep|fundamentalHz|45.0->46.25
SND-192|Deep|pitchEnvDepth|0.55->0.80
SND-193|Deep|pitchEnvDecay|150->60
SND-194|Deep|bodyLevel|0.75->0.70
SND-195|Deep|bodyDecay|520->420
SND-196|Deep|clickLevel|0.35->0.70
SND-197|Deep|driveAmount|0.45->0.40
SND-198|Deep|driveMix|0.55->0.45
SND-199|Deep|eqSubGain|3->2
SND-200|Deep|eqPunchGain|2->3
SND-201|Deep|compThreshold|-14->-8
SND-202|Deep|compRatio|4->3
SND-203|Deep|compMakeup|3->0
SND-204|Deep|roomLevel|0.25->0.18
SND-205|Deep|reverbMaterial|2->4
SND-206|Deep|reverbMix|0.12->0.08
SND-207|Deep|higgsEnabled|1->0 (bis SND-117, danach 1)
SND-208|Deep|higgsMix|0.15->0.12

### T2 Hard Acid Kick

SND-209|Hard|pitchEnvDepth|0.85->0.95
SND-210|Hard|pitchEnvDecay|90->45
SND-211|Hard|clickLevel|0.80->0.55
SND-212|Hard|clickTone|0.65->0.50
SND-213|Hard|bodyLevel|0.60->0.70
SND-214|Hard|driveAmount|0.85->0.45
SND-215|Hard|driveMix|0.80->0.45
SND-216|Hard|driveCharacter|2->1
SND-217|Hard|drivePreHP|0.10->0.25
SND-218|Hard|eqPunchFreq|2200->3000
SND-219|Hard|eqPunchGain|5->3
SND-220|Hard|compThreshold|-10->-8
SND-221|Hard|compRatio|6->3
SND-222|Hard|outputGain|-3.0->3.4
SND-223|Hard|roomLevel|0.10->0.15
SND-224|Hard|reverbMix|0.05->0.08
SND-225|Hard|membraneDamping|0.20->0.35
SND-226|Hard|membraneModes|8->4

### T3 808 Boomy

SND-227|808|fundamentalHz|36.0->41.20
SND-228|808|pitchEnvDepth|0.90->0.85
SND-229|808|pitchEnvDecay|600->220
SND-230|808|bodyLevel|0.90->0.85
SND-231|808|bodyDecay|1400->1800
SND-232|808|clickLevel|0.15->0.28
SND-233|808|subLevel|0.80->0.30
SND-234|808|driveAmount|0.25->0.20
SND-235|808|eqSubGain|5->2
SND-236|808|eqRumbleHP|20->22
SND-237|808|compThreshold|-18->-10
SND-238|808|compMakeup|4->0
SND-239|808|outputGain|-1.0->2.8
SND-240|808|roomLevel|0.40->0.30
SND-241|808|roomRumble|0.50->0.30
SND-242|808|reverbMix|0.20->0.10
SND-243|808|membraneDamping|0.20->0.14
SND-244|808|membraneModes|8->3

### T4 Tight Techno

SND-245|Tight|fundamentalHz|50.0->49.00
SND-246|Tight|pitchEnvDepth|0.40->0.75
SND-247|Tight|pitchEnvDecay|70->50
SND-248|Tight|clickLevel|0.60->0.55
SND-249|Tight|clickTone|0.60->0.42
SND-250|Tight|bodyDecay|240->320
SND-251|Tight|driveAmount|0.60->0.35
SND-252|Tight|compThreshold|-12->-8
SND-253|Tight|compRatio|8->3
SND-254|Tight|compAttack|1.0->8.0
SND-255|Tight|compRelease|60->110
SND-256|Tight|outputGain|-2.0->-0.5
SND-257|Tight|roomLevel|0.08->0.12
SND-258|Tight|higgsEnabled|1->0 (bis SND-117)
SND-259|Tight|higgsMix|0.08->0.10
SND-260|Tight|membraneDamping|0.20->0.32

### T5 CV Pulse Kick

SND-261|CV|fundamentalHz|52.0->51.91
SND-262|CV|pitchEnvDepth|0.75->0.85
SND-263|CV|pitchEnvDecay|120->55
SND-264|CV|clickLevel|0.70->0.60
SND-265|CV|clickTone|0.60->0.45
SND-266|CV|bodyLevel|0.80->0.70
SND-267|CV|driveAmount|0.75->0.45
SND-268|CV|driveMix|0.70->0.50
SND-269|CV|eqPunchGain|4->2
SND-270|CV|compThreshold|-10->-8
SND-271|CV|compRatio|6->3
SND-272|CV|compMakeup|3->0
SND-273|CV|outputGain|-2.0->-2.9
SND-274|CV|roomLevel|0.15->0.12
SND-275|CV|higgsEnabled|1->0 (bis SND-117)
SND-276|CV|higgsMix|0.25->0.12
SND-277|CV|higgsBosonHz|118->103.8
SND-278|CV|higgsBosonQ|12->8
SND-279|CV|parameterPatch|3 Kabel -> 1 (bodyLevel->driveMix); entfernen clickLevel->driveAmount, fundamentalHz->driveMix

### T6 Higgs Mod Kick

SND-280|Higgs|fundamentalHz|45.0->46.25
SND-281|Higgs|pitchEnvDepth|0.90->0.85
SND-282|Higgs|pitchEnvDecay|400->90
SND-283|Higgs|bodyLevel|0.90->0.70
SND-284|Higgs|bodyDecay|900->700
SND-285|Higgs|clickLevel|0.20->0.55
SND-286|Higgs|subLevel|0.70->0.30
SND-287|Higgs|subDecay|700->600
SND-288|Higgs|driveAmount|0.30->0.35
SND-289|Higgs|eqSubGain|5->2
SND-290|Higgs|eqRumbleHP|20->22
SND-291|Higgs|compThreshold|-18->-10
SND-292|Higgs|compMakeup|4->0
SND-293|Higgs|outputGain|-1.0->-3.3
SND-294|Higgs|roomLevel|0.40->0.30
SND-295|Higgs|roomRumble|0.50->0.30
SND-296|Higgs|reverbMix|0.20->0.10
SND-297|Higgs|higgsEnabled|1->0 (bis SND-117)
SND-298|Higgs|higgsMix|0.35->0.15
SND-299|Higgs|higgsBosonHz|132->92.5
SND-300|Higgs|higgsBosonQ|24->8
SND-301|Higgs|membraneDamping|0.20->0.24
SND-302|Higgs|membraneModes|8->4

### T7 Parameter Patch Kick

SND-303|Patch|fundamentalHz|58.0->58.27
SND-304|Patch|pitchEnvDepth|0.50->0.70
SND-305|Patch|pitchEnvDecay|180->60
SND-306|Patch|clickLevel|0.50->0.60
SND-307|Patch|clickTone|0.55->0.45
SND-308|Patch|subLevel|0.40->0.28
SND-309|Patch|driveAmount|0.65->0.40
SND-310|Patch|driveMix|0.60->0.40
SND-311|Patch|driveCharacter|3->1
SND-312|Patch|drivePreHP|0.10->0.25
SND-313|Patch|eqPunchFreq|1800->3000
SND-314|Patch|eqPunchGain|3->2
SND-315|Patch|compThreshold|-12->-8
SND-316|Patch|compRatio|8->3
SND-317|Patch|compAttack|1.0->8.0
SND-318|Patch|compRelease|60->110
SND-319|Patch|outputGain|-2.0->-1.5
SND-320|Patch|roomLevel|0.10->0.12
SND-321|Patch|roomDecay|200->250
SND-322|Patch|reverbMix|0.12->0.08
SND-323|Patch|higgsEnabled|1->0 (bis SND-117)
SND-324|Patch|higgsMix|0.20->0.12
SND-325|Patch|higgsBosonHz|120->116.5
SND-326|Patch|higgsBosonQ|15->8
SND-327|Patch|parameterPatch|4 Kabel -> 1 (higgsFieldStrength->higgsBosonQ); entfernen driveAmount->eqPunchGain, clickLevel->driveMix, reverbMix->roomLevel
