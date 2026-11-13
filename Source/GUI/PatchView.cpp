#include "PatchView.h"

#include "GUI/Theme/CERNLookAndFeel.h"

#include <cmath>

//==============================================================================
// Layout-Konstanten
#include "Parameters/ParameterIDs.h"

// Modul-Ein/Aus (ON-Schalter pro Modul-Panel, Audio-Modus):
// Der DSP-Vertrag ist pro Modultyp unterschiedlich -
//   * Quellen (Membrane/Sub/Click) und Sends (Room/Reverb): Mute, liefern 0;
//   * serielle Prozessoren (Tube/Drive/EQ/Comp): Bypass, reichen das Eingangs-
//     signal unverändert weiter;
//   * OUTPUT: Mute (Ausgang 0).
// Ein deaktiviertes Modul darf den Ausgang in keinem Fall beeinflussen; die
// Toggles sind reine Anzeige/Bedienung und ändern keine DSP-Logik.
namespace
{
    constexpr float panelPadding = 14.0f;
    constexpr float panelGap     = 10.0f;
    constexpr float topBarHeight = 44.0f;
    constexpr float jackRadius   = 9.0f;
    constexpr int   gridCols     = 4;
    constexpr int   gridRows     = 3;

    const char* moduleEnableId (ModuleIndex module)
    {
        switch (module)
        {
            case ModuleIndex::Membrane:    return ParameterIDs::membraneEnabled;
            case ModuleIndex::Tube:        return ParameterIDs::tubeEnabled;
            case ModuleIndex::Sub:         return ParameterIDs::subEnabled;
            case ModuleIndex::Click:       return ParameterIDs::clickEnabled;
            case ModuleIndex::Drive:       return ParameterIDs::driveEnabled;
            case ModuleIndex::Eq:          return ParameterIDs::eqEnabled;
            case ModuleIndex::Compressor:  return ParameterIDs::compEnabled;
            case ModuleIndex::Room:        return ParameterIDs::roomEnabled;
            case ModuleIndex::Higgs:       return ParameterIDs::higgsEnabled;
            case ModuleIndex::Reverb:      return ParameterIDs::reverbEnabled;
            case ModuleIndex::OutputModule:return ParameterIDs::outputEnabled;
            case ModuleIndex::MaterialRoom:return ParameterIDs::materialRoomEnabled;
            default:                      return nullptr;
        }
    }

    juce::Colour audioCableColour (const juce::AudioProcessorValueTreeState& apvts,
                                  Jack from, Jack to, bool selected)
    {
        if (selected)
            return juce::Colours::white;

        auto moduleDisabled = [&apvts] (ModuleIndex module) -> bool
        {
            const char* id = moduleEnableId (module);

            if (id == nullptr)
                return false;

            if (const auto* value = apvts.getRawParameterValue (id))
                return value->load() < 0.5f;

            return false;
        };

        if (moduleDisabled (jackDefs[(int) from].module) || moduleDisabled (jackDefs[(int) to].module))
            return juce::Colour (0xff5a5a5a);

        switch (jackDefs[(int) from].module)
        {
            case ModuleIndex::Membrane:    return juce::Colour (0xfff05a5a);
            case ModuleIndex::Tube:        return juce::Colour (0xffff922b);
            case ModuleIndex::Sub:         return juce::Colour (0xfff6d258);
            case ModuleIndex::Click:       return juce::Colour (0xfff06292);
            case ModuleIndex::Drive:       return juce::Colour (0xffff4d4d);
            case ModuleIndex::Eq:          return juce::Colour (0xfff59f00);
            case ModuleIndex::Compressor:  return juce::Colour (0xffc92a2a);
            case ModuleIndex::Room:        return juce::Colour (0xff20c997);
            case ModuleIndex::Higgs:       return juce::Colour (0xff9775fa);
            case ModuleIndex::Reverb:      return juce::Colour (0xff4dabf7);
            case ModuleIndex::OutputModule:return juce::Colour (0xfff2f2f2);
            case ModuleIndex::MaterialRoom:return juce::Colour (0xff0ca678);
            default:                      return CERNLookAndFeel::accentColour;
        }
    }
}

