#include "CERNLookAndFeel.h"

#include <BinaryData.h>

//==============================================================================
const juce::Colour CERNLookAndFeel::backgroundColour  = juce::Colour (0xff08080a);
const juce::Colour CERNLookAndFeel::panelColour       = juce::Colour (0xff111216);
const juce::Colour CERNLookAndFeel::panelRaisedColour = juce::Colour (0xff1b1d22);
const juce::Colour CERNLookAndFeel::accentColour      = juce::Colour (0xffe10600);
const juce::Colour CERNLookAndFeel::accentBrightColour = juce::Colour (0xffff3b3b);
const juce::Colour CERNLookAndFeel::textColour        = juce::Colour (0xfff4f4f6);
const juce::Colour CERNLookAndFeel::knobColour        = juce::Colour (0xff181a1f);
const juce::Colour CERNLookAndFeel::knobActiveColour  = juce::Colour (0xff23262d);
const juce::Colour CERNLookAndFeel::edgeColour        = juce::Colour (0xff3a3d45);
const juce::Colour CERNLookAndFeel::signalColour      = juce::Colour (0xff2ecc71);

namespace
{
    juce::Image loadEmbedded (const char* data, int size)
    {
        return juce::ImageFileFormat::loadFrom (data, (size_t) size);
    }

    juce::Image loadAsset (const char* name)
    {
        int size = 0;

        if (auto* data = BinaryData::getNamedResource (name, size))
            return loadEmbedded (data, size);

        return {};
    }
}

const juce::Image& CERNLookAndFeel::metalTexture()
{
    static const juce::Image image = loadAsset ("metalbrushed_png");
    return image;
}

const juce::Image& CERNLookAndFeel::knobCapTexture()
{
    static const juce::Image image = loadAsset ("knobcap_png");
    return image;
}

const juce::Image& CERNLookAndFeel::faderTrackTexture()
{
    static const juce::Image image = loadAsset ("fadertrack_png");
    return image;
}

const juce::Image& CERNLookAndFeel::grainTexture()
{
    static const juce::Image image = loadAsset ("grain_png");
    return image;
}

//==============================================================================
CERNLookAndFeel::CERNLookAndFeel()
{
    setColour (juce::ResizableWindow::backgroundColourId, backgroundColour);
    setColour (juce::DocumentWindow::backgroundColourId, backgroundColour);

    setColour (juce::Label::textColourId, textColour);
    setColour (juce::Label::backgroundColourId, juce::Colour (0x00000000));

    setColour (juce::Slider::textBoxTextColourId, textColour);
    setColour (juce::Slider::textBoxOutlineColourId, juce::Colour (0x00000000));
    setColour (juce::Slider::textBoxBackgroundColourId, juce::Colour (0x33000000));
    setColour (juce::Slider::thumbColourId, accentColour);
    setColour (juce::Slider::trackColourId, accentColour);
    setColour (juce::Slider::backgroundColourId, knobColour);

    setColour (juce::ToggleButton::textColourId, textColour);
    setColour (juce::ToggleButton::tickColourId, accentColour);

    setColour (juce::ComboBox::textColourId, textColour);
    setColour (juce::ComboBox::backgroundColourId, panelRaisedColour);
    setColour (juce::ComboBox::outlineColourId, edgeColour);
    setColour (juce::ComboBox::arrowColourId, accentBrightColour);
    setColour (juce::ComboBox::focusedOutlineColourId, accentColour);

    setColour (juce::PopupMenu::backgroundColourId, panelRaisedColour);
    setColour (juce::PopupMenu::textColourId, textColour);
    setColour (juce::PopupMenu::headerTextColourId, accentBrightColour);
    setColour (juce::PopupMenu::highlightedBackgroundColourId, accentColour.withAlpha (0.85f));
    setColour (juce::PopupMenu::highlightedTextColourId, textColour);

    setColour (juce::TextButton::buttonColourId, panelRaisedColour);
    setColour (juce::TextButton::buttonOnColourId, accentColour);
    setColour (juce::TextButton::textColourOffId, textColour);
    setColour (juce::TextButton::textColourOnId, textColour);

    setColour (juce::ScrollBar::backgroundColourId, juce::Colour (0x22000000));
    setColour (juce::ScrollBar::thumbColourId, edgeColour);
    setColour (juce::ScrollBar::trackColourId, panelColour);
}

