#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cmath>
#include <vector>

#include "JuceHeader.h"
#include "Parameters/ParameterIDs.h"
#include "Parameters/ParameterLayout.h"
#include "Parameters/ParameterSmoothing.h"
#include "DSP/KickSynthesisEngine.h"
#include "DSP/MidiFilter.h"
#include "Sequencer/StepSequencer.h"
#include "Presets/PresetManager.h"
#include "DSP/LoudnessMeter.h"


//==============================================================================
/** CERN AudioProcessor.
 *
 *  Host-seitige Logik: APVTS mit 1229 Parametern, ParameterSmoothing für
 *  click-free audio-thread Reads, StepSequencer (sample-genaue Clock),
 *  PresetManager (Factory XML + User XML) und Trigger-Pfad.
 *
 *  Thread-Modell gemäß §11:
 *  - APVTS-Parameter werden vom Message-Thread geschrieben, vom Audio-Thread
 *    über smoothing.getById() gelesen.
 *  - Pending-Trigger sind atomar (Message-Thread setzt, Audio-Thread konsumiert).
 *  - Doppelgepufferte Display-Buffers für GUI-Metering.
 *  - SND-176: Eingehende MIDI durchlaufen den MidiFilter (Kanal/Noten-Bereich/
 *    Choke) bevor der Note-On-Pfad sie in Trigger umsetzt.
 */
class CERNAudioProcessor : public juce::AudioProcessor
{
public:
    CERNAudioProcessor();
    ~CERNAudioProcessor() override;

    //==============================================================================
    void prepareToPlay (double sampleRate, int samplesPerBlock) override;
    void releaseResources() override;
    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;

    //==============================================================================
    juce::AudioProcessorEditor* createEditor() override;
    bool hasEditor() const override;

    //==============================================================================
    juce::AudioProcessorValueTreeState& getAPVTS() { return apvts; }
    ParameterSmoothing& getSmoothing() { return smoothing; }
    KickSynthesisEngine& getEngine() { return engine; }
    const KickSynthesisEngine& getEngine() const { return engine; }
    StepSequencer& getSequencer() { return sequencer; }
    PresetManager& getPresetManager() { return presetManager; }

    PatchGraph& getPatchGraph() { return engine.getPatchGraph(); }
    const PatchGraph& getPatchGraph() const { return engine.getPatchGraph(); }
    ParameterPatchGraph& getParameterPatchGraph() { return engine.getParameterPatchGraph(); }
    const ParameterPatchGraph& getParameterPatchGraph() const { return engine.getParameterPatchGraph(); }

    /** SND-183: Offline-Bounce des aktuellen Zustands als WAV (Processor-Methode).
        Delegiert an KickSynthesisEngine::renderOfflineBounce() und rendert
        `secondsToRender` Sekunden bei `bpm` (Viertelschlag-Trigger). */
    bool renderOfflineBounce (const juce::File& target,
                              double secondsToRender,
                              float bpm)
    {
        return engine.renderOfflineBounce (target, smoothing, secondsToRender, bpm);
    }

    //==============================================================================
    // Display-Werte (Audio -> GUI, thread-safe via atomics)
    struct DisplayBuffers
    {
        static constexpr int waveformSize       = 4096; // Ringpuffer (2er-Potenz)
        static constexpr int waveformDecimation = 2;    // Snapshot-Rate = SR / 2

        std::atomic<float> rmsDb { -120.0f };
        std::atomic<float> peakDb { -120.0f };
        std::atomic<float> grDb  { 0.0f };
        std::atomic<bool>  triggerLit { false };

        // SND-170: Kurzzeit-LUFS (BS.1770, 400 ms) und GR-Verlauf.
        std::atomic<float> shortTermLufs { LoudnessMeter::silenceLufs };
        static constexpr int grHistorySize = 128;   // 2er-Potenz (Index-Maske)
        std::array<std::atomic<float>, (size_t) grHistorySize> grHistory {};
        std::atomic<int> grHistoryWritePos { 0 };   // monoton, & (grHistorySize - 1)

        std::array<std::atomic<float>, (size_t) waveformSize> waveform {};
        std::atomic<int> waveformWritePos { 0 };