//==============================================================================
PatchView::PatchView (PatchGraph& patchGraph,
                      ParameterPatchGraph& parameterPatchGraph,
                      juce::AudioProcessorValueTreeState& apvts)
    : parameters (apvts), patch (patchGraph), parameterPatch (parameterPatchGraph)
{
    setOpaque (true);

    parameterView = std::make_unique<ParameterPatchView> (parameters, parameterPatch);
    addChildComponent (*parameterView);

    for (int m = 0; m < (int) ModuleIndex::ModuleCount; ++m)
    {
        const char* id = moduleEnableId (static_cast<ModuleIndex> (m));

        if (id == nullptr)
            continue;

        auto toggle = std::make_unique<PowerSwitch> ("Power");
        toggle->setClickingTogglesState (true);
        toggle->setTooltip (juce::String (moduleDefs[(size_t) m].displayName)
                            + " ein/aus - Parameter: " + id);
        addAndMakeVisible (*toggle);

        modulePowerToggles[(size_t) m] = std::move (toggle);
        modulePowerAttachments[(size_t) m] =
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                parameters, id, *modulePowerToggles[(size_t) m]);
    }

    resetButton.onClick = [this]
    {
        if (mode != ViewMode::Audio)
            return;

        patch.applyDefaultPatch();
        selectedCable = -1;
        repaint();
    };
    resetButton.setButtonText ("DEFAULT PATCH");
    addAndMakeVisible (resetButton);

    clearButton.onClick = [this]
    {
        if (mode == ViewMode::Audio)
            patch.clearAll();
        else
            parameterPatch.clearAll();

        selectedCable = -1;
        repaint();
    };
    clearButton.setButtonText ("CLEAR ALL");
    addAndMakeVisible (clearButton);

    audioPatchButton.onClick = [this] { setMode (ViewMode::Audio); };
    parameterPatchButton.onClick = [this] { setMode (ViewMode::Parameter); };
    addAndMakeVisible (audioPatchButton);
    addAndMakeVisible (parameterPatchButton);

    startTimerHz (20);
    setMode (ViewMode::Audio);
}

PatchView::~PatchView()
{
    stopTimer();
}

//==============================================================================
void PatchView::setMode (ViewMode newMode)
{
    mode = newMode;

    if (parameterView != nullptr)
        parameterView->setVisible (mode == ViewMode::Parameter);

    resetButton.setVisible (mode == ViewMode::Audio);

    for (auto& toggle : modulePowerToggles)
        if (toggle != nullptr)
            toggle->setVisible (mode == ViewMode::Audio);

    using juce::Colour;
    audioPatchButton.setColour (juce::TextButton::buttonColourId,
                                mode == ViewMode::Audio ? CERNLookAndFeel::accentColour
                                                         : juce::Colours::transparentBlack);
    parameterPatchButton.setColour (juce::TextButton::buttonColourId,
                                    mode == ViewMode::Parameter ? CERNLookAndFeel::accentColour
                                                                 : juce::Colours::transparentBlack);

    resized();
    repaint();
}

void PatchView::resized()
{
    auto r = getLocalBounds().reduced ((int) panelPadding);
    auto bar = r.removeFromTop ((int) topBarHeight);

    audioPatchButton.setBounds (bar.getX(), bar.getY() + 4, 150, 34);
    parameterPatchButton.setBounds (bar.getX() + 154, bar.getY() + 4, 164, 34);

    clearButton.setBounds (bar.removeFromRight (140).reduced (4, 8));
    resetButton.setBounds (bar.removeFromRight (160).reduced (4, 8));

    if (parameterView != nullptr)
        parameterView->setBounds (0, (int) (panelPadding + topBarHeight), getWidth(),
                                  juce::jmax (1, getHeight() - (int) (panelPadding + topBarHeight) - 18));

    for (int m = 0; m < (int) ModuleIndex::ModuleCount; ++m)
    {
        if (modulePowerToggles[(size_t) m] == nullptr)
            continue;

        const auto bounds = moduleBounds (static_cast<ModuleIndex> (m));
        modulePowerToggles[(size_t) m]->setBounds (
            (int) bounds.getRight() - 52, (int) bounds.getY() + 5, 48, 22);
        modulePowerToggles[(size_t) m]->setVisible (mode == ViewMode::Audio);
    }
}