//==============================================================================
juce::Font CERNLookAndFeel::getSectionTitleFont()
{
    return juce::Font (juce::FontOptions (16.0f, juce::Font::bold));
}

juce::Font CERNLookAndFeel::getNameFont()
{
    return juce::Font (juce::FontOptions (14.0f, juce::Font::bold));
}

juce::Font CERNLookAndFeel::getLabelFont()
{
    return juce::Font (juce::FontOptions (14.0f));
}

juce::Font CERNLookAndFeel::getValueFont()
{
    return juce::Font (juce::FontOptions (13.0f, juce::Font::bold));
}

juce::Font CERNLookAndFeel::getDisplayFont()
{
    return juce::Font (juce::FontOptions (22.0f, juce::Font::bold));
}

//==============================================================================
void CERNLookAndFeel::drawBackdrop (juce::Graphics& g, juce::Rectangle<float> bounds)
{
    juce::ColourGradient bg (juce::Colour (0xff040405), bounds.getX(), bounds.getY(),
                             juce::Colour (0xff101116), bounds.getX(), bounds.getBottom(), false);
    bg.addColour (0.45, juce::Colour (0xff0b0c0f));
    g.setGradientFill (bg);
    g.fillRect (bounds);

    // Sehr subtiler Metall-Schimmer: gibt der grossen Flaeche Tiefe, ohne
    // die Lesbarkeit der Controls zu beeintraechtigen.
    const juce::Image& metal = metalTexture();

    if (metal.isValid())
    {
        juce::Graphics::ScopedSaveState state (g);
        g.setOpacity (0.05f);
        g.drawImage (metal, bounds, juce::RectanglePlacement::fillDestination, false);
    }

    // Filmkorn: sehr subtiles Rauschen gegen Banding auf grossen Flaechen.
    const juce::Image& grain = grainTexture();

    if (grain.isValid())
    {
        juce::Graphics::ScopedSaveState state (g);
        g.setOpacity (0.05f);
        const int tile = grain.getWidth(); // kachelt in nativer Groesse: unscharfes Upscale vermeiden

        for (int gy = (int) bounds.getY(); gy < (int) bounds.getBottom(); gy += tile)
            for (int gx = (int) bounds.getX(); gx < (int) bounds.getRight(); gx += tile)
                g.drawImage (grain, juce::Rectangle<float> ((float) gx, (float) gy,
                                                            (float) tile, (float) tile),
                             juce::RectanglePlacement::fillDestination, false);
    }

    // Vignette: Mitte frei, Raender abgedunkelt (Licht von oben links).
    juce::ColourGradient vignette (juce::Colour (0x00000000), bounds.getCentreX(), bounds.getCentreY(),
                                   juce::Colour (0x59000000), bounds.getX(), bounds.getY(), true);
    g.setGradientFill (vignette);
    g.fillRect (bounds);
}

//==============================================================================
void CERNLookAndFeel::drawBevel (juce::Graphics& g, juce::Rectangle<float> bounds,
                                 float cornerSize, bool raised, float textureAlpha)
{
    if (bounds.isEmpty())
        return;

    const float r = juce::jlimit (0.0f, juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f,
                                  cornerSize);

    // Schlagschatten
    if (raised)
    {
        g.setColour (juce::Colours::black.withAlpha (0.55f));
        g.fillRoundedRectangle (bounds.translated (0.0f, 2.0f), r);
    }

    juce::Colour top    = raised ? juce::Colour (0xff24272e) : juce::Colour (0xff0a0b0d);
    juce::Colour bottom = raised ? juce::Colour (0xff121418) : juce::Colour (0xff171a1f);

    juce::ColourGradient base (top, bounds.getX(), bounds.getY(),
                               bottom, bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (base);
    g.fillRoundedRectangle (bounds, r);

    // Textur-Layer (gebuerstetes Metall) – additiv, sehr dezent.
    const juce::Image& metal = metalTexture();

    if (metal.isValid() && textureAlpha > 0.0f)
    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (bounds, r);
        g.reduceClipRegion (clip);

        g.setOpacity (textureAlpha);
        g.drawImage (metal, bounds, juce::RectanglePlacement::fillDestination, false);

        // Leichter Verlauf zurueck, damit die Textur nicht flach wirkt.
        juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.05f), bounds.getX(), bounds.getY(),
                                    juce::Colours::transparentBlack, bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (sheen);
        g.fillRect (bounds);
    }

    // Kanten: oben Licht, unten Schatten, aussen dunkle Fuge.
    g.setColour (juce::Colours::white.withAlpha (raised ? 0.10f : 0.04f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), r, 1.0f);

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawRoundedRectangle (bounds.reduced (1.5f), juce::jmax (0.0f, r - 1.0f), 1.0f);

    g.setColour (edgeColour.withAlpha (0.55f));
    g.drawRoundedRectangle (bounds, r, 1.0f);
}

