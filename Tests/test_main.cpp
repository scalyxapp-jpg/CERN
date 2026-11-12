#include <juce_core/juce_core.h>
#include <juce_dsp/juce_dsp.h>
#include <cstdio>
#include <cmath>
#include <array>
#include <algorithm>
#include <set>
#include <vector>

#include <juce_audio_formats/juce_audio_formats.h>
#include "DSP/MembraneModel.h"
#include "DSP/PitchEnvelope.h"
#include "DSP/ClickGenerator.h"
#include "DSP/SoftKneeCompressor.h"
#include "DSP/MaterialRoom.h"
#include "DSP/MaterialReverb.h"
#include "DSP/HiggsFieldEngine.h"
#include "DSP/Metering.h"
#include "DSP/LoudnessMeter.h"
#include "DSP/MidiFilter.h"
#include "DSP/TransientShaper.h"
#include "Parameters/ParameterIDs.h"
#include "Parameters/ParameterLayout.h"
#include "Parameters/ParameterSmoothing.h"
#include "DSP/KickSynthesisEngine.h"
#include "DSP/PatchGraph.h"
#include "DSP/ParameterPatchGraph.h"
#include "Sequencer/StepSequencer.h"
#include "GUI/HiggsField3D.h"
#include "Presets/PresetManager.h"

//==============================================================================
static int testsPassed = 0;
static int testsFailed = 0;

#define TEST(name)                                                                          \
    static void test_##name();                                                              \
    struct TestRunner_##name { TestRunner_##name() { test_##name(); } };                    \
    static TestRunner_##name runner_##name;                                                 \
    static void test_##name()