//==============================================================================
juce::Rectangle<float> PatchView::moduleBounds (ModuleIndex module) const
{
    const auto& def = moduleDefs[(size_t) module];
    const auto area = getLocalBounds().toFloat().reduced (panelPadding);
    const float gridY = area.getY() + topBarHeight;
    const float gridH = area.getBottom() - gridY - 18.0f; // Platz für Hint
    const float gridW = area.getWidth();

    const float cellW = (gridW - (gridCols - 1) * panelGap) / (float) gridCols;
    const float cellH = (gridH - (gridRows - 1) * panelGap) / (float) gridRows;

    const float x = area.getX() + def.column * (cellW + panelGap);
    const float y = gridY       + def.row    * (cellH + panelGap);
    return { x, y, cellW, cellH };
}

juce::Point<float> PatchView::jackCentre (Jack jack) const
{
    const auto& jd = jackDefs[(size_t) jack];
    const auto box = moduleBounds (jd.module);

    const int inCount  = jackCountFor (jd.module, true);
    const int outCount = jackCountFor (jd.module, false);

    // Position der Buchse innerhalb ihrer Richtungsgruppe (Reihenfolge jackDefs).
    int directionPos = 0;
    for (int j = 0; j < (int) jack; ++j)
    {
        const auto& other = jackDefs[(size_t) j];

        if (other.module == jd.module && other.isInput == jd.isInput)
            ++directionPos;
    }

    const int directionCount = jd.isInput ? inCount : outCount;
    const float y = box.getBottom() - 34.0f;

    // Einzelbuchsen behalten das bisherige Layout (Quellen mittig, sonst 30/70).
    if (directionCount <= 1)
    {
        if (inCount == 0 || outCount == 0)
            return { box.getCentreX(), y };

        return { box.getX() + box.getWidth() * (jd.isInput ? 0.30f : 0.70f), y };
    }

    // Mehrere Buchsen einer Richtung: gleichmäßig verteilen, damit sich weder
    // die Buchsen noch ihre Labels überdecken (z. B. MATERIAL ROOM: 2x IN).
    const float spread = directionCount > 3
                             ? juce::jmin (0.24f, 0.80f / (float) (directionCount - 1))
                             : 0.24f;
    float frac = (jd.isInput ? 0.18f : 0.72f) + spread * (float) directionPos;

    // Innerhalb des Panels halten (Buchse + Label bleiben sichtbar).
    frac = juce::jlimit (0.12f, 0.88f, frac);

    return { box.getX() + box.getWidth() * frac, y };
}

int PatchView::jackCountFor (ModuleIndex module, bool isInput) noexcept
{
    int count = 0;

    for (const auto& d : jackDefs)
        if (d.module == module && d.isInput == isInput)
            ++count;

    return count;
}

const char* PatchView::jackLabel (Jack jack) noexcept
{
    const auto& jd = jackDefs[(size_t) jack];

    // MATERIAL ROOM hat zwei Eingänge mit identischem jackDefs-Namen.
    if (jd.module == ModuleIndex::MaterialRoom && jd.isInput)
        return jack == Jack::MaterialRoomKickIn ? "IN KICK" : "IN RUMBLE";

    return jd.displayName;
}


int PatchView::findJackAt (juce::Point<float> pos, float radius) const
{
    for (int j = 0; j < (int) Jack::JackCount; ++j)
    {
        if (jackCentre ((Jack) j).getDistanceFrom (pos) <= radius)
            return j;
    }
    return -1;
}

