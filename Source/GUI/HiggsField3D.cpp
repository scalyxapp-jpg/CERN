#include "HiggsField3D.h"
#include "GUI/Theme/CERNLookAndFeel.h"
#include "Parameters/ParameterIDs.h"

//==============================================================================
namespace
{
    /** Deterministischer Hash -> [0, 1) (für Partikel-Positionen). */
    float hash01 (int seed)
    {
        const float x = std::sin ((float) seed * 12.9898f + 78.233f) * 43758.5453f;
        return x - std::floor (x);
    }
}

//==============================================================================
HiggsField3D::HiggsField3D (juce::AudioProcessorValueTreeState& apvtsToUse)
    : apvts (apvtsToUse)
{
    setTitle ("Higgs Field");
    setDescription ("Interactive Higgs field display. Drag on the surface to set all Higgs parameters.");
    setTooltip ("Drag on the field to set the Higgs parameters. Interaction is disabled while Higgs is off.");
}

//==============================================================================
HiggsField3D::CursorDerived HiggsField3D::deriveFromCursor (float normX, float normY)
{
    normX = juce::jlimit (0.0f, 1.0f, normX);
    normY = juce::jlimit (0.0f, 1.0f, normY);

    constexpr float pi = juce::MathConstants<float>::pi;

    CursorDerived d;
    d.phi          = normX;
    d.theta        = normY * 360.0f;
    d.fieldStrength = 1.0f - normX;
    d.vev          = 0.2f + 0.75f * (1.0f - normY);
    d.selfCoupling = normY;
    d.inertia      = juce::jmax (0.0f, std::sin (pi * normX));
    d.yukawaBoost  = std::abs (std::cos (2.0f * pi * normX)) * (1.0f - 0.75f * normY);
    d.bosonHz      = 100.0f + 40.0f * normY;
    d.bosonQ       = 1.0f + 29.0f * normX;
    d.symmetry     = juce::jmax (0.0f, std::sin (pi * normY));
    d.decay        = 1.0f - 0.5f * normY;
    d.mix          = std::abs (std::sin (2.0f * pi * normX));
    return d;
}

void HiggsField3D::setParams (float phiVal, float thetaVal, float fieldStrengthVal,
                            float vevVal, float selfCouplingVal, float inertiaVal,
                            float yukawaBoostVal, float bosonHzVal, float bosonQVal,
                            float symmetryVal, float decayVal, float mixVal,
                            bool enabledVal)
{
    const float newPhi           = juce::jlimit (0.0f, 1.0f, phiVal);
    const float newTheta         = juce::jlimit (0.0f, 360.0f, thetaVal);
    const float newFieldStrength = juce::jlimit (0.0f, 1.0f, fieldStrengthVal);
    const float newVev           = juce::jlimit (0.2f, 0.95f, vevVal);
    const float newSelfCoupling  = juce::jlimit (0.0f, 1.0f, selfCouplingVal);
    const float newInertia       = juce::jlimit (0.0f, 1.0f, inertiaVal);
    const float newYukawaBoost   = juce::jlimit (0.0f, 1.0f, yukawaBoostVal);
    const float newBosonHz       = juce::jlimit (100.0f, 140.0f, bosonHzVal);
    const float newBosonQ        = juce::jlimit (1.0f, 30.0f, bosonQVal);
    const float newSymmetry      = juce::jlimit (0.0f, 1.0f, symmetryVal);
    const float newDecay         = juce::jlimit (0.0f, 1.0f, decayVal);
    const float newMix           = juce::jlimit (0.0f, 1.0f, mixVal);

    // Nur neu zeichnen, wenn sich tatsächlich etwas geändert hat (30-Hz-Timer).
    if (newPhi == phi && newTheta == theta && newFieldStrength == fieldStrength
        && newVev == vev && newSelfCoupling == selfCoupling && newInertia == inertia
        && newYukawaBoost == yukawaBoost && newBosonHz == bosonHz
        && newBosonQ == bosonQ && newSymmetry == symmetry && newDecay == decay
        && newMix == mix && enabledVal == enabled)
        return;

    phi           = newPhi;
    theta         = newTheta;
    fieldStrength = newFieldStrength;
    vev           = newVev;
    selfCoupling  = newSelfCoupling;
    inertia       = newInertia;
    yukawaBoost   = newYukawaBoost;
    bosonHz       = newBosonHz;
    bosonQ        = newBosonQ;
    symmetry      = newSymmetry;
    decay         = newDecay;
    mix           = newMix;
    enabled       = enabledVal;
    repaint();
}

