#include "PresetManager.h"

#include <algorithm>
#include <initializer_list>
#include <utility>
#include <vector>

//==============================================================================
namespace
{
    struct FactoryPreset
    {
        const char* name;
        std::vector<std::pair<juce::String, float>> values;
        std::vector<std::pair<juce::String, juce::String>> parameterPatch {};
    };

    int findParameterIndex (const juce::String& id)
    {
        const auto& ids = ParameterIDs::getAllIDs();
        const auto it = std::find (ids.begin(), ids.end(), id);
        return it == ids.end() ? -1 : static_cast<int> (std::distance (ids.begin(), it));
    }

    void setParameterValue (juce::ValueTree& state, const juce::String& id, float value)
    {
        for (int i = 0; i < state.getNumChildren(); ++i)
        {
            auto child = state.getChild (i);

            if (child.hasType ("PARAM") && child.getProperty ("id").toString() == id)
            {
                child.setProperty ("value", value, nullptr);
                return;
            }
        }
    }

    void appendPatchState (juce::XmlElement& xml, const PatchGraph& patch,
                           const ParameterPatchGraph& parameterPatch)
    {
        // Unbekannte/mitgeladene <Patch>- bzw. <ParameterPatch>-Knoten nicht verdoppeln
        xml.deleteAllChildElementsWithTagName ("Patch");
        xml.deleteAllChildElementsWithTagName ("ParameterPatch");

        auto patchXml = patch.createStateXml();

        if (patchXml != nullptr)
            xml.addChildElement (patchXml.release());

        auto parameterPatchXml = parameterPatch.createStateXml();

        if (parameterPatchXml != nullptr)
            xml.addChildElement (parameterPatchXml.release());
    }
}

//==============================================================================
PresetManager::PresetManager (juce::AudioProcessorValueTreeState& apvtsRef,
                              PatchGraph& patchGraph,
                              ParameterPatchGraph& parameterPatchGraph)
    : apvts (apvtsRef), patch (patchGraph), parameterPatch (parameterPatchGraph)
{
    initFactoryPresets();
}

void PresetManager::prepare (double /*sampleRate*/)
{
}

void PresetManager::applyModuleEnableDefaults (juce::ValueTree& state, bool enabled)
{
    // Alle Module-Enables explizit in jedem Factory-Preset setzen, damit das
    // Laden deterministisch die Modul-Power zurücksetzt und nicht davon abhängt,
    // in welchem Zustand der Live-State beim Erzeugen der Presets war.
    const float value = enabled ? 1.0f : 0.0f;

    const std::initializer_list<const char*> enableIds
    {
        ParameterIDs::membraneEnabled,
        ParameterIDs::tubeEnabled,
        ParameterIDs::pitchEnvEnabled,
        ParameterIDs::clickEnabled,
        ParameterIDs::bodyEnabled,
        ParameterIDs::subEnabled,
        ParameterIDs::driveEnabled,
        ParameterIDs::eqEnabled,
        ParameterIDs::compEnabled,
        ParameterIDs::outputEnabled,
        ParameterIDs::roomEnabled,
        ParameterIDs::reverbEnabled,
        ParameterIDs::materialRoomEnabled
    };

    for (const auto* id : enableIds)
        setParameterValue (state, id, value);

    // Der Trigger ist ein Momentary-Parameter und darf nie als "gedrückt" starten.
    setParameterValue (state, ParameterIDs::triggerButton, 0.0f);
}