int PatchView::findCableAt (juce::Point<float> pos, float threshold) const
{
    // Grobe Distanz zur Bezier-Kurve per Abtastung
    const auto conns = patch.getConnections();
    for (int i = (int) conns.size() - 1; i >= 0; --i)
    {
        const auto p1 = jackCentre (conns[(size_t) i].from);
        const auto p2 = jackCentre (conns[(size_t) i].to);
        const float sag = 30.0f + std::abs (p2.x - p1.x) * 0.12f + std::abs (p2.y - p1.y) * 0.05f;
        const juce::Point<float> c1 { p1.x, p1.y + sag };
        const juce::Point<float> c2 { p2.x, p2.y + sag };

        juce::Point<float> prev = p1;
        for (int t = 1; t <= 24; ++t)
        {
            const float u = (float) t / 24.0f;
            const float iu = 1.0f - u;
            const juce::Point<float> pt {
                iu*iu*iu*p1.x + 3*iu*iu*u*c1.x + 3*iu*u*u*c2.x + u*u*u*p2.x,
                iu*iu*iu*p1.y + 3*iu*iu*u*c1.y + 3*iu*u*u*c2.y + u*u*u*p2.y
            };
            // Distanz Punkt-Strecke
            const auto d = pt - prev;
            const float len2 = d.getDotProduct (d);
            const float tproj = len2 > 0.0f ? juce::jlimit (0.0f, 1.0f, (pos - prev).getDotProduct (d) / len2) : 0.0f;
            const auto closest = prev + d * tproj;
            if (pos.getDistanceFrom (closest) <= threshold)
                return i;
            prev = pt;
        }
    }
    return -1;
}

//==============================================================================
void PatchView::paint (juce::Graphics& g)
{
    drawPanelBackground (g);

    // Die Titelzeile entfällt, die Aktiv-Buchsen kennzeichnen den Modus bereits.
    if (mode == ViewMode::Parameter)
    {
        // Parameter-Hint unten einblenden
        auto bar = getLocalBounds().removeFromBottom (18).toFloat().reduced (panelPadding, 0.0f);
        g.setColour (juce::Colour (0xff0a0b0d));
        g.fillRect (bar);
        g.setColour (CERNLookAndFeel::textColour.withAlpha (0.45f));
        g.setFont (juce::FontOptions (10.0f));
        g.drawText ("OUT = bipolar CV output  |  IN = additive CV input  |  Drag jack to jack = patch  |  Right-click removes  |  Repatching is immediate",
                    bar, juce::Justification::centredLeft);
        return;
    }

    // H6 (Review 2026-11-13): Ohne Eingangsverbindung am Output-Jack bleibt der
    // Kick stumm - im Eurorack ist das normal, hier aber eine stille Falle.
    {
        const auto conns = patch.getConnections();
        bool hasOutputPath = false;

        for (const auto& connection : conns)
            if (connection.to == Jack::OutputIn)
            {
                hasOutputPath = true;
                break;
            }

        if (! hasOutputPath)
        {
            auto bar = getLocalBounds().removeFromTop (22).toFloat().reduced (panelPadding, 1.0f);
            g.setColour (juce::Colour (0xff5a1414));
            g.fillRect (bar);
            g.setColour (juce::Colours::white);
            g.setFont (juce::FontOptions (11.0f));
            g.drawText ("NO PATH TO OUTPUT  -  der Kick bleibt stumm (am Output-Jack liegt keine Eingangsverbindung)",
                        bar, juce::Justification::centredLeft);
        }
    }

    // Module
    for (int m = 0; m < (int) ModuleIndex::ModuleCount; ++m)
        drawModule (g, (ModuleIndex) m);

    // Kabel (bestehende Verbindungen)
    const auto conns = patch.getConnections();
    for (int i = 0; i < (int) conns.size(); ++i)
    {
        const bool sel = (i == selectedCable);
        drawCable (g, jackCentre (conns[(size_t) i].from), jackCentre (conns[(size_t) i].to),
                   audioCableColour (parameters, conns[(size_t) i].from, conns[(size_t) i].to, sel), sel);
    }

    if (pending)
        drawPendingCable (g);

    // Jacks obenauf (damit Stecker sichtbar bleiben)
    for (int j = 0; j < (int) Jack::JackCount; ++j)
    {
        const bool hover = (hoverJack == j);
        bool target = false;
        if (pending && hover && pendingFrom != (Jack) j)
            target = patch.canConnect (pendingFrom, (Jack) j) || patch.canConnect ((Jack) j, pendingFrom);
        drawJack (g, (Jack) j, hover, target || (pending && (int) pendingFrom == j));
    }

    // Titelplatten NACH den Kabeln erneut: so bleiben Modultitel lesbar,
    // auch wenn eine Leitung über sie hinwegläuft.
    for (int m = 0; m < (int) ModuleIndex::ModuleCount; ++m)
        drawModuleTitle (g, (ModuleIndex) m, moduleBounds ((ModuleIndex) m));

    drawHintBar (g);
}