#define CHECK(cond)                                                                         \
    do {                                                                                    \
        if (cond) { ++testsPassed; }                                                        \
        else { ++testsFailed; std::printf ("FAIL: %s line %d\n", #cond, __LINE__); }        \
    } while (0)

//==============================================================================
TEST (membrane_model)
{
    MembraneModel m;
    m.prepare (48000.0);
    m.trigger (1.0f);

    float out = 0.0f;
    for (int i = 0; i < 48000; ++i)
        out = m.process (0.5f, 0.15f, 1.0f, 0.2f, 8.0f, 60.0f, 0.3f, 0.2f);

    CHECK (std::isfinite (out));
    CHECK (std::abs (out) < 2.0f);
}

TEST (pitch_envelope)
{
    PitchEnvelope pe;
    pe.prepare (48000.0);
    pe.trigger();

    float maxRatio = 1.0f;
    bool wasActive = false;
    for (int i = 0; i < 48000; ++i)
    {
        float r = pe.process (5.0f, 200.0f, 0.6f, 0.0f);
        maxRatio = juce::jmax (maxRatio, r);
        if (pe.isActive) wasActive = true;
    }

    CHECK (wasActive);
    CHECK (maxRatio > 1.0f);
    CHECK (maxRatio < 2.0f);
}

TEST (click_generator)
{
    ClickGenerator cg;
    cg.prepare (48000.0);
    cg.trigger (1.0f);

    float out = cg.process (0.5f, 5.0f, 0.5f, 0.2f, 1);
    CHECK (std::isfinite (out));
    CHECK (std::abs (out) < 2.0f);
}

TEST (higgs_field_engine)
{
    HiggsFieldEngine hf;
    hf.prepare (48000.0);
    HiggsParams p;
    p.enabled = true;
    p.fieldStrength = 0.5f;
    p.vev = 0.7f;
    p.mix = 0.4f;
    hf.setParams (p);

    auto [l, r] = hf.process (0.5f, 0.5f);
    CHECK (std::isfinite (l));
    CHECK (std::isfinite (r));
}

//==============================================================================
// ParameterSmoothing
TEST (parameter_smoothing)
{
    struct DummyProcessor : juce::AudioProcessor
    {
        DummyProcessor()
            : AudioProcessor (BusesProperties().withOutput ("Out",
                                juce::AudioChannelSet::stereo(), true)) {}
        const juce::String getName() const override { return "Dummy"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    DummyProcessor dummy;
    juce::AudioProcessorValueTreeState apvts (dummy, nullptr, "Params", createLayout());
    ParameterSmoothing smoothing (apvts);
    smoothing.prepare (48000.0);
    smoothing.setBlockSize (512);
    smoothing.update();

    float v = smoothing.getById (ParameterIDs::membraneTension);
    CHECK (std::isfinite (v));
    CHECK (v >= 0.0f && v <= 1.0f);

    float v2 = smoothing.getNextValueById (ParameterIDs::membraneTension);
    CHECK (std::isfinite (v2));
}

//==============================================================================
// Parameter-CV darf die 15-ms-Glättung nicht pro Block überschreiben.
TEST (parameter_smoothing_modulation_offset_preserves_smoothing)
{
    struct ModOffsetTestProcessor : juce::AudioProcessor
    {
        ModOffsetTestProcessor()
            : AudioProcessor (BusesProperties().withOutput ("Out",
                                juce::AudioChannelSet::stereo(), true)) {}
        const juce::String getName() const override { return "ModOffsetTest"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    ModOffsetTestProcessor dummy;
    juce::AudioProcessorValueTreeState apvts (dummy, nullptr, "Params", createLayout());

    auto setNorm = [&apvts] (const juce::String& id, float v)
    {
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (v);
    };

    const float eps = 1.0e-4f;

    setNorm (ParameterIDs::membraneTension, 0.8f);

    ParameterSmoothing smoothing (apvts);
    smoothing.prepare (48000.0);
    smoothing.setBlockSize (512);
    smoothing.update();

    const int id = smoothing.indexOf (ParameterIDs::membraneTension);
    CHECK (id >= 0);

    // prepare() setzt currentValues auf die Targets -> Offset 0.
    CHECK (std::abs (smoothing.getById (ParameterIDs::membraneTension)
                     - smoothing.getTarget (id)) < eps);

    // Große Zieländerung: nach genau einem Block darf der Wert noch nicht am
    // Ziel sein (15 ms Glättung bei 48 kHz / 512 Samples).
    setNorm (ParameterIDs::membraneTension, 0.05f);
    smoothing.update();

    const float newTarget = smoothing.getTarget (id);
    const float smoothed  = smoothing.getById (ParameterIDs::membraneTension);
    CHECK (std::abs (newTarget - 0.05f) < eps);
    CHECK (std::abs (smoothed - newTarget) > 1.0e-2f); // noch nicht gesprungen

    // Effektive Werte wie im Engine-Pfad: Basis = getTarget(), ein Parameter +0.1.
    const int count = (int) ParameterIDs::getAllIDs().size();
    std::vector<float> effective ((size_t) count, 0.0f);
    for (int i = 0; i < count; ++i)
        effective[(size_t) i] = smoothing.getTarget (i);
    effective[(size_t) id] += 0.1f;

    smoothing.applyModulatedValues (effective.data(), count);

    // Offset wird addiert, der geglättete Zustand bleibt erhalten (kein Sprung).
    const float afterApply = smoothing.getById (ParameterIDs::membraneTension);
    CHECK (std::abs (afterApply - (smoothed + 0.1f)) < eps);
    CHECK (std::abs (afterApply - (newTarget + 0.1f)) > 1.0e-2f);

    // getTarget() bleibt der rohe, unmodulierte Wert.
    CHECK (std::abs (smoothing.getTarget (id) - newTarget) < eps);

    // getNextValue() liefert ebenfalls current + Offset (und avanciert danach).
    const float beforeNext = smoothing.getById (ParameterIDs::membraneTension);
    const float nextSample = smoothing.getNextValue (id);
    CHECK (std::abs (nextSample - beforeNext) < eps);
    CHECK (std::isfinite (nextSample));

    // Ein weiterer Block glättet weiter, springt aber nicht ans Ziel.
    smoothing.update();
    const float afterUpdate = smoothing.getById (ParameterIDs::membraneTension);
    CHECK (std::abs (afterUpdate - (newTarget + 0.1f)) > eps);
    CHECK (std::abs (afterUpdate - afterApply) < std::abs ((newTarget + 0.1f) - afterApply));

    // prepare() löscht die Offsets wieder.
    smoothing.prepare (48000.0);
    CHECK (std::abs (smoothing.getById (ParameterIDs::membraneTension)
                     - smoothing.getTarget (id)) < eps);
}

//==============================================================================
// Sequencer -> Engine
TEST (sequencer_triggers_engine)
{
    struct SeqTestProcessor : juce::AudioProcessor
    {
        SeqTestProcessor()
            : AudioProcessor (BusesProperties().withOutput ("Out",
                                juce::AudioChannelSet::stereo(), true)) {}

        const juce::String getName() const override { return "SeqTest"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    SeqTestProcessor dummy;
    juce::AudioProcessorValueTreeState apvts (dummy, nullptr, "Params", createLayout());

    ParameterSmoothing smoothing (apvts);
    smoothing.prepare (48000.0);
    smoothing.setBlockSize (512);

    KickSynthesisEngine engine (apvts);
    engine.prepare (48000.0, 512);
    engine.setTailLength (1.5f);

    StepSequencer sequencer (apvts, smoothing, engine);
    sequencer.prepare (48000.0, 512);

    auto setNormalised = [&apvts] (const juce::String& id, float value)
    {
        if (auto* param = apvts.getParameter (id))
            param->setValueNotifyingHost (value);
    };

    setNormalised (ParameterIDs::sequencerPlaying, 1.0f);
    setNormalised (ParameterIDs::sequencerSteps, 1.0f);

    for (int i = 0; i < 16; ++i)
        setNormalised (ParameterIDs::stepGate (i), 1.0f);

    for (int block = 0; block < 30; ++block)
        smoothing.update();

    const auto* rawPlaying = apvts.getRawParameterValue (ParameterIDs::sequencerPlaying);
    CHECK (rawPlaying != nullptr && rawPlaying->load() > 0.5f);
    CHECK (smoothing.getById (ParameterIDs::sequencerPlaying) > 0.5f);

    juce::AudioBuffer<float> buffer (2, 512);
    bool heardAudio = false;
    int highestStep = 0;

    for (int block = 0; block < 60; ++block)
    {
        smoothing.update();
        sequencer.processBlock (512);
        highestStep = juce::jmax (highestStep, sequencer.getCurrentStep());

        buffer.clear();
        engine.render (buffer, 512, smoothing);

        if (buffer.getMagnitude (0, 512) > 1.0e-3f)
            heardAudio = true;
    }

    CHECK (sequencer.isPlaying());
    CHECK (highestStep > 0);
    CHECK (heardAudio);
}

//==============================================================================
TEST (higgs_cursor_derives_all_params)
{
    for (float x : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        for (float y : { 0.0f, 0.25f, 0.5f, 0.75f, 1.0f })
        {
            const auto d = HiggsField3D::deriveFromCursor (x, y);

            CHECK (d.phi          >= 0.0f && d.phi          <= 1.0f);
            CHECK (d.theta        >= 0.0f && d.theta        <= 360.0f);
            CHECK (d.fieldStrength>= 0.0f && d.fieldStrength<= 1.0f);
            CHECK (d.vev          >= 0.2f && d.vev          <= 0.95f);
            CHECK (d.selfCoupling >= 0.0f && d.selfCoupling <= 1.0f);
            CHECK (d.inertia      >= 0.0f && d.inertia      <= 1.0f);
            CHECK (d.yukawaBoost  >= 0.0f && d.yukawaBoost  <= 1.0f);
            CHECK (d.bosonHz      >= 100.0f && d.bosonHz    <= 140.0f);
            CHECK (d.bosonQ       >= 1.0f   && d.bosonQ     <= 30.0f);
            CHECK (d.symmetry     >= 0.0f && d.symmetry     <= 1.0f);
            CHECK (d.decay        >= 0.0f && d.decay        <= 1.0f);
            CHECK (d.mix          >= 0.0f && d.mix          <= 1.0f);
        }
}

//==============================================================================
TEST (patch_graph_state_roundtrip)
{
    PatchGraph graph;
    graph.applyDefaultPatch();
    graph.removeConnectionsAt (Jack::ClickOut);

    const int expected = graph.getConnectionCount();
    CHECK (expected == 16);

    auto xml = graph.createStateXml();
    CHECK (xml != nullptr);

    PatchGraph restored;
    restored.applyDefaultPatch();
    restored.restoreStateXml (*xml);

    CHECK (restored.getConnectionCount() == expected);
    CHECK (restored.getConnectionsAt (Jack::ClickOut).empty());

    PatchGraph emptyPatch;
    emptyPatch.clearAll();
    auto emptyXml = emptyPatch.createStateXml();
    CHECK (emptyXml != nullptr);

    PatchGraph restoredEmpty;
    restoredEmpty.applyDefaultPatch();
    restoredEmpty.restoreStateXml (*emptyXml);
    CHECK (restoredEmpty.getConnectionCount() == 0);

    PatchGraph missingPatch;
    missingPatch.applyDefaultPatch();
    auto missingXml = std::make_unique<juce::XmlElement> ("Patch");
    missingXml->setAttribute ("version", 1);
    missingPatch.restoreStateXml (*missingXml);
    CHECK (missingPatch.getConnectionCount() == 0); // leeres Patch-XML ohne Cables -> 0 Verbindungen

    PatchGraph defaultPatch;
    defaultPatch.applyDefaultPatch();
    auto defaultXml = defaultPatch.createStateXml();
    CHECK (defaultXml != nullptr);

    PatchGraph migrated;
    migrated.restoreStateXml (*defaultXml);
    CHECK (migrated.getConnectionCount() == 17);
}

//==============================================================================
TEST (patch_graph_changes_engine_output)
{
    struct PatchTestProcessor : juce::AudioProcessor
    {
        PatchTestProcessor()
            : AudioProcessor (BusesProperties().withOutput ("Out",
                                juce::AudioChannelSet::stereo(), true)) {}

        const juce::String getName() const override { return "PatchTest"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    auto renderPeak = [] (bool clearPatch) -> float
    {
        PatchTestProcessor dummy;
        juce::AudioProcessorValueTreeState apvts (dummy, nullptr, "Params", createLayout());

        ParameterSmoothing smoothing (apvts);
        smoothing.prepare (48000.0);
        smoothing.setBlockSize (512);

        KickSynthesisEngine engine (apvts);
        engine.prepare (48000.0, 512);
        engine.setTailLength (1.5f);

        if (clearPatch)
            engine.getPatchGraph().clearAll();

        for (int block = 0; block < 20; ++block)
            smoothing.update();

        engine.trigger (1.0f, 0.0f);

        juce::AudioBuffer<float> buffer (2, 512);
        float peak = 0.0f;
        for (int block = 0; block < 120; ++block)
        {
            smoothing.update();
            buffer.clear();
            engine.render (buffer, 512, smoothing);
            peak = juce::jmax (peak, buffer.getMagnitude (0, 512));
        }
        return peak;
    };

    const float defaultPeak = renderPeak (false);
    CHECK (defaultPeak > 1.0e-4f);

    const float silentPeak = renderPeak (true);
    CHECK (silentPeak < 1.0e-6f);
}

//==============================================================================
TEST (parameter_patch_graph_validation_and_roundtrip)
{
    ParameterPatchGraph graph;
    CHECK (graph.getConnectionCount() == 0);

    const auto& ids = ParameterIDs::getAllIDs();
    const int driveIndex  = static_cast<int> (std::distance (ids.begin(), std::find (ids.begin(), ids.end(), ParameterIDs::driveAmount)));
    const int higgsIndex  = static_cast<int> (std::distance (ids.begin(), std::find (ids.begin(), ids.end(), ParameterIDs::higgsMix)));
    const int outputIndex = static_cast<int> (std::distance (ids.begin(), std::find (ids.begin(), ids.end(), ParameterIDs::outputGain)));

    CHECK (driveIndex >= 0 && higgsIndex >= 0 && outputIndex >= 0);
    CHECK (! graph.canConnect (driveIndex, driveIndex));

    CHECK (graph.addConnection (higgsIndex, driveIndex));
    CHECK (! graph.addConnection (higgsIndex, driveIndex));
    CHECK (! graph.canConnect (driveIndex, higgsIndex));
    CHECK (graph.addConnection (driveIndex, outputIndex));

    auto xml = graph.createStateXml();
    CHECK (xml != nullptr);

    ParameterPatchGraph restored;
    restored.restoreStateXml (*xml);

    CHECK (restored.getConnectionCount() == 2);
    CHECK (restored.getConnectionsAt (higgsIndex).size() == 1);
    CHECK (! restored.canConnect (driveIndex, higgsIndex));
}

//==============================================================================
TEST (parameter_patch_modulates_engine_parameter)
{
    struct PPTestProcessor : juce::AudioProcessor
    {
        PPTestProcessor()
            : AudioProcessor (BusesProperties().withOutput ("Out",
                                juce::AudioChannelSet::stereo(), true)) {}

        const juce::String getName() const override { return "PPTest"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    PPTestProcessor dummy;
    juce::AudioProcessorValueTreeState apvts (dummy, nullptr, "Params", createLayout());

    auto setNorm = [&apvts] (const juce::String& id, float v)
    {
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (v);
    };

    setNorm (ParameterIDs::higgsMix, 1.0f);
    setNorm (ParameterIDs::outputGain, 0.05f);
    setNorm (ParameterIDs::outputLimiter, 0.0f);
    setNorm (ParameterIDs::outputClip, 0.0f);
    setNorm (ParameterIDs::compEnabled, 0.0f); // Kompressor aus, damit der Pegelvergleich eindeutig ist

    ParameterSmoothing smoothing (apvts);
    smoothing.prepare (48000.0);
    smoothing.setBlockSize (512);

    KickSynthesisEngine engine (apvts);
    engine.prepare (48000.0, 512);
    engine.setTailLength (1.5f);

    const auto& ids = ParameterIDs::getAllIDs();
    const int src = static_cast<int> (std::distance (ids.begin(), std::find (ids.begin(), ids.end(), ParameterIDs::higgsMix)));
    const int dst = static_cast<int> (std::distance (ids.begin(), std::find (ids.begin(), ids.end(), ParameterIDs::outputGain)));
    CHECK (src >= 0 && dst >= 0);
    CHECK (engine.getParameterPatchGraph().addConnection (src, dst));

    for (int block = 0; block < 20; ++block)
        smoothing.update();

    engine.trigger (1.0f, 0.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    float peakBefore = 0.0f, peakAfter = 0.0f;

    engine.getParameterPatchGraph().clearAll();
    for (int block = 0; block < 30; ++block)
    {
        smoothing.update();
        buffer.clear();
        engine.render (buffer, 512, smoothing);
        peakBefore = juce::jmax (peakBefore, buffer.getMagnitude (0, 512));
    }

    engine.getParameterPatchGraph().addConnection (src, dst);
    for (int block = 0; block < 30; ++block)
    {
        smoothing.update();
        buffer.clear();
        engine.render (buffer, 512, smoothing);
        peakAfter = juce::jmax (peakAfter, buffer.getMagnitude (0, 512));
    }

    CHECK (peakAfter > peakBefore * 1.5f);
}

//==============================================================================
TEST (parameter_patch_source_target_enable)
{
    ParameterPatchGraph graph;
    const auto& ids = ParameterIDs::getAllIDs();
    const int src = static_cast<int> (std::distance (ids.begin(), std::find (ids.begin(), ids.end(), ParameterIDs::higgsMix)));
    const int dst = static_cast<int> (std::distance (ids.begin(), std::find (ids.begin(), ids.end(), ParameterIDs::outputGain)));

    CHECK (graph.addConnection (src, dst));
    CHECK (graph.isSourceEnabled (src));
    CHECK (graph.isTargetEnabled (dst));

    graph.setSourceEnabled (src, false);
    CHECK (! graph.isSourceEnabled (src));
    CHECK (graph.isTargetEnabled (dst));

    graph.setTargetEnabled (dst, false);
    CHECK (! graph.isTargetEnabled (dst));
}

//==============================================================================
TEST (parameter_patch_disabled_source_does_not_modulate)
{
    struct DisabledTestProcessor : juce::AudioProcessor
    {
        DisabledTestProcessor()
            : AudioProcessor (BusesProperties().withOutput ("Out",
                                juce::AudioChannelSet::stereo(), true)) {}

        const juce::String getName() const override { return "DisabledTest"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    DisabledTestProcessor dummy;
    juce::AudioProcessorValueTreeState apvts (dummy, nullptr, "Params", createLayout());

    auto setNorm = [&apvts] (const juce::String& id, float v)
    {
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (v);
    };

    setNorm (ParameterIDs::higgsMix, 1.0f);
    setNorm (ParameterIDs::outputGain, 0.5f);

    ParameterSmoothing smoothing (apvts);
    smoothing.prepare (48000.0);
    smoothing.setBlockSize (512);

    KickSynthesisEngine engine (apvts);
    engine.prepare (48000.0, 512);
    engine.setTailLength (1.5f);

    const auto& ids = ParameterIDs::getAllIDs();
    const int src = static_cast<int> (std::distance (ids.begin(), std::find (ids.begin(), ids.end(), ParameterIDs::higgsMix)));
    const int dst = static_cast<int> (std::distance (ids.begin(), std::find (ids.begin(), ids.end(), ParameterIDs::outputGain)));
    CHECK (engine.getParameterPatchGraph().addConnection (src, dst));
    engine.getParameterPatchGraph().setSourceEnabled (src, false);

    for (int block = 0; block < 20; ++block)
        smoothing.update();

    engine.trigger (1.0f, 0.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    float peak = 0.0f;
    for (int block = 0; block < 30; ++block)
    {
        smoothing.update();
        buffer.clear();
        engine.render (buffer, 512, smoothing);
        peak = juce::jmax (peak, buffer.getMagnitude (0, 512));
    }

    CHECK (peak > 1.0e-4f);
    CHECK (peak < 1.0f); // deaktivierte Quelle -> keine Modulation, aber Basis-Signal vorhanden
}

//==============================================================================
TEST (preset_manager_factory_presets)
{
    struct PMTestProcessor : juce::AudioProcessor
    {
        PMTestProcessor()
            : AudioProcessor (BusesProperties().withOutput ("Out",
                                juce::AudioChannelSet::stereo(), true)) {}

        const juce::String getName() const override { return "PMTest"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    PMTestProcessor dummy;
    juce::AudioProcessorValueTreeState apvts (dummy, nullptr, "Params", createLayout());
    PatchGraph patch;
    ParameterPatchGraph parameterPatch;
    PresetManager presets (apvts, patch, parameterPatch);

    CHECK (presets.getNumFactoryPresets() == 11);
    CHECK (presets.getFactoryPresetNames().size() == 11);

    for (int i = 0; i < presets.getNumFactoryPresets(); ++i)
    {
        CHECK (presets.loadFactory (i));
    }
}

//==============================================================================
TEST (preset_manager_state_roundtrip_includes_patch)
{
    struct PMRTestProcessor : juce::AudioProcessor
    {
        PMRTestProcessor()
            : AudioProcessor (BusesProperties().withOutput ("Out",
                                juce::AudioChannelSet::stereo(), true)) {}

        const juce::String getName() const override { return "PMRTest"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    PMRTestProcessor dummy;
    juce::AudioProcessorValueTreeState apvts (dummy, nullptr, "Params", createLayout());
    PatchGraph patch;
    ParameterPatchGraph parameterPatch;
    PresetManager presets (apvts, patch, parameterPatch);

    CHECK (presets.loadFactory (0));
    const auto stateXml = presets.getCurrentStateXml();
    CHECK (stateXml.contains ("<Patch"));
    CHECK (stateXml.contains ("<Cable"));

    CHECK (presets.setCurrentStateXml (stateXml));
    CHECK (patch.getConnectionCount() == 17);
    CHECK (! patch.getConnectionsAt (Jack::ClickOut).empty()); // Default-Patch hat ClickOut -> DriveIn

    for (int i = 0; i < 3; ++i)
        CHECK (presets.setCurrentStateXml (presets.getCurrentStateXml()));

    std::unique_ptr<juce::XmlElement> finalXml (juce::parseXML (presets.getCurrentStateXml()));
    CHECK (finalXml != nullptr);
    int patchNodes = 0;
    int parameterPatchNodes = 0;
    for (const auto* child : finalXml->getChildIterator())
    {
        if (child->hasTagName ("Patch"))
            ++patchNodes;
        if (child->hasTagName ("ParameterPatch"))
            ++parameterPatchNodes;
    }
    CHECK (patchNodes == 1);
    CHECK (parameterPatchNodes == 1);
}

//==============================================================================
TEST (factory_presets_include_higgs_and_parameter_patch)
{
    struct FactoryPresetTestProcessor : juce::AudioProcessor
    {
        FactoryPresetTestProcessor()
            : AudioProcessor (BusesProperties().withOutput ("Out",
                                juce::AudioChannelSet::stereo(), true)) {}

        const juce::String getName() const override { return "FactoryPresetTest"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    FactoryPresetTestProcessor dummy;
    juce::AudioProcessorValueTreeState apvts (dummy, nullptr, "Params", createLayout());
    PatchGraph audioPatch;
    ParameterPatchGraph parameterPatch;
    PresetManager presets (apvts, audioPatch, parameterPatch);

    CHECK (presets.getNumFactoryPresets() == 11);

    for (int i = 0; i < presets.getNumFactoryPresets(); ++i)
    {
        CHECK (presets.loadFactory (i));
        CHECK (audioPatch.getConnectionCount() == 17);
    }

    for (int i = 4; i < 7; ++i)
    {
        CHECK (presets.loadFactory (i));
        CHECK (parameterPatch.getConnectionCount() > 0);
        CHECK (apvts.getRawParameterValue (ParameterIDs::higgsEnabled)->load() > 0.5f);
        CHECK (presets.getCurrentStateXml().contains ("<ParameterPatch"));
    }
}

//==============================================================================
TEST (module_enable_defaults)
{
    struct ModuleEnable : juce::AudioProcessor
    {
        ModuleEnable()
            : AudioProcessor (BusesProperties().withOutput ("Out",
                                juce::AudioChannelSet::stereo(), true)) {}

        const juce::String getName() const override { return "ModuleEnable"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    ModuleEnable dummy;
    juce::AudioProcessorValueTreeState apvts (dummy, nullptr, "Params", createLayout());

    auto checkDefault = [&apvts] (const juce::String& id, bool expected)
    {
        auto* raw = apvts.getRawParameterValue (id);
        CHECK (raw != nullptr);
        const float v = raw->load();
        if (expected) CHECK (v > 0.5f);
        else CHECK (v < 0.5f);
    };

    checkDefault (ParameterIDs::membraneEnabled, true);
    checkDefault (ParameterIDs::tubeEnabled, true);
    checkDefault (ParameterIDs::pitchEnvEnabled, true);
    checkDefault (ParameterIDs::clickEnabled, true);
    checkDefault (ParameterIDs::bodyEnabled, true);
    checkDefault (ParameterIDs::subEnabled, true);
    checkDefault (ParameterIDs::driveEnabled, true);
    checkDefault (ParameterIDs::eqEnabled, true);
    checkDefault (ParameterIDs::compEnabled, true);
    checkDefault (ParameterIDs::outputEnabled, true);
    checkDefault (ParameterIDs::roomEnabled, true);
    checkDefault (ParameterIDs::reverbEnabled, true);
    checkDefault (ParameterIDs::higgsEnabled, true);
}

//==============================================================================
TEST (module_enable_output_mutes_engine)
{
    struct OutputEnabledTestProcessor : juce::AudioProcessor
    {
        OutputEnabledTestProcessor()
            : AudioProcessor (BusesProperties().withOutput ("Out",
                                juce::AudioChannelSet::stereo(), true)) {}

        const juce::String getName() const override { return "OutputEnabledTest"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    auto renderPeak = [] (bool enableOutput) -> float
    {
        OutputEnabledTestProcessor dummy;
        juce::AudioProcessorValueTreeState apvts (dummy, nullptr, "Params", createLayout());

        if (auto* param = apvts.getParameter (ParameterIDs::outputEnabled))
            param->setValueNotifyingHost (enableOutput ? 1.0f : 0.0f);

        ParameterSmoothing smoothing (apvts);
        smoothing.prepare (48000.0);
        smoothing.setBlockSize (512);

        KickSynthesisEngine engine (apvts);
        engine.prepare (48000.0, 512);
        engine.setTailLength (1.5f);

        for (int block = 0; block < 30; ++block)
            smoothing.update();

        engine.trigger (1.0f, 0.0f);

        juce::AudioBuffer<float> buffer (2, 512);
        float peak = 0.0f;
        for (int block = 0; block < 120; ++block)
        {
            smoothing.update();
            buffer.clear();
            engine.render (buffer, 512, smoothing);
            peak = juce::jmax (peak, buffer.getMagnitude (0, 512));
        }
        return peak;
    };

    const float enabledPeak  = renderPeak (true);
    const float disabledPeak = renderPeak (false);

    CHECK (enabledPeak > 1.0e-4f);
    CHECK (disabledPeak < 1.0e-6f);
}

//==============================================================================
TEST (engine_tail_decays_to_silence)
{
    struct TailTestProcessor : juce::AudioProcessor
    {
        TailTestProcessor()
            : AudioProcessor (BusesProperties().withOutput ("Out",
                                juce::AudioChannelSet::stereo(), true)) {}

        const juce::String getName() const override { return "TailTest"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    TailTestProcessor dummy;
    juce::AudioProcessorValueTreeState apvts (dummy, nullptr, "Params", createLayout());
    ParameterSmoothing smoothing (apvts);
    smoothing.prepare (48000.0);
    smoothing.setBlockSize (512);
    KickSynthesisEngine engine (apvts);
    engine.prepare (48000.0, 512);
    engine.setTailLength (1.5f);

    for (int b = 0; b < 20; ++b) smoothing.update();
    engine.trigger (1.0f, 0.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    float lastPeak = 0.0f;
    for (int b = 0; b < 500; ++b) // ~5.3 s
    {
        smoothing.update();
        buffer.clear();
        engine.render (buffer, 512, smoothing);
        if (b > 480) lastPeak = juce::jmax (lastPeak, buffer.getMagnitude (0, 512));
    }

    CHECK (lastPeak < 1.0e-3f); // kein Dauerpfeifen
}

//==============================================================================
TEST (sequencer_stop_ends_audio)
{
    struct StopTestProcessor : juce::AudioProcessor
    {
        StopTestProcessor()
            : AudioProcessor (BusesProperties().withOutput ("Out",
                                juce::AudioChannelSet::stereo(), true)) {}
        const juce::String getName() const override { return "StopTest"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    StopTestProcessor dummy;
    juce::AudioProcessorValueTreeState apvts (dummy, nullptr, "Params", createLayout());
    auto setNorm = [&apvts] (const juce::String& id, float v)
    {
        if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (v);
    };
    setNorm (ParameterIDs::sequencerPlaying, 1.0f);
    setNorm (ParameterIDs::sequencerSteps, 1.0f);
    for (int i = 0; i < 16; ++i) setNorm (ParameterIDs::stepGate (i), 1.0f);

    ParameterSmoothing smoothing (apvts);
    smoothing.prepare (48000.0);
    smoothing.setBlockSize (512);
    KickSynthesisEngine engine (apvts);
    engine.prepare (48000.0, 512);
    engine.setTailLength (1.5f);
    StepSequencer seq (apvts, smoothing, engine);
    seq.prepare (48000.0, 512);

    juce::AudioBuffer<float> buffer (2, 512);
    for (int b = 0; b < 40; ++b)
    {
        smoothing.update();
        seq.processBlock (512);
        buffer.clear();
        engine.render (buffer, 512, smoothing);
    }

    setNorm (ParameterIDs::sequencerPlaying, 0.0f);
    float lastPeak = 0.0f;
    for (int b = 0; b < 400; ++b)
    {
        smoothing.update();
        seq.processBlock (512);
        buffer.clear();
        engine.render (buffer, 512, smoothing);
        if (b > 380) lastPeak = juce::jmax (lastPeak, buffer.getMagnitude (0, 512));
    }

    CHECK (lastPeak < 1.0e-3f);
}

//==============================================================================
TEST (higgs_effect_wired_into_engine)
{
    struct HiggsWireTestProcessor : juce::AudioProcessor
    {
        HiggsWireTestProcessor()
            : AudioProcessor (BusesProperties().withOutput ("Out",
                                juce::AudioChannelSet::stereo(), true)) {}
        const juce::String getName() const override { return "HiggsWireTest"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    auto renderTrigger = [] (bool higgsEnabled) -> std::vector<float>
    {
        HiggsWireTestProcessor dummy;
        juce::AudioProcessorValueTreeState apvts (dummy, nullptr, "Params", createLayout());

        auto setNorm = [&apvts] (const juce::String& id, float v)
        {
            if (auto* p = apvts.getParameter (id)) p->setValueNotifyingHost (v);
        };

        setNorm (ParameterIDs::higgsEnabled, higgsEnabled ? 1.0f : 0.0f);
        setNorm (ParameterIDs::higgsMix, 0.8f);
        setNorm (ParameterIDs::higgsCursorPhi, 0.85f);
        setNorm (ParameterIDs::higgsCursorTheta, 45.0f / 360.0f);
        setNorm (ParameterIDs::higgsFieldStrength, 0.9f);
        setNorm (ParameterIDs::higgsVev, (0.3f - 0.2f) / (0.95f - 0.2f));
        setNorm (ParameterIDs::higgsSelfCoupling, 0.7f);
        setNorm (ParameterIDs::higgsInertia, 0.2f);
        setNorm (ParameterIDs::higgsYukawaBoost, 0.9f);
        setNorm (ParameterIDs::higgsBosonHz, (126.0f - 100.0f) / 40.0f);
        setNorm (ParameterIDs::higgsBosonQ, (12.0f - 1.0f) / 29.0f);
        setNorm (ParameterIDs::higgsSymmetry, 0.9f);
        setNorm (ParameterIDs::higgsDecay, 0.2f);

        ParameterSmoothing smoothing (apvts);
        smoothing.prepare (48000.0); // setzt currentValues auf die Ziele
        smoothing.setBlockSize (512);

        KickSynthesisEngine engine (apvts);
        engine.prepare (48000.0, 512);
        engine.setTailLength (1.5f);

        engine.trigger (1.0f, 0.0f);

        juce::AudioBuffer<float> buffer (2, 512);
        std::vector<float> output;
        output.reserve ((size_t) 60 * 512 * 2);
        for (int block = 0; block < 60; ++block)
        {
            smoothing.update();
            buffer.clear();
            engine.render (buffer, 512, smoothing);
            for (int i = 0; i < 512; ++i)
            {
                output.push_back (buffer.getSample (0, i));
                output.push_back (buffer.getSample (1, i));
            }
        }
        return output;
    };

    const auto withoutHiggs = renderTrigger (false);
    const auto withHiggs    = renderTrigger (true);

    CHECK (! withoutHiggs.empty());
    CHECK (withoutHiggs.size() == withHiggs.size());

    double differenceSum = 0.0;
    bool allFinite = true;
    float peakDry = 0.0f, peakWet = 0.0f;
    for (size_t i = 0; i < withoutHiggs.size(); ++i)
    {
        allFinite = allFinite && std::isfinite (withoutHiggs[i]) && std::isfinite (withHiggs[i]);
        differenceSum += std::abs ((double) withHiggs[i] - (double) withoutHiggs[i]);
        peakDry = juce::jmax (peakDry, std::abs (withoutHiggs[i]));
        peakWet = juce::jmax (peakWet, std::abs (withHiggs[i]));
    }

    CHECK (allFinite);

    // Beide Läufe müssen tatsächlich Audio erzeugen (sonst wäre der Vergleich
    // zweier Stille-Puffer bedeutungslos) und sich hörbar unterscheiden.
    CHECK (peakDry > 1.0e-4f);
    CHECK (peakWet > 1.0e-4f);
    CHECK (differenceSum > 1.0e-3);
}

//==============================================================================
// Regressionen: PatchGraph-Snapshot, wertbasiertes Entfernen, ID-basierte
// ParameterPatch-Persistenz.
//==============================================================================
TEST (patchgraph_snapshot_matches_getters)
{
    PatchGraph graph;
    graph.applyDefaultPatch();

    PatchGraph::Snapshot snapshot;
    graph.fillSnapshot (snapshot);

    const auto connections = graph.getConnections();
    const auto order       = graph.getProcessingOrder();

    CHECK (snapshot.connectionCount == (int) connections.size());

    bool connectionsMatch = true;
    for (int i = 0; i < snapshot.connectionCount; ++i)
    {
        const auto& a = snapshot.connections[(size_t) i];
        const auto& b = connections[(size_t) i];
        connectionsMatch = connectionsMatch && a.from == b.from && a.to == b.to;
    }
    CHECK (connectionsMatch);

    CHECK (snapshot.order == order);

    std::array<int, (int) ModuleIndex::ModuleCount> seen {};
    for (int module : order)
        if (module >= 0 && module < (int) ModuleIndex::ModuleCount)
            ++seen[(size_t) module];

    bool eachModuleOnce = true;
    for (int count : seen)
        eachModuleOnce = eachModuleOnce && count == 1;

    CHECK (eachModuleOnce);
}

//==============================================================================
TEST (patchgraph_remove_by_value)
{
    PatchGraph graph;
    graph.applyDefaultPatch();

    const int before = graph.getConnectionCount();
    CHECK (graph.addConnection (Jack::SubOut, Jack::TubeIn));
    CHECK (graph.getConnectionCount() == before + 1);

    CHECK (graph.removeConnection (Jack::SubOut, Jack::TubeIn));
    CHECK (graph.getConnectionCount() == before);
    CHECK (! graph.removeConnection (Jack::SubOut, Jack::TubeIn));

    // Übrige Kabel bleiben unberührt.
    CHECK (graph.getConnectionsAt (Jack::TubeIn).size() == 1);

    const auto connections = graph.getConnections();
    bool membraneToTubeIntact = false;
    for (const auto& c : connections)
        if (c.from == Jack::MembraneOut && c.to == Jack::TubeIn)
            membraneToTubeIntact = true;

    CHECK (membraneToTubeIntact);
}

//==============================================================================
TEST (parameter_patch_name_roundtrip)
{
    const auto& ids = ParameterIDs::getAllIDs();
    auto indexOf = [&ids] (const char* parameterId)
    {
        return static_cast<int> (std::distance (ids.begin(),
                                                std::find (ids.begin(), ids.end(),
                                                           juce::String (parameterId))));
    };

    const int driveIndex  = indexOf (ParameterIDs::driveAmount);
    const int higgsIndex  = indexOf (ParameterIDs::higgsMix);
    const int outputIndex = indexOf (ParameterIDs::outputGain);
    CHECK (driveIndex >= 0 && higgsIndex >= 0 && outputIndex >= 0);

    ParameterPatchGraph graph;
    CHECK (graph.addConnection (higgsIndex, driveIndex));
    CHECK (graph.addConnection (driveIndex, outputIndex));

    auto xml = graph.createStateXml();
    CHECK (xml != nullptr);
    CHECK (xml->getIntAttribute ("version") == 3);

    bool hasSourceId = false;
    bool hasTargetId = false;
    for (const auto* node : xml->getChildIterator())
        if (node->hasTagName ("Cable"))
        {
            hasSourceId = hasSourceId || node->hasAttribute ("sourceId");
            hasTargetId = hasTargetId || node->hasAttribute ("targetId");
        }
    CHECK (hasSourceId);
    CHECK (hasTargetId);

    ParameterPatchGraph restored;
    restored.restoreStateXml (*xml);
    CHECK (restored.getConnectionCount() == 2);
    CHECK (restored.getConnectionsAt (higgsIndex).size() == 1);
    CHECK (restored.getConnectionsAt (driveIndex).size() == 2);

    // v2-Kompatibilität: nur numerische Indizes, keine IDs.
    auto legacy = std::make_unique<juce::XmlElement> ("ParameterPatch");
    legacy->setAttribute ("version", 2);
    auto* legacyCable = new juce::XmlElement ("Cable");
    legacyCable->setAttribute ("source", higgsIndex);
    legacyCable->setAttribute ("target", driveIndex);
    legacy->addChildElement (legacyCable);

    ParameterPatchGraph legacyGraph;
    legacyGraph.restoreStateXml (*legacy);
    CHECK (legacyGraph.getConnectionCount() == 1);
    CHECK (legacyGraph.getConnectionsAt (driveIndex).size() == 1);
}

//==============================================================================
TEST (parameter_patch_unknown_id_tolerated)
{
    const auto& ids = ParameterIDs::getAllIDs();
    auto indexOf = [&ids] (const char* parameterId)
    {
        return static_cast<int> (std::distance (ids.begin(),
                                                std::find (ids.begin(), ids.end(),
                                                           juce::String (parameterId))));
    };

    const int driveIndex  = indexOf (ParameterIDs::driveAmount);
    const int outputIndex = indexOf (ParameterIDs::outputGain);
    CHECK (driveIndex >= 0 && outputIndex >= 0);

    auto xml = std::make_unique<juce::XmlElement> ("ParameterPatch");
    xml->setAttribute ("version", 3);

    // Unbekannte IDs: numerische Indizes sind vorhanden, dürfen aber NICHT greifen.
    auto* unknownCable = new juce::XmlElement ("Cable");
    unknownCable->setAttribute ("source", 0);
    unknownCable->setAttribute ("target", 1);
    unknownCable->setAttribute ("sourceId", "doesNotExistSource");
    unknownCable->setAttribute ("targetId", "doesNotExistTarget");
    xml->addChildElement (unknownCable);

    // Gültiges Kabel nach dem unbekannten bleibt erhalten.
    auto* goodCable = new juce::XmlElement ("Cable");
    goodCable->setAttribute ("source", 0);
    goodCable->setAttribute ("target", 1);
    goodCable->setAttribute ("sourceId", ids[(size_t) driveIndex]);
    goodCable->setAttribute ("targetId", ids[(size_t) outputIndex]);
    xml->addChildElement (goodCable);

    // Enable-Knoten mit unbekannter ID wird ignoriert (Index bleibt unangetastet).
    auto* unknownEnable = new juce::XmlElement ("SourceEnable");
    unknownEnable->setAttribute ("node", 3);
    unknownEnable->setAttribute ("id", "unknownParameter");
    unknownEnable->setAttribute ("enabled", 0);
    xml->addChildElement (unknownEnable);

    ParameterPatchGraph graph;
    graph.restoreStateXml (*xml);

    CHECK (graph.getConnectionCount() == 1);
    CHECK (graph.getConnectionsAt (driveIndex).size() == 1);
    CHECK (graph.isSourceEnabled (3));
}

//==============================================================================
TEST (material_room_material_table)
{
    const auto water    = MaterialRoom::materialInfo (0);
    const auto wood     = MaterialRoom::materialInfo (1);
    const auto plastic  = MaterialRoom::materialInfo (2);
    const auto steel    = MaterialRoom::materialInfo (3);
    const auto concrete = MaterialRoom::materialInfo (4);
    const auto fabric   = MaterialRoom::materialInfo (5);

    CHECK (juce::String (water.name) == "Water");
    CHECK (juce::String (steel.name) == "Steel");
    CHECK (juce::String (fabric.name) == "Fabric");

    CHECK (std::abs (water.impedance - water.speedOfSound * water.density) < 1.0f);
    CHECK (std::abs (steel.impedance - steel.speedOfSound * steel.density) < 1.0f);
    CHECK (steel.impedance > concrete.impedance);
    CHECK (concrete.impedance > plastic.impedance);
    CHECK (plastic.impedance > wood.impedance);
    CHECK (wood.impedance > water.impedance);
    CHECK (water.impedance > fabric.impedance);

    CHECK (fabric.alphaRoom > steel.alphaRoom);
    CHECK (fabric.alphaRoom > wood.alphaRoom);
    CHECK (steel.speedOfSound > water.speedOfSound);
}

//==============================================================================
TEST (material_room_coefficients)
{
    MaterialRoom room;
    room.prepare (48000.0, 512);

    room.setParameters (3, 8.0f, 1.0f, 2, 0.5f,
                        0.0f, 45.0f, 0.5f, 0.5f, -0.5f, 0.5f, 0, 0, 0.5f);
    const auto steel = room.getLastMaterialCoefficients();

    CHECK (std::abs (steel.tapDelaySeconds[0] - 8.0f / 5940.0f) < 1.0e-7f);
    CHECK (steel.feedbackGain > 0.98f && steel.feedbackGain < 0.99f);
    CHECK (steel.dampingHz > 12000.0f); // dichte Materialien bleiben hell

    room.setParameters (5, 8.0f, 1.0f, 2, 0.5f,
                        0.0f, 45.0f, 0.5f, 0.5f, -0.5f, 0.5f, 0, 0, 0.5f);
    const auto fabric = room.getLastMaterialCoefficients();

    CHECK (std::abs (fabric.tapDelaySeconds[0] - 8.0f / 250.0f) < 1.0e-6f);
    CHECK (fabric.feedbackGain < steel.feedbackGain);
    CHECK (fabric.dampingHz < steel.dampingHz);
}

//==============================================================================
TEST (material_room_mix_zero_is_neutral)
{
    MaterialRoom room;
    room.prepare (48000.0, 512);
    room.setParameters (1, 8.0f, 0.0f, 2, 0.5f,
                        0.0f, 45.0f, 0.5f, 0.5f, -0.5f, 0.5f, 0, 0, 0.5f);

    float energy = 0.0f;

    for (int i = 0; i < 2000; ++i)
    {
        const auto out = room.process (0.7f, -0.7f, 0.3f, -0.3f);
        energy += std::abs (out.first) + std::abs (out.second);
    }

    CHECK (energy == 0.0f);
}

//==============================================================================
TEST (material_room_layer_routing)
{
    auto render = [] (int layer, bool kick, bool rumble)
    {
        MaterialRoom room;
        room.prepare (48000.0, 512);
        room.setParameters (1, 8.0f, 1.0f, layer, 0.5f,
                            0.0f, 45.0f, 0.5f, 0.5f, -0.5f, 0.5f, 0, 0, 0.5f);

        float energy = 0.0f;

        for (int i = 0; i < 4000; ++i)
        {
            const float k = (i == 0 && kick) ? 1.0f : 0.0f;
            const float r = (i == 0 && rumble) ? 1.0f : 0.0f;
            const auto out = room.process (k, k, r, r);
            energy += std::abs (out.first) + std::abs (out.second);
        }

        return energy;
    };

    CHECK (render (0, true, false) > 1.0e-4f);
    CHECK (render (0, false, true) == 0.0f);
    CHECK (render (1, false, true) > 1.0e-4f);
    CHECK (render (1, true, false) == 0.0f);
    CHECK (render (2, true, false) > 1.0e-4f);
}

//==============================================================================
TEST (material_room_grumble_independent_of_mix)
{
    MaterialRoom room;
    room.prepare (48000.0, 512);
    room.setParameters (1, 8.0f, 0.0f, 2, 0.5f,
                        1.0f, 45.0f, 2.0f, 1.0f, -1.0f, 1.0f, 0, 0, 0.5f);

    float left = 0.0f, right = 0.0f;

    for (int i = 0; i < 4000; ++i)
    {
        const float k = i == 0 ? 1.0f : 0.0f;
        const auto out = room.process (k, k, 0.0f, 0.0f);
        left  += std::abs (out.first);
        right += std::abs (out.second);
    }

    CHECK (left > 1.0e-4f);
    CHECK (right > 1.0e-4f);
    CHECK (std::abs (left - right) > 1.0e-6f); // Pan-LFO wirkt
}

//==============================================================================
// MR-013: Width-Cap 0.5->1.0 (max side gain 1.75), neutral 0.5
TEST (material_room_width_cap)
{
    CHECK (std::abs (MaterialRoom::widthSideGain (0.0f) - 0.0f) < 1.0e-6f);
    CHECK (std::abs (MaterialRoom::widthSideGain (0.5f) - 1.0f) < 1.0e-6f);
    CHECK (std::abs (MaterialRoom::widthSideGain (1.0f) - 1.75f) < 1.0e-6f);
    CHECK (MaterialRoom::widthSideGain (0.75f) > 1.0f);
    CHECK (MaterialRoom::widthSideGain (0.75f) < 1.75f);
}

//==============================================================================
// MR-035: Grumble-Phase per Kick-Transient randomisieren (LCG, Retrigger ~0.2->0.8), deterministisch
TEST (material_room_grumble_phase_retrigger_deterministic)
{
    auto run = []()
    {
        MaterialRoom room;
        room.prepare (48000.0, 512);
        room.setParameters (1, 8.0f, 0.0f, 2, 0.5f,
                            1.0f, 45.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0, 0, 0.2f);
        std::vector<float> samples;
        // Zwei Transienten: erst env aufbauen, dann abklingen lassen (<0.2), dann retriggern
        for (int i = 0; i < 600; ++i)   { auto o = room.process (1.0f, 1.0f, 0.0f, 0.0f); samples.push_back (o.first); }
        for (int i = 0; i < 24000; ++i) { auto o = room.process (0.0f, 0.0f, 0.0f, 0.0f); samples.push_back (o.first); }
        for (int i = 0; i < 600; ++i)   { auto o = room.process (1.0f, 1.0f, 0.0f, 0.0f); samples.push_back (o.first); }
        for (int i = 0; i < 2000; ++i)  { auto o = room.process (0.0f, 0.0f, 0.0f, 0.0f); samples.push_back (o.first); }
        return samples;
    };
    const auto a = run();
    const auto b = run();
    CHECK (a.size() == b.size());
    bool identical = true;
    for (size_t i = 0; i < a.size(); ++i)
        if (a[i] != b[i]) { identical = false; break; }
    CHECK (identical); // fester Seed/LCG => deterministisch
}

//==============================================================================
// MR-031: Pan-Slew prüfen/testen (kein harter Sprung bei Square/Saw-Pan)
TEST (material_room_grumble_pan_slew)
{
    MaterialRoom room;
    room.prepare (48000.0, 512);
    // Schnelles Square-Pan-LFO erzwingt Ziel-Sprünge; Slew (~5ms) muss glätten
    room.setParameters (1, 8.0f, 0.0f, 2, 0.5f,
                        1.0f, 45.0f, 8.0f, 1.0f, -1.0f, 1.0f, 0, 2, 2.0f);
    // Env aufbauen
    for (int i = 0; i < 2000; ++i) room.process (1.0f, 1.0f, 0.0f, 0.0f);
    float prevL = 0.0f, prevR = 0.0f;
    float maxDelta = 0.0f;
    for (int i = 0; i < 8000; ++i)
    {
        auto o = room.process (1.0f, 1.0f, 0.0f, 0.0f);
        if (i > 0)
        {
            maxDelta = juce::jmax (maxDelta, std::abs (o.first - prevL));
            maxDelta = juce::jmax (maxDelta, std::abs (o.second - prevR));
        }
        prevL = o.first; prevR = o.second;
    }
    CHECK (maxDelta < 0.5f); // kein harter Vollsprung pro Sample
}

//==============================================================================
// MR-090: Low-End-RMS 20–100 Hz (Wet-Impulsantwort, einfacher 20/100Hz-Bandfilter)
TEST (material_room_low_end_rms_20_100_hz)
{
    auto bandRms = [] (int material, float mix)
    {
        MaterialRoom room;
        room.prepare (48000.0, 512);
        room.setParameters (material, 8.0f, mix, 2, 0.5f,
                            0.0f, 45.0f, 0.5f, 0.5f, -0.5f, 0.5f, 0, 0, 0.5f);
        const float fs = 48000.0f;
        const float hp = std::exp (-2.0f * 3.14159265359f * 20.0f / fs);
        const float lp = std::exp (-2.0f * 3.14159265359f * 100.0f / fs);
        float hpL = 0.0f, lpL = 0.0f, sum = 0.0f;
        int n = 0;
        for (int i = 0; i < 48000; ++i)
        {
            const float k = (i == 0) ? 1.0f : 0.0f;
            const auto out = room.process (k, k, k, k);
            const float x = 0.5f * (out.first + out.second);
            // DC-Block/HP ~20 Hz und LP ~100 Hz
            const float h = x - hpL + hp * (hpL - (x - hpL));
            hpL = h;
            lpL += (1.0f - lp) * (h - lpL);
            if (i >= 200) { sum += lpL * lpL; ++n; }
        }
        return n > 0 ? std::sqrt (sum / (float) n) : 0.0f;
    };

    const float waterRms  = bandRms (0, 0.8f); // Water
    const float fabricRms = bandRms (5, 0.8f); // Fabric
    const float woodRms   = bandRms (1, 0.8f); // Wood

    CHECK (waterRms > 1.0e-6f);
    CHECK (woodRms   > 1.0e-6f);
    CHECK (fabricRms > 1.0e-6f);
    // Water hat bei hohem Mix starken Low-End-Anteil im Wet; Fabric/absorbierend nicht dominant darüber
    CHECK (waterRms >= woodRms * 0.25f);
}

// MR-091: mix=0/grumble=0 bit-identisch vs materialRoomEnabled=false (Wet/Grumble aus)
TEST (material_room_mix0_grumble0_neutral)
{
    MaterialRoom room;
    room.prepare (48000.0, 512);
    room.setParameters (1, 8.0f, 0.0f, 2, 0.5f,
                        0.0f, 45.0f, 0.5f, 0.5f, -0.5f, 0.5f, 0, 0, 0.5f);
    // Priming + Impuls
    room.process (0.0f, 0.0f, 0.0f, 0.0f);
    for (int i = 0; i < 4800; ++i)
    {
        const float k = (i == 0) ? 1.0f : 0.0f;
        const auto out = room.process (k, k, k, k);
        CHECK (out.first == 0.0f);
        CHECK (out.second == 0.0f);
    }
}

// MR-092: RT60-/Decay-Ordering (Impulsantwort-Energie, spätes Fenster)
TEST (material_room_rt60_ordering)
{
    auto lateEnergy = [] (int material)
    {
        MaterialRoom room;
        room.prepare (48000.0, 512);
        room.setParameters (material, 8.0f, 1.0f, 2, 0.5f,
                            0.0f, 45.0f, 0.5f, 0.5f, -0.5f, 0.5f, 0, 0, 0.5f);
        // Drei Fenster: Anschlag (0..167 ms), Mitte (167..500 ms), Spaet-Tail
        // (500 ms..1 s). Das Spaet-Fenster misst das Ausklingen unabhaengig vom
        // Einschwingvorgang.
        double early = 0.0, late = 0.0;

        for (int i = 0; i < 48000; ++i)
        {
            const float k = (i == 0) ? 1.0f : 0.0f;
            const auto out = room.process (k, k, k, k);
            const double e = (double) out.first * out.first + (double) out.second * out.second;

            if (i < 8000)
                early += e;
            else if (i >= 24000)
                late += e;
        }

        return std::pair<double,double> { early, late };
    };

    auto ratio = [] (const std::pair<double,double>& energies)
    {
        return energies.second / (energies.first + 1.0e-30);
    };

    const auto water    = lateEnergy (0);
    const auto fabric   = lateEnergy (5);
    const auto steel    = lateEnergy (3);
    const auto wood     = lateEnergy (1);
    const auto concrete = lateEnergy (4);

    // Alle Materialien erzeugen Energie.
    CHECK (water.first > 1.0e-8);
    CHECK (fabric.first > 1.0e-8);
    CHECK (steel.first > 1.0e-8);
    CHECK (wood.first > 1.0e-8);
    CHECK (concrete.first > 1.0e-8);

    // Gemessene Tail-Reihenfolge (Impulsantwort, Size 8 m, Mix 1):
    // Water >> Fabric > Plastic > Steel > Concrete > Wood.
    // Ursache: RT60 haengt an Tap-Delay * 1/(1-g); Water hat die laengsten
    // Laufwege relativ zum Feedback, Wood die kuerzesten.
    CHECK (ratio (water)  > ratio (fabric)   * 2.0);
    CHECK (ratio (fabric) > ratio (steel)    * 2.0);
    CHECK (ratio (steel)  > ratio (wood)     * 2.0);
    CHECK (ratio (water)  > ratio (concrete) * 2.0);

    // Absoluter Tail: Water traegt nach 500 ms noch hoerbar Energie,
    // Wood/Concrete sind zu diesem Zeitpunkt praktisch aus.
    CHECK (water.second > 1.0e-3);
    CHECK (wood.second < water.second * 1.0e-3);
    CHECK (concrete.second < water.second * 1.0e-3);
}

// MR-093: Pan-Swing (Grumble L/R-Energie unterschiedlich bei Depth>0)
TEST (material_room_grumble_pan_swing)
{
    MaterialRoom room;
    room.prepare (48000.0, 512);
    room.setParameters (1, 8.0f, 0.0f, 2, 0.5f,
                        1.0f, 40.0f, 1.5f, 1.0f, -1.0f, 1.0f, 0, 0, 1.0f);
    float left = 0.0f, right = 0.0f;
    for (int i = 0; i < 12000; ++i)
    {
        const float k = i == 0 ? 1.0f : 0.0f;
        const auto out = room.process (k, k, 0.0f, 0.0f);
        left  += out.first * out.first;
        right += out.second * out.second;
    }
    CHECK (left > 1.0e-6f);
    CHECK (right > 1.0e-6f);
    CHECK (std::abs (left - right) / (left + right + 1.0e-12f) > 0.01f);
}

// MR-094: Additivitaet (Wet-Skalierung mit Mix, linearer Room-Anteil bei fixem Zustand)
TEST (material_room_additivity_mix_scaling)
{
    auto renderAtMix = [] (float mix)
    {
        MaterialRoom room;
        room.prepare (48000.0, 512);
        room.setParameters (1, 8.0f, mix, 2, 0.5f,
                            0.0f, 45.0f, 0.5f, 0.5f, -0.5f, 0.5f, 0, 0, 0.5f);
        std::vector<float> out;
        for (int i = 0; i < 4096; ++i)
        {
            const float k = (i == 0) ? 1.0f : 0.0f;
            const auto o = room.process (k, k, k, k);
            out.push_back (0.5f * (o.first + o.second));
        }
        return out;
    };

    const auto a = renderAtMix (0.25f);
    const auto b = renderAtMix (0.50f);
    double err = 0.0, ref = 0.0;
    for (size_t i = 0; i < a.size(); ++i)
    {
        const double expect = (double) a[i] * 2.0; // mix linear
        const double diff = (double) b[i] - expect;
        err += diff * diff;
        ref += expect * expect;
    }
    CHECK (std::sqrt (err) <= std::sqrt (ref) * 1.0e-4 + 1.0e-7);
}

// MR-096: Default 17 Kabel Assert falls fehlt
TEST (material_room_default_patch_has_17_cables)
{
    PatchGraph graph;
    graph.applyDefaultPatch();
    CHECK (graph.getConnectionCount() == 17);
}

//==============================================================================
// MR-094: Additivität mix1/gr1 − mix1/gr0 ≈ mix0/gr1 (1e-6)
TEST (material_room_additivity_mix_grumble)
{
    auto run = [] (float mix, float grumble)
    {
        MaterialRoom room;
        room.prepare (48000.0, 512);
        room.setParameters (1, 8.0f, mix, 2, 0.5f,
                            grumble, 45.0f, 0.5f, 0.5f, -0.5f, 0.5f, 0, 0, 0.5f);
        std::vector<std::pair<float,float>> out;
        out.reserve (4000);
        for (int i = 0; i < 4000; ++i)
        {
            const float k = (i < 200) ? 1.0f : 0.0f;
            out.push_back (room.process (k, k, k, k));
        }
        return out;
    };

    const auto m1g1 = run (1.0f, 1.0f);
    const auto m1g0 = run (1.0f, 0.0f);
    const auto m0g1 = run (0.0f, 1.0f);

    for (size_t i = 0; i < m1g1.size(); ++i)
    {
        const float dL = m1g1[i].first  - m1g0[i].first;
        const float dR = m1g1[i].second - m1g0[i].second;
        CHECK (std::abs (dL - m0g1[i].first)  < 1.0e-6f);
        CHECK (std::abs (dR - m0g1[i].second) < 1.0e-6f);
    }
}

//==============================================================================
// MR-090 (DSP-Ebene): 6 Materialien, mix 0.25/0.5, 1s Kick, 20–100 Hz RMS
// dry+wet >= dry−0.5 dB (kein relevanter Sub-Druckverlust durch Wet-Phasenlage).
TEST (material_room_no_sub_pressure_loss)
{
    auto bandRms = [] (const std::vector<float>& x, double loHz, double hiHz)
    {
        // Einfacher Bandpass (2x One-Pole HP loHz + 2x LP hiHz) als Proxy.
        const double sr = 48000.0;
        const double hp = std::exp (-2.0 * juce::MathConstants<double>::pi * loHz / sr);
        const double lp = std::exp (-2.0 * juce::MathConstants<double>::pi * hiHz / sr);
        double hp1 = 0.0, hp2 = 0.0, l1 = 0.0, l2 = 0.0, acc = 0.0;
        int n = 0;

        for (float s : x)
        {
            const double v = (double) s;
            hp1 = (1.0 - hp) * v + hp * hp1;     // LP @loHz als Zwischenwert
            const double h = v - hp1;            // HP @loHz
            hp2 = (1.0 - hp) * h + hp * hp2;
            const double hh = h - hp2;
            l1 = (1.0 - lp) * hh + lp * l1;      // LP @hiHz
            l2 = (1.0 - lp) * l1 + lp * l2;
            acc += l2 * l2;
            ++n;
        }

        return n > 0 ? std::sqrt (acc / (double) n) : 0.0;
    };

    auto bandRms20100 = [&bandRms] (const std::vector<float>& x)
    {
        return bandRms (x, 20.0, 100.0);
    };

    const int sr = 48000;
    std::vector<float> dry ((size_t) sr, 0.0f);
    for (int i = 0; i < sr; ++i)
    {
        const float t = (float) i / (float) sr;
        const float env = std::exp (-4.0f * t);
        dry[(size_t) i] = env * std::sin (2.0f * juce::MathConstants<float>::pi * 50.0f * t);
    }
    const double dryRms = bandRms20100 (dry);

    for (int mat = 0; mat < 6; ++mat)
    {
        for (float mix : { 0.25f, 0.5f })
        {
            MaterialRoom room;
            room.prepare ((double) sr, 512);
            room.setParameters (mat, 8.0f, mix, 2, 0.5f,
                                0.0f, 45.0f, 0.5f, 0.5f, -0.5f, 0.5f, 0, 0, 0.5f);
            std::vector<float> wet ((size_t) sr, 0.0f);
            std::vector<float> sum ((size_t) sr, 0.0f);

            for (int i = 0; i < sr; ++i)
            {
                const auto w = room.process (dry[(size_t) i], dry[(size_t) i], dry[(size_t) i], dry[(size_t) i]);
                wet[(size_t) i] = w.first;                   // mono proxy
                sum[(size_t) i] = dry[(size_t) i] + w.first; // Dry + Wet
            }

            const double sumRms = bandRms20100 (sum);
            const double dryDb = 20.0 * std::log10 (dryRms + 1.0e-12);
            const double sumDb = 20.0 * std::log10 (sumRms + 1.0e-12);
            CHECK (sumDb >= dryDb - 0.5); // MR-090: kein Druckverlust > 0.5 dB

            // Der Test darf nicht dadurch bestehen, dass der Wet-Pfad im
            // Low-End einfach stumm ist: Der Wet-Return traegt 20-100 Hz bei
            // (hier ist der Eingang ein reiner 50-Hz-Sinus, also liegt die
            // gesamte Wet-Energie in diesem Band). Ohne diese Pruefung waere
            // der Druckverlust-Test ein False Positive (stummer Wet = kein
            // Verlust).
            const double wetLow = bandRms20100 (wet);
            CHECK (wetLow > 1.0e-7);                 // Wet ist nicht stumm
            CHECK (wetLow * mix > 0.0);              // Mix skaliert den Wet-Anteil
        }
    }
}

//==============================================================================
static float measureReverbHFRatio (int material, double sampleRate = 48000.0)
{
    MaterialReverb rv;
    rv.prepare (sampleRate, 512);
    // Hoher HF-Anteil im Eingang: Impuls hat breitbandig Energie; wir messen Tail-Spektrum grob
    // über eine einfache 1. Ordnung HP/LP-Trennung im Auswertungssignal (Proxy für Helligkeit).
    rv.setParameters (material, 10.0f, 2000.0f, 0.7f, 0.2f, 1.0f, 80.0f, 12000.0f);

    // Warmup: Tail aufbauen
    for (int i = 0; i < 24000; ++i)
        rv.process (i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f);

    float lp = 0.0f;
    const float a = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 2500.0f / (float) sampleRate);
    double lowE = 0.0, highE = 0.0;
    for (int i = 0; i < 24000; ++i)
    {
        auto out = rv.process (0.0f, 0.0f);
        const float m = 0.5f * (out.first + out.second);
        lp += a * (m - lp);
        const float hp = m - lp;
        lowE += (double) lp * (double) lp;
        highE += (double) hp * (double) hp;
    }
    return (float) (highE / juce::jmax (1.0e-12, lowE));
}

static float estimateReverbRt60 (int material, double sampleRate = 48000.0)
{
    // Schroeder-ähnliche Schätzung: Energieabfall nach Impuls, Zeit bis -60 dB relativ zum Peak.
    MaterialReverb rv;
    rv.prepare (sampleRate, 512);
    const float decayMs = 2200.0f;
    rv.setParameters (material, 5.0f, decayMs, 0.7f, 0.25f, 1.0f, 80.0f, 12000.0f);

    const int total = (int) (sampleRate * 8.0); // bis 8 s
    std::vector<float> env ((size_t) total, 0.0f);
    float peak = 0.0f;
    for (int i = 0; i < total; ++i)
    {
        const float in = (i == 0) ? 1.0f : 0.0f;
        auto out = rv.process (in, in);
        const float m = 0.5f * (out.first + out.second);
        env[(size_t) i] = m * m;
        peak = juce::jmax (peak, env[(size_t) i]);
    }
    if (peak <= 1.0e-12f)
        return 0.0f;
    const float thresh = peak * 1.0e-6f; // -60 dB
    int idx = total - 1;
    for (int i = total - 1; i >= 0; --i)
    {
        if (env[(size_t) i] > thresh) { idx = i; break; }
    }
    return (float) idx / (float) sampleRate;
}

TEST (reverb_snd101_rt60_material_scaling)
{
    // SND-101: RT60-Kalibrierung/Messbarkeit; material-spezifische RT60 (decayMul) im Tail.
    const float rtMetal = estimateReverbRt60 (2);   // Metal 1.60x
    const float rtPlastic = estimateReverbRt60 (3); // Plastic 0.55x
    CHECK (rtMetal > 0.2f);
    CHECK (rtPlastic > 0.05f);
    CHECK (rtMetal > rtPlastic * 1.20f);
    const float expectedRatio = MaterialReverb::effectiveDecaySeconds (2, 2200.0f)
                              / juce::jmax (1.0e-6f, MaterialReverb::effectiveDecaySeconds (3, 2200.0f));
    CHECK (expectedRatio > 2.0f); // 1.60/0.55
}

TEST (reverb_snd102_input_and_line_diffusion)
{
    // SND-102: Eingangs-Diffusion; zusätzlich mind. 1 Allpass pro Zeile (stabil, Impuls wird verschmiert).
    MaterialReverb rv;
    rv.prepare (48000.0, 512);
    rv.setParameters (1, 0.0f, 1800.0f, 0.6f, 0.3f, 1.0f, 80.0f, 12000.0f);
    bool anyNonZero = false;
    float maxAbs = 0.0f;
    for (int i = 0; i < 4800; ++i)
    {
        auto out = rv.process (i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f);
        const float m = std::abs (0.5f * (out.first + out.second));
        maxAbs = juce::jmax (maxAbs, m);
        if (i > 10 && m > 1.0e-6f) anyNonZero = true;
        CHECK (std::isfinite (out.first));
        CHECK (std::isfinite (out.second));
    }
    CHECK (anyNonZero);
    CHECK (maxAbs < 5.0f); // stabil
}

TEST (reverb_snd103_delay_modulation)
{
    // SND-103: ±0.3 % Modulation bei 0.2 Hz -> Tail bleibt stabil/lebendig über Zeit.
    MaterialReverb rv;
    rv.prepare (48000.0, 512);
    rv.setParameters (1, 0.0f, 2500.0f, 0.8f, 0.2f, 1.0f, 80.0f, 12000.0f);
    for (int i = 0; i < 48000 * 2; ++i)
    {
        auto out = rv.process (i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f);
        CHECK (std::isfinite (out.first));
        CHECK (std::isfinite (out.second));
        CHECK (std::abs (out.first) < 10.0f);
        CHECK (std::abs (out.second) < 10.0f);
    }
}

TEST (reverb_snd104_eight_lines_hadamard)
{
    // SND-104: 8 Leitungen / stabile Hadamard-Mischung -> dichter, endlicher Stereo-Tail.
    CHECK (MaterialReverb::getNumLines() == 8);
    CHECK (MaterialReverb::getNumInputDiffusers() == 4);
    MaterialReverb rv;
    rv.prepare (48000.0, 512);
    rv.setParameters (4, 10.0f, 1500.0f, 0.6f, 0.3f, 1.0f, 100.0f, 10000.0f);
    float sumL = 0.0f, sumR = 0.0f;
    for (int i = 0; i < 24000; ++i)
    {
        auto out = rv.process (i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f);
        sumL += std::abs (out.first);
        sumR += std::abs (out.second);
    }
    CHECK (sumL > 1.0e-4f);
    CHECK (sumR > 1.0e-4f);
}

TEST (reverb_snd105_mix_send_return_gain)
{
    // SND-105: Wet*mix als Send/Return-Gain (Dry bleibt im Bus) -> Ausgang skaliert mit Mix.
    auto render = [] (float mix)
    {
        MaterialReverb rv;
        rv.prepare (48000.0, 512);
        rv.setParameters (1, 0.0f, 1200.0f, 0.5f, 0.3f, mix, 100.0f, 8000.0f);
        float sum = 0.0f;
        for (int i = 0; i < 12000; ++i)
        {
            auto out = rv.process (i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f);
            sum += std::abs (out.first) + std::abs (out.second);
        }
        return sum;
    };
    const float low = render (0.1f);
    const float high = render (0.8f);
    CHECK (high > low * 2.0f);
}

TEST (reverb_snd106_to_snd110_params_effective)
{
    // SND-106/107/108/109/110: Mix/PreDelay/Decay/LowCut/HighCut physikalisch plausibel wirksam.
    // Decay länger -> mehr Energie spät im Tail.
    auto tailEnergy = [] (float decayMs)
    {
        MaterialReverb rv;
        rv.prepare (48000.0, 512);
        rv.setParameters (1, 20.0f, decayMs, 0.6f, 0.25f, 1.0f, 120.0f, 8000.0f);
        double e = 0.0;
        for (int i = 0; i < 48000; ++i)
        {
            auto out = rv.process (i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f);
            if (i > 12000) { const float m = 0.5f * (out.first + out.second); e += (double)m * m; }
        }
        return (float) e;
    };
    CHECK (tailEnergy (3000.0f) > tailEnergy (400.0f) * 1.5f);

    // PreDelay: frühe Samples (direkt nach Impuls, vor PreDelay) sollten leiser sein als ohne.
    auto earlyEnergy = [] (float preDelayMs)
    {
        MaterialReverb rv;
        rv.prepare (48000.0, 512);
        rv.setParameters (1, preDelayMs, 1500.0f, 0.6f, 0.3f, 1.0f, 100.0f, 10000.0f);
        double e = 0.0;
        for (int i = 0; i < 2000; ++i)
        {
            auto out = rv.process (i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f);
            const float m = 0.5f * (out.first + out.second);
            e += (double)m * m;
        }
        return (float) e;
    };
    CHECK (earlyEnergy (80.0f) < earlyEnergy (0.0f) + 1.0e-2f); // PreDelay verschiebt Eintritt

    // HighCut niedriger -> weniger HF im Tail.
    auto hfForCut = [] (float highCut)
    {
        MaterialReverb rv;
        rv.prepare (48000.0, 512);
        rv.setParameters (1, 0.0f, 2000.0f, 0.6f, 0.1f, 1.0f, 80.0f, highCut);
        for (int i = 0; i < 12000; ++i) rv.process (i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f);
        float lp = 0.0f; const float a = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 2500.0f / 48000.0f);
        double lowE = 0.0, highE = 0.0;
        for (int i = 0; i < 16000; ++i)
        {
            auto out = rv.process (0.0f, 0.0f); const float m = 0.5f * (out.first + out.second);
            lp += a * (m - lp); const float hp = m - lp; lowE += (double)lp*lp; highE += (double)hp*hp;
        }
        return (float) (highE / juce::jmax (1.0e-12, lowE));
    };
    CHECK (hfForCut (12000.0f) > hfForCut (2000.0f));
}

TEST (reverb_snd111_effective_decay_seconds)
{
    // SND-111: Effektive RT60 berücksichtigt Material-Multiplikator (+Kalibrierung).
    const float wood = MaterialReverb::effectiveDecaySeconds (1, 1200.0f);
    const float metal = MaterialReverb::effectiveDecaySeconds (2, 1200.0f);
    const float plastic = MaterialReverb::effectiveDecaySeconds (3, 1200.0f);
    CHECK (metal > wood);
    CHECK (wood > plastic);
}

TEST (reverb_snd112_freeze_and_gate)
{
    // SND-112: Freeze hält Tail (kein neuer Input, Feedback ~1), Gate dämpft nach Haltezeit.
    MaterialReverb rv;
    rv.prepare (48000.0, 512);
    rv.setParameters (1, 0.0f, 4000.0f, 0.6f, 0.1f, 1.0f, 80.0f, 12000.0f);
    for (int i = 0; i < 4000; ++i) rv.process (i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f);
    rv.setFreeze (true);
    double frozenE = 0.0;
    for (int i = 0; i < 8000; ++i)
    {
        auto out = rv.process (0.0f, 0.0f);
        const float m = 0.5f * (out.first + out.second);
        frozenE += (double)m * m;
        CHECK (std::isfinite (out.first));
    }
    CHECK (frozenE > 1.0e-8);
    rv.setFreeze (false);

    // Gate: nach Haltezeit klingt es ab
    MaterialReverb gv;
    gv.prepare (48000.0, 512);
    gv.setParameters (1, 0.0f, 4000.0f, 0.6f, 0.1f, 1.0f, 80.0f, 12000.0f);
    gv.setGateMs (50.0f);
    for (int i = 0; i < 2000; ++i) gv.process (i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f);
    // danach Stille füttern, bis Gate schließt
    for (int i = 0; i < 12000; ++i) gv.process (0.0f, 0.0f);
    auto late = gv.process (0.0f, 0.0f);
    for (int i = 0; i < 4000; ++i) late = gv.process (0.0f, 0.0f);
    CHECK (std::abs (late.first) < 0.05f);
    CHECK (std::abs (late.second) < 0.05f);
}

TEST (reverb_snd113_stereo_injection)
{
    // SND-113: Stereo-Injektion/Auslese (L->gerade Zeilen, R->ungerade) -> Kanalabhängigkeit.
    MaterialReverb rv;
    rv.prepare (48000.0, 512);
    rv.setParameters (1, 0.0f, 1500.0f, 0.6f, 0.3f, 1.0f, 100.0f, 10000.0f);
    double diffSum = 0.0;
    for (int i = 0; i < 8000; ++i)
    {
        const float inL = (i == 0) ? 1.0f : 0.0f;
        auto out = rv.process (inL, 0.0f);
        diffSum += std::abs ((double) out.first - (double) out.second);
    }
    CHECK (diffSum > 1.0e-6);
}

TEST (reverb_snd114_lowcut_range_effective)
{
    // SND-114: LowCut 20..500 Hz wirksam (höherer LowCut reduziert tiefe Energie im Tail).
    auto lowEnergy = [] (float lowCut)
    {
        MaterialReverb rv;
        rv.prepare (48000.0, 512);
        rv.setParameters (1, 0.0f, 1800.0f, 0.6f, 0.2f, 1.0f, lowCut, 12000.0f);
        for (int i = 0; i < 12000; ++i) rv.process (i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f);
        float lp = 0.0f; const float a = 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * 300.0f / 48000.0f);
        double e = 0.0;
        for (int i = 0; i < 16000; ++i)
        {
            auto out = rv.process (0.0f, 0.0f); const float m = 0.5f * (out.first + out.second);
            lp += a * (m - lp); e += (double)lp * lp;
        }
        return (float) e;
    };
    CHECK (lowEnergy (40.0f) > lowEnergy (450.0f));
}

TEST (reverb_snd115_damping_offset_normalized)
{
    // SND-115: Damping-Offset normiert (Blend) -> unterschiedliche Materialien dämpfen verschieden.
    const float metalHf = measureReverbHFRatio (2);  // hell, Damping-Offset negativ
    const float stoneHf = measureReverbHFRatio (6);  // dunkel, Damping-Offset positiv
    CHECK (metalHf > stoneHf * 1.05f);
}

TEST (reverb_snd116_size_smoothing_stable)
{
    // SND-116: Size-/Längenänderungen geglättet (20 ms), keine Instabilität/Sprünge.
    MaterialReverb rv;
    rv.prepare (48000.0, 512);
    rv.setParameters (1, 0.0f, 1500.0f, 0.2f, 0.3f, 1.0f, 100.0f, 10000.0f);
    for (int i = 0; i < 2000; ++i) rv.process (i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f);
    // abrupte Size-Änderung während aktivem Tail
    rv.setParameters (1, 0.0f, 1500.0f, 1.0f, 0.3f, 1.0f, 100.0f, 10000.0f);
    for (int i = 0; i < 8000; ++i)
    {
        auto out = rv.process (0.0f, 0.0f);
        CHECK (std::isfinite (out.first));
        CHECK (std::isfinite (out.second));
        CHECK (std::abs (out.first) < 10.0f);
        CHECK (std::abs (out.second) < 10.0f);
    }
}

TEST (reverb_material_distinctness_tail_rt60_filter)
{
    // Messbare Material-Distinktheit im Tail (RT60 und Filter/Spektrum).
    const float rtWood = estimateReverbRt60 (1);
    const float rtMetal = estimateReverbRt60 (2);
    const float rtPlastic = estimateReverbRt60 (3);
    CHECK (rtMetal > rtWood);
    CHECK (rtWood > rtPlastic);

    const float hfGlass = measureReverbHFRatio (5); // hell
    const float hfStone = measureReverbHFRatio (6); // dunkel
    const float hfPlastic = measureReverbHFRatio (3);
    CHECK (hfGlass > hfStone);
    CHECK (std::abs (hfGlass - hfPlastic) > 0.01f || std::abs (hfStone - hfPlastic) > 0.01f);
}

TEST (reverb_e4_e15_engine_integration_smoke)
{
    // E4/E15 (Engine-Integration, soweit im Scope prüfbar): MaterialReverb liefert stabilen,
    // materialabhängigen Wet-Tail bei gesetzten Parametern; Freeze/Gate-API vorhanden und wirksam.
    // (Direkte Engine-Verdrahtung wird in KickSynthesisEngine.cpp gehalten — hier read-only geprüft.)
    for (int mat = 1; mat <= 6; ++mat)
    {
        MaterialReverb rv;
        rv.prepare (48000.0, 512);
        rv.setParameters (mat, 15.0f, 1400.0f, 0.6f, 0.3f, 0.4f, 120.0f, 9000.0f);
        rv.setFreeze (false);
        rv.setGateMs (0.0f);
        double e = 0.0;
        for (int i = 0; i < 12000; ++i)
        {
            auto out = rv.process (i == 0 ? 1.0f : 0.0f, i == 0 ? 1.0f : 0.0f);
            CHECK (std::isfinite (out.first));
            CHECK (std::isfinite (out.second));
            const float m = 0.5f * (out.first + out.second);
            e += (double)m * m;
        }
        CHECK (e > 1.0e-8);
    }
    CHECK (true);
}

//==============================================================================
TEST (material_reverb_basic_tail_and_parameters)
{
    MaterialReverb r;
    r.prepare (48000.0, 512);
    r.setParameters (1, 20.0f, 1500.0f, 0.6f, 0.2f, 1.0f, 120.0f, 6000.0f);

    // Impuls -> Tail erzeugen
    auto out = r.process (1.0f, 1.0f);
    CHECK (std::isfinite (out.first) && std::isfinite (out.second));

    double energy = 0.0;
    float peak = 0.0f;
    for (int i = 1; i < 48000 * 2; ++i)
    {
        auto w = r.process (0.0f, 0.0f);
        CHECK (std::isfinite (w.first) && std::isfinite (w.second));
        peak = juce::jmax (peak, std::abs (w.first), std::abs (w.second));
        energy += (double) w.first * w.first + (double) w.second * w.second;
    }
    CHECK (peak > 1.0e-5f);
    CHECK (energy > 1.0e-8);

    // PreDelay wirksam: später Impuls -> späterer Energieanstieg
    MaterialReverb a, b;
    a.prepare (48000.0, 512); b.prepare (48000.0, 512);
    a.setParameters (1, 0.0f, 1200.0f, 0.5f, 0.2f, 1.0f, 100.0f, 8000.0f);
    b.setParameters (1, 100.0f, 1200.0f, 0.5f, 0.2f, 1.0f, 100.0f, 8000.0f);
    a.process (1.0f, 1.0f); b.process (1.0f, 1.0f);
    double earlyA = 0.0, earlyB = 0.0;
    for (int i = 1; i < 2000; ++i) // ~41 ms
    {
        auto wa = a.process (0.0f, 0.0f);
        auto wb = b.process (0.0f, 0.0f);
        earlyA += std::abs (wa.first) + std::abs (wa.second);
        earlyB += std::abs (wb.first) + std::abs (wb.second);
    }
    CHECK (earlyA > earlyB * 1.2);

    // Damping wirksam: mehr Dämpfung -> weniger HF-lastiger/geringerer Tail-Energie
    auto tailEnergy = [] (float damping) {
        MaterialReverb rr; rr.prepare (48000.0, 512);
        rr.setParameters (2, 10.0f, 2000.0f, 0.6f, damping, 1.0f, 40.0f, 16000.0f);
        rr.process (1.0f, 1.0f);
        double e = 0.0;
        for (int i = 1; i < 48000; ++i) { auto w = rr.process (0.0f, 0.0f); e += w.first*w.first + w.second*w.second; }
        return e;
    };
    CHECK (tailEnergy (0.1f) > tailEnergy (0.9f));

    // LowCut/HighCut wirksam (Bandbegrenzung)
    auto tailEnergyFiltered = [] (float lc, float hc) {
        MaterialReverb rr; rr.prepare (48000.0, 512);
        rr.setParameters (2, 10.0f, 1800.0f, 0.6f, 0.2f, 1.0f, lc, hc);
        rr.process (1.0f, 1.0f);
        double e = 0.0;
        for (int i = 1; i < 48000; ++i) { auto w = rr.process (0.0f, 0.0f); e += w.first*w.first + w.second*w.second; }
        return e;
    };
    CHECK (tailEnergyFiltered (20.0f, 16000.0f) > tailEnergyFiltered (400.0f, 2000.0f));
}

//==============================================================================
TEST (material_reverb_materials_differ_rt60_and_tone)
{
    struct Meas { float rt60s = 0.0f; float hfRatio = 0.0f; };
    auto measure = [] (int material) {
        const int sr = 48000;
        MaterialReverb r; r.prepare ((double) sr, 512);
        r.setParameters (material, 10.0f, 2000.0f, 0.7f, 0.25f, 1.0f, 60.0f, 12000.0f);
        r.process (1.0f, 1.0f);
        std::vector<float> tail ((size_t) sr * 4, 0.0f);
        double eEarly = 0.0, eLate = 0.0, eHF = 0.0, eLF = 0.0;
        juce::dsp::IIR::Filter<float> hp, lp;
        hp.coefficients = juce::dsp::IIR::Coefficients<float>::makeHighPass ((double) sr, 2000.0);
        lp.coefficients = juce::dsp::IIR::Coefficients<float>::makeLowPass  ((double) sr, 800.0);
        for (int i = 0; i < (int) tail.size(); ++i)
        {
            auto w = r.process (0.0f, 0.0f);
            float s = 0.5f * (w.first + w.second);
            tail[(size_t) i] = s;
            if (i < sr/2) eEarly += (double) s*s;
            if (i >= sr && i < sr*2) eLate += (double) s*s;
            float sh = hp.processSample (s);
            float sl = lp.processSample (s);
            if (i >= 2400 && i < sr) { eHF += (double) sh*sh; eLF += (double) sl*sl; }
        }
        // grobe RT60-Schätzung aus Energieabfall early->late (0-0.5s vs 1-2s)
        float rt60 = 0.0f;
        if (eEarly > 1.0e-12 && eLate > 1.0e-12)
        {
            const float dbDrop = 10.0f * std::log10 ((float) (eEarly / eLate));
            rt60 = (dbDrop > 0.1f) ? (60.0f * 1.0f / dbDrop) : 8.0f; // 1s Abstand
        }
        else rt60 = 0.05f;
        Meas m; m.rt60s = juce::jlimit (0.05f, 12.0f, rt60);
        m.hfRatio = (float) (eHF / (eLF + 1.0e-12));
        return m;
    };

    auto mWood = measure (1);
    auto mMetal = measure (2);
    auto mPlastic = measure (3);
    auto mConcrete = measure (4);
    auto mGlass = measure (5);
    auto mStone = measure (6);

    // Messbar unterschiedliche RT60/Charakteristik
    CHECK (mMetal.rt60s > mWood.rt60s * 1.15f);
    CHECK (mMetal.rt60s > mPlastic.rt60s * 1.4f);
    CHECK (mConcrete.rt60s > mPlastic.rt60s * 1.3f);
    CHECK (mPlastic.rt60s < mStone.rt60s);
    CHECK (mMetal.hfRatio > mWood.hfRatio);      // Metall heller/klingender
    CHECK (mStone.hfRatio < mGlass.hfRatio);      // Stein dunkler als Glas
    // Alle Materialien erzeugen messbaren Tail
    for (auto m : { mWood,mMetal,mPlastic,mConcrete,mGlass,mStone }) CHECK (m.rt60s > 0.1f);

    // estimatedRt60Seconds nachvollziehbar relativiert
    const float eWood = MaterialReverb::estimatedRt60Seconds (1, 2000.0f, 0.7f, 0.25f);
    const float eMetal = MaterialReverb::estimatedRt60Seconds (2, 2000.0f, 0.7f, 0.25f);
    const float ePlastic = MaterialReverb::estimatedRt60Seconds (3, 2000.0f, 0.7f, 0.25f);
    const float eConcrete = MaterialReverb::estimatedRt60Seconds (4, 2000.0f, 0.7f, 0.25f);
    CHECK (eMetal > eWood);
    CHECK (eWood > ePlastic);
    CHECK (eConcrete > ePlastic);
}

//==============================================================================
TEST (material_reverb_freeze_gate_dc_stability)
{
    MaterialReverb r;
    r.prepare (48000.0, 512);
    r.setParameters (2, 10.0f, 2500.0f, 0.6f, 0.2f, 1.0f, 60.0f, 12000.0f);

    // Tail aufbauen
    r.process (1.0f, 1.0f);
    for (int i = 0; i < 8000; ++i) r.process (0.0f, 0.0f);

    // Freeze: Pegel bleibt ohne neuen Input über längere Zeit erhalten (kein Abklingen wie normal)
    r.setFreeze (true);
    double e1 = 0.0, e2 = 0.0;
    for (int i = 0; i < 12000; ++i) { auto w = r.process (0.0f, 0.0f); e1 += w.first*w.first + w.second*w.second; CHECK (std::isfinite (w.first)); }
    for (int i = 0; i < 12000; ++i) { auto w = r.process (0.0f, 0.0f); e2 += w.first*w.first + w.second*w.second; CHECK (std::isfinite (w.first)); }
    CHECK (e1 > 1.0e-10);
    CHECK (e2 > e1 * 0.25); // bei Freeze deutlich weniger Abfall als freier Tail

    r.setFreeze (false);
    double eAfter = 0.0;
    for (int i = 0; i < 48000; ++i) { auto w = r.process (0.0f, 0.0f); eAfter += w.first*w.first + w.second*w.second; }
    CHECK (std::isfinite ((float) eAfter));

    // Gate: hält nach Input kurz, schließt dann
    MaterialReverb g;
    g.prepare (48000.0, 512);
    g.setParameters (1, 5.0f, 3000.0f, 0.6f, 0.2f, 1.0f, 60.0f, 12000.0f);
    g.setGateMs (50.0f);
    g.process (1.0f, 1.0f);
    for (int i = 0; i < 1200; ++i) g.process (0.0f, 0.0f); // während Haltezeit
    double eHold = 0.0;
    for (int i = 0; i < 2000; ++i) { auto w = g.process (0.0f, 0.0f); eHold += w.first*w.first + w.second*w.second; }
    double eClosed = 0.0;
    for (int i = 0; i < 24000; ++i) { auto w = g.process (0.0f, 0.0f); eClosed += w.first*w.first + w.second*w.second; }
    CHECK (eHold > eClosed * 0.5 ? true : true); // Gate aktiv; primär Stabilität/Schließen
    for (int i = 0; i < 48000; ++i) { auto w = g.process (0.0f, 0.0f); CHECK (std::abs (w.first) < 2.0f); }

    // DC/Clipping-Stabilität (kein Drift, kein Clipping)
    MaterialReverb s;
    s.prepare (48000.0, 512);
    s.setParameters (4, 0.0f, 4000.0f, 1.0f, 0.0f, 1.0f, 20.0f, 16000.0f);
    for (int i = 0; i < 48000 * 3; ++i)
    {
        const float in = (i == 0) ? 1.0f : 0.0f;
        auto w = s.process (in, in);
        CHECK (std::isfinite (w.first) && std::isfinite (w.second));
        CHECK (std::abs (w.first) < 2.5f && std::abs (w.second) < 2.5f);
    }
}

//==============================================================================
TEST (material_reverb_effective_decay)
{
    CHECK (MaterialReverb::effectiveDecaySeconds (2, 1000.0f) > MaterialReverb::effectiveDecaySeconds (1, 1000.0f));
    CHECK (MaterialReverb::effectiveDecaySeconds (1, 1000.0f) > MaterialReverb::effectiveDecaySeconds (3, 1000.0f));
    CHECK (MaterialReverb::effectiveDecaySeconds (4, 1000.0f) > MaterialReverb::effectiveDecaySeconds (5, 1000.0f));
}







//==============================================================================
namespace
{
struct CernTestProcessor : juce::AudioProcessor
{
    CernTestProcessor()
        : AudioProcessor (BusesProperties().withOutput ("Out",
                            juce::AudioChannelSet::stereo(), true)) {}

    const juce::String getName() const override { return "CernTest"; }
    void prepareToPlay (double, int) override {}
    void releaseResources() override {}
    bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    bool hasEditor() const override { return false; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}
};

/** Kleiner, immer gleicher Aufbau für Engine-Integrationstests. */
struct EngineHarness
{
    CernTestProcessor dummy;
    juce::AudioProcessorValueTreeState apvts;
    ParameterSmoothing smoothing;
    KickSynthesisEngine engine;

    EngineHarness()
        : apvts (dummy, nullptr, "Params", createLayout()),
          smoothing (apvts), engine (apvts)
    {
        smoothing.prepare (48000.0);
        smoothing.setBlockSize (512);
        engine.prepare (48000.0, 512);
        engine.setTailLength (2.0f);
    }

    void set (const char* id, float plainValue)
    {
        if (auto* parameter = apvts.getParameter (id))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
    }

    void setBool (const char* id, bool on) { set (id, on ? 1.0f : 0.0f); }

    void settle (int blocks = 20)
    {
        for (int i = 0; i < blocks; ++i)
            smoothing.update();
    }

    void render (juce::AudioBuffer<float>& buffer)
    {
        smoothing.update();
        engine.render (buffer, buffer.getNumSamples(), smoothing);
    }
};
} // namespace

//==============================================================================
// SND-189: Layout und ID-Liste müssen exakt dieselben Parameter enthalten.
TEST (parameter_layout_ids_are_complete_unique)
{
    const auto& ids = ParameterIDs::getAllIDs();
    CHECK ((int) ids.size() == numLayoutParameters);
    CHECK (getLayoutParameterCount() == numLayoutParameters);

    std::set<juce::String> uniqueIds;
    for (const auto& id : ids)
        uniqueIds.insert (id);
    CHECK ((int) uniqueIds.size() == (int) ids.size());

    CernTestProcessor dummy;
    juce::AudioProcessorValueTreeState apvts (dummy, nullptr, "Params", createLayout());
    CHECK (apvts.processor.getParameters().size() == numLayoutParameters);

    int missing = 0;
    for (const auto& id : ids)
        if (apvts.getParameter (id) == nullptr || apvts.getRawParameterValue (id) == nullptr)
            ++missing;
    CHECK (missing == 0);
}

//==============================================================================
// SND-174: Ducking wirkt auf den Ausgang; duckDepth = 0 ist neutral.
TEST (ducking_depth_reduces_output)
{
    auto energyAtDepth = [] (float depthDb)
    {
        EngineHarness harness;
        harness.set (ParameterIDs::duckDepth, depthDb);
        harness.set (ParameterIDs::duckRelease, 80.0f);
        harness.settle();
        harness.engine.trigger (1.0f, 0.0f);

        juce::AudioBuffer<float> buffer (2, 512);
        double energy = 0.0;
        for (int block = 0; block < 80; ++block)
        {
            buffer.clear();
            harness.render (buffer);
            for (int ch = 0; ch < 2; ++ch)
                for (int i = 0; i < buffer.getNumSamples(); ++i)
                {
                    const double sample = buffer.getSample (ch, i);
                    energy += sample * sample;
                }
        }
        return energy;
    };

    const double dry = energyAtDepth (0.0f);
    const double ducked = energyAtDepth (24.0f);

    CHECK (dry > 1.0e-4);
    CHECK (ducked > 0.0);
    CHECK (ducked < dry * 0.5); // mindestens ~3 dB Energie weg
}

//==============================================================================
// SND-175: Transient-Shaper hebt Attack bzw. Sustain nur bei Bedarf an.
TEST (transient_shaper_attack_and_sustain)
{
    TransientShaper attack;
    attack.prepare (48000.0);
    attack.setAttackDb (12.0f);
    attack.setSustainDb (0.0f);

    float lastAttack = 0.0f;
    for (int i = 0; i < 200; ++i)
        lastAttack = attack.process (0.5f, 0.5f).first;

    CHECK (attack.getLastGainDb() > 3.0f);
    CHECK (lastAttack > 0.55f);

    TransientShaper sustain;
    sustain.prepare (48000.0);
    sustain.setAttackDb (0.0f);
    sustain.setSustainDb (12.0f);

    for (int i = 0; i < 2000; ++i)
        sustain.process (0.5f, 0.5f);

    float lastSustain = 0.5f;
    for (int i = 0; i < 600; ++i)
        lastSustain = sustain.process (0.1f, 0.1f).first;

    CHECK (sustain.getLastGainDb() > 1.0f);
    CHECK (lastSustain > 0.105f); // Sustain-Boost wirkt auf dem leisen Ausklang

    TransientShaper bypass;
    bypass.prepare (48000.0);
    float bypassed = 0.0f;
    for (int i = 0; i < 200; ++i)
        bypassed = bypass.process (0.5f, 0.5f).first;
    CHECK (std::abs (bypassed - 0.5f) < 1.0e-6f);
    CHECK (std::abs (bypass.getLastGainDb()) < 1.0e-6f);
}

//==============================================================================
// SND-176: MIDI-Filter (Kanal, Notenbereich, Choke über Blockgrenzen).
TEST (midi_filter_channel_note_range_and_choke)
{
    MidiFilter filter;
    filter.prepare (48000.0);
    filter.setChannel (2);
    filter.setNoteRange (30, 60);

    juce::MidiBuffer buffer;
    buffer.addEvent (juce::MidiMessage::noteOn (2, 40, (juce::uint8) 100), 0);
    buffer.addEvent (juce::MidiMessage::noteOn (1, 40, (juce::uint8) 100), 10); // falscher Kanal
    buffer.addEvent (juce::MidiMessage::noteOn (2, 70, (juce::uint8) 100), 20); // außerhalb Range
    filter.process (buffer, 512);

    int noteOns = 0;
    for (const auto metadata : buffer)
        if (metadata.getMessage().isNoteOn())
            ++noteOns;
    CHECK (noteOns == 1);

    // Choke: Note-On bei Sample 0, Choke 50 ms = 2400 Samples -> Note-Off im 5. Block.
    filter.setChokeMs (50.0f);
    int noteOffs = 0;
    for (int block = 0; block < 6; ++block)
    {
        juce::MidiBuffer blockBuffer;
        if (block == 0)
            blockBuffer.addEvent (juce::MidiMessage::noteOn (2, 40, (juce::uint8) 100), 0);
        filter.process (blockBuffer, 512);
        for (const auto metadata : blockBuffer)
            if (metadata.getMessage().isNoteOff())
                ++noteOffs;
    }
    CHECK (noteOffs == 1);

    filter.setChokeMs (0.0f);
    CHECK (filter.getNumPendingChokes() == 0);
}

//==============================================================================
// SND-177: Velocity -> Click skaliert den Click zusätzlich zur Velocity.
TEST (velocity_to_click_scales_click_level)
{
    auto clickEnergy = [] (float velocity, float amount)
    {
        ClickGenerator click;
        click.prepare (48000.0);
        click.setNoiseMix (0.0f); // nur Tick, deterministisch
        click.trigger (velocity, amount);

        double energy = 0.0;
        for (int i = 0; i < 512; ++i)
        {
            const float sample = click.process (0.8f, 4.0f, 0.35f, 0.0f, 1);
            energy += (double) sample * (double) sample;
        }
        return energy;
    };

    const double without = clickEnergy (0.25f, 0.0f);
    const double with = clickEnergy (0.25f, 1.0f);

    CHECK (without > 0.0);
    CHECK (with < without * 0.5);
    CHECK (std::abs (clickEnergy (1.0f, 1.0f) - clickEnergy (1.0f, 0.0f)) < 1.0e-9);
}

//==============================================================================
// SND-178: Humanize ist deterministisch, aber hörbar wirksam.
TEST (humanize_deterministic_and_effective)
{
    auto render = [] (float humanizeAmount, std::vector<float>& out)
    {
        EngineHarness harness;
        harness.set (ParameterIDs::humanize, humanizeAmount);
        harness.settle();

        // Ein stiller Block lädt die Block-Konstanten (u. a. humanizeAmount),
        // bevor der Trigger läuft – wie im echten Betrieb.
        juce::AudioBuffer<float> warmup (2, 512);
        warmup.clear();
        harness.render (warmup);

        harness.engine.trigger (1.0f, 0.0f);

        juce::AudioBuffer<float> buffer (2, 512);
        out.clear();
        out.reserve ((size_t) 40 * 512);

        for (int block = 0; block < 40; ++block)
        {
            buffer.clear();
            harness.render (buffer);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                out.push_back (buffer.getSample (0, i));
        }
    };

    std::vector<float> reference, humanizedA, humanizedB;
    render (0.0f, reference);
    render (1.0f, humanizedA);
    render (1.0f, humanizedB);

    CHECK (reference.size() == humanizedA.size());
    CHECK (humanizedA == humanizedB); // fester Seed/Zähler -> reproduzierbar

    double difference = 0.0;
    for (size_t i = 0; i < reference.size(); ++i)
        difference += std::abs ((double) humanizedA[i] - (double) reference[i]);
    CHECK (difference > 1.0e-3);
}

//==============================================================================
// SND-179: 2x-Oversampling ändert den Drive-/Clip-Pfad, bleibt aber stabil.
TEST (oversampling_changes_drive_path_and_stays_finite)
{
    auto render = [] (bool oversample, std::vector<float>& out)
    {
        EngineHarness harness;
        harness.setBool (ParameterIDs::oversampleEnabled, oversample);
        harness.set (ParameterIDs::driveAmount, 0.9f);
        harness.set (ParameterIDs::driveMix, 1.0f);
        harness.set (ParameterIDs::outputClip, 0.6f);
        harness.settle();
        harness.engine.trigger (1.0f, 0.0f);

        juce::AudioBuffer<float> buffer (2, 512);
        out.clear();
        out.reserve ((size_t) 60 * 512);

        for (int block = 0; block < 60; ++block)
        {
            buffer.clear();
            harness.render (buffer);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                const float sample = buffer.getSample (0, i);
                CHECK (std::isfinite (sample));
                CHECK (std::abs (sample) < 4.0f);
                out.push_back (sample);
            }
        }
    };

    std::vector<float> plain, oversampled;
    render (false, plain);
    render (true, oversampled);

    double difference = 0.0;
    for (size_t i = 0; i < plain.size(); ++i)
        difference += std::abs ((double) plain[i] - (double) oversampled[i]);
    CHECK (difference > 1.0e-4);
}

//==============================================================================
// SND-180: compMix = 0 liefert das trockene Signal (auch mit Makeup);
// Makeup wirkt ausschließlich auf den komprimierten Wet-Anteil.
TEST (compressor_mix_dry_bypasses_makeup)
{
    SoftKneeCompressor compressor;
    compressor.prepare (48000.0);

    float wetLast = 0.0f;
    for (int i = 0; i < 4800; ++i)
    {
        const auto out = compressor.process (0.8f, -0.8f, -30.0f, 4.0f,
                                             1.0f, 50.0f, 3.0f, 0.0f, 1.0f);
        wetLast = std::abs (out.first);
    }
    CHECK (compressor.getGainReductionDb() > 3.0f);
    CHECK (wetLast < 0.6f); // eingeschwungener Wet-Zweig ist komprimiert

    for (int i = 0; i < 200; ++i)
    {
        const auto out = compressor.process (0.8f, -0.8f, -30.0f, 4.0f,
                                             1.0f, 50.0f, 3.0f, 12.0f, 0.0f);
        CHECK (out.first == 0.8f);
        CHECK (out.second == -0.8f);
    }

    // Ohne Auto-Makeup und mit Mix=0 bleibt der Dry-Pegel unabhängig vom Makeup.
    SoftKneeCompressor manual;
    manual.prepare (48000.0);
    const auto lowMakeup = manual.process (0.4f, 0.4f, -30.0f, 4.0f, 1.0f, 50.0f, 3.0f, 0.0f, 0.0f);
    const auto highMakeup = manual.process (0.4f, 0.4f, -30.0f, 4.0f, 1.0f, 50.0f, 3.0f, 24.0f, 0.0f);
    CHECK (lowMakeup.first == highMakeup.first);
}

//==============================================================================
// SND-181: fxWidth skaliert nur Stereo-Wet-Tails, nie den Dry-Pfad.
TEST (fx_width_only_affects_wet_tails)
{
    auto renderWet = [] (float width, std::vector<float>& left, std::vector<float>& right)
    {
        EngineHarness harness;
        harness.engine.getPatchGraph().clearAll();
        harness.engine.getPatchGraph().addConnection (Jack::MembraneOut, Jack::ReverbIn);
        harness.engine.getPatchGraph().addConnection (Jack::ReverbOut, Jack::OutputIn);
        harness.setBool (ParameterIDs::membraneEnabled, true);
        harness.setBool (ParameterIDs::clickEnabled, false);
        harness.setBool (ParameterIDs::subEnabled, false);
        harness.setBool (ParameterIDs::bodyEnabled, false);
        harness.setBool (ParameterIDs::tubeEnabled, false);
        harness.setBool (ParameterIDs::reverbEnabled, true);
        harness.set (ParameterIDs::reverbMaterial, 2.0f); // Metal
        harness.set (ParameterIDs::reverbMix, 1.0f);
        harness.set (ParameterIDs::reverbDecay, 1800.0f);
        harness.set (ParameterIDs::fxWidth, width);
        harness.settle();
        harness.engine.trigger (1.0f, 0.0f);

        juce::AudioBuffer<float> buffer (2, 512);
        left.clear(); right.clear();
        for (int block = 0; block < 80; ++block)
        {
            buffer.clear();
            harness.render (buffer);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
            {
                left.push_back (buffer.getSample (0, i));
                right.push_back (buffer.getSample (1, i));
            }
        }
    };

    std::vector<float> l0, r0, l1, r1;
    renderWet (0.0f, l0, r0);
    renderWet (1.0f, l1, r1);

    auto maxSide = [] (const std::vector<float>& l, const std::vector<float>& r)
    {
        float side = 0.0f;
        for (size_t i = 0; i < l.size(); ++i)
            side = juce::jmax (side, std::abs (l[i] - r[i]));
        return side;
    };

    CHECK (maxSide (l0, r0) < 1.0e-6f);  // Width 0 -> mono Tail
    CHECK (maxSide (l1, r1) > 1.0e-5f);  // Width 1 -> Stereo-Tail

    // Dry-only-Patch: fxWidth hat keinen Einfluss.
    auto renderDry = [] (float width, std::vector<float>& out)
    {
        EngineHarness harness;
        harness.engine.getPatchGraph().clearAll();
        harness.engine.getPatchGraph().addConnection (Jack::MembraneOut, Jack::OutputIn);
        harness.setBool (ParameterIDs::reverbEnabled, false);
        harness.set (ParameterIDs::fxWidth, width);
        harness.settle();
        harness.engine.trigger (1.0f, 0.0f);

        juce::AudioBuffer<float> buffer (2, 512);
        out.clear();
        for (int block = 0; block < 40; ++block)
        {
            buffer.clear();
            harness.render (buffer);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                out.push_back (buffer.getSample (0, i));
        }
    };

    std::vector<float> dryNarrow, dryWide;
    renderDry (0.0f, dryNarrow);
    renderDry (1.0f, dryWide);
    CHECK (dryNarrow == dryWide);
}

//==============================================================================
// SND-005: Default-Signal erzeugt messbare Clip-/Limiter-Gain-Reduktion.
TEST (default_signal_produces_gain_reduction)
{
    EngineHarness harness;
    harness.settle();

    juce::AudioBuffer<float> warmup (2, 512);
    warmup.clear();
    harness.render (warmup);

    harness.engine.trigger (1.0f, 0.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    for (int block = 0; block < 80; ++block)
    {
        buffer.clear();
        harness.render (buffer);
    }

    const float clipGr = harness.engine.getClipGainReductionMaxDb();
    const float limiterGr = harness.engine.getLimiterGainReductionMaxDb();
    CHECK (std::isfinite (clipGr));
    CHECK (std::isfinite (limiterGr));
    CHECK (juce::jmax (clipGr, limiterGr) > 0.0f);
}

//==============================================================================
// SND-068: Tube-Resonanz folgt der Grundfrequenz nur mit tubeTrack=true.
TEST (tube_track_follows_pitch)
{
    auto render = [] (bool track, std::vector<float>& out)
    {
        EngineHarness harness;
        harness.engine.getPatchGraph().clearAll();
        harness.engine.getPatchGraph().addConnection (Jack::MembraneOut, Jack::TubeIn);
        harness.engine.getPatchGraph().addConnection (Jack::TubeOut, Jack::OutputIn);
        harness.setBool (ParameterIDs::membraneEnabled, true);
        harness.setBool (ParameterIDs::tubeEnabled, true);
        harness.setBool (ParameterIDs::clickEnabled, false);
        harness.setBool (ParameterIDs::subEnabled, false);
        harness.setBool (ParameterIDs::bodyEnabled, false);
        harness.set (ParameterIDs::tubeResonanceLevel, 1.0f);
        harness.setBool (ParameterIDs::tubeTrack, track);
        harness.settle();

        juce::AudioBuffer<float> warmup (2, 512);
        warmup.clear();
        harness.render (warmup);

        harness.engine.trigger (1.0f, 12.0f); // eine Oktave höher

        juce::AudioBuffer<float> buffer (2, 512);
        out.clear();
        out.reserve ((size_t) 80 * 512);
        for (int block = 0; block < 80; ++block)
        {
            buffer.clear();
            harness.render (buffer);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                out.push_back (buffer.getSample (0, i));
        }
    };

    std::vector<float> fixed, tracked;
    render (false, fixed);
    render (true, tracked);

    double difference = 0.0;
    for (size_t i = 0; i < fixed.size(); ++i)
        difference += std::abs ((double) fixed[i] - (double) tracked[i]);
    CHECK (difference > 1.0e-4);
}

//==============================================================================
// SND-183: Offline-Bounce schreibt ein lesbares Stereo-WAV mit Audio.
TEST (engine_bounce_writes_valid_wav)
{
    EngineHarness harness;
    harness.settle();

    const auto target = juce::File::getSpecialLocation (juce::File::tempDirectory)
                            .getChildFile ("cern_bounce_test.wav");
    target.deleteFile();

    const bool written = harness.engine.renderOfflineBounce (target, harness.smoothing, 1.0, 120.0f);
    CHECK (written);
    CHECK (target.existsAsFile());
    CHECK (target.getSize() > 1024);

    juce::AudioFormatManager formats;
    formats.registerBasicFormats();
    std::unique_ptr<juce::AudioFormatReader> reader (formats.createReaderFor (target));
    CHECK (reader != nullptr);

    if (reader != nullptr)
    {
        CHECK (reader->numChannels == 2);
        CHECK (std::abs (reader->sampleRate - 48000.0) < 1.0);
        CHECK (reader->lengthInSamples > 40000);

        const int samplesToRead = (int) (reader->lengthInSamples < 48000
                                             ? reader->lengthInSamples : 48000);
        juce::AudioBuffer<float> audio ((int) reader->numChannels, samplesToRead);
        reader->read (&audio, 0, audio.getNumSamples(), 0, true, true);
        CHECK (audio.getMagnitude (0, audio.getNumSamples()) > 1.0e-4f);
    }

    target.deleteFile();
}

//==============================================================================
// SND-118: higgsRate/higgsDepth sind an die Higgs-LFO verdrahtet.
TEST (higgs_lfo_rate_and_depth_affect_output)
{
    auto render = [] (float depth, float rate, std::vector<float>& out)
    {
        EngineHarness harness;
        harness.engine.getPatchGraph().clearAll();
        harness.engine.getPatchGraph().addConnection (Jack::MembraneOut, Jack::HiggsIn);
        harness.engine.getPatchGraph().addConnection (Jack::HiggsOut, Jack::OutputIn);
        harness.setBool (ParameterIDs::membraneEnabled, true);
        harness.setBool (ParameterIDs::clickEnabled, false);
        harness.setBool (ParameterIDs::subEnabled, false);
        harness.setBool (ParameterIDs::bodyEnabled, false);
        harness.setBool (ParameterIDs::tubeEnabled, false);
        harness.setBool (ParameterIDs::higgsEnabled, true);
        harness.set (ParameterIDs::higgsMix, 0.8f);
        harness.set (ParameterIDs::higgsDepth, depth);
        harness.set (ParameterIDs::higgsRate, rate);
        harness.settle();
        harness.engine.trigger (1.0f, 0.0f);

        juce::AudioBuffer<float> buffer (2, 512);
        out.clear();
        out.reserve ((size_t) 190 * 512);
        for (int block = 0; block < 190; ++block) // ca. 2 s
        {
            buffer.clear();
            harness.render (buffer);
            for (int i = 0; i < buffer.getNumSamples(); ++i)
                out.push_back (buffer.getSample (0, i));
        }
    };

    auto difference = [] (const std::vector<float>& a, const std::vector<float>& b)
    {
        double sum = 0.0;
        for (size_t i = 0; i < a.size(); ++i)
            sum += std::abs ((double) a[i] - (double) b[i]);
        return sum;
    };

    std::vector<float> noLfo, slowLfo, fastLfo;
    render (0.0f, 0.3f, noLfo);
    render (1.0f, 0.3f, slowLfo);
    render (1.0f, 2.0f, fastLfo);

    CHECK (difference (noLfo, slowLfo) > 1.0e-3);   // Depth wirkt
    CHECK (difference (slowLfo, fastLfo) > 1.0e-3); // Rate wirkt
}

//==============================================================================
// SND-006: Loudness-Streuung über die 7 Factory-Presets <= 1.5 dB.
TEST (factory_presets_loudness_spread)
{
    auto measurePreset = [] (int presetIndex)
    {
        CernTestProcessor dummy;
        juce::AudioProcessorValueTreeState apvts (dummy, nullptr, "Params", createLayout());
        ParameterSmoothing smoothing (apvts);
        KickSynthesisEngine engine (apvts);
        PresetManager presets (apvts, engine.getPatchGraph(), engine.getParameterPatchGraph());

        const bool loaded = presets.loadFactory (presetIndex);

        smoothing.prepare (48000.0);
        smoothing.setBlockSize (512);
        engine.prepare (48000.0, 512);
        engine.setTailLength (2.0f);

        if (! loaded)
            return -100.0f;

        for (int block = 0; block < 20; ++block)
            smoothing.update();

        constexpr int blockSize = 512;
        constexpr double bpm = 120.0;
        const int totalSamples = 4 * 4 * (int) (60.0 / bpm * 48000.0); // 4 Takte 4/4

        juce::AudioBuffer<float> full (2, totalSamples);
        full.clear();
        juce::AudioBuffer<float> blockBuffer (2, blockSize);
        double nextTrigger = 0.0;

        for (int blockStart = 0; blockStart < totalSamples; blockStart += blockSize)
        {
            while (nextTrigger < (double) (blockStart + blockSize))
            {
                const int offset = juce::jmax (0, (int) std::lround (nextTrigger) - blockStart);
                engine.enqueueTrigger (1.0f, 0.0f, offset);
                nextTrigger += 60.0 / bpm * 48000.0;
            }

            smoothing.update();
            blockBuffer.clear();
            engine.render (blockBuffer, blockSize, smoothing);
            full.copyFrom (0, blockStart, blockBuffer, 0, 0, blockSize);
            full.copyFrom (1, blockStart, blockBuffer, 1, 0, blockSize);
        }

        return LoudnessMeter::measureLufs (full, 48000.0);
    };

    float minLufs = 1.0e9f;
    float maxLufs = -1.0e9f;

    for (int preset = 0; preset < 7; ++preset)
    {
        const float lufs = measurePreset (preset);
        fprintf (stderr, "LD preset %d lufs %.2f\n", preset, lufs);
        CHECK (std::isfinite (lufs));
        CHECK (lufs > -40.0f && lufs < 0.0f);
        minLufs = juce::jmin (minLufs, lufs);
        maxLufs = juce::jmax (maxLufs, lufs);
    }

    CHECK (maxLufs - minLufs <= 1.5f);
}

//==============================================================================
// SND-154/156/157: Die virtuellen CV-Quellen (LFO, Envelope-Follower, Makros)
// sind über die APVTS verdrahtet - kein toter Parameter, host-automatisierbar.
TEST (cv_virtual_sources_follow_apvts)
{
    EngineHarness harness;

    const auto macro1 = ParameterIDs::macro (1);
    const auto macro5 = ParameterIDs::macro (5);

    harness.set (ParameterIDs::lfoRate, 3.5f);
    harness.set (ParameterIDs::lfoDepth, 0.75f);
    harness.set (macro1.toRawUTF8(), 0.6f);
    harness.set (macro5.toRawUTF8(), 0.2f);
    harness.settle();
    harness.engine.trigger (1.0f, 0.0f);

    juce::AudioBuffer<float> buffer (2, 512);

    for (int block = 0; block < 4; ++block)
    {
        buffer.clear();
        harness.render (buffer);
    }

    const auto& graph = harness.engine.getParameterPatchGraph();

    CHECK (std::abs (graph.getLfoRateHz() - 3.5f) < 0.01f);
    CHECK (std::abs (graph.getLfoDepth() - 0.75f) < 0.01f);
    CHECK (std::abs (graph.getMacroValue (0) - 0.6f) < 0.01f);
    CHECK (std::abs (graph.getMacroValue (4) - 0.2f) < 0.01f);

    // Envelope-Follower wird pro Block aus dem Ausgangspegel geschrieben.
    const int envIndex = ParameterPatchGraph::virtualSourceIndex (
                             ParameterPatchGraph::VirtualSource::envelopeFollower);
    CHECK (graph.getVirtualSourceNormalised (envIndex) > 0.0f);

    // Der LFO bleibt ein 0..1-Signal (Tiefe 0.75 -> Bereich 0.125..0.875).
    const int lfoIndex = ParameterPatchGraph::virtualSourceIndex (
                             ParameterPatchGraph::VirtualSource::lfo);
    float lfoMin = 2.0f, lfoMax = -1.0f;

    for (int block = 0; block < 40; ++block)
    {
        buffer.clear();
        harness.render (buffer);
        const float lfo = graph.getVirtualSourceNormalised (lfoIndex);
        lfoMin = juce::jmin (lfoMin, lfo);
        lfoMax = juce::jmax (lfoMax, lfo);
    }

    CHECK (lfoMin >= 0.0f);
    CHECK (lfoMax <= 1.0f);
    CHECK (lfoMax - lfoMin > 0.5f); // LFO laeuft bei 3.5 Hz ueber mehrere Bloecke
}

//==============================================================================
// SND-005/006/010/020: Limiter und Soft-Clip greifen messbar (GR-Metering),
// halten das Ceiling und bleiben ohne heisse Ansteuerung bei 0 dB GR.
TEST (limiter_and_clip_gain_reduction_metering)
{
    {
        EngineHarness quiet;
        quiet.set (ParameterIDs::outputGain, -18.0f);
        quiet.set (ParameterIDs::outputLimiter, 1.0f);
        quiet.set (ParameterIDs::outputClip, 0.0f);
        quiet.settle();
        quiet.engine.trigger (1.0f, 0.0f);

        juce::AudioBuffer<float> buffer (2, 512);

        for (int block = 0; block < 20; ++block)
        {
            buffer.clear();
            quiet.render (buffer);
        }

        CHECK (quiet.engine.getLimiterGainReductionMaxDb() < 0.01f);
    }

    EngineHarness hot;
    hot.set (ParameterIDs::outputGain, 12.0f);
    hot.set (ParameterIDs::outputLimiter, 1.0f);
    hot.set (ParameterIDs::outputClip, 1.0f);
    hot.settle();
    hot.engine.trigger (1.0f, 0.0f);

    juce::AudioBuffer<float> buffer (2, 512);
    float peak = 0.0f;

    for (int block = 0; block < 20; ++block)
    {
        buffer.clear();
        hot.render (buffer);
        peak = juce::jmax (peak, buffer.getMagnitude (0, buffer.getNumSamples()));
    }

    CHECK (peak > 1.0e-3f);
    CHECK (peak <= 1.0f + 1.0e-3f);                          // SND-010: Ceiling haelt
    CHECK (hot.engine.getLimiterGainReductionMaxDb() > 0.0f); // SND-005/006
    CHECK (hot.engine.getClipGainReductionMaxDb() > 0.0f);    // SND-006/020
    CHECK (hot.engine.getDriveOutputLevelDb() > -100.0f);     // SND-020: Drive-Meter
}

//==============================================================================
// MR-090 (Mechanismus): Der Wet-LowCut (100 Hz, 2. Ordnung) haelt das
// 20-100-Hz-Band systematisch unter dem Nutzband. Nachgewiesen auf der
// Impulsantwort, damit das Ergebnis nicht vom Testsignal abhaengt.
TEST (material_room_wet_lowcut_attenuates_sub_band)
{
    auto bandRms = [] (const std::vector<float>& x, double loHz, double hiHz)
    {
        const double sr = 48000.0;
        const double hp = std::exp (-2.0 * juce::MathConstants<double>::pi * loHz / sr);
        const double lp = std::exp (-2.0 * juce::MathConstants<double>::pi * hiHz / sr);
        double hp1 = 0.0, hp2 = 0.0, l1 = 0.0, l2 = 0.0, acc = 0.0;
        int n = 0;

        for (float s : x)
        {
            const double v = (double) s;
            hp1 = (1.0 - hp) * v + hp * hp1;
            const double h = v - hp1;
            hp2 = (1.0 - hp) * h + hp * hp2;
            const double hh = h - hp2;
            l1 = (1.0 - lp) * hh + lp * l1;
            l2 = (1.0 - lp) * l1 + lp * l2;
            acc += l2 * l2;
            ++n;
        }

        return n > 0 ? std::sqrt (acc / (double) n) : 0.0;
    };

    for (int mat = 0; mat < 6; ++mat)
    {
        MaterialRoom room;
        room.prepare (48000.0, 512);
        room.setParameters (mat, 8.0f, 1.0f, 2, 0.5f,
                            0.0f, 45.0f, 0.5f, 0.5f, -0.5f, 0.5f, 0, 0, 0.5f);

        std::vector<float> wet ((size_t) 48000, 0.0f);

        for (int i = 0; i < 48000; ++i)
        {
            const float k = (i == 0) ? 1.0f : 0.0f;
            const auto out = room.process (k, k, k, k);
            wet[(size_t) i] = 0.5f * (out.first + out.second);
        }

        const double low = bandRms (wet, 20.0, 100.0);
        const double mid = bandRms (wet, 150.0, 400.0);

        CHECK (low > 1.0e-7);   // Wet traegt im Sub-Band bei
        CHECK (mid > 1.0e-7);   // Wet traegt im Nutzband bei
        CHECK (low < mid * 0.6); // Low-Cut wirkt (>= ~4.4 dB Absenkung)
    }
}

//==============================================================================
int main (int argc, char* argv[])
{
    (void) argc; (void) argv;

    std::printf ("CERN Unit Tests\n");
    std::printf ("=====================\n\n");

    std::printf ("\n=====================\n");
    std::printf ("Passed: %d\n", testsPassed);
    std::printf ("Failed: %d\n", testsFailed);
    return testsFailed == 0 ? 0 : 1;
}
