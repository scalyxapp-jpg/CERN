#include "WaveformDisplay.h"
#include "GUI/Theme/CERNLookAndFeel.h"

namespace
{
    const juce::Colour layerLowColour  { 0xff8fd4ff }; // kuehles Weiss-Blau = Tiefe
    const juce::Colour layerHighColour { 0xffffc36b }; // warmes Amber      = Praesenz

    juce::String hzText (float hz)
    {
        if (hz >= 1000.0f)
            return juce::String (hz / 1000.0f, 2) + " kHz";

        return juce::String (hz, hz < 100.0f ? 1 : 0) + " Hz";
    }

    juce::String msText (float ms)
    {
        return juce::String (ms, ms < 100.0f ? 0 : 0) + " ms";
    }

    juce::String dbText (float db)
    {
        return juce::String (db, 1) + " dB";
    }
}

//==============================================================================
WaveformDisplay::WaveformDisplay()
{
    setTitle ("Waveform Display");
    setDescription ("Waveform of the most recent output samples");

    layeringToggle.setClickingTogglesState (true);
    layeringToggle.setTooltip ("Adds two automatically derived layers "
                               "(depth + presence) to the kick");
    layeringToggle.setTitle ("Layering enabled");
    layeringToggle.setDescription ("Enable automatic two-layer layering");
    addAndMakeVisible (layeringToggle);

    amountSlider.setRange (0.0, 200.0, 1.0);
    amountSlider.setValue (100.0, juce::dontSendNotification);
    amountSlider.setTextValueSuffix (" %");
    amountSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 46, 18);
    amountSlider.setTooltip ("Scales both auto layers (0 % = neutral, 200 % = double)");
    amountSlider.setTitle ("Layering amount");
    amountSlider.setDescription ("Amount of the two automatically derived layers");
    amountSlider.setEnabled (false);
    addAndMakeVisible (amountSlider);

    layeringToggle.onStateChange = [this]
    {
        amountSlider.setEnabled (layeringToggle.getToggleState());
        repaint();
    };
}

void WaveformDisplay::attachLayeringControl (juce::AudioProcessorValueTreeState& apvts,
                                             const juce::String& enabledParameterId,
                                             const juce::String& amountParameterId)
{
    if (apvts.getParameter (enabledParameterId) != nullptr)
        layeringAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            apvts, enabledParameterId, layeringToggle);

    if (apvts.getParameter (amountParameterId) != nullptr)
        amountAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
            apvts, amountParameterId, amountSlider);

    amountSlider.setEnabled (layeringToggle.getToggleState());

    resized();
    repaint();
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

void WaveformDisplay::setLayerSamples (const std::vector<float>& low,
                                       const std::vector<float>& high)
{
    layerLowSamples = low;
    layerHighSamples = high;

    for (auto& sample : layerLowSamples)
        if (! std::isfinite (sample))
            sample = 0.0f;

    for (auto& sample : layerHighSamples)
        if (! std::isfinite (sample))
            sample = 0.0f;
}

void WaveformDisplay::setLayerInfo (const LayeringStage::Info& newInfo)
{
    bool changed = newInfo.enabled != layerInfo.enabled
                || newInfo.lowHz != layerInfo.lowHz
                || newInfo.lowDecayMs != layerInfo.lowDecayMs
                || newInfo.highHz != layerInfo.highHz
                || newInfo.highDecayMs != layerInfo.highDecayMs
                || std::abs (newInfo.lowGainDb - layerInfo.lowGainDb) > 0.05f
                || std::abs (newInfo.highGainDb - layerInfo.highGainDb) > 0.05f
                || std::abs (newInfo.density - layerInfo.density) > 0.005f
                || newInfo.highDelayMs != layerInfo.highDelayMs;

    layerInfo = newInfo;

    if (changed)
        repaint();
}

void WaveformDisplay::clear()
{
    if (samples.empty())
        return;

    samples.clear();
    repaint();
}

void WaveformDisplay::resized()
{
    // Checkbox sitzt rechts in der Titelzeile.
    const int toggleW = juce::jmin (getWidth() - 24, 160);
    layeringToggle.setBounds (getWidth() - toggleW - 8, 5, toggleW, 18);

    // Amount-Regler nur, wenn das Panel genug Platz hat (sonst ueber FEATURES).
    const bool showAmount = getHeight() >= 112 && getWidth() >= 190;
    amountSlider.setVisible (showAmount);

    if (showAmount)
        amountSlider.setBounds (74, 26, juce::jmin (getWidth() - 84, 150), 20);
}

