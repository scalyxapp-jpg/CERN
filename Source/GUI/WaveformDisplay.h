#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <vector>

#include "DSP/LayeringStage.h"

//==============================================================================
/** Zeichnet die letzten Ausgangs-Samples als Waveform (Ringpuffer-Snapshot).

    Zusaetzlich werden – wenn "Layering enabled" aktiv ist – die beiden
    automatisch abgeleiteten Zusatz-Layer als eigene Kurven ueberlagert und
    mit ihren errechneten Werten (Frequenz, Laenge, Pegel, Dichte) beschriftet.
*/
class WaveformDisplay : public juce::Component
{
public:
    WaveformDisplay();
    ~WaveformDisplay() override = default;

    void setSamples (const std::vector<float>& newSamples);

    /** Layer-Taps (Depth + Presence) fuer die Overlay-Anzeige. */
    void setLayerSamples (const std::vector<float>& low, const std::vector<float>& high);

    /** Abgeleitete Layer-Werte fuer die Legende. */
    void setLayerInfo (const LayeringStage::Info& newInfo);

    /** Baut Checkbox "LAYERING ENABLED" + Regler "AMOUNT" in die Kopfzeile ein. */
    void attachLayeringControl (juce::AudioProcessorValueTreeState& apvts,
                                const juce::String& enabledParameterId,
                                const juce::String& amountParameterId);

    void clear();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void paintLayer (juce::Graphics& g, const juce::Rectangle<float>& area,
                     const std::vector<float>& data, float referencePeak,
                     juce::Colour colour, float thickness) const;

    std::vector<float> samples;
    std::vector<float> layerLowSamples;
    std::vector<float> layerHighSamples;
    LayeringStage::Info layerInfo;

    juce::ToggleButton layeringToggle { "LAYERING ENABLED" };
    juce::Slider amountSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> layeringAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> amountAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
};