//==============================================================================
juce::Rectangle<float> HiggsField3D::getBowlArea() const
{
    const float width = (float) getWidth();
    const float height = (float) getHeight();
    constexpr float legendWidth = 148.0f;
    constexpr float titleHeight = 24.0f;

    return { 8.0f, titleHeight, width - legendWidth - 16.0f, height - titleHeight - 8.0f };
}

float HiggsField3D::potentialHeight (float radiusFraction) const
{
    // mexican hat: V(f) = lambda/4 * (f² - v²)²   (f normalisiert, 0..1)
    const float f  = radiusFraction;
    const float v2 = vev * vev;
    const float f2 = f * f;

    const float raw = selfCoupling * 0.25f * (f2 - v2) * (f2 - v2);
    const float ref = selfCoupling * 0.25f * juce::jmax ((1.0f - v2) * (1.0f - v2), v2 * v2);

    return (ref > 1.0e-7f) ? (raw / ref) : 0.0f;
}

juce::Point<float> HiggsField3D::projectPoint (float radiusFraction, float azimuthRad) const
{
    const auto bowl = getBowlArea();
    const float Rmax = juce::jmin (bowl.getWidth(), bowl.getHeight()) * 0.44f;
    const float cx = bowl.getCentreX();
    const float baseCy = bowl.getCentreY() + Rmax * 0.18f;

    const float r = radiusFraction * Rmax;
    const float v = potentialHeight (radiusFraction);
    const float domeHeight = bowl.getHeight() * (0.32f + fieldStrength * 0.52f);
    const float tilt = std::sin (symmetry * juce::MathConstants<float>::pi);

    const float x = cx + r * std::cos (azimuthRad);
    const float y = baseCy
                  + r * std::sin (azimuthRad) * 0.42f
                  - v * domeHeight * (1.0f - radiusFraction * 0.35f)
                  - tilt * std::sin (azimuthRad + 0.7f) * 7.0f;

    return { x, y };
}

//==============================================================================
void HiggsField3D::paint (juce::Graphics& g)
{
    const juce::Rectangle<float> bounds (0.0f, 0.0f,
                                         (float) getWidth(), (float) getHeight());

    g.setColour (enabled ? CERNLookAndFeel::accentColour.withAlpha (0.10f)
                         : CERNLookAndFeel::panelColour);
    g.fillRect (bounds);

    g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.35f));
    g.drawRect (bounds, 1.0f);

    // Titel + Status
    auto titleArea = bounds.reduced (10.0f, 5.0f).removeFromTop (16.0f);

    g.setFont (CERNLookAndFeel::getNameFont());
    g.setColour (CERNLookAndFeel::textColour.withAlpha (0.85f));
    g.drawText ("HIGGS FIELD", titleArea.removeFromLeft (titleArea.getWidth() * 0.6f),
                juce::Justification::centredLeft, false);

    g.setColour (enabled ? CERNLookAndFeel::accentColour
                         : CERNLookAndFeel::textColour.withAlpha (0.4f));
    g.drawText (enabled ? "ENABLED" : "OFF", titleArea,
                juce::Justification::centredRight, false);

    drawBowl (g);
    drawCursor (g);
    drawParameterColumn (g);
}

