#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

//==============================================================================
/** FFT-Spectrum-Analyzer (Balken-Diagramm, logarithmische Frequenzachse). */
class SpectrumAnalyzer : public juce::Component
{
public:
    SpectrumAnalyzer();
    ~SpectrumAnalyzer() override = default;

    void setSpectrum (const std::vector<float>& magnitudes);
    void clear();

    void paint (juce::Graphics& g) override;

private:
    std::vector<float> magnitudes;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SpectrumAnalyzer)
};