void PatchView::drawPanelBackground (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff08080a));

    // Gebürstetes Metall / dezente Streifen
    auto bounds = getLocalBounds().toFloat();
    for (int y = 0; y < getHeight(); y += 3)
    {
        const float a = 0.015f + 0.01f * std::sin ((float) y * 0.7f);
        g.setColour (juce::Colours::white.withAlpha (a));
        g.drawHorizontalLine (y, 0.0f, (float) getWidth());
    }

    // Vignette
    juce::ColourGradient vignette (juce::Colours::transparentBlack,
                                   bounds.getCentreX(), bounds.getCentreY(),
                                   juce::Colours::black.withAlpha (0.55f),
                                   0.0f, 0.0f, true);
    g.setGradientFill (vignette);
    g.fillRect (bounds);

    // Rahmen
    g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.35f));
    g.drawRect (getLocalBounds().reduced (4), 2);
}

void PatchView::drawModule (juce::Graphics& g, ModuleIndex module)
{
    const auto& def = moduleDefs[(size_t) module];
    auto box = moduleBounds (module);

    // Schatten
    g.setColour (juce::Colours::black.withAlpha (0.5f));
    g.fillRoundedRectangle (box.translated (4.0f, 5.0f), 8.0f);

    // Extrusion / Tiefe (rechte und untere Kante)
    juce::Path side;
    side.addRoundedRectangle (box.translated (6.0f, 6.0f), 8.0f);
    g.setColour (juce::Colour (0xff0b0c0f));
    g.fillPath (side);

    // Körper
    juce::ColourGradient body (juce::Colour (0xff24272e), box.getX(), box.getY(),
                               juce::Colour (0xff121418), box.getX(), box.getBottom(), false);
    g.setGradientFill (body);
    g.fillRoundedRectangle (box, 8.0f);

    g.setColour (juce::Colour (0xff3a3d45));
    g.drawRoundedRectangle (box, 8.0f, 1.2f);

    // Obere Glanzkante
    g.setColour (juce::Colours::white.withAlpha (0.08f));
    g.drawHorizontalLine ((int) box.getY() + 2, box.getX() + 8.0f, box.getRight() - 8.0f);

    // Titelplatte
    drawModuleTitle (g, module, box);

    // Parameterliste (Titelstreile freilassen, wie vorher)
    box.removeFromTop (26.0f);
    auto paramArea = box.reduced (10.0f, 4.0f);
    paramArea.removeFromBottom (40.0f); // Platz für Jacks
    g.setColour (CERNLookAndFeel::textColour.withAlpha (0.55f));
    g.setFont (juce::FontOptions (10.5f));
    g.drawFittedText (def.paramNames, paramArea.toNearestInt(),
                      juce::Justification::topLeft, 3);

    // Schrauben
    auto drawScrew = [&] (juce::Point<float> c)
    {
        g.setColour (juce::Colour (0xff54585f));
        g.fillEllipse (c.x - 4.0f, c.y - 4.0f, 8.0f, 8.0f);
        g.setColour (juce::Colour (0xff23262c));
        g.drawLine (c.x - 2.5f, c.y, c.x + 2.5f, c.y, 1.0f);
        g.drawLine (c.x, c.y - 2.5f, c.x, c.y + 2.5f, 1.0f);
    };
    auto b = moduleBounds (module);
    drawScrew ({ b.getX() + 10.0f, b.getY() + 10.0f });
    drawScrew ({ b.getRight() - 10.0f, b.getY() + 10.0f });
    drawScrew ({ b.getX() + 10.0f, b.getBottom() - 10.0f });
    drawScrew ({ b.getRight() - 10.0f, b.getBottom() - 10.0f });
}