void PresetManager::initFactoryPresets()
{
    factoryPresetNames.clear();
    factoryPresetXml.clear();

    // SND-006/005/020: outputGain pro Preset auf gemeinsamen Pegel kalibriert
    // (Ziel ~−13 LUFS integriert @48 kHz, Peak ~−1 dBFS; „Hard Acid Kick“ nicht
    // am +12-dB-Max). Soft-Clip deutlich reduziert (outputClip 0.05 statt 0.15);
    // Limiter fängt Restspitzen. Offline-Messung via LoudnessMeter::measureLufs
    // (4 Takte @120 bpm, 48 kHz) nach Kalibrierung; Streuung über 7 Presets ≤1,5 dB.
    std::vector<FactoryPreset> presets
    {
        { "Deep Techno Kick", {
            { ParameterIDs::fundamentalHz,       46.25f },
            { ParameterIDs::membraneTension,      0.45f },
            { ParameterIDs::pitchEnvDepth,        0.80f },
            { ParameterIDs::pitchEnvDecay,        60.0f },
            { ParameterIDs::bodyLevel,            0.70f },
            { ParameterIDs::bodyDecay,           420.0f },
            { ParameterIDs::clickLevel,           0.70f },
            { ParameterIDs::driveAmount,          0.40f },
            { ParameterIDs::driveMix,             0.45f },
            { ParameterIDs::eqSubGain,            2.0f },
            { ParameterIDs::eqPunchGain,          3.0f },
            { ParameterIDs::compThreshold,       -8.0f },
            { ParameterIDs::compRatio,            3.0f },
            { ParameterIDs::compMakeup,           0.0f },
            { ParameterIDs::outputGain,           0.3f },
            { ParameterIDs::outputClip,           0.05f },
            { ParameterIDs::roomLevel,            0.18f },
            { ParameterIDs::reverbMaterial,       4.0f },
            { ParameterIDs::reverbMix,           0.08f },
            { ParameterIDs::higgsEnabled,         1.0f },
            { ParameterIDs::higgsMix,            0.12f }
        } },
        { "Hard Acid Kick", {
            { ParameterIDs::fundamentalHz,       46.25f },
            { ParameterIDs::membraneTension,      0.7f },
            { ParameterIDs::membraneDamping,      0.35f },
            { ParameterIDs::membraneModes,        4.0f },
            { ParameterIDs::pitchEnvDepth,        0.95f },
            { ParameterIDs::pitchEnvDecay,        45.0f },
            { ParameterIDs::clickLevel,           0.55f },
            { ParameterIDs::clickTone,            0.50f },
            { ParameterIDs::clickBeaterMaterial,  2.0f },
            { ParameterIDs::bodyLevel,            0.70f },
            { ParameterIDs::bodyDecay,           350.0f },
            { ParameterIDs::driveAmount,          0.45f },
            { ParameterIDs::driveMix,             0.45f },
            { ParameterIDs::driveCharacter,       1.0f },
            { ParameterIDs::drivePreHP,           0.25f },
            { ParameterIDs::eqPunchFreq,       3000.0f },
            { ParameterIDs::eqPunchGain,          3.0f },
            { ParameterIDs::compThreshold,       -8.0f },
            { ParameterIDs::compRatio,            3.0f },
            { ParameterIDs::outputGain,           1.5f },
            { ParameterIDs::outputClip,           0.05f },
            { ParameterIDs::roomLevel,            0.15f },
            { ParameterIDs::reverbMix,           0.08f },
            { ParameterIDs::higgsEnabled,         0.0f }
        } },
        { "808 Boomy", {
            { ParameterIDs::fundamentalHz,       46.25f },
            { ParameterIDs::membraneTension,      0.25f },
            { ParameterIDs::membraneDamping,      0.14f },
            { ParameterIDs::membraneModes,        3.0f },
            { ParameterIDs::pitchEnvDepth,        0.85f },
            { ParameterIDs::pitchEnvDecay,       220.0f },
            { ParameterIDs::bodyLevel,            0.85f },
            { ParameterIDs::bodyDecay,          1800.0f },
            { ParameterIDs::clickLevel,           0.28f },
            { ParameterIDs::subLevel,             0.30f },
            { ParameterIDs::subDecay,            900.0f },
            { ParameterIDs::driveAmount,          0.20f },
            { ParameterIDs::eqSubGain,            2.0f },
            { ParameterIDs::eqRumbleHP,          22.0f },
            { ParameterIDs::compThreshold,      -10.0f },
            { ParameterIDs::compRatio,            3.0f },
            { ParameterIDs::compMakeup,           0.0f },
            { ParameterIDs::outputGain,          -3.0f },
            { ParameterIDs::outputClip,           0.05f },
            { ParameterIDs::roomLevel,            0.30f },
            { ParameterIDs::roomRumble,           0.30f },
            { ParameterIDs::reverbMaterial,       1.0f },
            { ParameterIDs::reverbMix,            0.10f },
            { ParameterIDs::higgsEnabled,         0.0f }
        } },
        { "Tight Techno", {
            { ParameterIDs::fundamentalHz,       46.25f },
            { ParameterIDs::membraneDamping,      0.32f },
            { ParameterIDs::pitchEnvDepth,        0.75f },
            { ParameterIDs::pitchEnvDecay,        50.0f },
            { ParameterIDs::clickLevel,           0.55f },
            { ParameterIDs::clickTone,            0.42f },
            { ParameterIDs::bodyDecay,           320.0f },
            { ParameterIDs::driveAmount,          0.35f },
            { ParameterIDs::compThreshold,       -8.0f },
            { ParameterIDs::compRatio,            3.0f },
            { ParameterIDs::compAttack,           8.0f },
            { ParameterIDs::compRelease,        110.0f },
            { ParameterIDs::outputGain,           0.8f },
            { ParameterIDs::outputClip,           0.05f },
            { ParameterIDs::roomLevel,           0.12f },
            { ParameterIDs::reverbMaterial,       0.0f },
            { ParameterIDs::higgsEnabled,         1.0f },
            { ParameterIDs::higgsMix,            0.10f }
        } }
    };

    // Drei zusätzliche C.E.R.N-Presets mit Parameter-CV-Modulation und Higgs-Feld.
    presets.push_back ({ "CV Pulse Kick", {
        { ParameterIDs::fundamentalHz,       46.25f },
        { ParameterIDs::membraneTension,      0.30f },
        { ParameterIDs::pitchEnvDepth,        0.85f },
        { ParameterIDs::pitchEnvDecay,        55.0f },
        { ParameterIDs::bodyLevel,            0.70f },
        { ParameterIDs::bodyDecay,           520.0f },
        { ParameterIDs::clickLevel,           0.60f },
        { ParameterIDs::clickTone,            0.45f },
        { ParameterIDs::clickBeaterMaterial,  2.0f },
        { ParameterIDs::driveAmount,          0.45f },
        { ParameterIDs::driveMix,             0.50f },
        { ParameterIDs::driveCharacter,       1.0f },
        { ParameterIDs::eqPunchFreq,       2000.0f },
        { ParameterIDs::eqPunchGain,          2.0f },
        { ParameterIDs::compThreshold,       -8.0f },
        { ParameterIDs::compRatio,            3.0f },
        { ParameterIDs::compMakeup,           0.0f },
        { ParameterIDs::outputGain,          -2.5f },
        { ParameterIDs::outputClip,           0.05f },
        { ParameterIDs::roomLevel,            0.12f },
        { ParameterIDs::reverbMix,           0.08f },
        { ParameterIDs::higgsEnabled,         1.0f },
        { ParameterIDs::higgsMix,            0.12f },
        { ParameterIDs::higgsFieldStrength,   0.60f },
        { ParameterIDs::higgsSelfCoupling,    0.40f },
        { ParameterIDs::higgsInertia,         0.40f },
        { ParameterIDs::higgsYukawaBoost,     0.50f },
        { ParameterIDs::higgsBosonHz,        92.50f },
        { ParameterIDs::higgsBosonQ,          8.0f },
        { ParameterIDs::higgsSymmetry,        0.30f },
        { ParameterIDs::higgsDecay,           0.60f },
        { ParameterIDs::higgsVev,             0.75f },
        { ParameterIDs::higgsCursorPhi,       0.35f },
        { ParameterIDs::higgsCursorTheta,     90.0f }
    }, {
        { ParameterIDs::bodyLevel, ParameterIDs::driveMix }
    } });

    presets.push_back ({ "Higgs Mod Kick", {
        { ParameterIDs::fundamentalHz,       46.25f },
        { ParameterIDs::membraneTension,      0.65f },
        { ParameterIDs::membraneDamping,      0.24f },
        { ParameterIDs::membraneModes,        4.0f },
        { ParameterIDs::pitchEnvDepth,        0.85f },
        { ParameterIDs::pitchEnvDecay,        90.0f },
        { ParameterIDs::bodyLevel,            0.70f },
        { ParameterIDs::bodyDecay,           700.0f },
        { ParameterIDs::clickLevel,           0.55f },
        { ParameterIDs::subLevel,             0.30f },
        { ParameterIDs::subDecay,            600.0f },
        { ParameterIDs::driveAmount,          0.25f },
        { ParameterIDs::driveMix,             0.35f },
        { ParameterIDs::eqSubGain,            2.0f },
        { ParameterIDs::eqRumbleHP,          22.0f },
        { ParameterIDs::compThreshold,      -10.0f },
        { ParameterIDs::compRatio,            3.0f },
        { ParameterIDs::compMakeup,           0.0f },
        { ParameterIDs::outputGain,          -3.5f },
        { ParameterIDs::outputClip,           0.05f },
        { ParameterIDs::roomLevel,            0.30f },
        { ParameterIDs::roomRumble,           0.30f },
        { ParameterIDs::reverbMaterial,       1.0f },
        { ParameterIDs::reverbMix,            0.10f },
        { ParameterIDs::higgsEnabled,         1.0f },
        { ParameterIDs::higgsMix,            0.15f },
        { ParameterIDs::higgsFieldStrength,   0.80f },
        { ParameterIDs::higgsSelfCoupling,    0.70f },
        { ParameterIDs::higgsInertia,         0.80f },
        { ParameterIDs::higgsYukawaBoost,     0.60f },
        { ParameterIDs::higgsBosonHz,        92.50f },
        { ParameterIDs::higgsBosonQ,          8.0f },
        { ParameterIDs::higgsSymmetry,        0.20f },
        { ParameterIDs::higgsDecay,           0.70f },
        { ParameterIDs::higgsVev,             0.75f },
        { ParameterIDs::higgsCursorPhi,       0.65f },
        { ParameterIDs::higgsCursorTheta,    220.0f }
    }, {
        { ParameterIDs::higgsSelfCoupling, ParameterIDs::higgsFieldStrength },
        { ParameterIDs::higgsBosonQ, ParameterIDs::higgsMix },
        { ParameterIDs::subLevel, ParameterIDs::roomRumble },
        { ParameterIDs::membraneTension, ParameterIDs::higgsSymmetry }
    } });

    presets.push_back ({ "Parameter Patch Kick", {
        { ParameterIDs::fundamentalHz,       46.25f },
        { ParameterIDs::membraneTension,      0.40f },
        { ParameterIDs::pitchEnvDepth,        0.70f },
        { ParameterIDs::pitchEnvDecay,        60.0f },
        { ParameterIDs::bodyLevel,            0.60f },
        { ParameterIDs::bodyDecay,           500.0f },
        { ParameterIDs::clickLevel,           0.60f },
        { ParameterIDs::clickTone,            0.45f },
        { ParameterIDs::subLevel,             0.28f },
        { ParameterIDs::subDecay,            500.0f },
        { ParameterIDs::driveAmount,          0.30f },
        { ParameterIDs::driveMix,             0.35f },
        { ParameterIDs::driveCharacter,       1.0f },
        { ParameterIDs::drivePreHP,           0.25f },
        { ParameterIDs::eqPunchFreq,       3000.0f },
        { ParameterIDs::eqPunchGain,          2.0f },
        { ParameterIDs::compThreshold,       -8.0f },
        { ParameterIDs::compRatio,            3.0f },
        { ParameterIDs::compAttack,           8.0f },
        { ParameterIDs::compRelease,        110.0f },
        { ParameterIDs::outputGain,          -2.5f },
        { ParameterIDs::outputClip,           0.05f },
        { ParameterIDs::roomLevel,            0.12f },
        { ParameterIDs::roomDecay,          250.0f },
        { ParameterIDs::reverbMaterial,       2.0f },
        { ParameterIDs::reverbMix,           0.08f },
        { ParameterIDs::higgsEnabled,         1.0f },
        { ParameterIDs::higgsMix,            0.12f },
        { ParameterIDs::higgsFieldStrength,   0.55f },
        { ParameterIDs::higgsSelfCoupling,    0.45f },
        { ParameterIDs::higgsInertia,         0.40f },
        { ParameterIDs::higgsYukawaBoost,     0.35f },
        { ParameterIDs::higgsBosonHz,        92.50f },
        { ParameterIDs::higgsBosonQ,          8.0f },
        { ParameterIDs::higgsSymmetry,        0.45f },
        { ParameterIDs::higgsDecay,           0.45f },
        { ParameterIDs::higgsVev,             0.70f },
        { ParameterIDs::higgsCursorPhi,       0.45f },
        { ParameterIDs::higgsCursorTheta,    150.0f }
    }, {
        { ParameterIDs::higgsFieldStrength, ParameterIDs::higgsBosonQ }
    } });

    // Aus Analyse von „Ean Roley Jade - FREMD LAB.wav" (Segment 30–38 s,
    // klarer Kick+Sub, ~46 Hz Grundton, sehr sublastig, kurzer Pitch-Sweep,
    // dunkler Click, dichter Druck): tiefer Underground-Techno-Kick.
    // Optimiert nach Sound-Engineer-Review (tighter Body/Sweep, definierter Click,
    // fokussierter Sub, trockener/deterministisch, explizite Defaults gesetzt).
    presets.push_back ({ "Underground Techno", {
        { ParameterIDs::fundamentalHz,       46.25f },
        { ParameterIDs::membraneTension,      0.38f },
        { ParameterIDs::membraneRadius,       0.16f },
        { ParameterIDs::membraneDamping,      0.22f },
        { ParameterIDs::membraneModes,        4.0f },
        { ParameterIDs::membraneNonlinearity, 0.18f },
        { ParameterIDs::pitchEnvAttack,       0.1f },
        { ParameterIDs::pitchEnvDecay,        60.0f },
        { ParameterIDs::pitchEnvDepth,        0.82f },
        { ParameterIDs::pitchEnvCurve,        -0.15f },
        { ParameterIDs::clickLevel,           0.54f },
        { ParameterIDs::clickDecay,           3.5f },
        { ParameterIDs::clickTone,            0.34f },
        { ParameterIDs::clickDistortion,      0.18f },
        { ParameterIDs::clickBeaterMaterial,  1.0f },
        { ParameterIDs::bodyLevel,            0.64f },
        { ParameterIDs::bodyAttack,           0.5f },
        { ParameterIDs::bodyDecay,           320.0f },
        { ParameterIDs::bodyTuning,           1.0f },
        { ParameterIDs::tubeResonanceLevel,   0.14f },
        { ParameterIDs::tubeCoupling,         0.34f },
        { ParameterIDs::subLevel,             0.64f },
        { ParameterIDs::subDecay,            820.0f },
        { ParameterIDs::subDetune,            0.0f },
        { ParameterIDs::subShape,             0.0f },
        { ParameterIDs::driveAmount,          0.28f },
        { ParameterIDs::driveMix,             0.40f },
        { ParameterIDs::driveCharacter,       0.0f }, // Tube
        { ParameterIDs::drivePreHP,           0.10f },
        { ParameterIDs::eqSubFreq,            46.0f },
        { ParameterIDs::eqSubGain,            3.0f },
        { ParameterIDs::eqPunchFreq,       2200.0f },
        { ParameterIDs::eqPunchGain,          1.5f },
        { ParameterIDs::eqRumbleHP,          22.0f },
        { ParameterIDs::compThreshold,      -10.0f },
        { ParameterIDs::compRatio,            3.5f },
        { ParameterIDs::compAttack,          12.0f },
        { ParameterIDs::compRelease,        110.0f },
        { ParameterIDs::compKnee,             6.0f },
        { ParameterIDs::compMakeup,           0.0f },
        { ParameterIDs::compAutoMakeup,       0.0f },
        { ParameterIDs::transientAttack,      2.0f },
        { ParameterIDs::transientSustain,     0.0f },
        { ParameterIDs::outputGain,          -1.0f },
        { ParameterIDs::outputClip,           0.05f },
        { ParameterIDs::outputLimiter,        0.15f },
        { ParameterIDs::roomLevel,            0.10f },
        { ParameterIDs::roomDecay,           380.0f },
        { ParameterIDs::roomRumble,           0.25f },
        { ParameterIDs::reverbMaterial,       0.0f }, // None (trocken/dicht)
        { ParameterIDs::reverbMix,            0.00f },
        { ParameterIDs::reverbDecay,         900.0f },
        { ParameterIDs::reverbLowCut,        180.0f },
        { ParameterIDs::materialRoomEnabled,  1.0f },
        { ParameterIDs::materialRoomMaterial, 4.0f }, // Concrete
        { ParameterIDs::materialRoomSize,    10.0f },
        { ParameterIDs::materialRoomMix,      0.08f },
        { ParameterIDs::materialRoomLayer,    0.0f }, // Kick (kontrollierter Tail)
        { ParameterIDs::materialRoomWidth,    0.35f },
        { ParameterIDs::higgsEnabled,         0.0f }
    } });

    // Variante „Darker Sub Roller“ (Review): mehr Gewicht/Tiefe, weniger Click,
    // trockener/straffer – auf Basis des optimierten Underground-Techno-Presets.
    presets.push_back ({ "Darker Sub Roller", {
        { ParameterIDs::fundamentalHz,       46.25f },
        { ParameterIDs::membraneTension,      0.38f },
        { ParameterIDs::membraneRadius,       0.16f },
        { ParameterIDs::membraneDamping,      0.22f },
        { ParameterIDs::membraneModes,        4.0f },
        { ParameterIDs::membraneNonlinearity, 0.18f },
        { ParameterIDs::pitchEnvAttack,       0.1f },
        { ParameterIDs::pitchEnvDecay,        60.0f },
        { ParameterIDs::pitchEnvDepth,        0.82f },
        { ParameterIDs::pitchEnvCurve,        -0.15f },
        { ParameterIDs::clickLevel,           0.38f },
        { ParameterIDs::clickDecay,           3.5f },
        { ParameterIDs::clickTone,            0.24f },
        { ParameterIDs::clickDistortion,      0.18f },
        { ParameterIDs::clickBeaterMaterial,  1.0f },
        { ParameterIDs::bodyLevel,            0.62f },
        { ParameterIDs::bodyAttack,           0.5f },
        { ParameterIDs::bodyDecay,           320.0f },
        { ParameterIDs::bodyTuning,           1.0f },
        { ParameterIDs::tubeResonanceLevel,   0.20f },
        { ParameterIDs::tubeCoupling,         0.34f },
        { ParameterIDs::subLevel,             0.68f },
        { ParameterIDs::subDecay,            980.0f },
        { ParameterIDs::subDetune,            0.0f },
        { ParameterIDs::subShape,             0.0f },
        { ParameterIDs::driveAmount,          0.28f },
        { ParameterIDs::driveMix,             0.40f },
        { ParameterIDs::driveCharacter,       0.0f }, // Tube
        { ParameterIDs::drivePreHP,           0.10f },
        { ParameterIDs::eqSubFreq,            46.0f },
        { ParameterIDs::eqSubGain,            4.0f },
        { ParameterIDs::eqPunchFreq,       2200.0f },
        { ParameterIDs::eqPunchGain,          0.5f },
        { ParameterIDs::eqRumbleHP,          22.0f },
        { ParameterIDs::compThreshold,      -10.0f },
        { ParameterIDs::compRatio,            3.5f },
        { ParameterIDs::compAttack,          12.0f },
        { ParameterIDs::compRelease,        140.0f },
        { ParameterIDs::compKnee,             6.0f },
        { ParameterIDs::compMakeup,           0.0f },
        { ParameterIDs::compAutoMakeup,       0.0f },
        { ParameterIDs::transientAttack,      2.0f },
        { ParameterIDs::transientSustain,     0.0f },
        { ParameterIDs::outputGain,          -2.3f },
        { ParameterIDs::outputClip,           0.05f },
        { ParameterIDs::outputLimiter,        0.15f },
        { ParameterIDs::roomLevel,            0.10f },
        { ParameterIDs::roomDecay,           380.0f },
        { ParameterIDs::roomRumble,           0.25f },
        { ParameterIDs::reverbMaterial,       0.0f }, // None (trocken/fokussiert)
        { ParameterIDs::reverbMix,            0.00f },
        { ParameterIDs::reverbDecay,         900.0f },
        { ParameterIDs::reverbLowCut,        180.0f },
        { ParameterIDs::materialRoomEnabled,  1.0f },
        { ParameterIDs::materialRoomMaterial, 4.0f }, // Concrete
        { ParameterIDs::materialRoomSize,    10.0f },
        { ParameterIDs::materialRoomMix,      0.08f },
        { ParameterIDs::materialRoomLayer,    0.0f }, // Kick (kontrollierter Tail)
        { ParameterIDs::materialRoomWidth,    0.35f },
        { ParameterIDs::higgsEnabled,         0.0f }
    } });

    // Aus Analyse von „recording-11-12-2026,-6-08-04-PM.wav" (5.94 s, −9 LUFS,
    // Peak 0.1 dBTP): punchiger, klickbetonter Techno-Kick, Grundton ~46–50 Hz,
    // ausgewogener Sub/Low mit präsenter Mid/Click-Energie (2–8 kHz ~20 %),
    // schnelle, teils sehr kurze Körperanteile.
    presets.push_back ({ "Punch-roller", {
        { ParameterIDs::fundamentalHz,       46.25f },
        { ParameterIDs::membraneTension,      0.46f },
        { ParameterIDs::membraneRadius,       0.14f },
        { ParameterIDs::membraneDamping,      0.38f },
        { ParameterIDs::membraneModes,        5.0f },
        { ParameterIDs::membraneNonlinearity, 0.16f },
        { ParameterIDs::pitchEnvAttack,       0.1f },
        { ParameterIDs::pitchEnvDecay,        38.0f },
        { ParameterIDs::pitchEnvDepth,        0.72f },
        { ParameterIDs::pitchEnvCurve,        -0.10f },
        { ParameterIDs::clickLevel,           0.72f },
        { ParameterIDs::clickDecay,           3.8f },
        { ParameterIDs::clickTone,            0.50f },
        { ParameterIDs::clickDistortion,      0.22f },
        { ParameterIDs::clickBeaterMaterial,  2.0f }, // Hard
        { ParameterIDs::bodyLevel,            0.58f },
        { ParameterIDs::bodyAttack,           0.4f },
        { ParameterIDs::bodyDecay,           240.0f },
        { ParameterIDs::bodyTuning,           1.0f },
        { ParameterIDs::tubeResonanceLevel,   0.28f },
        { ParameterIDs::tubeCoupling,         0.40f },
        { ParameterIDs::subLevel,             0.34f },
        { ParameterIDs::subDecay,            300.0f },
        { ParameterIDs::subDetune,            0.0f },
        { ParameterIDs::subShape,             0.0f },
        { ParameterIDs::driveAmount,          0.34f },
        { ParameterIDs::driveMix,             0.42f },
        { ParameterIDs::driveCharacter,       0.0f }, // Tube
        { ParameterIDs::drivePreHP,           0.12f },
        { ParameterIDs::eqSubFreq,            46.0f },
        { ParameterIDs::eqSubGain,            2.0f },
        { ParameterIDs::eqPunchFreq,       3000.0f },
        { ParameterIDs::eqPunchGain,          4.0f },
        { ParameterIDs::eqRumbleHP,          24.0f },
        { ParameterIDs::compThreshold,       -8.0f },
        { ParameterIDs::compRatio,            3.5f },
        { ParameterIDs::compAttack,           8.0f },
        { ParameterIDs::compRelease,        110.0f },
        { ParameterIDs::compKnee,             6.0f },
        { ParameterIDs::compMakeup,           1.0f },
        { ParameterIDs::compAutoMakeup,       0.0f },
        { ParameterIDs::transientAttack,      3.0f },
        { ParameterIDs::transientSustain,     0.0f },
        { ParameterIDs::outputGain,          -0.5f },
        { ParameterIDs::outputClip,           0.05f },
        { ParameterIDs::outputLimiter,        0.15f },
        { ParameterIDs::roomLevel,            0.08f },
        { ParameterIDs::roomDecay,           260.0f },
        { ParameterIDs::roomRumble,           0.20f },
        { ParameterIDs::reverbMaterial,       0.0f }, // None
        { ParameterIDs::reverbMix,            0.00f },
        { ParameterIDs::materialRoomEnabled,  0.0f },
        { ParameterIDs::higgsEnabled,         0.0f }
    } });

    // Aus Analyse von „recording-11-12-2026,-6-08-46-PM.wav" (7.11 s, −7.1 LUFS,
    // Peak 2.5 dBTP): sehr tieffrequenzlastiger, schwerer Kick/Roller,
    // Grundtonbereich ~45–50 Hz, dominant 80–200 Hz (~45 %) + Sub, wenig Click
    // (~7 %), lange Tails, heiß gepegelt.
    presets.push_back ({ "Low-end-roller", {
        { ParameterIDs::fundamentalHz,       46.25f },
        { ParameterIDs::membraneTension,      0.34f },
        { ParameterIDs::membraneRadius,       0.17f },
        { ParameterIDs::membraneDamping,      0.18f },
        { ParameterIDs::membraneModes,        4.0f },
        { ParameterIDs::membraneNonlinearity, 0.20f },
        { ParameterIDs::pitchEnvAttack,       0.1f },
        { ParameterIDs::pitchEnvDecay,        95.0f },
        { ParameterIDs::pitchEnvDepth,        0.86f },
        { ParameterIDs::pitchEnvCurve,        -0.20f },
        { ParameterIDs::clickLevel,           0.22f },
        { ParameterIDs::clickDecay,           5.0f },
        { ParameterIDs::clickTone,            0.12f },
        { ParameterIDs::clickDistortion,      0.10f },
        { ParameterIDs::clickBeaterMaterial,  0.0f }, // Soft
        { ParameterIDs::bodyLevel,            0.84f },
        { ParameterIDs::bodyAttack,           1.0f },
        { ParameterIDs::bodyDecay,           620.0f },
        { ParameterIDs::bodyTuning,           1.0f },
        { ParameterIDs::tubeResonanceFreq,    85.0f },
        { ParameterIDs::tubeResonanceQ,       9.0f },
        { ParameterIDs::tubeResonanceLevel,   0.52f },
        { ParameterIDs::tubeCoupling,         0.55f },
        { ParameterIDs::hollowMetallic,       0.35f },
        { ParameterIDs::subLevel,             0.50f },
        { ParameterIDs::subDecay,            980.0f },
        { ParameterIDs::subDetune,            0.0f },
        { ParameterIDs::subShape,             0.0f },
        { ParameterIDs::driveAmount,          0.28f },
        { ParameterIDs::driveMix,             0.32f },
        { ParameterIDs::driveCharacter,       0.0f }, // Tube
        { ParameterIDs::drivePreHP,           0.06f },
        { ParameterIDs::eqSubFreq,            46.0f },
        { ParameterIDs::eqSubGain,            3.0f },
        { ParameterIDs::eqPunchFreq,       2200.0f },
        { ParameterIDs::eqPunchGain,          0.0f },
        { ParameterIDs::eqRumbleHP,          20.0f },
        { ParameterIDs::compThreshold,      -10.0f },
        { ParameterIDs::compRatio,            3.0f },
        { ParameterIDs::compAttack,          12.0f },
        { ParameterIDs::compRelease,        160.0f },
        { ParameterIDs::compKnee,             6.0f },
        { ParameterIDs::compMakeup,           0.0f },
        { ParameterIDs::compAutoMakeup,       0.0f },
        { ParameterIDs::transientAttack,      1.0f },
        { ParameterIDs::transientSustain,     0.0f },
        { ParameterIDs::outputGain,          -4.0f },
        { ParameterIDs::outputClip,           0.05f },
        { ParameterIDs::outputLimiter,        0.22f },
        { ParameterIDs::roomLevel,            0.14f },
        { ParameterIDs::roomDecay,           520.0f },
        { ParameterIDs::roomRumble,           0.50f },
        { ParameterIDs::reverbMaterial,       0.0f }, // None
        { ParameterIDs::reverbMix,            0.00f },
        { ParameterIDs::materialRoomEnabled,  1.0f },
        { ParameterIDs::materialRoomMaterial, 4.0f }, // Concrete
        { ParameterIDs::materialRoomSize,    12.0f },
        { ParameterIDs::materialRoomMix,      0.09f },
        { ParameterIDs::materialRoomLayer,    0.0f }, // Kick
        { ParameterIDs::materialRoomWidth,    0.30f },
        { ParameterIDs::higgsEnabled,         0.0f }
    } });

    // Standard-Audio-Patch einmal anwenden, damit alle Factory-Presets
    // ein sinnvolles Routing haben (Membrane -> Tube -> Drive -> EQ -> Comp -> Out + FX).
    patch.applyDefaultPatch();

    for (const auto& preset : presets)
    {
        parameterPatch.clearAll();

        juce::ValueTree state = apvts.state.createCopy();

        // Modul-Power und Trigger explizit setzen (siehe Finding: Preset-Determinismus).
        applyModuleEnableDefaults (state, true);

        for (const auto& [id, value] : preset.values)
            setParameterValue (state, id, value);

        for (const auto& [sourceId, targetId] : preset.parameterPatch)
        {
            const int sourceIndex = findParameterIndex (sourceId);
            const int targetIndex = findParameterIndex (targetId);

            if (sourceIndex >= 0 && targetIndex >= 0)
            {
                parameterPatch.addConnection (sourceIndex, targetIndex);
            }
            else
            {
                // Nur im Debug-Build sichtbar: ein unbekanntes CV-Ziel würde sonst
                // stillschweigend aus dem Preset verschwinden.
                jassertfalse;
                DBG ("Preset \"" << preset.name << "\": unbekannte CV-ID "
                     << (sourceIndex < 0 ? sourceId : targetId));
            }
        }

        std::unique_ptr<juce::XmlElement> xml (state.createXml());

        if (xml == nullptr)
        {
            jassertfalse; // Factory-Preset ohne XML: Name und XML bleiben synchron (beide fehlen).
            continue;
        }

        appendPatchState (*xml, patch, parameterPatch);

        // Namen und XML nur gemeinsam hinzufügen, damit beide Listen synchron bleiben.
        factoryPresetNames.add (preset.name);
        factoryPresetXml.add (xml->toString());
    }

    // Wichtig: Die Factory-Erzeugung darf den Live-Zustand der Engine nicht
    // mit dem zuletzt gebauten Preset (Parameter-CV-Kabel!) zurücklassen.
    parameterPatch.clearAll();
    patch.applyDefaultPatch();

    buildInitPreset();
}

