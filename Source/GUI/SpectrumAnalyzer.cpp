#include "SpectrumAnalyzer.h"
#include "GUI/Theme/CERNLookAndFeel.h"

#include <juce_audio_basics/juce_audio_basics.h>

#include <cmath>
#include <utility>

//==============================================================================
SpectrumAnalyzer::SpectrumAnalyzer()
{
    setTitle ("Spectrum Analyzer");
    setDescription ("Frequency spectrum of the output signal");
}

void SpectrumAnalyzer::setSpectrum (const std::vector<float>& mags)
{
    std::vector<float> sanitised;
    sanitised.reserve (mags.size());

    for (const float magnitude : mags)
        sanitised.push_back (std::isfinite (magnitude) ? juce::jmax (0.0f, magnitude) : 0.0f);

    if (sanitised == magnitudes)
        return;

    magnitudes = std::move (sanitised);
    repaint();
}

void SpectrumAnalyzer::setBarFrequencies (const std::vector<float>& frequencies)
{
    if (frequencies == barFrequencies)
        return;

    barFrequencies = frequencies;
    repaint();
}

void SpectrumAnalyzer::clear()
{
    if (magnitudes.empty())
        return;

    magnitudes.clear();
    repaint();
}

void SpectrumAnalyzer::paint (juce::Graphics& g)
{
    const juce::Rectangle<float> bounds (0.0f, 0.0f,
                                         (float) getWidth(), (float) getHeight());

    // 3D-Display: erhabener Rahmen, versenkte Anzeigeflaeche.
    CERNLookAndFeel::drawBevel (g, bounds, 5.0f, true, 0.10f);
    CERNLookAndFeel::drawWell (g, bounds.reduced (3.0f), 3.5f);

    g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.55f));
    g.fillRect (bounds.getX() + 3.0f, bounds.getY() + 3.0f, 28.0f, 2.0f);

    auto area = bounds.reduced (10.0f, 8.0f);
    area.removeFromTop (18.0f);

    // Titel
    g.setColour (CERNLookAndFeel::textColour.withAlpha (0.85f));
    g.setFont (CERNLookAndFeel::getNameFont());
    g.drawText ("SPECTRUM", bounds.reduced (10.0f, 6.0f).removeFromTop (16.0f),
                juce::Justification::topLeft, false);

    // dB-Gitter (-12 / -24 / -36 / -48 dB)
    g.setColour (CERNLookAndFeel::textColour.withAlpha (0.07f));
    for (int i = 1; i < 5; ++i)
    {
        const float y = area.getY() + area.getHeight() * (float) i / 5.0f;
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
    }

    if (magnitudes.empty())
    {
        g.setColour (CERNLookAndFeel::textColour.withAlpha (0.45f));
        g.drawText ("NO SIGNAL", area, juce::Justification::centred, false);
        return;
    }

    float maxMagnitude = 0.0f;
    for (const auto magnitude : magnitudes)
        if (std::isfinite (magnitude))
            maxMagnitude = juce::jmax (maxMagnitude, magnitude);

    if (maxMagnitude < 1.0e-6f)
    {
        g.setColour (CERNLookAndFeel::textColour.withAlpha (0.45f));
        g.drawText ("NO SIGNAL", area, juce::Justification::centred, false);
        return;
    }

    const int numBars = (int) magnitudes.size();

    if (numBars <= 0 || area.getWidth() <= 0.0f || area.getHeight() <= 0.0f)
        return;

    const float barWidth = area.getWidth() / (float) numBars;

    // Frequenz-Achsenbeschriftung (50/120/500/2k/10k Hz).
    if (barFrequencies.size() == magnitudes.size())
    {
        const float thresholds[] = { 50.0f, 120.0f, 500.0f, 2000.0f, 10000.0f };
        const char* labels[] = { "50", "120", "500", "2k", "10k" };

        g.setFont (CERNLookAndFeel::getLabelFont());
        g.setColour (CERNLookAndFeel::textColour.withAlpha (0.40f));

        for (int t = 0; t < 5; ++t)
        {
            for (size_t i = 0; i < barFrequencies.size(); ++i)
            {
                if (barFrequencies[i] >= thresholds[t])
                {
                    const float x = area.getX() + (float) i * barWidth;
                    g.drawVerticalLine ((int) x, area.getBottom() - 8.0f, area.getBottom());
                    g.drawText (labels[t], x - 12.0f, area.getBottom() - 16.0f,
                                24.0f, 12.0f, juce::Justification::centred, false);
                    break;
                }
            }
        }
    }

    for (int i = 0; i < numBars; ++i)
    {
        const float magnitude = magnitudes[(size_t) i];

        if (! std::isfinite (magnitude))
            continue;

        const float db = juce::Decibels::gainToDecibels (magnitude + 1.0e-7f, -72.0f);
        const float norm = juce::jlimit (0.0f, 1.0f, (db + 72.0f) / 72.0f);
        const float barHeight = norm * area.getHeight();

        if (barHeight < 1.0f)
            continue;

        const float x = area.getX() + (float) i * barWidth;

        g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.35f + 0.65f * norm));
        g.fillRect (x, area.getBottom() - barHeight, juce::jmax (1.0f, barWidth - 1.0f), barHeight);
    }
}