void PatchView::drawModuleTitle (juce::Graphics& g, ModuleIndex module,
                                 juce::Rectangle<float> moduleBox)
{
    const auto& def = moduleDefs[(size_t) module];
    auto titleBar = moduleBox.removeFromTop (26.0f).reduced (6.0f, 4.0f);

    g.setColour (juce::Colour (0xff0d0e11));
    g.fillRoundedRectangle (titleBar, 4.0f);
    g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.85f));
    g.fillEllipse (titleBar.getX() + 6.0f, titleBar.getCentreY() - 3.0f, 6.0f, 6.0f);
    g.setColour (CERNLookAndFeel::textColour);
    g.setFont (CERNLookAndFeel::getNameFont().withHeight (13.0f).boldened());
    g.drawText (def.displayName, titleBar.translated (14.0f, 0.0f), juce::Justification::centredLeft);
}

void PatchView::drawJack (juce::Graphics& g, Jack jack, bool highlight, bool target)
{
    const auto& jd = jackDefs[(size_t) jack];
    const auto c = jackCentre (jack);
    const float r = jackRadius;

    // Äussere Mutter (Metall)
    juce::ColourGradient nut (juce::Colour (0xff9a9ea6), c.x - r, c.y - r,
                              juce::Colour (0xff2b2e34), c.x + r, c.y + r, true);
    g.setGradientFill (nut);
    g.fillEllipse (c.x - r, c.y - r, r * 2.0f, r * 2.0f);

    // Bohrung
    g.setColour (juce::Colours::black);
    g.fillEllipse (c.x - r * 0.45f, c.y - r * 0.45f, r * 0.9f, r * 0.9f);

    // Ring
    if (target)
        g.setColour (juce::Colours::lime.withAlpha (0.9f));
    else if (highlight)
        g.setColour (CERNLookAndFeel::accentColour.brighter (0.4f));
    else
        g.setColour (juce::Colours::white.withAlpha (0.15f));
    g.drawEllipse (c.x - r - 2.0f, c.y - r - 2.0f, (r + 2.0f) * 2.0f, (r + 2.0f) * 2.0f,
                   target || highlight ? 2.0f : 1.0f);

    // Label: bei mehreren Buchsen einer Richtung schmaler, damit sich die
    // Beschriftungen nicht überlappen (z. B. "IN KICK"/"IN RUMBLE").
    const bool crowded = jackCountFor (jd.module, jd.isInput) > 1;
    const float labelW = crowded ? 48.0f : 36.0f;

    // Dunkle Pille hinter dem Text: Kabel kreuzen die Labelzone sonst
    // und machen die Beschriftung unlesbar.
    const juce::Rectangle<float> labelArea (c.x - labelW * 0.5f, c.y + r + 3.0f, labelW, 11.0f);
    g.setColour (juce::Colour (0xe008080a));
    g.fillRoundedRectangle (labelArea.expanded (2.0f, 0.5f), 2.5f);

    g.setColour (CERNLookAndFeel::textColour.withAlpha (0.85f));
    g.setFont (juce::FontOptions (crowded ? 8.5f : 9.5f).withStyle ("Bold"));
    g.drawText (jackLabel (jack), labelArea, juce::Justification::centred);
}

