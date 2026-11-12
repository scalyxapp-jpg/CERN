#include "LogoComponent.h"
#include "GUI/Theme/CERNLookAndFeel.h"

#include <BinaryData.h>

#include <limits>

namespace
{
    constexpr const char* letters[] = { "C", ".", "E", ".", "R", ".", "N" };
    constexpr int numGlyphs = (int) (sizeof (letters) / sizeof (letters[0]));
    constexpr float tracking = 2.0f;
}

//==============================================================================
LogoComponent::LogoComponent()
{
    setTitle ("C.E.R.N");
    setDescription ("C.E.R.N logo");
    setInterceptsMouseClicks (false, false);

    int size = 0;

    if (auto* data = BinaryData::getNamedResource ("logoemblem_png", size))
        emblem = juce::ImageFileFormat::loadFrom (data, (size_t) size);
}

//==============================================================================
int LogoComponent::preferredWidthForHeight (int height)
{
    // Emblem quadratisch + Wortmarke (ca. 3.1x Hoehe) + Abstaende.
    return juce::roundToInt ((float) height * 1.02f + 10.0f + (float) height * 2.55f);
}

//==============================================================================
void LogoComponent::paint (juce::Graphics& g)
{
    auto area = getLocalBounds().toFloat().reduced (0.0f, 2.0f);

    if (area.isEmpty())
        return;

    // ---- Emblem ----
    const float emblemSize = juce::jmin (area.getHeight(), area.getWidth() * 0.34f);
    const auto emblemArea = juce::Rectangle<float> (emblemSize, emblemSize)
                                .withCentre ({ area.getX() + emblemSize * 0.5f,
                                               area.getCentreY() });

    if (emblem.isValid())
    {
        g.drawImage (emblem, emblemArea, juce::RectanglePlacement::centred, false);
    }
    else
    {
        // Fallback: prozeduraler Ring, falls das Asset fehlt.
        g.setColour (CERNLookAndFeel::textColour);
        g.drawEllipse (emblemArea.reduced (6.0f), 2.0f);
        g.setColour (CERNLookAndFeel::accentColour);
        g.fillEllipse (emblemArea.withSizeKeepingCentre (emblemSize * 0.24f, emblemSize * 0.24f));
    }

    // ---- Wortmarke (Vektor-Typografie) ----
    const float markX = emblemArea.getRight() + 9.0f;
    const float markW = area.getRight() - markX;

    if (markW < 20.0f)
        return;

    const float fontSize = juce::jlimit (12.0f, 30.0f, area.getHeight() * 0.62f);
    const juce::Font wordFont (juce::FontOptions (fontSize, juce::Font::bold));
    const juce::Font dotFont (juce::FontOptions (fontSize, juce::Font::bold));

    // Breiten einzeln messen: dadurch sitzen die roten Punkte exakt.
    std::array<float, numGlyphs> widths {};
    float totalWidth = 0.0f;

    for (int i = 0; i < numGlyphs; ++i)
    {
        juce::TextLayout layout;
        juce::AttributedString attributed;
        attributed.append (juce::String (letters[i]),
                           (i == 0 || i == 2 || i == 4 || i == 6) ? wordFont : dotFont);
        layout.createLayout (attributed, std::numeric_limits<float>::max());
        widths[(size_t) i] = layout.getWidth();
        totalWidth += widths[(size_t) i] + tracking;
    }

    totalWidth -= tracking;

    const float scale = totalWidth > markW ? markW / totalWidth : 1.0f;
    const float glyphHeight = fontSize * 1.35f;
    const float markY = area.getCentreY() - glyphHeight * 0.5f;

    float x = markX;

    for (int i = 0; i < numGlyphs; ++i)
    {
        const float w = widths[(size_t) i] * scale;
        const bool isLetter = (i == 0 || i == 2 || i == 4 || i == 6);
        const auto cell = juce::Rectangle<float> (x, markY, juce::jmax (1.0f, w), glyphHeight);

        if (isLetter)
        {
            juce::TextLayout layout;
            juce::AttributedString attributed;
            attributed.append (juce::String (letters[i]), wordFont,
                               CERNLookAndFeel::textColour);
            layout.createLayout (attributed, std::numeric_limits<float>::max());
            layout.draw (g, cell);
        }
        else
        {
            // Punkt als rotes Quadrat (Logo-Marke), exakt auf der Grundlinie.
            const float dotSize = juce::jmax (2.5f, fontSize * 0.14f);
            const float dotY = cell.getY() + glyphHeight * 0.63f;
            g.setColour (CERNLookAndFeel::accentColour);
            g.fillRect (juce::Rectangle<float> (cell.getX() + (cell.getWidth() - dotSize) * 0.5f,
                                                dotY, dotSize, dotSize));
        }

        x += w + tracking * scale;
    }
}
