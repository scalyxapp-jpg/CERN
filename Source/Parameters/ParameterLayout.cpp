#include "ParameterLayout.h"

//==============================================================================
namespace
{
std::unique_ptr<juce::RangedAudioParameter> addFloat (const juce::String& id,
                                                      const juce::String& name,
                                                      juce::NormalisableRange<float> range,
                                                      float defaultValue)
{
    return std::make_unique<juce::AudioParameterFloat> (id, name, range, defaultValue);
}

std::unique_ptr<juce::RangedAudioParameter> addInt (const juce::String& id,
                                                    const juce::String& name,
                                                    int minValue, int maxValue,
                                                    int defaultValue)
{
    return std::make_unique<juce::AudioParameterInt> (id, name, minValue, maxValue, defaultValue);
}
} // namespace

//==============================================================================
juce::AudioProcessorValueTreeState::ParameterLayout createLayout()
{
    juce::AudioProcessorValueTreeState::ParameterLayout layout;

    // ------------------------------------------------------------------
    // 2.1 Membrane / Physical Model (8)
    // ------------------------------------------------------------------
    layout.add (addFloat (ParameterIDs::membraneTension,      "Membrane Tension",      { 0.0f, 1.0f, 0.01f }, DefaultValues::membraneTension));
    layout.add (addFloat (ParameterIDs::membraneRadius,       "Membrane Radius",       { 0.05f, 0.5f, 0.01f, 0.5f }, DefaultValues::membraneRadius));
    layout.add (addFloat (ParameterIDs::membraneMass,         "Membrane Mass",         { 0.1f, 2.0f, 0.01f, 0.5f }, DefaultValues::membraneMass));
    layout.add (addFloat (ParameterIDs::membraneDamping,      "Membrane Damping",      { 0.0f, 1.0f, 0.01f }, DefaultValues::membraneDamping));
    layout.add (addFloat (ParameterIDs::membraneModes,        "Membrane Modes",        { 1.0f, 12.0f, 1.0f }, DefaultValues::membraneModes));
    layout.add (addFloat (ParameterIDs::fundamentalHz,        "Fundamental Hz",        { 25.0f, 180.0f, 0.01f, 0.5f }, DefaultValues::fundamentalHz));
    layout.add (addFloat (ParameterIDs::membraneNonlinearity, "Membrane Nonlinearity", { 0.0f, 1.0f, 0.01f }, DefaultValues::membraneNonlinearity));
    layout.add (addFloat (ParameterIDs::membraneStiffness,    "Membrane Stiffness",    { 0.0f, 1.0f, 0.01f }, DefaultValues::membraneStiffness));

    // ------------------------------------------------------------------
    // 2.2 Hollow Steel / Tube Resonance (5)
    // ------------------------------------------------------------------
    layout.add (addFloat (ParameterIDs::tubeResonanceFreq,    "Tube Resonance Freq",   { 40.0f, 400.0f, 0.1f, 0.4f }, DefaultValues::tubeResonanceFreq));
    layout.add (addFloat (ParameterIDs::tubeResonanceQ,       "Tube Resonance Q",      { 0.5f, 20.0f, 0.1f, 0.5f }, DefaultValues::tubeResonanceQ));
    layout.add (addFloat (ParameterIDs::tubeResonanceLevel,   "Tube Resonance Level",  { 0.0f, 1.0f, 0.01f }, DefaultValues::tubeResonanceLevel));
    layout.add (addFloat (ParameterIDs::tubeCoupling,         "Tube Coupling",         { 0.0f, 1.0f, 0.01f }, DefaultValues::tubeCoupling));
    layout.add (addFloat (ParameterIDs::hollowMetallic,       "Hollow Metallic",       { 0.0f, 1.0f, 0.01f }, DefaultValues::hollowMetallic));

    // ------------------------------------------------------------------
    // 2.3 Pitch Envelope (4)
    // ------------------------------------------------------------------
    layout.add (addFloat (ParameterIDs::pitchEnvAttack,       "Pitch Attack",          { 0.1f, 50.0f, 0.1f, 0.5f }, DefaultValues::pitchEnvAttack));
    layout.add (addFloat (ParameterIDs::pitchEnvDecay,        "Pitch Decay",           { 20.0f, 1500.0f, 1.0f, 0.5f }, DefaultValues::pitchEnvDecay));
    layout.add (addFloat (ParameterIDs::pitchEnvDepth,        "Pitch Depth",           { 0.0f, 2.0f, 0.01f }, DefaultValues::pitchEnvDepth));
    layout.add (addFloat (ParameterIDs::pitchEnvCurve,        "Pitch Curve",           { -1.0f, 1.0f, 0.01f }, DefaultValues::pitchEnvCurve));

    // ------------------------------------------------------------------
    // 2.4 Click / Beater (5)
    // ------------------------------------------------------------------
    layout.add (addFloat (ParameterIDs::clickLevel,           "Click Level",           { 0.0f, 2.0f, 0.01f }, DefaultValues::clickLevel));
    layout.add (addFloat (ParameterIDs::clickDecay,           "Click Decay",           { 0.5f, 30.0f, 0.1f, 0.5f }, DefaultValues::clickDecay));
    layout.add (addFloat (ParameterIDs::clickTone,            "Click Tone",            { 0.0f, 1.0f, 0.01f }, DefaultValues::clickTone));
    layout.add (addFloat (ParameterIDs::clickDistortion,      "Click Distortion",      { 0.0f, 1.0f, 0.01f }, DefaultValues::clickDistortion));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ParameterIDs::clickBeaterMaterial, "Click Beater Material",
        juce::StringArray { "Soft", "Medium", "Hard" },
        (int) DefaultValues::clickBeaterMaterial));

    // ------------------------------------------------------------------
    // 2.5 Body (4)
    // ------------------------------------------------------------------
    layout.add (addFloat (ParameterIDs::bodyLevel,            "Body Level",            { 0.0f, 1.0f, 0.01f }, DefaultValues::bodyLevel));
    layout.add (addFloat (ParameterIDs::bodyAttack,           "Body Attack",           { 0.5f, 10.0f, 0.1f, 0.5f }, DefaultValues::bodyAttack));
    layout.add (addFloat (ParameterIDs::bodyDecay,            "Body Decay",            { 50.0f, 4000.0f, 1.0f, 0.5f }, DefaultValues::bodyDecay));
    layout.add (addFloat (ParameterIDs::bodyTuning,           "Body Tuning",           { 0.5f, 2.0f, 0.01f, 0.5f }, DefaultValues::bodyTuning));

    // ------------------------------------------------------------------
    // 2.6 Sub Oscillator (4)
    // ------------------------------------------------------------------
    layout.add (addFloat (ParameterIDs::subLevel,             "Sub Level",             { 0.0f, 1.0f, 0.01f }, DefaultValues::subLevel));
    layout.add (addFloat (ParameterIDs::subDecay,             "Sub Decay",             { 50.0f, 3000.0f, 1.0f, 0.5f }, DefaultValues::subDecay));
    layout.add (addFloat (ParameterIDs::subDetune,            "Sub Detune",            { -50.0f, 50.0f, 0.01f }, DefaultValues::subDetune));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ParameterIDs::subShape, "Sub Shape",
        juce::StringArray { "Sine", "Triangle", "Square" },
        (int) DefaultValues::subShape));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ParameterIDs::subRatio, "Sub Ratio",
        juce::StringArray { "0.5", "1.0" },
        1 /* 1.0 -> Default-Ratio (SND-049) */));

    // ------------------------------------------------------------------
    // 2.7 Drive / Saturation (4)
    // ------------------------------------------------------------------
    layout.add (addFloat (ParameterIDs::driveAmount,          "Drive Amount",          { 0.0f, 1.0f, 0.01f }, DefaultValues::driveAmount));
    layout.add (addFloat (ParameterIDs::driveMix,             "Drive Mix",             { 0.0f, 1.0f, 0.01f }, DefaultValues::driveMix));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ParameterIDs::driveCharacter, "Drive Character",
        juce::StringArray { "Tube", "Diode", "Fuzz", "Wave Fold", "Alias" },
        (int) DefaultValues::driveCharacter));
    layout.add (addFloat (ParameterIDs::drivePreHP,           "Drive Pre-Highpass",    { 0.0f, 1.0f, 0.01f }, DefaultValues::drivePreHP));

    // ------------------------------------------------------------------
    // 2.8 EQ (7)
    // ------------------------------------------------------------------
    layout.add (addFloat (ParameterIDs::eqSubFreq,            "EQ Sub Freq",           { 20.0f, 200.0f, 0.1f, 0.4f }, DefaultValues::eqSubFreq));
    layout.add (addFloat (ParameterIDs::eqSubGain,            "EQ Sub Gain",           { -12.0f, 12.0f, 0.1f }, DefaultValues::eqSubGain));
    layout.add (addFloat (ParameterIDs::eqPunchFreq,          "EQ Punch Freq",         { 500.0f, 8000.0f, 1.0f, 0.4f }, DefaultValues::eqPunchFreq));
    layout.add (addFloat (ParameterIDs::eqPunchGain,          "EQ Punch Gain",         { -12.0f, 12.0f, 0.1f }, DefaultValues::eqPunchGain));
    layout.add (addFloat (ParameterIDs::eqAirFreq,            "EQ Air Freq",           { 4000.0f, 16000.0f, 10.0f, 0.4f }, DefaultValues::eqAirFreq));
    layout.add (addFloat (ParameterIDs::eqAirGain,            "EQ Air Gain",           { -12.0f, 12.0f, 0.1f }, DefaultValues::eqAirGain));
    layout.add (addFloat (ParameterIDs::eqRumbleHP,           "EQ Rumble Highpass",    { 15.0f, 120.0f, 0.1f, 0.5f }, DefaultValues::eqRumbleHP));

    // ------------------------------------------------------------------
    // 2.9 Compressor (8)
    // ------------------------------------------------------------------
    layout.add (addFloat (ParameterIDs::compThreshold,        "Compressor Threshold",  { -60.0f, 0.0f, 0.1f }, DefaultValues::compThreshold));
    layout.add (addFloat (ParameterIDs::compRatio,            "Compressor Ratio",      { 1.0f, 20.0f, 0.1f }, DefaultValues::compRatio));
    layout.add (addFloat (ParameterIDs::compAttack,           "Compressor Attack",     { 0.1f, 100.0f, 0.1f, 0.5f }, DefaultValues::compAttack));
    layout.add (addFloat (ParameterIDs::compRelease,          "Compressor Release",    { 10.0f, 1000.0f, 1.0f, 0.5f }, DefaultValues::compRelease));
    layout.add (addFloat (ParameterIDs::compKnee,             "Compressor Knee",       { 0.0f, 12.0f, 0.1f }, DefaultValues::compKnee));
    layout.add (addFloat (ParameterIDs::compMakeup,           "Compressor Makeup",     { -12.0f, 24.0f, 0.1f }, DefaultValues::compMakeup));
    layout.add (addFloat (ParameterIDs::compMix,              "Compressor Mix",        { 0.0f, 1.0f, 0.01f }, DefaultValues::compMix));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        ParameterIDs::compAutoMakeup, "Compressor Auto Makeup", DefaultValues::compAutoMakeup, "Auto gain reduction makeup"));

    // ------------------------------------------------------------------
    // 2.10 Output / Spatial (6)
    // ------------------------------------------------------------------
    layout.add (addFloat (ParameterIDs::outputGain,           "Output Gain",           { -60.0f, 12.0f, 0.1f }, DefaultValues::outputGain));
    layout.add (addFloat (ParameterIDs::outputPan,            "Output Pan",            { -1.0f, 1.0f, 0.01f }, DefaultValues::outputPan));
    layout.add (addFloat (ParameterIDs::outputWidth,          "Output Width",          { 0.0f, 1.0f, 0.01f }, DefaultValues::outputWidth));
    layout.add (addFloat (ParameterIDs::outputMonoBelow,      "Output Mono Below",     { 40.0f, 200.0f, 1.0f, 0.5f }, DefaultValues::outputMonoBelow));
    layout.add (addFloat (ParameterIDs::outputClip,           "Output Clip",           { 0.0f, 1.0f, 0.01f }, DefaultValues::outputClip));
    layout.add (addFloat (ParameterIDs::outputLimiter,        "Output Limiter",        { 0.0f, 1.0f, 0.01f }, DefaultValues::outputLimiter));

    // ------------------------------------------------------------------
    // 2.11 Room / Late Rumble (3)
    // ------------------------------------------------------------------
    layout.add (addFloat (ParameterIDs::roomLevel,            "Room Level",            { 0.0f, 1.0f, 0.01f }, DefaultValues::roomLevel));
    layout.add (addFloat (ParameterIDs::roomDecay,            "Room Decay",            { 50.0f, 2000.0f, 1.0f, 0.5f }, DefaultValues::roomDecay));
    layout.add (addFloat (ParameterIDs::roomRumble,           "Room Rumble",           { 0.0f, 1.0f, 0.01f }, DefaultValues::roomRumble));

    // ------------------------------------------------------------------
    // 2.11 Reverb / Modulation Stage (10)
    // ------------------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ParameterIDs::reverbMaterial, "Reverb Material",
        juce::StringArray { "None", "Wood", "Metal", "Plastic", "Concrete", "Glass", "Stone" },
        (int) DefaultValues::reverbMaterial));
    layout.add (addFloat (ParameterIDs::reverbPreDelay,       "Reverb Pre-Delay",      { 0.0f, 150.0f, 0.1f, 0.5f }, DefaultValues::reverbPreDelay));
    layout.add (addFloat (ParameterIDs::reverbDecay,          "Reverb Decay (RT60)",   { 200.0f, 5000.0f, 1.0f, 0.5f }, DefaultValues::reverbDecay));
    layout.add (addFloat (ParameterIDs::reverbSize,           "Reverb Size",           { 0.0f, 1.0f, 0.01f }, DefaultValues::reverbSize));
    layout.add (addFloat (ParameterIDs::reverbDamping,        "Reverb Damping",        { 0.0f, 1.0f, 0.01f }, DefaultValues::reverbDamping));
    layout.add (addFloat (ParameterIDs::reverbMix,            "Reverb Mix",            { 0.0f, 1.0f, 0.01f }, DefaultValues::reverbMix));
    layout.add (addFloat (ParameterIDs::reverbLowCut,         "Reverb Low Cut",        { 20.0f, 500.0f, 0.1f, 0.4f }, DefaultValues::reverbLowCut));
    layout.add (addFloat (ParameterIDs::reverbHighCut,        "Reverb High Cut",       { 1000.0f, 16000.0f, 10.0f, 0.4f }, DefaultValues::reverbHighCut));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        ParameterIDs::reverbFreeze, "Reverb Freeze", DefaultValues::reverbFreeze, "Freeze reverb tail"));
    // 0 = Gate aus; aktive Gate-Zeiten 10..500 ms (SND-112/182).
    layout.add (addFloat (ParameterIDs::reverbGate,           "Reverb Gate",           { 0.0f, 500.0f, 1.0f, 0.5f }, DefaultValues::reverbGate));

    // ------------------------------------------------------------------
    // 2.12 Global / Tuning (6)
    // ------------------------------------------------------------------
    layout.add (addInt   (ParameterIDs::rootNote,             "Root Note",             0, 127, (int) DefaultValues::rootNote));
    layout.add (addFloat (ParameterIDs::velocitySensitivity,  "Velocity Sensitivity",  { 0.0f, 1.0f, 0.01f }, DefaultValues::velocitySensitivity));
    layout.add (addFloat (ParameterIDs::keyRoot,              "Key Root",              { 0.0f, 11.0f, 1.0f }, DefaultValues::keyRoot));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ParameterIDs::scale, "Scale",
        juce::StringArray { "Chromatic", "Minor" },
        DefaultValues::scale));
    layout.add (addFloat (ParameterIDs::glideMs,              "Glide",                 { 0.0f, 200.0f, 0.1f, 0.5f }, DefaultValues::glideMs));
    layout.add (addFloat (ParameterIDs::velToPitch,           "Velocity to Pitch",     { 0.0f, 1.0f, 0.01f }, DefaultValues::velToPitch));

    // ------------------------------------------------------------------
    // 2.13 Step Sequencer (17)
    // ------------------------------------------------------------------
    layout.add (addFloat (ParameterIDs::sequencerBpm,         "Sequencer BPM",         { 40.0f, 240.0f, 0.1f, 0.5f }, DefaultValues::sequencerBpm));
    // Hinweis: Der 4. Range-Parameter (0.5) ist der Skew von NormalisableRange,
    // hier also sqrt-Kennlinie: feinere Auflösung bei niedrigem BPM, gröbere
    // Schritte bei hohem BPM.
    layout.add (std::make_unique<juce::AudioParameterBool> (
        ParameterIDs::sequencerPlaying, "Sequencer Playing", false, "Start/Stop"));
    layout.add (addFloat (ParameterIDs::sequencerSteps,       "Sequencer Steps",       { 1.0f, 64.0f, 1.0f }, DefaultValues::sequencerSteps));
    layout.add (addFloat (ParameterIDs::sequencerSwing,       "Sequencer Swing",       { 0.0f, 0.75f, 0.01f }, DefaultValues::sequencerSwing));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ParameterIDs::sequencerSyncMode, "Sequencer Sync Mode",
        juce::StringArray { "Internal", "Host" },
        DefaultValues::sequencerSyncMode));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ParameterIDs::sequencerGrid, "Sequencer Grid",
        juce::StringArray { "1/16", "1/8T", "1/16T" },
        DefaultValues::sequencerGrid));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ParameterIDs::sequencerPattern, "Sequencer Pattern",
        juce::StringArray { "A", "B", "C", "D" },
        DefaultValues::sequencerPattern));
    layout.add (addFloat (ParameterIDs::sequencerChain,       "Sequencer Chain",       { 0.0f, 8.0f, 1.0f }, DefaultValues::sequencerChain));
    for (int i = 0; i < 8; ++i)
        layout.add (std::make_unique<juce::AudioParameterChoice> (
            ParameterIDs::sequencerChainSlot (i), "Chain Slot " + juce::String (i + 1),
            juce::StringArray { "A", "B", "C", "D" }, 0));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        ParameterIDs::sequencerClockOut, "Sequencer MIDI Clock Out", DefaultValues::sequencerClockOut, "Send MIDI clock/start/stop"));

    // ------------------------------------------------------------------
    // Pro-Schritt-Parameter (48): stepVel_i, stepGate_i, stepPitch_i (i=0..15)
    // ------------------------------------------------------------------
    for (int i = 0; i < 16; ++i)
    {
        layout.add (addFloat (ParameterIDs::stepVel (i), "Step " + juce::String (i + 1) + " Vel",
            { 0.0f, 1.0f, 0.01f }, 1.0f)); // SND-149: alle Steps default 1.0
        layout.add (std::make_unique<juce::AudioParameterBool> (
            ParameterIDs::stepGate (i), "Step " + juce::String (i + 1) + " Gate",
            (i % 4 == 0), "Step on/off"));
        layout.add (addFloat (ParameterIDs::stepPitch (i), "Step " + juce::String (i + 1) + " Pitch",
            { -24.0f, 24.0f, 0.01f }, 0.0f));
    }

    // ------------------------------------------------------------------
    // Neue Pro-Schritt-Parameter: 4 Patterns x 64 Slots (patternStride) = 256
    // ------------------------------------------------------------------
    for (int i = 0; i < 4 * 64; ++i)
    {
        const juce::String stepName = "Step " + juce::String (i + 1);
        layout.add (addFloat (ParameterIDs::stepRatchet (i),     stepName + " Ratchet",     { 1.0f, 4.0f, 1.0f }, 1.0f));
        layout.add (addFloat (ParameterIDs::stepFlam (i),        stepName + " Flam",        { 0.0f, 50.0f, 0.1f }, 0.0f));
        layout.add (addFloat (ParameterIDs::stepProbability (i), stepName + " Probability", { 0.0f, 1.0f, 0.01f }, 1.0f));
        layout.add (addFloat (ParameterIDs::stepLength (i),      stepName + " Length",      { 0.25f, 4.0f, 0.01f }, 1.0f));
    }

    // ------------------------------------------------------------------
    // 2.14 Higgs Field (15)
    // ------------------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterBool> (
        ParameterIDs::higgsEnabled, "Higgs Enabled", false, "Higgs Effect"));
    layout.add (addFloat (ParameterIDs::higgsCursorPhi,       "Higgs Cursor Phi",      { 0.0f, 1.0f, 0.001f }, DefaultValues::higgsCursorPhi));
    layout.add (addFloat (ParameterIDs::higgsCursorTheta,     "Higgs Cursor Theta",    { 0.0f, 360.0f, 0.1f }, DefaultValues::higgsCursorTheta));
    layout.add (addFloat (ParameterIDs::higgsFieldStrength,   "Higgs Field Strength",  { 0.0f, 1.0f, 0.01f }, DefaultValues::higgsFieldStrength));
    layout.add (addFloat (ParameterIDs::higgsVev,             "Higgs VEV",             { 0.2f, 0.95f, 0.001f }, DefaultValues::higgsVev));
    layout.add (addFloat (ParameterIDs::higgsSelfCoupling,    "Higgs Self Coupling",   { 0.0f, 1.0f, 0.01f }, DefaultValues::higgsSelfCoupling));
    layout.add (addFloat (ParameterIDs::higgsInertia,         "Higgs Inertia",         { 0.0f, 1.0f, 0.01f }, DefaultValues::higgsInertia));
    layout.add (addFloat (ParameterIDs::higgsYukawaBoost,     "Higgs Yukawa Boost",    { 0.0f, 1.0f, 0.01f }, DefaultValues::higgsYukawaBoost));
    layout.add (addFloat (ParameterIDs::higgsBosonHz,         "Higgs Boson Hz",        { 40.0f, 400.0f, 0.01f, 0.4f }, DefaultValues::higgsBosonHz));
    layout.add (addFloat (ParameterIDs::higgsBosonQ,          "Higgs Boson Q",         { 1.0f, 40.0f, 0.1f }, DefaultValues::higgsBosonQ));
    layout.add (addFloat (ParameterIDs::higgsSymmetry,        "Higgs Symmetry",        { 0.0f, 1.0f, 0.01f }, DefaultValues::higgsSymmetry));
    layout.add (addFloat (ParameterIDs::higgsDecay,           "Higgs Decay",           { 0.0f, 1.0f, 0.01f }, DefaultValues::higgsDecay));
    layout.add (addFloat (ParameterIDs::higgsMix,             "Higgs Mix",             { 0.0f, 1.0f, 0.01f }, DefaultValues::higgsMix));
    layout.add (addFloat (ParameterIDs::higgsRate,            "Higgs LFO Rate",        { 0.1f, 2.0f, 0.001f, 0.5f }, DefaultValues::higgsRate));
    layout.add (addFloat (ParameterIDs::higgsDepth,           "Higgs LFO Depth",       { 0.0f, 1.0f, 0.01f }, DefaultValues::higgsDepth));

    // ------------------------------------------------------------------
    // Trigger (1) - muss in getAllIDs() an gleicher Position stehen
    // ------------------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterBool> (
        ParameterIDs::triggerButton, "Trigger", false, "Manual trigger (momentary)"));

    // ------------------------------------------------------------------
    // 2.17 Transient Shaper (2)
    // ------------------------------------------------------------------
    layout.add (addFloat (ParameterIDs::transientAttack,      "Transient Attack",      { 0.0f, 12.0f, 0.1f }, DefaultValues::transientAttack));
    layout.add (addFloat (ParameterIDs::transientSustain,     "Transient Sustain",     { 0.0f, 12.0f, 0.1f }, DefaultValues::transientSustain));

    // ------------------------------------------------------------------
    // 2.18 MIDI Filter / Choke (4)
    // ------------------------------------------------------------------
    layout.add (addInt   (ParameterIDs::midiChannel,          "MIDI Channel",          0, 16, DefaultValues::midiChannel));
    layout.add (addInt   (ParameterIDs::midiNoteLow,          "MIDI Note Low",         0, 127, DefaultValues::midiNoteLow));
    layout.add (addInt   (ParameterIDs::midiNoteHigh,         "MIDI Note High",        0, 127, DefaultValues::midiNoteHigh));
    layout.add (addFloat (ParameterIDs::midiChokeMs,          "MIDI Choke",            { 0.0f, 5000.0f, 1.0f, 0.5f }, DefaultValues::midiChokeMs));

    // ------------------------------------------------------------------
    // 2.19 CV / Macros (9)
    // ------------------------------------------------------------------
    layout.add (addFloat (ParameterIDs::lfoRate,              "LFO Rate",              { 0.05f, 20.0f, 0.001f, 0.35f }, DefaultValues::lfoRate));
    layout.add (addFloat (ParameterIDs::lfoDepth,             "LFO Depth",             { 0.0f, 1.0f, 0.01f }, DefaultValues::lfoDepth));
    layout.add (addFloat (ParameterIDs::envFollowAttack,      "Env Follow Attack",     { 1.0f, 500.0f, 1.0f, 0.5f }, DefaultValues::envFollowAttack));
    layout.add (addFloat (ParameterIDs::envFollowRelease,     "Env Follow Release",    { 1.0f, 500.0f, 1.0f, 0.5f }, DefaultValues::envFollowRelease));
    for (int i = 1; i <= 5; ++i)
        layout.add (addFloat (ParameterIDs::macro (i), "Macro " + juce::String (i), { 0.0f, 1.0f, 0.01f }, 0.0f));

    // ------------------------------------------------------------------
    // 2.20 Performance / FX (4)
    // ------------------------------------------------------------------
    layout.add (addFloat (ParameterIDs::velToClick,           "Velocity to Click",     { 0.0f, 1.0f, 0.01f }, DefaultValues::velToClick));
    layout.add (addFloat (ParameterIDs::humanize,             "Humanize",              { 0.0f, 1.0f, 0.01f }, DefaultValues::humanize));
    layout.add (std::make_unique<juce::AudioParameterBool> (
        ParameterIDs::oversampleEnabled, "Oversampling", DefaultValues::oversampleEnabled, "2x oversampling for drive/clip"));
    layout.add (addFloat (ParameterIDs::fxWidth,              "FX Width",              { 0.0f, 1.0f, 0.01f }, DefaultValues::fxWidth));

    // ------------------------------------------------------------------
    // Module Enable (12; higgsEnabled ist bereits in 2.14)
    // ------------------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterIDs::membraneEnabled, "Membrane Enabled", true, "Module on/off"));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterIDs::tubeEnabled, "Tube Enabled", true, "Module on/off"));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterIDs::pitchEnvEnabled, "Pitch Envelope Enabled", true, "Module on/off"));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterIDs::clickEnabled, "Click Enabled", true, "Module on/off"));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterIDs::bodyEnabled, "Body Enabled", true, "Module on/off"));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterIDs::subEnabled, "Sub Enabled", true, "Module on/off"));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterIDs::driveEnabled, "Drive Enabled", true, "Module on/off"));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterIDs::eqEnabled, "EQ Enabled", true, "Module on/off"));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterIDs::compEnabled, "Compressor Enabled", true, "Module on/off"));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterIDs::outputEnabled, "Output Enabled", true, "Module on/off"));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterIDs::roomEnabled, "Room Enabled", true, "Module on/off"));
    layout.add (std::make_unique<juce::AudioParameterBool> (ParameterIDs::reverbEnabled, "Reverb Enabled", true, "Module on/off"));

    // ------------------------------------------------------------------
    // 2.21 Material Room / Sub Grumble (15)
    // Reihenfolge identisch zu ParameterIDs::getAllIDs() (dort am Ende).
    // ------------------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterBool> (
        ParameterIDs::materialRoomEnabled, "Material Room Enabled", DefaultValues::materialRoomEnabled, "Module on/off"));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ParameterIDs::materialRoomMaterial, "Material Room Material",
        juce::StringArray { "Water", "Wood", "Plastic", "Steel", "Concrete", "Fabric" },
        DefaultValues::materialRoomMaterial));
    layout.add (addFloat (ParameterIDs::materialRoomSize,     "Material Room Size",    { 1.0f, 30.0f, 0.1f }, DefaultValues::materialRoomSize));
    layout.add (addFloat (ParameterIDs::materialRoomMix,      "Material Room Mix",     { 0.0f, 1.0f, 0.01f }, DefaultValues::materialRoomMix));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ParameterIDs::materialRoomLayer, "Material Room Layer",
        juce::StringArray { "Kick", "Rumble", "Both" },
        DefaultValues::materialRoomLayer));
    layout.add (addFloat (ParameterIDs::materialRoomWidth,    "Material Room Width",   { 0.0f, 1.0f, 0.01f }, DefaultValues::materialRoomWidth));
    layout.add (addFloat (ParameterIDs::grumbleLevel,         "Grumble Level",         { 0.0f, 1.0f, 0.01f }, DefaultValues::grumbleLevel));
    layout.add (addFloat (ParameterIDs::grumbleFreq,          "Grumble Freq",          { 20.0f, 120.0f, 0.1f }, DefaultValues::grumbleFreq));
    layout.add (addFloat (ParameterIDs::grumbleRate,          "Grumble Rate",          { 0.05f, 8.0f, 0.01f, 0.5f }, DefaultValues::grumbleRate));
    layout.add (addFloat (ParameterIDs::grumbleDepth,         "Grumble Depth",         { 0.0f, 1.0f, 0.01f }, DefaultValues::grumbleDepth));
    layout.add (addFloat (ParameterIDs::grumblePanA,          "Grumble Pan A",         { -1.0f, 1.0f, 0.01f }, DefaultValues::grumblePanA));
    layout.add (addFloat (ParameterIDs::grumblePanB,          "Grumble Pan B",         { -1.0f, 1.0f, 0.01f }, DefaultValues::grumblePanB));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ParameterIDs::grumbleWave, "Grumble Wave",
        juce::StringArray { "Sine", "Triangle", "Square" },
        DefaultValues::grumbleWave));
    layout.add (std::make_unique<juce::AudioParameterChoice> (
        ParameterIDs::grumblePanWave, "Grumble Pan Wave",
        juce::StringArray { "Sine", "Triangle", "Square", "Saw" },
        DefaultValues::grumblePanWave));
    layout.add (addFloat (ParameterIDs::grumbleDecay,         "Grumble Decay",         { 0.1f, 2.0f, 0.01f, 0.5f }, DefaultValues::grumbleDecay));

    // ------------------------------------------------------------------
    // 2.22 Feature-Findings (3): SND-068/174
    // Reihenfolge identisch zu ParameterIDs::getAllIDs() (dort am Ende).
    // ------------------------------------------------------------------
    layout.add (std::make_unique<juce::AudioParameterBool> (
        ParameterIDs::tubeTrack, "Tube Track", DefaultValues::tubeTrack, "Tube resonance tracks f0"));
    layout.add (addFloat (ParameterIDs::duckDepth,   "Duck Depth",   { 0.0f, 24.0f, 0.1f }, DefaultValues::duckDepth));
    layout.add (addFloat (ParameterIDs::duckRelease, "Duck Release", { 20.0f, 500.0f, 1.0f }, DefaultValues::duckRelease));

    // 1199 Basis-Parameter + 12 Module-Enables + 15 Material Room + 3 Feature = 1229
    // (siehe numLayoutParameters).
    jassert (getLayoutParameterCount() == numLayoutParameters);

    return layout;
}

//==============================================================================
int getLayoutParameterCount()
{
    return static_cast<int> (ParameterIDs::getAllIDs().size());
}