//==============================================================================
void CERNLookAndFeel::drawWell (juce::Graphics& g, juce::Rectangle<float> bounds, float cornerSize)
{
    if (bounds.isEmpty())
        return;

    const float r = juce::jlimit (0.0f, juce::jmin (bounds.getWidth(), bounds.getHeight()) * 0.5f,
                                  cornerSize);

    juce::ColourGradient base (juce::Colour (0xff060708), bounds.getX(), bounds.getY(),
                               juce::Colour (0xff101216), bounds.getX(), bounds.getBottom(), false);
    g.setGradientFill (base);
    g.fillRoundedRectangle (bounds, r);

    // Innenkante oben dunkel (Tiefe), unten ein feiner Lichtsaum.
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.drawRoundedRectangle (bounds.reduced (0.5f), r, 1.2f);

    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.drawRoundedRectangle (bounds.reduced (1.5f), juce::jmax (0.0f, r - 1.0f), 0.8f);
}

//==============================================================================
void CERNLookAndFeel::drawScrew (juce::Graphics& g, juce::Point<float> centre, float radius)
{
    const auto area = juce::Rectangle<float> (radius * 2.0f, radius * 2.0f).withCentre (centre);

    juce::ColourGradient metal (juce::Colour (0xff5a5e68), area.getX(), area.getY(),
                                juce::Colour (0xff23262c), area.getRight(), area.getBottom(), false);
    g.setGradientFill (metal);
    g.fillEllipse (area);

    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawEllipse (area, 0.8f);

    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.drawLine (centre.x - radius * 0.55f, centre.y - radius * 0.2f,
                centre.x + radius * 0.4f, centre.y + radius * 0.45f, 1.0f);

    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.drawLine (centre.x - radius * 0.5f, centre.y - radius * 0.28f,
                centre.x + radius * 0.3f, centre.y + radius * 0.3f, 0.7f);
}

//==============================================================================
void CERNLookAndFeel::drawSectionPanel (juce::Graphics& g, int x, int y, int w, int h,
                                        const juce::String& title)
{
    const juce::Rectangle<float> bounds ((float) x, (float) y, (float) w, (float) h);

    if (bounds.isEmpty())
        return;

    drawBevel (g, bounds, 6.0f, true, 0.13f);

    // Titel-Leiste: leicht abgesetzter Streifen mit rotem Lichtkanten-Akzent.
    const float titleBandHeight = juce::jmin (28.0f, bounds.getHeight());
    const auto band = bounds.withHeight (titleBandHeight);

    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (bounds, 6.0f);
        g.reduceClipRegion (clip);

        juce::ColourGradient bandGradient (juce::Colour (0xff202329), bounds.getX(), band.getY(),
                                           juce::Colour (0xff14161a), bounds.getX(), band.getBottom(), false);
        g.setGradientFill (bandGradient);
        g.fillRect (band);

        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.drawHorizontalLine ((int) band.getBottom() - 1, bounds.getX(), bounds.getRight());

        // Roter Akzent: kurzer Strich links + duenne Linie unter dem Titel.
        g.setColour (accentColour);
        g.fillRect (bounds.getX() + 2.0f, band.getBottom() - 2.5f, 34.0f, 2.0f);

        g.setColour (accentColour.withAlpha (0.18f));
        g.fillRect (bounds.getX() + 36.0f, band.getBottom() - 2.0f,
                    bounds.getWidth() - 38.0f, 1.0f);
    }

    // Zier-Schrauben unten rechts (Hardware-Look).
    if (bounds.getWidth() > 90.0f && bounds.getHeight() > 60.0f)
    {
        drawScrew (g, { bounds.getRight() - 10.0f, bounds.getBottom() - 10.0f }, 3.0f);
        drawScrew (g, { bounds.getX() + 10.0f, bounds.getBottom() - 10.0f }, 3.0f);
    }

    if (title.isNotEmpty())
    {
        g.setColour (textColour);
        g.setFont (getSectionTitleFont());
        g.drawText (title.toUpperCase(), x + 12, y + 5, w - 24, 20,
                    juce::Justification::left, true);
    }
}

