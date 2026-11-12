#include "WaveformDisplay.h"
#include "GUI/Theme/CERNLookAndFeel.h"

//==============================================================================
WaveformDisplay::WaveformDisplay()
{
    setTitle ("Waveform Display");
    setDescription ("Waveform of the most recent output samples");
}

void WaveformDisplay::setSamples (const std::vector<float>& newSamples)
{
    bool changed = newSamples.size() != samples.size();

    if (! changed)
    {
        for (size_t i = 0; i < newSamples.size(); ++i)
        {
            const float value = std::isfinite (newSamples[i]) ? newSamples[i] : 0.0f;

            if (value != samples[i])
            {
                changed = true;
                break;
            }
        }
    }

    if (! changed)
        return;

    samples = newSamples;

    for (auto& sample : samples)
        if (! std::isfinite (sample))
            sample = 0.0f;

    repaint();
}

void WaveformDisplay::clear()
{
    if (samples.empty())
        return;

    samples.clear();
    repaint();
}

void WaveformDisplay::paint (juce::Graphics& g)
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
    g.drawText ("WAVEFORM", bounds.reduced (10.0f, 6.0f).removeFromTop (16.0f),
                juce::Justification::topLeft, false);

    // Gitter
    g.setColour (CERNLookAndFeel::textColour.withAlpha (0.07f));
    for (int i = 1; i < 4; ++i)
    {
        const float y = area.getY() + area.getHeight() * (float) i / 4.0f;
        g.drawHorizontalLine ((int) y, area.getX(), area.getRight());
    }

    g.setColour (CERNLookAndFeel::textColour.withAlpha (0.18f));
    g.drawHorizontalLine ((int) area.getCentreY(), area.getX(), area.getRight());

    if (samples.size() < 2)
    {
        g.setColour (CERNLookAndFeel::textColour.withAlpha (0.45f));
        g.drawText ("NO SIGNAL", area, juce::Justification::centred, false);
        return;
    }

    if (area.getWidth() <= 0.0f || area.getHeight() <= 0.0f)
        return;

    float maxAbs = 0.0f;
    for (const auto sample : samples)
        if (std::isfinite (sample))
            maxAbs = juce::jmax (maxAbs, std::abs (sample));

    if (maxAbs < 1.0e-5f)
    {
        g.setColour (CERNLookAndFeel::textColour.withAlpha (0.45f));
        g.drawText ("NO SIGNAL", area, juce::Justification::centred, false);
        return;
    }

    // Waveform als gefuellte Flaeche (Peak-orientiert)
    const int numPoints = juce::jmin ((int) samples.size(), juce::jmax (2, (int) area.getWidth()));

    if (numPoints < 2)
        return;

    const float centreY = area.getCentreY();
    const float amplitude = area.getHeight() * 0.46f;

    juce::Path path;
    path.startNewSubPath (area.getX(), centreY);

    for (int i = 0; i < numPoints; ++i)
    {
        const int start = (int) ((int64_t) i * (int64_t) samples.size() / numPoints);
        const int end   = (int) ((int64_t) (i + 1) * (int64_t) samples.size() / numPoints);

        float peak = 0.0f;
        for (int s = start; s < juce::jmax (start + 1, end) && s < (int) samples.size(); ++s)
            if (std::isfinite (samples[(size_t) s]))
                peak = juce::jmax (peak, std::abs (samples[(size_t) s]));

        const float x = area.getX() + area.getWidth() * (float) i / (float) (numPoints - 1);
        path.lineTo (x, centreY - peak * amplitude);
    }

    g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.85f));
    g.strokePath (path, juce::PathStrokeType (1.5f,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}