void PresetManager::buildInitPreset()
{
    // Init = neutrale Werkseinstellung: jeder Parameter auf seinem Default,
    // alle Module an, Default-Audio-Patch, keine Parameter-CV-Kabel (SND-166).
    juce::ValueTree state = apvts.copyState();

    for (int i = 0; i < state.getNumChildren(); ++i)
    {
        auto child = state.getChild (i);

        if (! child.hasType ("PARAM"))
            continue;

        const auto id = child.getProperty ("id").toString();

        if (auto* parameter = apvts.getParameter (id))
            child.setProperty ("value", parameter->convertFrom0to1 (parameter->getDefaultValue()),
                               nullptr);
    }

    applyModuleEnableDefaults (state, true);
    setParameterValue (state, ParameterIDs::triggerButton, 0.0f);

    std::unique_ptr<juce::XmlElement> xml (state.createXml());

    if (xml == nullptr)
    {
        jassertfalse;
        return;
    }

    parameterPatch.clearAll();
    patch.applyDefaultPatch();
    appendPatchState (*xml, patch, parameterPatch);

    initPresetXml = xml->toString();
}

//==============================================================================
juce::File PresetManager::getUserPresetDirectory()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("C.E.R.N")
               .getChildFile ("Presets");
}

juce::File PresetManager::getLegacyUserPresetFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("C.E.R.N")
               .getChildFile ("user-preset.xml");
}