//==============================================================================
void CERNLookAndFeel::drawRedKnob (juce::Graphics& g, int x, int y, int w, int h,
                                   float angle, bool active)
{
    const float cx = (float) x + (float) w * 0.5f;
    const float cy = (float) y + (float) h * 0.5f;
    const float radius = juce::jmin ((float) w, (float) h) * 0.38f;

    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillEllipse (cx - radius, cy - radius + 2.0f, radius * 2.0f, radius * 2.0f);

    juce::ColourGradient cap (active ? knobActiveColour.brighter (0.25f) : knobActiveColour,
                              cx - radius, cy - radius,
                              knobColour.darker (0.4f), cx + radius, cy + radius, false);
    g.setGradientFill (cap);
    g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);

    g.setColour (accentColour.withAlpha (active ? 0.9f : 0.35f));
    g.drawEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.4f);

    const float lineLen = radius * 0.78f;
    g.setColour (accentBrightColour);
    g.drawLine (cx, cy, cx + std::cos (angle) * lineLen, cy + std::sin (angle) * lineLen,
                juce::jmax (1.5f, radius * 0.16f));
}

//==============================================================================
void CERNLookAndFeel::drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float rotaryStartAngle,
                                        float rotaryEndAngle, juce::Slider& slider)
{
    const float radius = juce::jmin ((float) width, (float) height) * 0.44f;

    if (radius < 6.0f)
        return;

    const float cx = (float) x + (float) width * 0.5f;
    const float cy = (float) y + (float) height * 0.5f;
    const float angle = rotaryStartAngle + sliderPos * (rotaryEndAngle - rotaryStartAngle);

    const double minValue = slider.getMinimum();
    const double maxValue = slider.getMaximum();
    const double range = maxValue - minValue;
    const bool hasValue = range > 0.0
                          && (slider.getValue() - minValue) / range > 0.002;
    const bool hot = slider.isMouseOverOrDragging() || slider.isMouseButtonDown();

    // ---- Schlagschatten unter dem Regler ----
    g.setColour (juce::Colours::black.withAlpha (0.55f));
    g.fillEllipse (cx - radius * 0.98f, cy - radius * 0.86f + 4.0f,
                   radius * 1.96f, radius * 1.96f);

    // ---- Aeusserer Metallkranz ----
    {
        juce::ColourGradient bezel (juce::Colour (0xff4b4f58), cx - radius, cy - radius,
                                    juce::Colour (0xff16181c), cx + radius, cy + radius, true);
        g.setGradientFill (bezel);
        g.fillEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f);
    }

    // ---- Track-Bogen (vertieft) ----
    const float arcRadius = radius * 0.94f;
    const float strokeWidth = juce::jmax (2.2f, radius * 0.115f);

    juce::Path track;
    track.addCentredArc (cx, cy, arcRadius, arcRadius, 0.0f,
                         rotaryStartAngle, rotaryEndAngle, true);
    g.setColour (juce::Colour (0xff08090b));
    g.strokePath (track, juce::PathStrokeType (strokeWidth * 1.9f,
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));
    g.setColour (juce::Colour (0xff2a2d34));
    g.strokePath (track, juce::PathStrokeType (strokeWidth,
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // ---- Wert-Bogen mit Glow ----
    if (hasValue)
    {
        juce::Path arc;
        arc.addCentredArc (cx, cy, arcRadius, arcRadius, 0.0f,
                           rotaryStartAngle, angle, true);

        g.setColour (accentColour.withAlpha (hot ? 0.30f : 0.18f));
        g.strokePath (arc, juce::PathStrokeType (strokeWidth * 2.6f,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
        g.setColour (accentColour.withAlpha (0.55f));
        g.strokePath (arc, juce::PathStrokeType (strokeWidth * 1.5f,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
        g.setColour (hot ? accentBrightColour : accentColour);
        g.strokePath (arc, juce::PathStrokeType (strokeWidth,
                                                 juce::PathStrokeType::curved,
                                                 juce::PathStrokeType::rounded));
    }

    // ---- Kappe (echte Metall-Textur, vektoriell begrenzt) ----
    const float capRadius = radius * 0.68f;
    const auto capArea = juce::Rectangle<float> (capRadius * 2.0f, capRadius * 2.0f).withCentre ({ cx, cy });
    const juce::Image& capImage = knobCapTexture();

    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addEllipse (capArea);
        g.reduceClipRegion (clip);

        if (capImage.isValid())
        {
            g.drawImage (capImage, capArea, juce::RectanglePlacement::fillDestination, false);

            // Rotationsunabhaengiger Lichtreflex von links oben.
            juce::ColourGradient sheen (juce::Colours::white.withAlpha (0.18f),
                                        capArea.getX(), capArea.getY(),
                                        juce::Colours::transparentBlack,
                                        capArea.getCentreX(), capArea.getCentreY(), true);
            g.setGradientFill (sheen);
            g.fillEllipse (capArea);
        }
        else
        {
            juce::ColourGradient fallback (knobActiveColour.brighter (0.2f), capArea.getX(), capArea.getY(),
                                           knobColour.darker (0.3f), capArea.getRight(), capArea.getBottom(), false);
            g.setGradientFill (fallback);
            g.fillEllipse (capArea);
        }

        // Unterer Schatten im Kranz -> Kappe sitzt sichtbar tiefer.
        juce::ColourGradient depth (juce::Colours::transparentBlack,
                                    capArea.getCentreX(), capArea.getCentreY(),
                                    juce::Colours::black.withAlpha (0.55f),
                                    capArea.getCentreX(), capArea.getBottom(), true);
        g.setGradientFill (depth);
        g.fillEllipse (capArea);
    }

    // Kappenrand
    g.setColour (juce::Colours::black.withAlpha (0.85f));
    g.drawEllipse (capArea.reduced (0.5f), 1.2f);
    g.setColour (juce::Colours::white.withAlpha (0.16f));
    g.drawEllipse (capArea.reduced (1.6f), 0.8f);

    // ---- Zeiger (roter Lichtstreifen mit Glow) ----
    const float pointerInner = capRadius * 0.18f;
    const float pointerOuter = capRadius * 0.92f;
    const juce::Point<float> p0 (cx + std::cos (angle) * pointerInner,
                                 cy + std::sin (angle) * pointerInner);
    const juce::Point<float> p1 (cx + std::cos (angle) * pointerOuter,
                                 cy + std::sin (angle) * pointerOuter);

    g.setColour (accentColour.withAlpha (hot ? 0.55f : 0.30f));
    g.drawLine ({ p0, p1 }, juce::jmax (3.5f, capRadius * 0.30f));
    g.setColour (hot ? accentBrightColour : accentColour);
    g.drawLine ({ p0, p1 }, juce::jmax (1.6f, capRadius * 0.11f));

    // ---- Zentrum ----
    g.setColour (juce::Colour (0xff0b0c0e));
    g.fillEllipse (juce::Rectangle<float> (capRadius * 0.30f, capRadius * 0.30f).withCentre ({ cx, cy }));
    g.setColour (juce::Colours::white.withAlpha (0.10f));
    g.drawEllipse (juce::Rectangle<float> (capRadius * 0.30f, capRadius * 0.30f).withCentre ({ cx, cy }), 0.8f);

    // ---- Hover: feiner roter Ring um den Kranz ----
    if (hot)
    {
        g.setColour (accentColour.withAlpha (0.35f));
        g.drawEllipse (cx - radius, cy - radius, radius * 2.0f, radius * 2.0f, 1.5f);
    }
}

//==============================================================================
void CERNLookAndFeel::drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                                        float sliderPos, float /*minSliderPos*/,
                                        float /*maxSliderPos*/,
                                        const juce::Slider::SliderStyle style,
                                        juce::Slider& slider)
{
    const float proportion =
        juce::jlimit (0.0f, 1.0f, (float) slider.valueToProportionOfLength (slider.getValue()));

    const bool isBar = (style == juce::Slider::LinearBar
                        || style == juce::Slider::LinearBarVertical);
    const bool isVertical = (style == juce::Slider::LinearVertical
                             || style == juce::Slider::LinearBarVertical);

    const juce::Rectangle<float> area ((float) x, (float) y, (float) width, (float) height);
    const bool hot = slider.isMouseOverOrDragging();

    if (isBar)
    {
        // Sequencer-Velocity/Pitch: schmaler Level-Pfeiler in versenktem
        // Schacht. Frueher fuellte der Balken die ganze Zellbreite -> 16
        // grosse rote Flaeche dominierten die UI ("Red Wall").
        drawWell (g, area, 3.0f);

        const float barWidth = isVertical
            ? juce::jmax (10.0f, area.getWidth() * 0.58f)
            : area.getWidth();

        juce::Rectangle<float> inner = area.reduced (1.5f);

        if (isVertical)
            inner = juce::Rectangle<float> (barWidth, inner.getHeight())
                        .withCentre ({ area.getCentreX(), inner.getCentreY() });

        juce::Rectangle<float> fill =
            isVertical ? inner.withTop (inner.getBottom() - inner.getHeight() * proportion)
                       : inner.withWidth (inner.getWidth() * proportion);

        if (fill.getHeight() < 1.0f && fill.getWidth() < 1.0f)
            return; // valeur 0: nur der leere Schacht

        juce::ColourGradient fillGradient (accentBrightColour,
                                           fill.getX(), fill.getY(),
                                           accentColour,
                                           fill.getX(), fill.getBottom(), false);
        g.setGradientFill (fillGradient);
        g.fillRoundedRectangle (fill, 2.5f);

        // Kappe: helle Oberkante nur am Fuell-Ende (Licht oben links).
        if (fill.getWidth() > 2.0f && fill.getHeight() > 3.0f)
        {
            g.setColour (juce::Colours::white.withAlpha (0.30f));
            g.fillRoundedRectangle (fill.withHeight (1.5f), 0.75f);
        }
        return;
    }

    // ---- 3D-Fader ----
    const float trackThickness = isVertical ? juce::jmin (area.getWidth() * 0.42f, 12.0f)
                                            : juce::jmin (area.getHeight() * 0.42f, 12.0f);
    const juce::Rectangle<float> track =
        isVertical ? juce::Rectangle<float> (trackThickness, area.getHeight()).withCentre (area.getCentre())
                   : juce::Rectangle<float> (area.getWidth(), trackThickness).withCentre (area.getCentre());

    // Aussenfuge + Schacht
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.fillRoundedRectangle (track.expanded (2.0f), trackThickness * 0.5f);
    drawWell (g, track, trackThickness * 0.5f);

    // Textur im Schacht: vertikal die Fader-Track-Textur, horizontal Metall.
    const juce::Image& channelTexture = isVertical ? faderTrackTexture() : metalTexture();

    if (channelTexture.isValid())
    {
        juce::Graphics::ScopedSaveState state (g);
        juce::Path clip;
        clip.addRoundedRectangle (track, trackThickness * 0.5f);
        g.reduceClipRegion (clip);
        g.setOpacity (0.35f);
        g.drawImage (channelTexture, track, juce::RectanglePlacement::fillDestination, false);
    }

    // Roter Wert-Anteil (vom Minimum zum Griff)
    const juce::Rectangle<float> fill =
        isVertical ? track.withTop (track.getY() + track.getHeight() * (1.0f - proportion))
                   : track.withWidth (track.getWidth() * proportion);

    if (! fill.isEmpty())
    {
        juce::ColourGradient glow (accentColour.withAlpha (0.85f), fill.getX(), fill.getY(),
                                   accentColour.withAlpha (0.45f), fill.getX(), fill.getBottom(), false);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (fill.reduced (isVertical ? 2.5f : 0.5f,
                                              isVertical ? 0.5f : 2.5f),
                                juce::jmax (1.0f, trackThickness * 0.30f));
    }

    // ---- Griff: breite Metallkappe senkrecht zur Fader-Achse ----
    const float handleThickness = juce::jmin (isVertical ? area.getWidth() * 0.92f
                                                         : area.getHeight() * 0.92f,
                                              30.0f);
    const float handleLength = 15.0f;

    juce::Rectangle<float> handle =
        isVertical ? juce::Rectangle<float> (handleThickness, handleLength)
                        .withCentre ({ area.getCentreX(), sliderPos })
                   : juce::Rectangle<float> (handleLength, handleThickness)
                        .withCentre ({ sliderPos, area.getCentreY() });

    handle = handle.constrainedWithin (area.expanded (0.0f, 1.0f));

    // Griff-Schatten
    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.fillRoundedRectangle (handle.translated (0.0f, 2.0f), 3.0f);

    juce::ColourGradient handleMetal (juce::Colour (0xff60656f), handle.getX(), handle.getY(),
                                      juce::Colour (0xff1a1d22), handle.getX(), handle.getBottom(), false);
    g.setGradientFill (handleMetal);
    g.fillRoundedRectangle (handle, 3.0f);

    g.setColour (juce::Colours::white.withAlpha (0.22f));
    g.fillRoundedRectangle (handle.reduced (1.5f).withHeight (1.0f), 0.5f);

    g.setColour (juce::Colours::black.withAlpha (0.7f));
    g.drawRoundedRectangle (handle, 3.0f, 1.0f);

    // Rote Griff-Markierung (Rille in der Mitte)
    g.setColour (hot ? accentBrightColour : accentColour);
    if (isVertical)
        g.fillRoundedRectangle (handle.withSizeKeepingCentre (handle.getWidth() * 0.70f, 2.0f), 1.0f);
    else
        g.fillRoundedRectangle (handle.withSizeKeepingCentre (2.0f, handle.getHeight() * 0.70f), 1.0f);

    // Glow am Griff bei Interaktion
    if (hot)
    {
        g.setColour (accentColour.withAlpha (0.22f));
        g.drawRoundedRectangle (handle.expanded (2.0f), 4.0f, 2.0f);
    }
}

//==============================================================================
juce::Label* CERNLookAndFeel::createSliderTextBox (juce::Slider& slider)
{
    auto* box = new juce::Label();
    box->setJustificationType (juce::Justification::centred);
    box->setFont (getValueFont());
    box->setColour (juce::Label::textColourId, textColour);
    box->setColour (juce::Label::backgroundColourId, juce::Colour (0xff0b0c0f));
    box->setColour (juce::Label::outlineColourId, edgeColour.withAlpha (0.6f));
    box->setInterceptsMouseClicks (false, true);
    juce::ignoreUnused (slider);
    return box;
}

//==============================================================================
void CERNLookAndFeel::drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                                        bool shouldDrawButtonAsHighlighted,
                                        bool shouldDrawButtonAsDown)
{
    const bool on = button.getToggleState();
    const bool compact = (bool) button.getProperties().getWithDefault ("compact", false);
    const bool playhead = (bool) button.getProperties().getWithDefault ("playhead", false);
    const juce::Rectangle<float> bounds (0.0f, 0.0f,
                                         (float) button.getWidth(),
                                         (float) button.getHeight());

    if (compact)
    {
        // Sequencer-Step: versenkte Taste, bei "on" rote LED-Flaeche.
        if (on)
        {
            juce::ColourGradient lit (accentBrightColour, bounds.getX(), bounds.getY(),
                                      accentColour.darker (0.3f), bounds.getX(), bounds.getBottom(), false);
            g.setGradientFill (lit);
            g.fillRoundedRectangle (bounds.reduced (1.0f), 4.0f);

            g.setColour (juce::Colours::white.withAlpha (0.25f));
            g.fillRoundedRectangle (bounds.reduced (2.0f).withHeight (2.0f), 1.0f);
        }
        else
        {
            drawBevel (g, bounds.reduced (1.0f), 4.0f, true, 0.05f);
        }

        if (shouldDrawButtonAsHighlighted)
        {
            g.setColour (juce::Colours::white.withAlpha (0.10f));
            g.fillRoundedRectangle (bounds.reduced (1.0f), 4.0f);
        }

        if (playhead)
        {
            g.setColour (accentBrightColour);
            g.drawRoundedRectangle (bounds.reduced (1.0f), 4.0f, 2.0f);
        }

        g.setColour (on ? juce::Colours::white : textColour.withAlpha (0.55f));
        g.setFont (getNameFont());
        g.drawText (button.getButtonText(), bounds, juce::Justification::centred, false);
        return;
    }

    auto area = bounds;
    const float switchWidth = juce::jmin (38.0f, area.getWidth() * 0.42f);
    auto switchArea = area.removeFromLeft (switchWidth).withSizeKeepingCentre (switchWidth, 18.0f);

    // Schalter-Gehaeuse
    drawWell (g, switchArea, 9.0f);

    if (on)
    {
        juce::ColourGradient glow (accentBrightColour.withAlpha (0.9f),
                                   switchArea.getX(), switchArea.getY(),
                                   accentColour.darker (0.2f),
                                   switchArea.getRight(), switchArea.getBottom(), false);
        g.setGradientFill (glow);
        g.fillRoundedRectangle (switchArea.reduced (1.5f), 7.5f);
    }

    g.setColour (on ? accentColour.withAlpha (0.9f) : edgeColour.withAlpha (0.7f));
    g.drawRoundedRectangle (switchArea.reduced (0.5f), 9.0f, 1.0f);

    // Metall-Knopf
    auto thumb = switchArea.withSizeKeepingCentre (14.0f, 14.0f);
    thumb.setX (on ? switchArea.getRight() - 16.0f : switchArea.getX() + 2.0f);

    juce::ColourGradient thumbMetal (juce::Colour (0xffeeeff2), thumb.getX(), thumb.getY(),
                                     juce::Colour (0xff8d9199), thumb.getRight(), thumb.getBottom(), false);
    g.setGradientFill (thumbMetal);
    g.fillEllipse (thumb);
    g.setColour (juce::Colours::black.withAlpha (0.45f));
    g.drawEllipse (thumb, 0.8f);

    area.removeFromLeft (8.0f);
    g.setColour (textColour.withAlpha (on ? 0.95f : 0.7f));
    g.setFont (getNameFont());
    g.drawText (button.getButtonText(), area, juce::Justification::centredLeft, true);
}

//==============================================================================
void CERNLookAndFeel::drawComboBox (juce::Graphics& g, int width, int height, bool /*isButtonDown*/,
                                    int /*buttonX*/, int /*buttonY*/, int /*buttonW*/, int /*buttonH*/,
                                    juce::ComboBox& box)
{
    const juce::Rectangle<float> bounds (0.0f, 0.0f, (float) width, (float) height);

    drawBevel (g, bounds.reduced (0.5f), 4.0f, true, 0.06f);

    if (box.hasKeyboardFocus (false))
    {
        g.setColour (accentColour.withAlpha (0.8f));
        g.drawRoundedRectangle (bounds.reduced (0.5f), 4.0f, 1.2f);
    }

    // Pfeil rechts
    const float arrowSize = 7.0f;
    const float ax = (float) width - 14.0f;
    const float ay = (float) height * 0.5f;

    juce::Path arrow;
    arrow.addTriangle (ax - arrowSize * 0.5f, ay - arrowSize * 0.4f,
                       ax + arrowSize * 0.5f, ay - arrowSize * 0.4f,
                       ax, ay + arrowSize * 0.55f);
    g.setColour (accentBrightColour);
    g.fillPath (arrow);
}

void CERNLookAndFeel::positionComboBoxText (juce::ComboBox& box, juce::Label& label)
{
    // Platz fuer den Pfeil freilassen, damit Text und Pfeil nicht ueberlappen.
    label.setBounds (6, 1, juce::jmax (0, box.getWidth() - 26), juce::jmax (0, box.getHeight() - 2));
    label.setFont (getNameFont());
}

//==============================================================================
void CERNLookAndFeel::drawButtonBackground (juce::Graphics& g, juce::Button& button,
                                            const juce::Colour& /*backgroundColour*/,
                                            bool shouldDrawButtonAsHighlighted,
                                            bool shouldDrawButtonAsDown)
{
    const juce::Rectangle<float> bounds (0.0f, 0.0f,
                                         (float) button.getWidth(),
                                         (float) button.getHeight());
    const bool on = button.getToggleState();

    if (on)
    {
        juce::ColourGradient lit (accentBrightColour, bounds.getX(), bounds.getY(),
                                  accentColour.darker (0.35f), bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (lit);
        g.fillRoundedRectangle (bounds.reduced (0.5f), 4.0f);

        g.setColour (juce::Colours::white.withAlpha (0.22f));
        g.fillRoundedRectangle (bounds.reduced (1.5f).withHeight (2.0f), 1.0f);
    }
    else
    {
        drawBevel (g, bounds.reduced (0.5f), 4.0f,
                   true, shouldDrawButtonAsDown ? 0.02f : 0.07f);

        if (shouldDrawButtonAsDown)
        {
            g.setColour (juce::Colours::black.withAlpha (0.35f));
            g.fillRoundedRectangle (bounds.reduced (0.5f), 4.0f);
        }
        else if (shouldDrawButtonAsHighlighted)
        {
            g.setColour (juce::Colours::white.withAlpha (0.07f));
            g.fillRoundedRectangle (bounds.reduced (0.5f), 4.0f);
        }
    }
}

//==============================================================================
CERNLookAndFeel& CERNLookAndFeel::getDefaultLookAndFeel()
{
    static CERNLookAndFeel lookAndFeel;
    return lookAndFeel;
}