        // Automatisches Layering: separate Taps der beiden Zusatz-Layer,
        // damit die Waveform-Anzeige sie sichtbar ueberlagern kann.
        std::array<std::atomic<float>, (size_t) waveformSize> layerLow {};
        std::array<std::atomic<float>, (size_t) waveformSize> layerHigh {};

        // Modul-Beitrags-Taps (Membrane/Body/Sub/Click) fuer das Modul-Overlay.
        std::array<std::atomic<float>, (size_t) waveformSize> moduleMembrane {};
        std::array<std::atomic<float>, (size_t) waveformSize> moduleBody {};
        std::array<std::atomic<float>, (size_t) waveformSize> moduleSub {};
        std::array<std::atomic<float>, (size_t) waveformSize> moduleClick {};
    };
    DisplayBuffers& getDisplayBuffers() { return displayBuffers; }
    const DisplayBuffers& getDisplayBuffers() const { return displayBuffers; }

    /** SND-170: letzter Kurzzeit-LUFS-Wert (Audio -> GUI). */
    float getShortTermLufs() const { return displayBuffers.shortTermLufs.load (std::memory_order_relaxed); }

    /** SND-170: kopiert die GR-History in chronologischer Reihenfolge
        (aeltester -> neuester Wert) nach `dest`; nicht gefuellte Slots = 0 dB. */
    void copyGainReductionHistory (float* dest, int numValues) const;

    /** SND-168: UndoManager des APVTS (Ctrl+Z im Editor). */
    juce::UndoManager& getUndoManager() { return undoManager; }

    // Trigger von extern (GUI, MIDI, Sequencer)
    void triggerNote (float velocity, float pitchSemitones = 0.0f);

    //==============================================================================
    const juce::String getName() const override { return "C.E.R.N"; }

    bool acceptsMidi() const override { return true; }
    // SND-150: MIDI-Clock/Start/Stop verlassen das Plugin.
    bool producesMidi() const override { return true; }
    bool isMidiEffect() const override { return false; }

    double getTailLengthSeconds() const override { return engine.getTailLengthSeconds(); }

    //==============================================================================
    /** Host-seitiges Preset-Menue: Init + alle eingebetteten Factory-Presets. */
    int getNumPrograms() override;
    int getCurrentProgram() override;
    void setCurrentProgram (int index) override;
    const juce::String getProgramName (int index) override;
    void changeProgramName (int, const juce::String&) override;

    //==============================================================================
    void getStateInformation (juce::MemoryBlock& destData) override;
    void setStateInformation (const void* data, int sizeInBytes) override;

private:
    //==============================================================================
    /** Anzahl Host-Programme: Init + Factory-Presets. */
    int factoryProgramCount() const;

    /** Host-Preset-Menue setzt Factory- bzw. Init-Preset (Index 0 = Init). */
    void setFactoryProgram (int index);

    /** Tail-Laenge aus roomDecay/reverbDecay ableiten (nur bei Aenderung setzen). */
    void updateTailLength();

    /** SND-150: 24-PPQN-Clock plus Start/Stop in den MIDI-Ausgang schreiben. */
    void renderMidiClock (juce::MidiBuffer& midiMessages, int numSamples);

    // SND-168: muss VOR apvts deklariert sein (Initialisierungsreihenfolge).
    juce::UndoManager undoManager;
    juce::AudioProcessorValueTreeState apvts;
    ParameterSmoothing smoothing;
    KickSynthesisEngine engine;
    StepSequencer sequencer;
    PresetManager presetManager;

    DisplayBuffers displayBuffers;
    LoudnessMeter loudnessMeter; // SND-170

    bool lastTriggerButtonState { false };
    bool prepared { false };
    int currentProgram { 0 };
    float tailLengthSeconds { 1.5f };

    // B7: Zuletzt an den Host gemeldete Latenz (Oversampling-Pfad).
    int reportedLatencySamples { 0 };

    // ---- MIDI-Clock-Ausgang (SND-150) ----
    double midiClockSamplesUntilTick { 0.0 };
    bool   midiClockWasRunning { false };

    // ---- MIDI-Filter / Choke (SND-176) ----
    MidiFilter midiFilter;

    //==============================================================================
    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CERNAudioProcessor)
};