void PatchView::drawCable (juce::Graphics& g, juce::Point<float> from, juce::Point<float> to,
                           juce::Colour colour, bool selected, bool pendingCable) const
{
    const float sag = 28.0f + std::abs (to.x - from.x) * 0.14f + std::abs (to.y - from.y) * 0.06f;
    juce::Path path;
    path.startNewSubPath (from);
    path.cubicTo (from.x, from.y + sag, to.x, to.y + sag, to.x, to.y);

    // Schatten
    juce::Path shadow = path;
    juce::AffineTransform t = juce::AffineTransform::translation (2.0f, 4.0f);
    shadow.applyTransform (t);
    g.setColour (juce::Colours::black.withAlpha (selected ? 0.5f : 0.35f));
    g.strokePath (shadow, juce::PathStrokeType (selected ? 8.0f : 6.5f));

    // Körper
    g.setColour (colour.withAlpha (pendingCable ? 0.85f : 0.95f));
    g.strokePath (path, juce::PathStrokeType (selected ? 6.5f : 5.0f,
                                               juce::PathStrokeType::curved,
                                               juce::PathStrokeType::rounded));

    // Highlight
    juce::Path hi = path;
    hi.applyTransform (juce::AffineTransform::translation (-0.5f, -1.5f));
    g.setColour (juce::Colours::white.withAlpha (selected ? 0.45f : 0.25f));
    g.strokePath (hi, juce::PathStrokeType (2.0f));

    // Steckerenden
    auto plug = [&] (juce::Point<float> p)
    {
        g.setColour (juce::Colour (0xff33373e));
        g.fillEllipse (p.x - 5.5f, p.y - 5.5f, 11.0f, 11.0f);
        g.setColour (colour.brighter (0.2f));
        g.fillEllipse (p.x - 2.5f, p.y - 2.5f, 5.0f, 5.0f);
    };
    plug (from);
    plug (to);
}

void PatchView::drawPendingCable (juce::Graphics& g)
{
    const auto from = jackCentre (pendingFrom);
    drawCable (g, from, mousePos, CERNLookAndFeel::accentColour.brighter (0.3f), false, true);

    // Zielhinweis
    g.setColour (juce::Colours::white.withAlpha (0.7f));
    g.setFont (juce::FontOptions (11.0f));
    g.drawText ("Select destination jack (ESC/right-click cancels)", from.x + 12.0f, from.y - 24.0f, 320.0f, 18.0f,
                juce::Justification::centredLeft);
}

void PatchView::drawHintBar (juce::Graphics& g)
{
    auto bar = getLocalBounds().removeFromBottom (20).toFloat().reduced (panelPadding, 0.0f);

    // UNDURCHSICHTIGER Trager: im Screenshot liefen die grauen/gruenen Kabel
    // direkt durch den Hint-Text.
    g.setColour (juce::Colour (0xf20a0b0d));
    g.fillRoundedRectangle (bar.reduced (2.0f, 1.0f), 3.0f);

    g.setColour (CERNLookAndFeel::textColour.withAlpha (0.55f));
    g.setFont (juce::FontOptions (11.0f));

    // MR-003 / MR-004 / MR-020 / MR-043: Hover-Hints fuer den MaterialRoom-Strip
    // (keine echten Tooltip-Fenster — die Hint-Bar ist die einzige Hover-Anzeige).
    juce::String hint = "Click/drag a jack to patch  |  Right-click a jack or cable to remove  |  DEFAULT = factory patch  |  Repatching changes the sound immediately";

    if (hoverJack >= 0)
    {
        const auto& jd = jackDefs[(size_t) hoverJack];

        if (jd.module == ModuleIndex::MaterialRoom)
        {
            const auto jack = (Jack) hoverJack;
            hint = jack == Jack::MaterialRoomKickIn    ? juce::String ("IN KICK = CompOut (Pre-Output)")
                 : jack == Jack::MaterialRoomRumbleIn  ? juce::String ("IN RUMBLE = RoomOut (Room-Tail; bei ROOM-Off stumm)")
                 :                                       juce::String ("OUT = Wet-Return (Wet-only) + Grumble; Dry bleibt im Bus");
        }
    }
    else if (moduleBounds (ModuleIndex::MaterialRoom).contains (mousePos))
    {
        hint = "STEEL = Metall-Resonanz, kein Raum  |  WATER bei Mix >= 0.5: bis +17 dB @ 20-100 Hz  |  SIZE modulieren = metallischer Comb-Sweep (Wub)";
    }

    g.drawText (hint, bar, juce::Justification::centredLeft);
}