//==============================================================================
void HiggsField3D::drawBowl (juce::Graphics& g)
{
    const auto bowl = getBowlArea();

    // Wet/Dry-Volumen (Mix) als Boden-Füllung
    if (enabled && mix > 0.001f)
    {
        const float Rmax = juce::jmin (bowl.getWidth(), bowl.getHeight()) * 0.44f;
        const float cx = bowl.getCentreX();
        const float cy = bowl.getCentreY() + Rmax * 0.18f;

        g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.05f + mix * 0.22f));
        g.fillEllipse (cx - vev * Rmax, cy - vev * Rmax * 0.42f,
                       vev * Rmax * 2.0f, vev * Rmax * 0.84f);
    }

    // Mexican-Hat-Wireframe (nichtlineare Ringverteilung: lambda -> Wandsteilheit)
    const int rings = 13;
    const int spokes = 24;
    const float ringShape = juce::jmap (selfCoupling, 0.0f, 1.0f, 2.1f, 0.75f);

    auto alpha = [this] (float a) { return enabled ? a : a * 0.25f; };

    for (int ring = rings; ring >= 0; --ring)
    {
        const float t = (float) ring / (float) rings;
        const float u = (ring == 0) ? 0.0f : 1.0f - std::pow (1.0f - t, ringShape);

        juce::Path path;

        for (int seg = 0; seg <= 48; ++seg)
        {
            const float angle = (float) seg / 48.0f * 2.0f * juce::MathConstants<float>::pi;
            const auto p = projectPoint (u, angle);

            if (seg == 0)
                path.startNewSubPath (p);
            else
                path.lineTo (p);
        }

        path.closeSubPath();

        const float ringAlpha = 0.10f + 0.65f * u;
        g.setColour (CERNLookAndFeel::accentColour.withAlpha (alpha (ringAlpha)));
        g.strokePath (path, juce::PathStrokeType (ring == rings ? 1.8f : 1.0f));
    }

    for (int spoke = 0; spoke < spokes; spoke += 2)
    {
        const float angle = (float) spoke / (float) spokes * 2.0f * juce::MathConstants<float>::pi;

        juce::Path path;
        for (int step = 0; step <= 16; ++step)
        {
            const float u = (float) step / 16.0f;
            const auto p = projectPoint (u, angle);

            if (step == 0)
                path.startNewSubPath (p);
            else
                path.lineTo (p);
        }

        g.setColour (CERNLookAndFeel::textColour.withAlpha (alpha (0.13f)));
        g.strokePath (path, juce::PathStrokeType (0.7f));
    }

    // VEV-Marker: gestrichelter Ring am Potentialminimum (manuell als Bogen-Segmente)
    {
        const auto bowl = getBowlArea();
        const float Rmax = juce::jmin (bowl.getWidth(), bowl.getHeight()) * 0.44f;
        const float cx = bowl.getCentreX();
        const float cy = bowl.getCentreY() + Rmax * 0.18f;
        const float r  = vev * Rmax;
        const float z  = potentialHeight (vev) * bowl.getHeight() * (0.32f + fieldStrength * 0.52f);

        constexpr float dashStep = 0.38f;
        constexpr float gapStep  = 0.14f;

        juce::Path vevRing;

        for (float a0 = 0.0f; a0 < 2.0f * juce::MathConstants<float>::pi; a0 += dashStep + gapStep)
        {
            juce::Path dash;
            dash.addArc (cx - r, cy - r * 0.42f - z, r * 2.0f, r * 0.84f,
                         a0, a0 + dashStep, false);
            vevRing.addPath (dash);
        }

        g.setColour (CERNLookAndFeel::textColour.withAlpha (alpha (0.55f)));
        g.strokePath (vevRing, juce::PathStrokeType (1.3f));
    }

    // Boson-Resonanz-Ring: Radius aus bosonHz, Strichstaerke aus bosonQ
    if (enabled && bosonQ > 1.5f)
    {
        const float bosonU = 0.18f + 0.72f * (bosonHz - 100.0f) / 40.0f;
        const float strokeW = juce::jmap (bosonQ, 1.0f, 30.0f, 1.0f, 5.0f);

        juce::Path bosonRing;
        for (int seg = 0; seg <= 48; ++seg)
        {
            const float angle = (float) seg / 48.0f * 2.0f * juce::MathConstants<float>::pi;
            const auto p = projectPoint (bosonU, angle);

            if (seg == 0)
                bosonRing.startNewSubPath (p);
            else
                bosonRing.lineTo (p);
        }
        bosonRing.closeSubPath();

        g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.85f));
        g.strokePath (bosonRing, juce::PathStrokeType (strokeW));
    }

    // Decay-Geister-Ringe (radial versetzt ausserhalb der Schale)
    {
        const float Rmax = juce::jmin (bowl.getWidth(), bowl.getHeight()) * 0.44f;
        const float cx = bowl.getCentreX();
        const float cy = bowl.getCentreY() + Rmax * 0.18f;

        for (int ghost = 1; ghost <= 3; ++ghost)
        {
            const float ringScale = (1.0f + 0.16f * (float) ghost);
            const float alphaVal = ((1.0f - decay * 0.7f) * 0.5f) / (float) ghost + 0.04f;

            g.setColour (CERNLookAndFeel::accentColour.withAlpha (alpha (alphaVal)));
            g.drawEllipse (cx - Rmax * ringScale, cy - Rmax * ringScale * 0.42f - 6.0f * (float) ghost,
                           Rmax * ringScale * 2.0f, Rmax * ringScale * 0.84f, 1.0f);
        }
    }

    // Yukawa-Partikel: Anzahl/Intensitaet aus yukawaBoost
    if (enabled)
    {
        const int count = (int) std::lround (yukawaBoost * 10.0f);

        for (int i = 0; i < count; ++i)
        {
            const float angle = hash01 (i * 31 + 5) * 2.0f * juce::MathConstants<float>::pi;
            const float u     = 0.22f + hash01 (i * 47 + 13) * 0.62f;
            const auto p      = projectPoint (u, angle);
            const float size  = 1.4f + hash01 (i * 71 + 29) * 2.0f * (0.4f + yukawaBoost * 0.8f);

            g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.16f));
            g.fillEllipse (p.x - size * 2.2f, p.y - size * 2.2f, size * 4.4f, size * 4.4f);
            g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.9f));
            g.fillEllipse (p.x - size, p.y - size, size * 2.0f, size * 2.0f);
        }
    }
}