void PresetManager::addUserEntries (juce::Array<Entry>& entries, const juce::File& directory,
                                    const juce::String& category)
{
    if (! directory.isDirectory())
        return;

    auto files = directory.findChildFiles (juce::File::findFiles, false, "*.xml");

    std::sort (files.begin(), files.end(), [] (const juce::File& a, const juce::File& b)
    {
        return a.getFileName().compareIgnoreCase (b.getFileName()) < 0;
    });

    for (const auto& file : files)
    {
        Entry entry;
        entry.kind = Entry::Kind::user;
        entry.name = file.getFileNameWithoutExtension();
        entry.category = category;
        entry.file = file;
        entries.add (entry);
    }
}

juce::Array<PresetManager::Entry> PresetManager::getPresetEntries() const
{
    juce::Array<Entry> entries;

    // Preset 0 des Browsers: Init (SND-166). Bleibt aus den Factory-XML-Listen
    // heraus, damit die bestehenden Factory-Indizes stabil bleiben.
    Entry initEntry;
    initEntry.kind = Entry::Kind::init;
    initEntry.name = "Init";
    initEntry.category = "FACTORY";
    entries.add (initEntry);

    {
        const juce::ScopedLock sl (lock);

        for (int i = 0; i < factoryPresetNames.size(); ++i)
        {
            Entry entry;
            entry.kind = Entry::Kind::factory;
            entry.name = factoryPresetNames[i];
            entry.category = "FACTORY";
            entry.factoryIndex = i;
            entries.add (entry);
        }
    }

    const auto directory = getUserPresetDirectory();
    addUserEntries (entries, directory, "USER");

    auto subDirectories = directory.findChildFiles (juce::File::findDirectories, false);
    std::sort (subDirectories.begin(), subDirectories.end(),
               [] (const juce::File& a, const juce::File& b)
               {
                   return a.getFileName().compareIgnoreCase (b.getFileName()) < 0;
               });

    for (const auto& subDirectory : subDirectories)
        addUserEntries (entries, subDirectory, subDirectory.getFileName());

    // Backward-Compatibility: das alte user-preset.xml (SND-164) bleibt ladbar.
    const auto legacy = getLegacyUserPresetFile();

    if (legacy.existsAsFile() && ! legacy.isAChildOf (directory))
    {
        Entry entry;
        entry.kind = Entry::Kind::user;
        entry.name = legacy.getFileNameWithoutExtension();
        entry.category = "USER (LEGACY)";
        entry.file = legacy;
        entries.add (entry);
    }

    return entries;
}