//==============================================================================
void PatchView::mouseDown (const juce::MouseEvent& e)
{
    if (mode != ViewMode::Audio)
        return;

    mouseDownPos = e.position;
    dragMoved = false;

    if (e.mods.isPopupMenu())
    {
        const int j = findJackAt (e.position);
        if (j >= 0)
        {
            patch.removeConnectionsAt ((Jack) j);
            pending = false;
            selectedCable = -1;
            repaint();
            return;
        }

        const int cable = findCableAt (e.position);
        if (cable >= 0)
        {
            // Wertbasiert entfernen (Quelle/Ziel), damit ein zwischen Paint und
            // Klick verschobener Index keine falsche Verbindung löscht.
            const auto conns = patch.getConnections();
            if (cable < (int) conns.size())
                patch.removeConnection (conns[(size_t) cable].from,
                                        conns[(size_t) cable].to);
            selectedCable = -1;
            repaint();
            return;
        }
    }

    const int j = findJackAt (e.position);
    if (pending)
    {
        if (j >= 0)
        {
            // Verbindung herstellen (beide Richtungen probieren)
            const Jack a = pendingFrom;
            const Jack b = (Jack) j;
            bool ok = patch.addConnection (a, b) || patch.addConnection (b, a);
            if (ok) { pending = false; repaint(); return; }
        }
        // Klick ins Leere oder ungueltig: Abbruch
        pending = false;
        repaint();
        return;
    }

    if (j >= 0)
    {
        // Neue Verbindung von dieser Buchse starten
        pending = true;
        pendingFrom = (Jack) j;
        mousePos = e.position;
        selectedCable = -1;
        repaint();
        return;
    }

    const int cable = findCableAt (e.position);
    selectedCable = cable;
    repaint();
}

void PatchView::mouseDrag (const juce::MouseEvent& e)
{
    if (mode != ViewMode::Audio)
        return;

    mousePos = e.position;
    if (e.position.getDistanceFrom (mouseDownPos) > 4.0f)
        dragMoved = true;
    if (pending)
        repaint();
}

void PatchView::mouseMove (const juce::MouseEvent& e)
{
    if (mode != ViewMode::Audio)
        return;

    mousePos = e.position;
    const int h = findJackAt (e.position);
    // MR-003/004/020/043: Hint-Bar folgt Hover-Kontext (Modul/Buchsen) -> immer neu zeichnen.
    if (h != hoverJack)
        hoverJack = h;
    repaint();
}

void PatchView::mouseUp (const juce::MouseEvent& e)
{
    if (mode != ViewMode::Audio || ! pending || ! dragMoved)
        return;

    const int j = findJackAt (e.position);
    if (j >= 0)
    {
        const Jack a = pendingFrom;
        const Jack b = (Jack) j;
        if (patch.addConnection (a, b) || patch.addConnection (b, a))
        {
            pending = false;
            repaint();
            return;
        }
    }

    // Drag ins Leere -> Abbruch
    pending = false;
    repaint();
}

void PatchView::mouseExit (const juce::MouseEvent&)
{
    if (mode != ViewMode::Audio)
        return;

    hoverJack = -1;
    mousePos = { -1000.0f, -1000.0f }; // Hint-Bar zurück auf Standardtext
    repaint();
}

void PatchView::timerCallback()
{
    dashPhase += 0.15f;
    if (dashPhase > 1000.0f) dashPhase = 0.0f;
    if (pending && mode == ViewMode::Audio) repaint();
}