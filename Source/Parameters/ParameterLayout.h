#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include "ParameterIDs.h"

//==============================================================================
/** Standardwerte für alle Parameter (entspricht TECHNICAL_WIRING_FULL.md §2). */
struct DefaultValues
{
    // Membrane
    static constexpr float membraneTension      = 0.5f;
    static constexpr float membraneRadius       = 0.22f;
    static constexpr float membraneMass         = 1.0f;
    static constexpr float membraneDamping      = 0.45f;
    static constexpr float membraneModes        = 4.0f;
    static constexpr float fundamentalHz        = 46.25f; // TV/Small-Speaker: 46 Hz sinnvoll
    static constexpr float membraneNonlinearity = 0.10f;
    static constexpr float membraneStiffness    = 0.10f;

    // Tube / Hollow
    static constexpr float tubeResonanceFreq    = 70.0f; // ~1.5x bei 46 Hz
    static constexpr float tubeResonanceQ       = 2.5f;
    static constexpr float tubeResonanceLevel   = 0.15f;
    static constexpr float tubeCoupling         = 0.30f;
    static constexpr float hollowMetallic       = 0.08f;

    // Pitch Envelope
    static constexpr float pitchEnvAttack       = 0.2f;
    static constexpr float pitchEnvDecay        = 60.0f;
    static constexpr float pitchEnvDepth        = 0.30f;
    static constexpr float pitchEnvCurve        = 0.0f;

    // Click / Beater
    static constexpr float clickLevel           = 0.38f;
    static constexpr float clickDecay           = 4.0f;
    static constexpr float clickTone            = 0.22f;
    static constexpr float clickDistortion      = 0.2f;
    static constexpr int   clickBeaterMaterial  = 1; // Medium

    // Body
    static constexpr float bodyLevel            = 0.58f;
    static constexpr float bodyAttack           = 0.8f;
    static constexpr float bodyDecay            = 500.0f;
    static constexpr float bodyTuning           = 1.0f;

    // Sub
    static constexpr float subLevel             = 0.58f;
    static constexpr float subDecay             = 520.0f;
    static constexpr float subDetune            = 0.0f;
    static constexpr int   subShape             = 0; // Sine

    // Drive
    static constexpr float driveAmount          = 0.4f;
    static constexpr float driveMix             = 0.45f;
    static constexpr int   driveCharacter       = 0; // Tube
    static constexpr float drivePreHP           = 0.0f;

    // EQ
    static constexpr float eqSubFreq            = 46.0f;
    static constexpr float eqSubGain            = 3.0f;
    static constexpr float eqPunchFreq          = 1200.0f;
    static constexpr float eqPunchGain          = 1.5f;
    static constexpr float eqAirFreq            = 10000.0f;
    static constexpr float eqAirGain            = 0.0f;
    static constexpr float eqRumbleHP           = 20.0f;

    // Compressor
    static constexpr float compThreshold        = -10.0f;
    static constexpr float compRatio            = 2.5f;
    static constexpr float compAttack           = 15.0f;
    static constexpr float compRelease          = 120.0f;
    static constexpr float compKnee             = 6.0f;
    static constexpr float compMakeup           = 0.0f;
    static constexpr float compMix              = 1.0f;
    static constexpr bool  compAutoMakeup       = false;

    // Output
    static constexpr float outputGain           = 0.0f;
    static constexpr float outputPan            = 0.0f;
    static constexpr float outputWidth          = 1.0f;
    static constexpr float outputMonoBelow      = 150.0f;
    static constexpr float outputClip           = 0.35f;
    static constexpr float outputLimiter        = 0.15f;

    // Room
    static constexpr float roomLevel            = 0.0f;
    static constexpr float roomDecay            = 350.0f;
    static constexpr float roomRumble           = 0.3f;

    // Reverb
    static constexpr int   reverbMaterial       = 0; // None = Reverb bypass
    static constexpr float reverbPreDelay       = 30.0f;
    static constexpr float reverbDecay          = 1200.0f;
    static constexpr float reverbSize           = 0.5f;
    static constexpr float reverbDamping        = 0.3f;
    static constexpr float reverbMix            = 0.15f;
    static constexpr float reverbLowCut         = 150.0f;
    static constexpr float reverbHighCut        = 6000.0f;
    static constexpr bool  reverbFreeze         = false;
    static constexpr float reverbGate           = 0.0f; // 0 = Gate aus (aktiv 10–500 ms)