bool PresetManager::deleteUserPreset (const juce::File& file) const
{
    // Nur Dateien innerhalb des Preset-Ordners löschen (kein beliebiger Pfad).
    if (! file.existsAsFile() || ! file.isAChildOf (getUserPresetDirectory()))
        return false;

    return file.deleteFile();
}

//==============================================================================
bool PresetManager::loadFactory (int index)
{
    juce::String xmlString;

    {
        const juce::ScopedLock sl (lock);

        if (index < 0 || index >= factoryPresetXml.size())
            return false;

        xmlString = factoryPresetXml[index];
    }

    std::unique_ptr<juce::XmlElement> xml (juce::parseXML (xmlString));

    if (xml == nullptr)
        return false;

    return applyXml (*xml);
}

bool PresetManager::loadInit()
{
    juce::String xmlString;

    {
        const juce::ScopedLock sl (lock);
        xmlString = initPresetXml;
    }

    if (xmlString.isEmpty())
        return false;

    std::unique_ptr<juce::XmlElement> xml (juce::parseXML (xmlString));

    if (xml == nullptr)
        return false;

    return applyXml (*xml);
}

bool PresetManager::loadUser (const juce::File& file)
{
    if (! file.existsAsFile())
        return false;

    std::unique_ptr<juce::XmlElement> xml (juce::parseXML (file));

    if (xml == nullptr)
        return false;

    return applyXml (*xml);
}