//==============================================================================
void HiggsField3D::drawCursor (juce::Graphics& g)
{
    const float a = phi * 2.0f * juce::MathConstants<float>::pi;
    const float u = theta / 360.0f;
    const auto p  = projectPoint (u, a);

    const float halo = 6.0f + inertia * 11.0f;
    const float dim  = enabled ? 1.0f : 0.3f;

    // Fadenschlagen zum Hintergrund (Fadenkreuz)
    const auto bowl = getBowlArea();

    g.setColour (CERNLookAndFeel::textColour.withAlpha (0.14f * dim));
    g.drawLine (bowl.getX(), p.y, p.x - halo * 0.8f, p.y, 1.0f);
    g.drawLine (p.x + halo * 0.8f, p.y, bowl.getRight(), p.y, 1.0f);
    g.drawLine (p.x, bowl.getY(), p.x, p.y - halo * 0.8f, 1.0f);
    g.drawLine (p.x, p.y + halo * 0.8f, p.x, bowl.getBottom(), 1.0f);

    // Halo (Größe -> Inertia)
    g.setColour (CERNLookAndFeel::accentColour.withAlpha ((dragging ? 0.30f : 0.20f) * dim));
    g.fillEllipse (p.x - halo, p.y - halo, halo * 2.0f, halo * 2.0f);

    // Aussenring
    g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.95f * dim));
    g.drawEllipse (p.x - halo * 0.55f, p.y - halo * 0.55f, halo * 1.1f, halo * 1.1f,
                   dragging ? 2.2f : 1.5f);

    // Kern
    g.setColour (enabled ? CERNLookAndFeel::textColour
                         : CERNLookAndFeel::textColour.withAlpha (0.45f));
    g.fillEllipse (p.x - 2.6f, p.y - 2.6f, 5.2f, 5.2f);
}