//==============================================================================
void WaveformDisplay::paintLayer (juce::Graphics& g,
                                  const juce::Rectangle<float>& area,
                                  const std::vector<float>& data,
                                  float referencePeak,
                                  juce::Colour colour,
                                  float thickness) const
{
    if (data.size() < 2 || area.getWidth() <= 1.0f || referencePeak <= 1.0e-5f)
        return;

    const int numPoints = juce::jmin ((int) data.size(),
                                      juce::jmax (2, (int) area.getWidth()));

    if (numPoints < 2)
        return;

    const float centreY = area.getCentreY();
    const float amplitude = area.getHeight() * 0.46f;

    juce::Path path;
    path.startNewSubPath (area.getX(), centreY);

    for (int i = 0; i < numPoints; ++i)
    {
        const int start = (int) ((int64_t) i * (int64_t) data.size() / numPoints);
        const int end   = (int) ((int64_t) (i + 1) * (int64_t) data.size() / numPoints);

        float peak = 0.0f;
        for (int s = start; s < juce::jmax (start + 1, end) && s < (int) data.size(); ++s)
            peak = juce::jmax (peak, std::abs (data[(size_t) s]));

        const float x = area.getX() + area.getWidth() * (float) i / (float) (numPoints - 1);
        const float norm = juce::jlimit (0.0f, 1.0f, peak / referencePeak);
        path.lineTo (x, centreY - norm * amplitude);
    }

    g.setColour (colour);
    g.strokePath (path, juce::PathStrokeType (thickness,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
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
    area.removeFromTop (20.0f);

    if (amountSlider.isVisible())
    {
        // Label links neben dem Amount-Regler.
        g.setColour (CERNLookAndFeel::textColour.withAlpha (layeringToggle.getToggleState()
                                                                ? 0.80f : 0.35f));
        g.setFont (CERNLookAndFeel::getLabelFont());
        g.drawText ("AMOUNT", juce::Rectangle<float> (area.getX(), area.getY() + 2.0f,
                                                      60.0f, 16.0f),
                    juce::Justification::centredLeft, false);

        area.removeFromTop (24.0f);
    }

    // Titel – nur wenn die Checkbox rechts nicht hineinragt (schmale Fenster).
    {
        const auto titleArea = bounds.reduced (10.0f, 6.0f).removeFromTop (16.0f);
        const auto titleFont = CERNLookAndFeel::getNameFont();

        juce::GlyphArrangement glyphs;
        glyphs.addLineOfText (titleFont, "WAVEFORM", 0.0f, 0.0f);
        const float titleWidth = glyphs.getBoundingBox (0, -1, true).getWidth();

        if (layeringToggle.getX() - 8.0f > titleArea.getX() + titleWidth)
        {
            g.setColour (CERNLookAndFeel::textColour.withAlpha (0.85f));
            g.setFont (titleFont);
            g.drawText ("WAVEFORM", titleArea, juce::Justification::topLeft, false);
        }
    }

    // Legende unten (nur wenn Layering aktiv): zeigt die errechneten Werte.
    const bool showLegend = layerInfo.enabled;
    juce::Rectangle<float> legend;

    if (showLegend)
        legend = area.removeFromBottom (juce::jmin (24.0f, area.getHeight() * 0.30f));

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

    // ---- Layer-Overlays (gleiche absolute Skala wie das Hauptsignal) ----
    if (showLegend && ! layerLowSamples.empty())
    {
        paintLayer (g, area, layerLowSamples, maxAbs, layerLowColour.withAlpha (0.95f), 1.3f);
        paintLayer (g, area, layerHighSamples, maxAbs, layerHighColour.withAlpha (0.95f), 1.3f);
    }

    // ---- Legende: automatisch errechnete Layer-Werte ----
    if (showLegend && legend.getHeight() > 8.0f)
    {
        const float lineH = legend.getHeight() * 0.5f;

        auto row1 = legend.removeFromTop (lineH);
        auto row2 = legend;

        g.setFont (CERNLookAndFeel::getLabelFont());

        g.setColour (layerLowColour);
        g.drawText ("L1 DEPTH", row1.removeFromLeft (62.0f),
                    juce::Justification::centredLeft, false);
        g.setColour (CERNLookAndFeel::textColour.withAlpha (0.80f));
        g.drawText (hzText (layerInfo.lowHz) + "  ·  " + msText (layerInfo.lowDecayMs)
                        + "  ·  " + dbText (layerInfo.lowGainDb),
                    row1, juce::Justification::centredLeft, false);

        g.setColour (layerHighColour);
        g.drawText ("L2 PRESENCE", row2.removeFromLeft (78.0f),
                    juce::Justification::centredLeft, false);
        g.setColour (CERNLookAndFeel::textColour.withAlpha (0.80f));
        g.drawText (hzText (layerInfo.highHz) + "  ·  " + msText (layerInfo.highDecayMs)
                        + "  ·  " + dbText (layerInfo.highGainDb)
                        + "  ·  D " + juce::String (juce::roundToInt (layerInfo.density * 100.0f)) + "%",
                    row2, juce::Justification::centredLeft, false);
    }
}
