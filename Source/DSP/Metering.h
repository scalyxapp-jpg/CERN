#pragma once

#include <cmath>
#include <vector>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
/** Einfache RMS-/Peak-Messung für Displays. */
class Metering
{
public:
    static float rms (const juce::AudioBuffer<float>& buffer, int channel)
    {
        const int numSamples = buffer.getNumSamples();

        if (numSamples <= 0 || ! juce::isPositiveAndBelow (channel, buffer.getNumChannels()))
            return 0.0f;

        float sum = 0.0f;
        const float* data = buffer.getReadPointer (channel);
        for (int i = 0; i < numSamples; ++i)
            sum += data[i] * data[i];
        return std::sqrt (sum / (float) numSamples);
    }

    static float peak (const juce::AudioBuffer<float>& buffer, int channel)
    {
        const int numSamples = buffer.getNumSamples();

        if (numSamples <= 0 || ! juce::isPositiveAndBelow (channel, buffer.getNumChannels()))
            return 0.0f;

        float p = 0.0f;
        const float* data = buffer.getReadPointer (channel);
        for (int i = 0; i < numSamples; ++i)
            p = juce::jmax (p, std::abs (data[i]));
        return p;
    }

    static float toDb (float linear)
    {
        return juce::Decibels::gainToDecibels (linear + 1.0e-9f);
    }
};
