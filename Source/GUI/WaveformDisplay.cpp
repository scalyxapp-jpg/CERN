#include "WaveformDisplay.h"
#include "GUI/Theme/CERNLookAndFeel.h"

namespace
{
    const juce::Colour layerLowColour  { 0xff8fd4ff }; // kuehles Weiss-Blau = Tiefe
    const juce::Colour layerHighColour { 0xffffc36b }; // warmes Amber      = Praesenz
    const juce::Colour moduleMembraneColour { 0xffe8e8e8 };
    const juce::Colour moduleBodyColour     { 0xffb07cff }; // Violett = Shell-Band
    const juce::Colour moduleSubColour      { 0xff8fd4ff }; // Cyan   = Sub
    const juce::Colour moduleClickColour    { 0xffffc36b }; // Amber  = Click

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

    moduleToggle.setClickingTogglesState (true);
    moduleToggle.setTooltip ("Show the Membrane/Body/Sub/Click contribution curves");
    moduleToggle.setTitle ("Module overlay");
    moduleToggle.setDescription ("Show the membrane, body, sub and click contributions");
    moduleToggle.onClick = [this] { repaint(); };
    addAndMakeVisible (moduleToggle);

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

void WaveformDisplay::setModuleSamples (const std::vector<float>& membrane,
                                        const std::vector<float>& body,
                                        const std::vector<float>& sub,
                                        const std::vector<float>& click)
{
    moduleMembraneSamples = membrane;
    moduleBodySamples = body;
    moduleSubSamples = sub;
    moduleClickSamples = click;

    for (auto& sample : moduleMembraneSamples)
        if (! std::isfinite (sample))
            sample = 0.0f;

    for (auto& sample : moduleBodySamples)
        if (! std::isfinite (sample))
            sample = 0.0f;

    for (auto& sample : moduleSubSamples)
        if (! std::isfinite (sample))
            sample = 0.0f;

    for (auto& sample : moduleClickSamples)
        if (! std::isfinite (sample))
            sample = 0.0f;
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
    // Checkbox sitzt rechts in der Titelzeile; der MOD-Button links daneben.
    const int toggleW = juce::jmin (getWidth() - 24, 215);
    layeringToggle.setBounds (getWidth() - toggleW - 8, 5, toggleW, 18);

    const bool showModuleToggle = getWidth() >= 285;
    moduleToggle.setVisible (showModuleToggle);

    if (showModuleToggle)
        moduleToggle.setBounds (layeringToggle.getX() - 50, 5, 46, 18);

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

        const float reservedRight = moduleToggle.isVisible() ? (float) moduleToggle.getX()
                                                               : (float) layeringToggle.getX();

        if (reservedRight - 8.0f > titleArea.getX() + titleWidth)
        {
            g.setColour (CERNLookAndFeel::textColour.withAlpha (0.85f));
            g.setFont (titleFont);
            g.drawText ("WAVEFORM", titleArea, juce::Justification::topLeft, false);
        }
    }

    // Legende unten (nur wenn Layering aktiv): zeigt die errechneten Werte.
    const bool showLegend = layerInfo.enabled;
    const bool showModules = moduleToggle.getToggleState() && ! moduleMembraneSamples.empty();

    juce::Rectangle<float> moduleLegend;

    if (showModules && area.getHeight() > 60.0f)
        moduleLegend = area.removeFromBottom (16.0f);

    juce::Rectangle<float> legend;

    if (showLegend)
        legend = area.removeFromBottom (juce::jmin (34.0f, area.getHeight() * 0.34f));

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

    // ---- Modul-Overlay: einzelne Beitragskurven (gleiche Skala) ----
    if (showModules && ! moduleMembraneSamples.empty())
    {
        paintLayer (g, area, moduleMembraneSamples, maxAbs, moduleMembraneColour.withAlpha (0.85f), 1.2f);
        paintLayer (g, area, moduleBodySamples,     maxAbs, moduleBodyColour.withAlpha (0.90f), 1.2f);
        paintLayer (g, area, moduleSubSamples,      maxAbs, moduleSubColour.withAlpha (0.90f), 1.2f);
        paintLayer (g, area, moduleClickSamples,    maxAbs, moduleClickColour.withAlpha (0.90f), 1.2f);
    }