//==============================================================================
void HiggsField3D::drawParameterColumn (juce::Graphics& g)
{
    const float width  = (float) getWidth();
    const float height = (float) getHeight();
    constexpr float titleHeight = 24.0f;
    constexpr float legendWidth = 148.0f;

    const juce::Rectangle<float> area (width - legendWidth, titleHeight,
                                       legendWidth - 8.0f, height - titleHeight - 4.0f);

    // Undurchsichtige Platte hinter der Legende: Die Wireframe-Ringe und die
    // Decay-Geister-Ringe reichen bei schmalen Panels bis hierher und machen
    // die Werte sonst unlesbar (900x650-Befund).
    g.setColour (CERNLookAndFeel::panelColour);
    g.fillRect (area.getX(), titleHeight, width - area.getX(), height - titleHeight);

    // Trennlinie
    g.setColour (CERNLookAndFeel::textColour.withAlpha (0.10f));
    g.drawLine (area.getX() - 6.0f, area.getY() + 2.0f,
                area.getX() - 6.0f, area.getBottom() - 2.0f, 1.0f);

    struct Row { const char* name; juce::String value; bool isCursor; };

    const Row rows[] = {
        { "PHI",      juce::String (phi, 2),                                  true  },
        { "THETA",    juce::String (theta, 1) + " deg",                       true  },
        { "STRENGTH", juce::String (fieldStrength, 2),                        false },
        { "MASSE",    juce::String (vev, 3),                                  false },
        { "KOPPLUNG", juce::String (selfCoupling, 2),                         false },
        { "INERTIA",  juce::String (inertia, 2),                              false },
        { "PRAESENZ", juce::String (yukawaBoost, 2),                          false },
        { "RESONANZ", juce::String (bosonHz, 1) + " Hz",                      false },
        { "RESO-Q",   juce::String (bosonQ, 1),                               false },
        { "SYMMETRY", juce::String (symmetry, 2),                             false },
        { "DECAY",    juce::String (decay, 2),                                false },
        { "MIX",      juce::String (mix, 2),                                  false },
        { "ENABLED",  enabled ? juce::String ("on") : juce::String ("off"),   false }
    };

    // Zeilenzahl an die Display-Hoehe anpassen: unter 12 px/Zeile waeren die
    // Texte nicht mehr lesbar (das war die Quetschung bei 900x650).
    const int visibleRows
        = juce::jlimit (0, (int) std::size (rows), (int) (area.getHeight() / 12.0f));

    if (visibleRows <= 0)
        return;

    const float rowHeight = area.getHeight() / (float) visibleRows;
    const float fontSize  = juce::jlimit (8.0f, 10.0f, rowHeight - 3.0f);

    g.setFont (juce::Font (juce::FontOptions (fontSize)));

    for (int i = 0; i < visibleRows; ++i)
    {
        const float y = area.getY() + (float) i * rowHeight;
        auto rowArea = juce::Rectangle<float> (area.getX(), y, area.getWidth(), rowHeight - 1.0f);

        if (rows[i].isCursor)
        {
            g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.14f));
            g.fillRoundedRectangle (rowArea, 2.0f);
        }

        // 1 px vertikaler Puffer: verhindert abgeschnittene Ziffern am unteren Rand.
        auto textRow = rowArea.reduced (0.0f, 0.5f);

        g.setColour (CERNLookAndFeel::textColour.withAlpha (0.55f));
        g.drawText (rows[i].name, textRow.removeFromLeft (textRow.getWidth() * 0.46f),
                    juce::Justification::centredLeft, false);

        g.setColour (rows[i].isCursor ? CERNLookAndFeel::accentColour
                                      : CERNLookAndFeel::textColour.withAlpha (0.92f));
        g.drawText (rows[i].value, textRow, juce::Justification::centredRight, false);
    }
}

//==============================================================================
void HiggsField3D::mouseDown (const juce::MouseEvent& e)
{
    if (! enabled)
        return;

    if (! getBowlArea().contains ((float) e.x, (float) e.y))
        return;

    dragging = true;
    updateCursorFromMouse (e);
}

void HiggsField3D::mouseDrag (const juce::MouseEvent& e)
{
    if (enabled && dragging)
        updateCursorFromMouse (e);
}