    // Global / Tuning
    static constexpr float rootNote             = 30.0f; // F#1 = 46,25 Hz (SND-129)
    static constexpr float velocitySensitivity  = 0.6f;
    static constexpr float keyRoot              = 0.0f;  // C
    static constexpr int   scale                = 0;     // Chromatic
    static constexpr float glideMs              = 0.0f;
    static constexpr float velToPitch           = 0.0f;

    // Step Sequencer
    static constexpr float sequencerBpm         = 128.0f;
    static constexpr float sequencerPlaying     = 0.0f;
    static constexpr float sequencerSteps       = 16.0f;
    static constexpr float sequencerSwing       = 0.0f;
    static constexpr int   sequencerSyncMode    = 0; // Internal
    static constexpr int   sequencerGrid        = 0; // 1/16
    static constexpr int   sequencerPattern     = 0; // A
    static constexpr float sequencerChain       = 0.0f;
    static constexpr bool  sequencerClockOut    = false;

    // Higgs
    static constexpr float higgsEnabled         = 0.0f;
    static constexpr float higgsCursorPhi       = 0.5f;
    static constexpr float higgsCursorTheta     = 180.0f;
    static constexpr float higgsFieldStrength   = 0.75f;
    static constexpr float higgsVev             = 0.7f;
    static constexpr float higgsSelfCoupling    = 0.65f;
    static constexpr float higgsInertia         = 0.45f;
    static constexpr float higgsYukawaBoost     = 0.30f;
    static constexpr float higgsBosonHz         = 46.25f;
    static constexpr float higgsBosonQ          = 10.0f;
    static constexpr float higgsSymmetry        = 0.5f;
    static constexpr float higgsDecay           = 0.62f;
    static constexpr float higgsMix             = 0.12f;
    static constexpr float higgsRate            = 0.65f;
    static constexpr float higgsDepth           = 0.35f;

    // Trigger
    static constexpr float triggerButton        = 0.0f;

    // Transient Shaper
    static constexpr float transientAttack      = 0.0f;
    static constexpr float transientSustain     = 0.0f;

    // MIDI
    static constexpr int   midiChannel          = 0;    // Omni
    static constexpr int   midiNoteLow          = 0;
    static constexpr int   midiNoteHigh         = 127;
    static constexpr float midiChokeMs          = 0.0f;

    // CV / Macros
    static constexpr float lfoRate              = 0.5f;
    static constexpr float lfoDepth             = 0.5f;
    static constexpr float envFollowAttack      = 10.0f;
    static constexpr float envFollowRelease     = 100.0f;

    // Performance / FX
    static constexpr float velToClick           = 0.0f;
    static constexpr float humanize             = 0.0f;
    static constexpr bool  oversampleEnabled    = false;
    static constexpr float fxWidth              = 1.0f;

    // Material Room / Sub Grumble (MATERIAL_ROOM_SPEC.md §3)
    static constexpr bool  materialRoomEnabled  = true;
    static constexpr int   materialRoomMaterial = 1;    // Wood
    static constexpr float materialRoomSize     = 8.0f; // Meter
    static constexpr float materialRoomMix      = 0.0f; // 0 = Modul neutral
    static constexpr int   materialRoomLayer    = 2;    // Both
    static constexpr float materialRoomWidth    = 0.5f;
    static constexpr float grumbleLevel         = 0.0f;
    static constexpr float grumbleFreq          = 45.0f;
    static constexpr float grumbleRate          = 0.5f;
    static constexpr float grumbleDepth         = 0.5f;
    static constexpr float grumblePanA          = -0.5f;
    static constexpr float grumblePanB          = 0.5f;
    static constexpr int   grumbleWave          = 0;    // Sine
    static constexpr int   grumblePanWave       = 0;    // Sine
    static constexpr float grumbleDecay         = 0.5f;

    // Feature-Findings (SND-068/174)
    static constexpr bool  tubeTrack             = true;
    static constexpr float duckDepth             = 0.0f;  // dB
    static constexpr float duckRelease           = 80.0f; // ms
};

//==============================================================================
/** Anzahl der Parameter im Layout: 1199 Basis-Parameter + 12 Module-Enables
    + 15 Material Room / Sub Grumble + 3 Feature-Findings = 1229. */
inline constexpr int numLayoutParameters = 1308; // 1229 + 78 Mixer + 1 Sidechain

/** Erstellt das APVTS-Parameter-Layout mit allen 1229 Parametern. */
juce::AudioProcessorValueTreeState::ParameterLayout createLayout();

/** Anzahl der IDs in ParameterIDs::getAllIDs(); muss numLayoutParameters entsprechen. */
int getLayoutParameterCount();