    // ---- Layer-Overlays (gleiche absolute Skala wie das Hauptsignal) ----
    if (showLegend && ! layerLowSamples.empty())
    {
        paintLayer (g, area, layerLowSamples, maxAbs, layerLowColour.withAlpha (0.95f), 1.3f);
        paintLayer (g, area, layerHighSamples, maxAbs, layerHighColour.withAlpha (0.95f), 1.3f);
    }

    // ---- Legende: Modulfarben (nur wenn das Overlay aktiv ist) ----
    if (showModules && moduleLegend.getHeight() > 8.0f)
    {
        const float cellW = moduleLegend.getWidth() * 0.25f;
        const juce::String labels[] = { "MEM", "BODY", "SUB", "CLICK" };
        const juce::Colour colours[] = { moduleMembraneColour, moduleBodyColour,
                                         moduleSubColour, moduleClickColour };

        g.setFont (CERNLookAndFeel::getLabelFont());

        for (int i = 0; i < 4; ++i)
        {
            auto cell = moduleLegend.removeFromLeft (cellW);
            g.setColour (colours[(size_t) i]);
            g.drawText (labels[(size_t) i], cell, juce::Justification::centredLeft, false);
        }
    }

    // ---- Legende: automatisch errechnete Layer-Werte ----
    if (showLegend && legend.getHeight() > 8.0f)
    {
        const auto legendFont = CERNLookAndFeel::getLabelFont();
        g.setFont (legendFont);

        const juce::String densityText = juce::String (juce::roundToInt (layerInfo.density * 100.0f)) + "%";
        const bool showAmount = std::abs (layerInfo.amountPct - 100.0f) >= 0.5f;

        const juce::String lowText  = hzText (layerInfo.lowHz) + " · " + msText (layerInfo.lowDecayMs)
                                    + " · " + dbText (layerInfo.lowGainDb);
        const juce::String highText = hzText (layerInfo.highHz) + " · " + msText (layerInfo.highDecayMs)
                                    + " · " + dbText (layerInfo.highGainDb);

        // Drei Zeilen, wenn Platz ist (Dichte + Amount), sonst zwei Zeilen mit
        // der Dichte am Ende der L2-Zeile.
        const bool threeRows = legend.getHeight() >= 30.0f && legend.getWidth() >= 170.0f;
        const float lineH = legend.getHeight() / (threeRows ? 3.0f : 2.0f);

        auto drawRow = [&] (juce::Rectangle<float> row, juce::Colour colour,
                            const juce::String& label, float labelWidth,
                            const juce::String& value)
        {
            g.setColour (colour);
            g.drawText (label, row.removeFromLeft (labelWidth),
                        juce::Justification::centredLeft, false);
            g.setColour (CERNLookAndFeel::textColour.withAlpha (0.80f));
            g.drawText (value, row, juce::Justification::centredLeft, true);
        };

        auto row1 = legend.removeFromTop (lineH);
        auto row2 = legend.removeFromTop (lineH);

        // Bei schmalem Panel kurze Labels, damit die Messwerte vollstaendig passen.
        const bool narrowLabels = getWidth() < 250;
        const float labelWidth = narrowLabels ? 52.0f : 72.0f;

        drawRow (row1, layerLowColour,  "L1 DEPTH", labelWidth, lowText);
        drawRow (row2, layerHighColour, narrowLabels ? "L2 PRES" : "L2 PRESENCE", labelWidth,
                 threeRows ? highText : highText + " · D " + densityText);

        if (threeRows)
        {
            // Wichtig: das Nicht-ASCII-Literal ("·") immer RECHTS von einem
            // juce::String verketten. operator+ (const char*, const String&)
            // dekodiert als ASCII/Latin-1, operator+ (String, const char*)
            // dagegen als UTF-8 (sonst erscheint "Â·").
            juce::String extra = juce::String ("D ") + densityText;

            if (showAmount)
                extra = extra + " · AMOUNT "
                              + juce::String (juce::roundToInt (layerInfo.amountPct)) + " %";

            g.setColour (CERNLookAndFeel::textColour.withAlpha (0.55f));
            g.drawText (extra, legend.removeFromTop (lineH),
                        juce::Justification::centredLeft, false);
        }
        else if (showAmount)
        {
            const juce::String amountText = "x " + juce::String (layerInfo.amountPct / 100.0f, 2);
            g.setColour (CERNLookAndFeel::textColour.withAlpha (0.55f));
            g.drawText (amountText,
                        juce::Rectangle<float> (row1.getX(), row1.getY(),
                                                row1.getWidth(), row1.getHeight()),
                        juce::Justification::centredRight, false);
        }
    }
}
