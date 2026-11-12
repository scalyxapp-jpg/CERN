#include "EnvelopeEditor.h"
#include "GUI/Theme/CERNLookAndFeel.h"

#include <cmath>

//==============================================================================
namespace
{
    constexpr int curvePoints = 96;
    constexpr float decayToFortyDb = 4.605f; // exp(-4.605) = 1 % Restpegel (DSP)
    constexpr float maxDepth = 2.0f;         // Depth 2.0 = +24 Halbtoene
}

EnvelopeEditor::EnvelopeEditor()
{
    setTitle ("Pitch Envelope");
    setDescription ("Attack, exponential decay, depth and curve of the pitch envelope");
    rebuildInfoText();
}

float EnvelopeEditor::attackShape (float phase, float curve) noexcept
{
    const float u = juce::jlimit (0.0f, 1.0f, phase);

    if (curve < 0.0f)
        return 1.0f - std::pow (1.0f - u, 1.0f + 2.0f * -curve);

    if (curve > 0.0f)
        return std::pow (u, 1.0f + 2.0f * curve);

    return u;
}

void EnvelopeEditor::rebuildInfoText()
{
    // Depth 0..2 entspricht 0..+24 Halbtoenen (exp2-Kennlinie der DSP).
    infoFull = "A " + juce::String (attackMs, 1) + " ms  |  D " + juce::String (decayMs, 0)
             + " ms  |  DEPTH +" + juce::String (depth * 12.0f, 1) + " st"
             + "  |  CURVE " + juce::String (curve, 2);

    infoMedium = "A " + juce::String (attackMs, 1) + " ms  |  D " + juce::String (decayMs, 0)
               + " ms  |  +" + juce::String (depth * 12.0f, 1) + " st";

    infoShort = "A " + juce::String (attackMs, 1)
              + " / D " + juce::String (decayMs, 0) + " ms";
}

void EnvelopeEditor::setParameters (float attackMsIn, float decayMsIn,
                                    float depthIn, float curveIn)
{
    const auto sanitise = [] (float value, float minimum, float maximum)
    {
        return std::isfinite (value) ? juce::jlimit (minimum, maximum, value) : minimum;
    };

    const float newAttack = sanitise (attackMsIn, 0.0f, 1000.0f);
    const float newDecay  = sanitise (decayMsIn, 1.0f, 10000.0f);
    const float newDepth  = sanitise (depthIn, 0.0f, maxDepth);
    const float newCurve  = sanitise (curveIn, -1.0f, 1.0f);

    if (newAttack == attackMs && newDecay == decayMs
        && newDepth == depth && newCurve == curve)
        return;

    attackMs = newAttack;
    decayMs  = newDecay;
    depth    = newDepth;
    curve    = newCurve;

    rebuildInfoText(); // Text einmalig hier, nicht in paint()
    repaint();
}

void EnvelopeEditor::paint (juce::Graphics& g)
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
    const float infoLineH = 18.0f;
    area.removeFromBottom (infoLineH); // Platz fuer die Info-Zeile (nicht am unteren Rand abschneiden)

    // Titel
    g.setColour (CERNLookAndFeel::textColour.withAlpha (0.85f));
    g.setFont (CERNLookAndFeel::getNameFont());
    g.drawText ("PITCH ENVELOPE", bounds.reduced (10.0f, 6.0f).removeFromTop (16.0f),
                juce::Justification::topLeft, false);

    if (area.getWidth() <= 1.0f || area.getHeight() <= 1.0f)
        return;

    // Gitter
    g.setColour (CERNLookAndFeel::textColour.withAlpha (0.07f));
    for (int i = 1; i < 4; ++i)
    {
        const float x = area.getX() + area.getWidth() * (float) i / 4.0f;
        g.drawVerticalLine ((int) x, area.getY(), area.getBottom());
    }

    // Zeitachse: Attack + Decay in ms (linear, wie die DSP-Position).
    const float totalMs = juce::jmax (1.0f, attackMs + decayMs);
    const float bottom  = area.getBottom();
    const float depthGain = juce::jlimit (0.0f, 1.0f, depth / maxDepth);

    // Huellkurve segmentweise zeichnen (keine Allokation im paint()).
    float prevX = area.getX();
    float prevY = bottom;

    const juce::Colour fillColour = CERNLookAndFeel::accentColour.withAlpha (0.18f);
    const juce::Colour lineColour = CERNLookAndFeel::accentColour;

    for (int i = 1; i <= curvePoints; ++i)
    {
        const float phase = (float) i / (float) curvePoints;
        const float timeMs = phase * totalMs;

        float env;

        if (timeMs < attackMs)
        {
            const float attackPhase = attackMs > 0.0001f ? timeMs / attackMs : 1.0f;
            env = attackShape (attackPhase, curve);
        }
        else
        {
            // Exponentieller Abfall; die Curve skaliert die Abfallrate (0.5..2).
            const float u = juce::jlimit (0.0f, 1.0f, (timeMs - attackMs) / decayMs);
            env = std::exp (-decayToFortyDb * u * std::pow (2.0f, curve));
        }

        const float x = area.getX() + area.getWidth() * phase;
        const float y = bottom - area.getHeight() * juce::jlimit (0.0f, 1.0f, depthGain * env);

        g.setColour (fillColour);
        g.fillRect (juce::Rectangle<float> (prevX, juce::jmin (prevY, y),
                                            juce::jmax (1.0f, x - prevX),
                                            bottom - juce::jmin (prevY, y)));

        g.setColour (lineColour);
        g.drawLine (prevX, prevY, x, y, 2.0f);

        prevX = x;
        prevY = y;
    }

    // Peak-Marker am Ende des Attacks
    const float peakX = area.getX() + area.getWidth() * juce::jlimit (0.0f, 1.0f,
                                                                     attackMs / totalMs);
    const float peakY = bottom - area.getHeight() * depthGain;

    g.setColour (CERNLookAndFeel::textColour);
    g.fillEllipse (peakX - 3.0f, peakY - 3.0f, 6.0f, 6.0f);

    // Info-Zeile: die breiteste Variante, die in die Breite passt.
    g.setColour (CERNLookAndFeel::textColour.withAlpha (0.65f));
    const juce::Font infoFont (CERNLookAndFeel::getValueFont());
    g.setFont (infoFont);

    const float avail = bounds.getWidth() - 20.0f;
    const juce::String& shown =
        juce::GlyphArrangement::getStringWidth (infoFont, infoFull) <= avail ? infoFull
      : (juce::GlyphArrangement::getStringWidth (infoFont, infoMedium) <= avail ? infoMedium
                                                                                : infoShort);

    g.drawText (shown, bounds.reduced (10.0f, 4.0f).removeFromBottom (16.0f),
                juce::Justification::bottomLeft, true);
}