void HiggsField3D::mouseMove (const juce::MouseEvent& e)
{
    const bool nowHovering = enabled
                              && getBowlArea().contains ((float) e.x, (float) e.y);

    if (nowHovering != hovering)
    {
        hovering = nowHovering;
        repaint();
    }
}

void HiggsField3D::mouseExit (const juce::MouseEvent&)
{
    const bool wasActive = hovering || dragging;
    hovering = false;
    dragging  = false;

    if (wasActive)
        repaint();
}

void HiggsField3D::updateCursorFromMouse (const juce::MouseEvent& e)
{
    const auto bowl = getBowlArea();

    if (bowl.getWidth() <= 0.0f || bowl.getHeight() <= 0.0f)
        return;

    const float nx = juce::jlimit (0.0f, 1.0f, ((float) e.x - bowl.getX()) / bowl.getWidth());
    const float ny = juce::jlimit (0.0f, 1.0f, ((float) e.y - bowl.getY()) / bowl.getHeight());

    const auto derived = deriveFromCursor (nx, ny);

    phi           = derived.phi;
    theta         = derived.theta;
    fieldStrength = derived.fieldStrength;
    vev           = derived.vev;
    selfCoupling  = derived.selfCoupling;
    inertia       = derived.inertia;
    yukawaBoost   = derived.yukawaBoost;
    bosonHz       = derived.bosonHz;
    bosonQ        = derived.bosonQ;
    symmetry      = derived.symmetry;
    decay         = derived.decay;
    mix           = derived.mix;

    writeDerivedToApvts (derived);
    repaint();
}

void HiggsField3D::writeDerivedToApvts (const CursorDerived& derived)
{
    const auto changed = [this] (float newValue, float cachedValue)
    {
        return ! hasWrittenDerived || std::abs (newValue - cachedValue) > 1.0e-6f;
    };

    const auto setParam = [this] (const juce::String& id, float value, bool valueChanged)
    {
        auto* parameter = apvts.getParameter (id);

        if (parameter == nullptr)
            return;

        const float normalised = juce::jlimit (0.0f, 1.0f, parameter->convertTo0to1 (value));

        // Auch bei unveränderter Cursorposition schreiben, wenn der Parameter
        // inzwischen extern (z. B. per Knob oder Automation) verstellt wurde.
        if (! valueChanged && std::abs (parameter->getValue() - normalised) <= 1.0e-6f)
            return;

        parameter->setValueNotifyingHost (normalised);
    };

    setParam (ParameterIDs::higgsCursorPhi,     derived.phi,          changed (derived.phi,          lastWritten.phi));
    setParam (ParameterIDs::higgsCursorTheta,   derived.theta,        changed (derived.theta,        lastWritten.theta));
    setParam (ParameterIDs::higgsFieldStrength, derived.fieldStrength, changed (derived.fieldStrength, lastWritten.fieldStrength));
    setParam (ParameterIDs::higgsVev,           derived.vev,          changed (derived.vev,          lastWritten.vev));
    setParam (ParameterIDs::higgsSelfCoupling,  derived.selfCoupling, changed (derived.selfCoupling, lastWritten.selfCoupling));
    setParam (ParameterIDs::higgsInertia,       derived.inertia,      changed (derived.inertia,      lastWritten.inertia));
    setParam (ParameterIDs::higgsYukawaBoost,   derived.yukawaBoost,  changed (derived.yukawaBoost,  lastWritten.yukawaBoost));
    setParam (ParameterIDs::higgsBosonHz,       derived.bosonHz,      changed (derived.bosonHz,      lastWritten.bosonHz));
    setParam (ParameterIDs::higgsBosonQ,        derived.bosonQ,       changed (derived.bosonQ,       lastWritten.bosonQ));
    setParam (ParameterIDs::higgsSymmetry,      derived.symmetry,     changed (derived.symmetry,     lastWritten.symmetry));
    setParam (ParameterIDs::higgsDecay,         derived.decay,        changed (derived.decay,        lastWritten.decay));
    setParam (ParameterIDs::higgsMix,           derived.mix,          changed (derived.mix,          lastWritten.mix));

    lastWritten = derived;
    hasWrittenDerived = true;
}
