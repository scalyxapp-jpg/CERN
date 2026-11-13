#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include <atomic>
#include <memory>
#include <vector>

#include "DSP/KickSynthesisEngine.h"
#include "Parameters/ParameterLayout.h"
#include "Parameters/ParameterSmoothing.h"
#include "Presets/PresetManager.h"

//==============================================================================
/** Rendert alle Factory-Presets (und Init) im Hintergrund in einen
    64-Punkt-Wellenform-Snapshot, damit der Preset-Auswahlknopf eine
    Vorschau des Klangs zeigen kann (Review §7.7).

    Kein GUI-Thread-Render: 16 Presets à 0,6 s Audio dauern sonst sichtbar
    lange; das Rendern passiert im Low-Priority-Thread, das GUI liest nur
    den fertiggestellten Snapshot.
*/
class PresetThumbnailCache : private juce::Thread
{
public:
    PresetThumbnailCache();
    ~PresetThumbnailCache() override;

    /** Startet das Rendern (einmalig, lazy). */
    void startRendering();

    /** 0 = Init, 1..N = Factory-Presets (factoryIndex + 1). */
    static constexpr int initThumbnailIndex = 0;

    /** 64 Punkte 0..1 (Peak-Pegel, relativ zum Preset-Peak) oder leer,
        solange der Preset noch rendert. Kopie – lockfrei und sicher. */
    std::vector<float> getCurve (int thumbnailIndex) const;

    /** true, wenn der Preset bereits gerendert wurde. */
    bool isReady (int thumbnailIndex) const;

private:
    void run() override;

    /** Deterministischer Dummy-AudioProcessor wie in den Tests. */
    class LabProcessor : public juce::AudioProcessor
    {
    public:
        LabProcessor()
            : AudioProcessor (BusesProperties().withOutput ("Out",
                                juce::AudioChannelSet::stereo(), true)) {}

        const juce::String getName() const override { return "CernThumbs"; }
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

    static constexpr int curvePoints = 64;

    std::vector<std::vector<float>> curves;
    std::vector<char> ready;          ///< 1 = fertig (nur unter dataLock lesbar)
    mutable juce::SpinLock dataLock;
    bool started { false };
};

//==============================================================================
/** Kleine Wellenform-Vorschau für den Preset-Auswahlknopf. */
class PresetPreview : public juce::Component,
                      public juce::SettableTooltipClient
{
public:
    PresetPreview();

    /** Kurve übergeben (64 Werte 0..1). Leer = "NO PREVIEW". */
    void setCurve (const std::vector<float>& newCurve);

    void paint (juce::Graphics& g) override;

private:
    std::vector<float> curve;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PresetPreview)
};
