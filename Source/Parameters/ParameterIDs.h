#pragma once

#include <juce_core/juce_core.h>
#include <array>
#include <vector>
#include <string>

//==============================================================================
/** Alle Parameter-IDs des CERN-Plugins (1229 Parameter).

    Die IDs entsprechen der Spezifikation in TECHNICAL_WIRING_FULL.md §2 und
    den SOUND_REVIEW-Findings (2026-11-12). Bestehende IDs behalten ihre
    relative Reihenfolge; neue Parameter stehen am Ende ihrer jeweiligen
    Gruppe. Die Modul-Enable-IDs bleiben der letzte Block der Kernmodule;
    Material Room / Sub Grumble (MATERIAL_ROOM_SPEC.md §3) folgt dahinter,
    damit der Zustands-/Preset-Aufbau stabil bleibt.
    Die APVTS-Erstellungsreihenfolge folgt dieser Datei (ParameterLayout.cpp).
*/
namespace ParameterIDs
{

// ============================================================================
// 2.1 Membrane / Physical Model (8): #0 - #7
// ============================================================================
static constexpr auto* membraneTension      = "membraneTension";      // 0.0–1.0
static constexpr auto* membraneRadius       = "membraneRadius";       // 0.05–0.5 m
static constexpr auto* membraneMass         = "membraneMass";         // 0.1–2.0 kg/m²
static constexpr auto* membraneDamping      = "membraneDamping";      // 0.0–1.0
static constexpr auto* membraneModes        = "membraneModes";        // 1–24
static constexpr auto* fundamentalHz        = "fundamentalHz";        // 30–120 Hz
static constexpr auto* membraneNonlinearity = "membraneNonlinearity"; // 0.0–1.0
static constexpr auto* membraneStiffness    = "membraneStiffness";    // 0.0–1.0
static constexpr auto* membraneEnabled     = "membraneEnabled";     // 0/1

// ============================================================================
// 2.2 Hollow Steel / Tube Resonance (5): #8 - #12
// ============================================================================
static constexpr auto* tubeResonanceFreq    = "tubeResonanceFreq";    // 40–300 Hz
static constexpr auto* tubeResonanceQ       = "tubeResonanceQ";       // 0.5–20
static constexpr auto* tubeResonanceLevel   = "tubeResonanceLevel";   // 0.0–1.0
static constexpr auto* tubeCoupling         = "tubeCoupling";         // 0.0–1.0
static constexpr auto* hollowMetallic       = "hollowMetallic";       // 0.0–1.0
static constexpr auto* tubeEnabled          = "tubeEnabled";          // 0/1

// ============================================================================
// 2.3 Pitch Envelope (TR-909/808 Style) (4): #13 - #16
// ============================================================================
static constexpr auto* pitchEnvAttack       = "pitchEnvAttack";       // 0.1–50 ms
static constexpr auto* pitchEnvDecay        = "pitchEnvDecay";        // 20–1000 ms
static constexpr auto* pitchEnvDepth        = "pitchEnvDepth";        // 0.0–1.0
static constexpr auto* pitchEnvCurve        = "pitchEnvCurve";        // -1.0–1.0
static constexpr auto* pitchEnvEnabled      = "pitchEnvEnabled";      // 0/1

// ============================================================================
// 2.4 Click / Beater (5): #17 - #21
// ============================================================================
static constexpr auto* clickLevel           = "clickLevel";           // 0.0–1.0
static constexpr auto* clickDecay           = "clickDecay";           // 0.5–20 ms
static constexpr auto* clickTone            = "clickTone";            // 0.0–1.0
static constexpr auto* clickDistortion      = "clickDistortion";      // 0.0–1.0
static constexpr auto* clickBeaterMaterial  = "clickBeaterMaterial";  // Choice: Soft/Medium/Hard
static constexpr auto* clickEnabled         = "clickEnabled";         // 0/1

// ============================================================================
// 2.5 Body (4): #22 - #25
// ============================================================================
static constexpr auto* bodyLevel            = "bodyLevel";            // 0.0–1.0
static constexpr auto* bodyAttack           = "bodyAttack";           // 0.5–10 ms
static constexpr auto* bodyDecay            = "bodyDecay";            // 50–2000 ms
static constexpr auto* bodyTuning           = "bodyTuning";           // 0.5–2.0
static constexpr auto* bodyEnabled          = "bodyEnabled";          // 0/1

// ============================================================================
// 2.6 Sub Oscillator (5): #26 - #29 (+ subRatio)
// ============================================================================
static constexpr auto* subLevel             = "subLevel";             // 0.0–1.0
static constexpr auto* subDecay             = "subDecay";             // 50–3000 ms
static constexpr auto* subDetune            = "subDetune";            // -50–+50 cents
static constexpr auto* subShape             = "subShape";             // Choice: Sine/Triangle/Square
static constexpr auto* subRatio             = "subRatio";             // Choice: 0.5 / 1.0 (SND-056/049)
static constexpr auto* subEnabled           = "subEnabled";           // 0/1

// ============================================================================
// 2.7 Drive / Saturation (4): #30 - #33
// ============================================================================
static constexpr auto* driveAmount          = "driveAmount";          // 0.0–1.0
static constexpr auto* driveMix             = "driveMix";             // 0.0–1.0
static constexpr auto* driveCharacter       = "driveCharacter";       // Choice: Tube/Diode/Fuzz/Wave Fold/Alias
static constexpr auto* drivePreHP           = "drivePreHP";           // 0.0–1.0
static constexpr auto* driveEnabled         = "driveEnabled";         // 0/1

// ============================================================================
// 2.8 EQ (7): #34 - #40
// ============================================================================
static constexpr auto* eqSubFreq            = "eqSubFreq";            // 20–120 Hz
static constexpr auto* eqSubGain            = "eqSubGain";            // -12–+12 dB
static constexpr auto* eqPunchFreq          = "eqPunchFreq";          // 500–5000 Hz
static constexpr auto* eqPunchGain          = "eqPunchGain";          // -12–+12 dB
static constexpr auto* eqAirFreq            = "eqAirFreq";            // 4000–16000 Hz
static constexpr auto* eqAirGain            = "eqAirGain";            // -12–+12 dB
static constexpr auto* eqRumbleHP           = "eqRumbleHP";           // 15–60 Hz
static constexpr auto* eqEnabled            = "eqEnabled";            // 0/1

// ============================================================================
// 2.9 Compressor (8): #41 - #46 (+ compMix/compAutoMakeup)
// ============================================================================
static constexpr auto* compThreshold        = "compThreshold";        // -60–0 dB
static constexpr auto* compRatio            = "compRatio";            // 1–20 :1
static constexpr auto* compAttack           = "compAttack";           // 0.1–100 ms
static constexpr auto* compRelease          = "compRelease";          // 10–1000 ms
static constexpr auto* compKnee             = "compKnee";             // 0–12 dB
static constexpr auto* compMakeup           = "compMakeup";           // -12–+24 dB
static constexpr auto* compMix              = "compMix";              // 0–1 (SND-090/180)
static constexpr auto* compAutoMakeup       = "compAutoMakeup";       // bool (SND-088/180)
static constexpr auto* compEnabled          = "compEnabled";          // 0/1

// ============================================================================
// 2.10 Output / Spatial (6): #47 - #52
// ============================================================================
static constexpr auto* outputGain           = "outputGain";           // -60–+12 dB
static constexpr auto* outputPan            = "outputPan";            // -1–+1
static constexpr auto* outputWidth          = "outputWidth";          // 0–1
static constexpr auto* outputMonoBelow      = "outputMonoBelow";      // 40–200 Hz
static constexpr auto* outputClip           = "outputClip";           // 0–1
static constexpr auto* outputLimiter        = "outputLimiter";        // 0–1
static constexpr auto* outputEnabled        = "outputEnabled";        // 0/1

// ============================================================================
// 2.11 Room / Late Rumble (3): #53 - #55
// ============================================================================
static constexpr auto* roomLevel            = "roomLevel";            // 0.0–1.0
static constexpr auto* roomDecay            = "roomDecay";            // 100–1000 ms
static constexpr auto* roomRumble           = "roomRumble";           // 0.0–1.0
static constexpr auto* roomEnabled          = "roomEnabled";          // 0/1

// ============================================================================
// 2.12 Reverb / Modulation Stage (10): #56 - #63 (+ Freeze/Gate)
// ============================================================================
static constexpr auto* reverbMaterial       = "reverbMaterial";       // Choice 0–6
static constexpr auto* reverbPreDelay       = "reverbPreDelay";       // 0–150 ms
static constexpr auto* reverbDecay          = "reverbDecay";          // 200–5000 ms (RT60)
static constexpr auto* reverbSize           = "reverbSize";           // 0–1
static constexpr auto* reverbDamping        = "reverbDamping";        // 0–1
static constexpr auto* reverbMix            = "reverbMix";            // 0–1
static constexpr auto* reverbLowCut         = "reverbLowCut";         // 20–500 Hz
static constexpr auto* reverbHighCut        = "reverbHighCut";        // 1000–16000 Hz
static constexpr auto* reverbFreeze         = "reverbFreeze";         // bool (SND-112/182)
static constexpr auto* reverbGate           = "reverbGate";           // 0–500 ms (0=off, aktiv 10–500)
static constexpr auto* reverbEnabled        = "reverbEnabled";        // 0/1

// ============================================================================
// 2.13 Global / Tuning (6): #64 - #65 (+ Key/Scale/Glide/Vel->Pitch)
// ============================================================================
static constexpr auto* rootNote             = "rootNote";             // 0–127 note
static constexpr auto* velocitySensitivity  = "velocitySensitivity";  // 0.0–1.0
static constexpr auto* keyRoot              = "keyRoot";              // 0–11 pitch class (SND-132)
static constexpr auto* scale                = "scale";                // Choice: Chromatic/Minor (SND-132)
static constexpr auto* glideMs              = "glideMs";              // 0–200 ms (SND-135)
static constexpr auto* velToPitch           = "velToPitch";           // 0–1 (SND-030)

// ============================================================================
// 2.14 Step Sequencer (17): #66 - #69 (+ Sync/Grid/Pattern/Chain/Clock)
// ============================================================================
static constexpr auto* sequencerBpm         = "sequencerBpm";         // 40–240 BPM
static constexpr auto* sequencerPlaying     = "sequencerPlaying";     // bool
static constexpr auto* sequencerSteps       = "sequencerSteps";       // 1–64 (SND-145)
static constexpr auto* sequencerSwing       = "sequencerSwing";       // 0–0.75 (SND-138)
static constexpr auto* sequencerSyncMode    = "sequencerSyncMode";    // Choice: Internal/Host (SND-136)
static constexpr auto* sequencerGrid        = "sequencerGrid";        // Choice: 1/16, 1/8T, 1/16T (SND-144)
static constexpr auto* sequencerPattern     = "sequencerPattern";     // Choice: A–D (SND-146)
static constexpr auto* sequencerChain       = "sequencerChain";       // 0–8 (SND-146)
static constexpr auto* sequencerChainSlotPrefix = "sequencerChainSlot_"; // 0..7, Choice A–D
static constexpr auto* sequencerClockOut    = "sequencerClockOut";    // bool (SND-150)

inline juce::String sequencerChainSlot (int i) { return juce::String (sequencerChainSlotPrefix) + juce::String (i); }

// ============================================================================
// Pro-Schritt-Parameter (bestehend 48): #70 - #117 (i = 0..15)
// stepVel_i, stepGate_i, stepPitch_i
// ----------------------------------------------------------------------------
// Neue Pro-Schritt-Parameter (SND-140..143, SND-147): je Pattern 64 Slots
// (patternStride = StepSequencer::maxSteps), 4 Patterns -> i = 0..255.
// ============================================================================
static constexpr auto* stepPrefixVel  = "stepVel_";  // 0.0–1.0
static constexpr auto* stepPrefixGate = "stepGate_"; // 0.0–1.0 (bool-ish)
static constexpr auto* stepPrefixPitch= "stepPitch_";// -24–+24 semitones

static constexpr auto* stepPrefixRatchet    = "stepRatchet_";     // 1–4
static constexpr auto* stepPrefixFlam       = "stepFlam_";        // 0–50 ms
static constexpr auto* stepPrefixProbability= "stepProbability_";// 0–1
static constexpr auto* stepPrefixLength     = "stepLength_";      // 0.25–4 Steps

inline juce::String stepVel  (int i) { return juce::String (stepPrefixVel)  + juce::String (i); }
inline juce::String stepGate (int i) { return juce::String (stepPrefixGate) + juce::String (i); }
inline juce::String stepPitch(int i) { return juce::String (stepPrefixPitch) + juce::String (i); }
inline juce::String stepRatchet    (int i) { return juce::String (stepPrefixRatchet)     + juce::String (i); }
inline juce::String stepFlam       (int i) { return juce::String (stepPrefixFlam)        + juce::String (i); }
inline juce::String stepProbability(int i) { return juce::String (stepPrefixProbability) + juce::String (i); }
inline juce::String stepLength     (int i) { return juce::String (stepPrefixLength)      + juce::String (i); }

// ============================================================================
// 2.15 Higgs Field (15): #118 - #130 (+ LFO Rate/Depth, SND-118)
// ============================================================================
static constexpr auto* higgsEnabled         = "higgsEnabled";         // bool
static constexpr auto* higgsCursorPhi       = "higgsCursorPhi";       // 0.0–1.0
static constexpr auto* higgsCursorTheta     = "higgsCursorTheta";     // 0.0–360 deg
static constexpr auto* higgsFieldStrength   = "higgsFieldStrength";   // 0.0–1.0
static constexpr auto* higgsVev             = "higgsVev";             // 0.2–0.95
static constexpr auto* higgsSelfCoupling    = "higgsSelfCoupling";    // 0.0–1.0
static constexpr auto* higgsInertia         = "higgsInertia";         // 0.0–1.0
static constexpr auto* higgsYukawaBoost     = "higgsYukawaBoost";     // 0.0–1.0
static constexpr auto* higgsBosonHz         = "higgsBosonHz";         // 40–400 Hz
static constexpr auto* higgsBosonQ          = "higgsBosonQ";          // 1–40
static constexpr auto* higgsSymmetry        = "higgsSymmetry";        // 0.0–1.0
static constexpr auto* higgsDecay           = "higgsDecay";           // 0.0–1.0
static constexpr auto* higgsMix             = "higgsMix";             // 0.0–1.0
static constexpr auto* higgsRate            = "higgsRate";            // 0.1–2.0 Hz (SND-118)
static constexpr auto* higgsDepth           = "higgsDepth";           // 0.0–1.0 (SND-118)

// ============================================================================
// 2.16 Preset / Trigger (1): #131
// ============================================================================
static constexpr auto* triggerButton        = "triggerButton";        // bool (momentary)

// ============================================================================
// 2.17 Transient Shaper (2): SND-175
// ============================================================================
static constexpr auto* transientAttack      = "transientAttack";      // 0–12 dB
static constexpr auto* transientSustain     = "transientSustain";     // 0–12 dB

// ============================================================================
// 2.18 MIDI Filter / Choke (4): SND-176
// ============================================================================
static constexpr auto* midiChannel          = "midiChannel";          // 0–16 (0 = Omni)
static constexpr auto* midiNoteLow          = "midiNoteLow";          // 0–127
static constexpr auto* midiNoteHigh         = "midiNoteHigh";         // 0–127
static constexpr auto* midiChokeMs          = "midiChokeMs";          // 0–5000 ms

// ============================================================================
// 2.19 CV / Macros (9): SND-156/157/162
// ============================================================================
static constexpr auto* lfoRate              = "lfoRate";              // 0.05–20 Hz
static constexpr auto* lfoDepth             = "lfoDepth";             // 0–1
static constexpr auto* envFollowAttack      = "envFollowAttack";      // 1–500 ms
static constexpr auto* envFollowRelease     = "envFollowRelease";     // 1–500 ms
static constexpr auto* macroPrefix          = "macro";                // macro1..macro5, 0–1

inline juce::String macro (int i) { return juce::String (macroPrefix) + juce::String (i); }

// ============================================================================
// 2.20 Performance / FX (4): SND-177/178/179/181
// ============================================================================
static constexpr auto* velToClick           = "velToClick";           // 0–1
static constexpr auto* humanize             = "humanize";             // 0–1
static constexpr auto* oversampleEnabled    = "oversampleEnabled";    // bool
static constexpr auto* fxWidth              = "fxWidth";              // 0–1

// ============================================================================
// 2.21 Material Room / Sub Grumble (15): MATERIAL_ROOM_SPEC.md §3
// Ans Ende aller IDs angehängt, damit bestehende Indizes stabil bleiben.
// ============================================================================
static constexpr auto* materialRoomEnabled  = "materialRoomEnabled";  // bool
static constexpr auto* materialRoomMaterial = "materialRoomMaterial"; // Choice: Water/Wood/Plastic/Steel/Concrete/Fabric
static constexpr auto* materialRoomSize     = "materialRoomSize";     // 1–30 m
static constexpr auto* materialRoomMix      = "materialRoomMix";      // 0–1
static constexpr auto* materialRoomLayer    = "materialRoomLayer";    // Choice: Kick/Rumble/Both
static constexpr auto* materialRoomWidth    = "materialRoomWidth";    // 0–1
static constexpr auto* grumbleLevel         = "grumbleLevel";         // 0–1
static constexpr auto* grumbleFreq          = "grumbleFreq";          // 20–120 Hz
static constexpr auto* grumbleRate          = "grumbleRate";          // 0.05–8 Hz
static constexpr auto* grumbleDepth         = "grumbleDepth";         // 0–1
static constexpr auto* grumblePanA          = "grumblePanA";          // -1–+1
static constexpr auto* grumblePanB          = "grumblePanB";          // -1–+1
static constexpr auto* grumbleWave          = "grumbleWave";          // Choice: Sine/Triangle/Square
static constexpr auto* grumblePanWave       = "grumblePanWave";       // Choice: Sine/Triangle/Square/Saw
static constexpr auto* grumbleDecay         = "grumbleDecay";         // 0.1–2 s

// ============================================================================
// 2.22 Feature-Findings (3): SND-068/174
// Ans Ende aller IDs angehängt, damit bestehende Indizes stabil bleiben.
// ============================================================================
static constexpr auto* tubeTrack             = "tubeTrack";              // bool (SND-068)
static constexpr auto* duckDepth             = "duckDepth";              // 0–24 dB (SND-174)
static constexpr auto* duckRelease           = "duckRelease";            // 20–500 ms (SND-174)

// ============================================================================
// 2.23 Mixer (13 Kanäle x 6 Parameter = 78 + 1 globale Sidechain-Tiefe)
// pro Sound-Modul ein Kanal. Gain 0–2 (1=neutral), Low/MidLow/Mid/High in dB.
// Ans Ende angehängt, um bestehende Indizes stabil zu halten.
// ============================================================================
static constexpr int mixerChannelCount = 13;
inline juce::String mixerId (int channel, const char* param) { return juce::String ("mixer") + juce::String (channel) + param; }
static constexpr auto* mixerSidechainDepth = "mixerSidechainDepth"; // 0–1, globale Sidechain-Tiefe

// ============================================================================
// 2.24 Automatisches Layering (1 Bool): blendet zwei vollstaendig aus den
// Modul-Parametern abgeleitete Zusatz-Layer ein (Depth + Presence).
// Ans Ende angehaengt, damit bestehende Indizes stabil bleiben.
// ============================================================================
static constexpr auto* layeringEnabled = "layeringEnabled"; // bool

/** Reihenfolge der Mixer-Parameter je Kanal (Gain, Low, MidLow, Mid, High, Mute). */
inline constexpr std::array<const char*, 6> mixerParamOrder { "Gain", "Low", "MidLow", "Mid", "High", "Mute" };

/** Statische ID-Tabelle (allokationsfrei für den Audio-Thread). */
inline const std::vector<std::string>& mixerParamIdTable()
{
    static const std::vector<std::string> table = []
    {
        std::vector<std::string> v;
        v.reserve ((size_t) mixerChannelCount * mixerParamOrder.size());

        for (int i = 0; i < mixerChannelCount; ++i)
            for (const auto* p : mixerParamOrder)
                v.push_back (mixerId (i, p).toStdString());

        return v;
    }();
    return table;
}


// ============================================================================
// Alle Parameter-IDs in APVTS-Erstellungsreihenfolge
// (#132 - #143: Modul-Enable-IDs. Sie sind bei den jeweiligen Modulen
//  deklariert, werden aber ans Ende einsortiert, damit bestehende
//  Preset-/State-Indizes stabil bleiben. Danach folgen #1211 - #1225:
//  Material Room / Sub Grumble, ebenfalls nur angehängt. Als letzte drei
//  folgen #1226 - #1228: tubeTrack / duckDepth / duckRelease.)
// ============================================================================
inline const std::vector<juce::String>& getAllIDs()
{
    static const std::vector<juce::String> ids = []()
    {
        std::vector<juce::String> v;
        // Membrane (8)
        v.push_back (membraneTension);      v.push_back (membraneRadius);
        v.push_back (membraneMass);         v.push_back (membraneDamping);
        v.push_back (membraneModes);        v.push_back (fundamentalHz);
        v.push_back (membraneNonlinearity); v.push_back (membraneStiffness);
        // Tube (5)
        v.push_back (tubeResonanceFreq);    v.push_back (tubeResonanceQ);
        v.push_back (tubeResonanceLevel);   v.push_back (tubeCoupling);
        v.push_back (hollowMetallic);
        // Pitch Envelope (4)
        v.push_back (pitchEnvAttack);       v.push_back (pitchEnvDecay);
        v.push_back (pitchEnvDepth);        v.push_back (pitchEnvCurve);
        // Click (5)
        v.push_back (clickLevel);           v.push_back (clickDecay);
        v.push_back (clickTone);            v.push_back (clickDistortion);
        v.push_back (clickBeaterMaterial);
        // Body (4)
        v.push_back (bodyLevel);            v.push_back (bodyAttack);
        v.push_back (bodyDecay);            v.push_back (bodyTuning);
        // Sub (5)
        v.push_back (subLevel);             v.push_back (subDecay);
        v.push_back (subDetune);            v.push_back (subShape);
        v.push_back (subRatio);
        // Drive (4)
        v.push_back (driveAmount);          v.push_back (driveMix);
        v.push_back (driveCharacter);       v.push_back (drivePreHP);
        // EQ (7)
        v.push_back (eqSubFreq);            v.push_back (eqSubGain);
        v.push_back (eqPunchFreq);          v.push_back (eqPunchGain);
        v.push_back (eqAirFreq);            v.push_back (eqAirGain);
        v.push_back (eqRumbleHP);
        // Compressor (8)
        v.push_back (compThreshold);        v.push_back (compRatio);
        v.push_back (compAttack);           v.push_back (compRelease);
        v.push_back (compKnee);             v.push_back (compMakeup);
        v.push_back (compMix);              v.push_back (compAutoMakeup);
        // Output (6)
        v.push_back (outputGain);           v.push_back (outputPan);
        v.push_back (outputWidth);          v.push_back (outputMonoBelow);
        v.push_back (outputClip);           v.push_back (outputLimiter);
        // Room (3)
        v.push_back (roomLevel);            v.push_back (roomDecay);
        v.push_back (roomRumble);
        // Reverb (10)
        v.push_back (reverbMaterial);       v.push_back (reverbPreDelay);
        v.push_back (reverbDecay);          v.push_back (reverbSize);
        v.push_back (reverbDamping);        v.push_back (reverbMix);
        v.push_back (reverbLowCut);         v.push_back (reverbHighCut);
        v.push_back (reverbFreeze);         v.push_back (reverbGate);
        // Global / Tuning (6)
        v.push_back (rootNote);             v.push_back (velocitySensitivity);
        v.push_back (keyRoot);              v.push_back (scale);
        v.push_back (glideMs);              v.push_back (velToPitch);
        // Sequencer (17)
        v.push_back (sequencerBpm);         v.push_back (sequencerPlaying);
        v.push_back (sequencerSteps);       v.push_back (sequencerSwing);
        v.push_back (sequencerSyncMode);    v.push_back (sequencerGrid);
        v.push_back (sequencerPattern);     v.push_back (sequencerChain);
        for (int i = 0; i < 8; ++i)
            v.push_back (sequencerChainSlot (i));
        v.push_back (sequencerClockOut);
        // Step-Parameter (48 bestehende)
        for (int i = 0; i < 16; ++i)
        {
            v.push_back (stepVel  (i));
            v.push_back (stepGate (i));
            v.push_back (stepPitch(i));
        }
        // Neue Step-Parameter: 4 Patterns x 64 Slots (patternStride) = 256
        for (int i = 0; i < 4 * 64; ++i)
        {
            v.push_back (stepRatchet    (i));
            v.push_back (stepFlam       (i));
            v.push_back (stepProbability(i));
            v.push_back (stepLength     (i));
        }
        // Higgs (15)
        v.push_back (higgsEnabled);         v.push_back (higgsCursorPhi);
        v.push_back (higgsCursorTheta);     v.push_back (higgsFieldStrength);
        v.push_back (higgsVev);             v.push_back (higgsSelfCoupling);
        v.push_back (higgsInertia);         v.push_back (higgsYukawaBoost);
        v.push_back (higgsBosonHz);         v.push_back (higgsBosonQ);
        v.push_back (higgsSymmetry);        v.push_back (higgsDecay);
        v.push_back (higgsMix);             v.push_back (higgsRate);
        v.push_back (higgsDepth);
        // Trigger (1)
        v.push_back (triggerButton);
        // Transient Shaper (2)
        v.push_back (transientAttack);      v.push_back (transientSustain);
        // MIDI (4)
        v.push_back (midiChannel);          v.push_back (midiNoteLow);
        v.push_back (midiNoteHigh);         v.push_back (midiChokeMs);
        // CV / Macros (9)
        v.push_back (lfoRate);              v.push_back (lfoDepth);
        v.push_back (envFollowAttack);      v.push_back (envFollowRelease);
        for (int i = 1; i <= 5; ++i)
            v.push_back (macro (i));
        // Performance / FX (4)
        v.push_back (velToClick);           v.push_back (humanize);
        v.push_back (oversampleEnabled);    v.push_back (fxWidth);

        // Module Enable (12; higgsEnabled ist bereits in der Higgs-Liste)
        v.push_back (membraneEnabled);
        v.push_back (tubeEnabled);
        v.push_back (pitchEnvEnabled);
        v.push_back (clickEnabled);
        v.push_back (bodyEnabled);
        v.push_back (subEnabled);
        v.push_back (driveEnabled);
        v.push_back (eqEnabled);
        v.push_back (compEnabled);
        v.push_back (outputEnabled);
        v.push_back (roomEnabled);
        v.push_back (reverbEnabled);

        // Material Room / Sub Grumble (15; MATERIAL_ROOM_SPEC.md §3).
        // Steht nach den Modul-Enables, damit alle bestehenden Indizes
        // unverändert bleiben - nie umsortieren.
        v.push_back (materialRoomEnabled);
        v.push_back (materialRoomMaterial);
        v.push_back (materialRoomSize);
        v.push_back (materialRoomMix);
        v.push_back (materialRoomLayer);
        v.push_back (materialRoomWidth);
        v.push_back (grumbleLevel);
        v.push_back (grumbleFreq);
        v.push_back (grumbleRate);
        v.push_back (grumbleDepth);
        v.push_back (grumblePanA);
        v.push_back (grumblePanB);
        v.push_back (grumbleWave);
        v.push_back (grumblePanWave);
        v.push_back (grumbleDecay);
        // Feature-Findings (3, SND-068/174). Ganz am Ende, Indizes stabil.
        v.push_back (tubeTrack);
        v.push_back (duckDepth);
        v.push_back (duckRelease);

        // Mixer (13 Kanäle × 6 Parameter = 78 + 1 globale Sidechain-Tiefe).
        // Nach allen bestehenden IDs angehängt.
        for (int i = 0; i < mixerChannelCount; ++i)
        {
            v.push_back (mixerId (i, "Gain"));
            v.push_back (mixerId (i, "Low"));
            v.push_back (mixerId (i, "MidLow"));
            v.push_back (mixerId (i, "Mid"));
            v.push_back (mixerId (i, "High"));
            v.push_back (mixerId (i, "Mute"));
        }
        v.push_back (mixerSidechainDepth);
        // Automatisches Layering (1 Bool). Ganz am Ende, Indizes stabil.
        v.push_back (layeringEnabled);
        return v;
    }();
    return ids;
}

} // namespace ParameterIDs
