#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <cmath>
#include <vector>

//==============================================================================
/** Zeichnet die letzten Ausgangs-Samples als Waveform (Ringpuffer-Snapshot). */
class WaveformDisplay : public juce::Component
{
public:
    WaveformDisplay();
    ~WaveformDisplay() override = default;

    void setSamples (const std::vector<float>& newSamples);
    void clear();

    void paint (juce::Graphics& g) override;

private:
    std::vector<float> samples;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (WaveformDisplay)
};
