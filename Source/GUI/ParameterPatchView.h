#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "DSP/ParameterPatchGraph.h"

//==============================================================================
/** ParameterPatchView – Eurorack-artige CV/Modulations-Patch-Ansicht für alle
    Parameter aus ParameterIDs::getAllIDs() (1229) plus die virtuellen Quellen
    LFO, ENV FOLLOW und MACRO 1-5. Jeder Parameter hat OUT (bipolarer CV-Wert)
    und IN (CV-Eingang, der auf den Basis-Parameter summiert wird).

    Oben sitzt der Kabel-Inspektor: AMOUNT (-1..+1), UNIPOLAR, SLEW (ms),
    1/64 HOLD und CV WRAP. Die Ansicht ist scrollbar, da die Zeilen mehr Höhe
    benötigen als die 750px des Front-Panels. */
class ParameterPatchView : public juce::Component,
                           private juce::Timer
{
public:
    explicit ParameterPatchView (juce::AudioProcessorValueTreeState& apvts,
                                 ParameterPatchGraph& graph);
    ~ParameterPatchView() override;

    void resized() override;

    /** Index des ausgewählten Kabels (Reihenfolge aus getConnections()). */
    int getSelectedCable() const noexcept { return selectedCable; }
    void setSelectedCable (int cableIndex);

    /** Live-CV eines Parameters 0..1 (für Ring-Anzeige). */
    float sourceNormalised (int parameterIndex) const;

private:
    class Canvas;

    void timerCallback() override;
    void refreshInspector();
    void writeInspectorToCable();

    juce::AudioProcessorValueTreeState& parameters;
    ParameterPatchGraph& parameterPatch;

    std::unique_ptr<Canvas> canvas;
    juce::Viewport viewport;

    juce::Label cableLabel;
    juce::Slider amountSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider slewSlider   { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider lfoRateSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Slider lfoDepthSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::ToggleButton unipolarButton { "UNIPOLAR" };
    juce::ToggleButton noteRateButton { "1/64 HOLD" };
    juce::ToggleButton wrapButton { "CV WRAP" };
    juce::TextButton removeCableButton { "REMOVE CABLE" };

    int selectedCable = -1;
    bool updatingInspector = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParameterPatchView)
};