bool PresetManager::saveUser (const juce::File& file) const
{
    // Elternverzeichnis bei Bedarf anlegen (z. B. erster Save in einen neuen Ordner).
    const auto parent = file.getParentDirectory();

    if (! parent.isDirectory() && ! parent.createDirectory().wasOk())
        return false;

    std::unique_ptr<juce::XmlElement> xml (apvts.copyState().createXml());

    if (xml == nullptr)
        return false;

    appendPatchState (*xml, patch, parameterPatch);

    return file.replaceWithText (xml->toString());
}

juce::StringArray PresetManager::getFactoryPresetNames() const
{
    const juce::ScopedLock sl (lock);
    return factoryPresetNames;
}

int PresetManager::getNumFactoryPresets() const
{
    const juce::ScopedLock sl (lock);
    return factoryPresetNames.size();
}

juce::String PresetManager::getCurrentStateXml() const
{
    std::unique_ptr<juce::XmlElement> xml (apvts.copyState().createXml());

    if (xml == nullptr)
        return {};

    appendPatchState (*xml, patch, parameterPatch);
    return xml->toString();
}

bool PresetManager::setCurrentStateXml (const juce::String& xmlString)
{
    std::unique_ptr<juce::XmlElement> xml (juce::parseXML (xmlString));

    return xml != nullptr && applyXml (*xml);
}

bool PresetManager::applyXml (const juce::XmlElement& xml)
{
    // Die Patches sind Engine-Zustand, nicht APVTS-Parameter: vor dem replaceState
    // entfernen, damit geladene <Patch>/<ParameterPatch>-Knoten nicht im APVTS verbleiben.
    auto apvtsXml = std::make_unique<juce::XmlElement> (xml);
    apvtsXml->deleteAllChildElementsWithTagName ("Patch");
    apvtsXml->deleteAllChildElementsWithTagName ("ParameterPatch");

    const juce::ValueTree state = juce::ValueTree::fromXml (*apvtsXml);

    if (! state.isValid())
        return false;

    apvts.replaceState (state);
    patch.restoreStateXml (xml);
    parameterPatch.restoreStateXml (xml);
    return true;
}
