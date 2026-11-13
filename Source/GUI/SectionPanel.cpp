#include "SectionPanel.h"
#include "GUI/Theme/CERNLookAndFeel.h"

//==============================================================================
namespace
{
    /** Kuerzt "Membrane Tension" in der Sektion "Membrane / Physical" auf "Tension". */
    juce::String makeDisplayName (const juce::String& parameterName,
                                 const juce::String& sectionTitle)
    {
        const juce::String sectionWord =
            sectionTitle.upToFirstOccurrenceOf (" /", false, false).trim();

        if (sectionWord.isNotEmpty()
            && parameterName.startsWithIgnoreCase (sectionWord + " "))
        {
            return parameterName.substring (sectionWord.length() + 1);
        }

        return parameterName;
    }
}

//==============================================================================
SectionPanel::SectionPanel (juce::AudioProcessorValueTreeState& apvts,
                            const juce::String& panelTitle,
                            const std::vector<juce::String>& ids,
                            const std::vector<juce::String>& enableIds)
    : title (panelTitle), upperTitle (panelTitle.toUpperCase())
{
    setTitle (title);
    setDescription (title + " section");

    for (const auto& id : ids)
        createControl (apvts, id);

    for (const auto& id : enableIds)
        createEnableToggle (apvts, id);
}

void SectionPanel::createEnableToggle (juce::AudioProcessorValueTreeState& apvts,
                                       const juce::String& parameterId)
{
    auto* parameter = apvts.getParameter (parameterId);

    if (parameter == nullptr)
        return;

    const juce::String parameterName = parameter->getName (64);

    auto toggle = std::make_unique<PowerSwitch> (parameterName);
    toggle->setClickingTogglesState (true);
    toggle->setTitle (parameterName);
    toggle->setDescription (parameterName + " on/off");
    toggle->setTooltip (parameterName + " on/off");

    PowerSwitch* rawToggle = toggle.get();
    addAndMakeVisible (*toggle);
    enableAttachments.push_back (
        std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            apvts, parameterId, *rawToggle));

    enableToggles.push_back (std::move (toggle));
}

SectionPanel::~SectionPanel() = default;

bool SectionPanel::prefersFader (const juce::String& parameterId)
{
    return parameterId == "driveMix"
        || parameterId == "compMix"
        || parameterId == "reverbMix"
        || parameterId == "materialRoomMix"
        || parameterId == "higgsMix"
        || parameterId == "outputWidth";
}

int SectionPanel::getPreferredHeight (int numControls, int sectionWidth)
{
    if (numControls <= 0)
        return titleHeight + 2 * padding;

    const int rows = (numControls + columns - 1) / columns;
    const int rh = rowHeightForWidth (sectionWidth);
    return titleHeight + padding + rows * rh + (rows - 1) * rowGap + padding;
}

void SectionPanel::createControl (juce::AudioProcessorValueTreeState& apvts,
                                  const juce::String& parameterId)
{
    auto* parameter = apvts.getParameter (parameterId);

    if (parameter == nullptr)
        return;

    const juce::String displayName = makeDisplayName (parameter->getName (64), title);

    if (dynamic_cast<juce::AudioParameterChoice*> (parameter) != nullptr)
    {
        auto combo = std::make_unique<juce::ComboBox>();
        combo->addItemList (dynamic_cast<juce::AudioParameterChoice*> (parameter)->choices, 1);
        combo->setJustificationType (juce::Justification::centred);
        combo->setTitle (displayName);
        combo->setDescription (displayName + " selection");
        combo->setTooltip (displayName);

        juce::ComboBox* rawCombo = combo.get();
        addAndMakeVisible (*combo);
        comboAttachments.push_back (
            std::make_unique<juce::AudioProcessorValueTreeState::ComboBoxAttachment> (
                apvts, parameterId, *rawCombo));

        Control control;
        control.component = std::move (combo);
        controls.push_back (std::move (control));
        return;
    }

    if (dynamic_cast<juce::AudioParameterBool*> (parameter) != nullptr)
    {
        auto toggle = std::make_unique<juce::ToggleButton> (displayName);
        toggle->setClickingTogglesState (true);
        toggle->setTitle (displayName);
        toggle->setDescription (displayName + " on/off");
        toggle->setTooltip (displayName + " on/off");

        juce::ToggleButton* rawToggle = toggle.get();
        addAndMakeVisible (*toggle);
        buttonAttachments.push_back (
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                apvts, parameterId, *rawToggle));

        Control control;
        control.component = std::move (toggle);
        controls.push_back (std::move (control));
        return;
    }

    if (prefersFader (parameterId))
    {
        auto fader = std::make_unique<CERNFader> (apvts, parameterId, displayName);
        CERNFader* rawFader = fader.get();
        addAndMakeVisible (*fader);

        Control control;
        control.component = std::move (fader);
        control.fader = rawFader;
        controls.push_back (std::move (control));
        return;
    }

    auto knob = std::make_unique<ParamKnob> (apvts, parameterId, displayName);
    ParamKnob* rawKnob = knob.get();
    addAndMakeVisible (*knob);

    Control control;
    control.component = std::move (knob);
    control.knob = rawKnob;
    controls.push_back (std::move (control));
}

//==============================================================================
// Kleines gluehendes Vakuumroehrchen fuer das Röhren-Stufe-Sektionsbadge.
namespace
{
void drawTubeBadge (juce::Graphics& g, juce::Rectangle<float> area, bool on)
{
    const float cx = area.getCentreX();

    // Halo bei eingeschaltetem Heizer
    if (on)
    {
        juce::ColourGradient halo (juce::Colour (0xffff7a1a).withAlpha (0.35f),
                                   cx, area.getCentreY(),
                                   juce::Colours::transparentBlack,
                                   cx + area.getHeight(), area.getCentreY(), true);
        g.setGradientFill (halo);
        g.fillEllipse (area);
    }

    // Glas-Kolben
    juce::Path glass;
    glass.addRoundedRectangle (area.reduced (2.0f, 1.0f), 4.0f);
    g.setColour (on ? juce::Colour (0xff2c1a08) : juce::Colour (0xff161a20));
    g.fillPath (glass);
    g.setColour (juce::Colour (0xff788898).withAlpha (0.40f));
    g.strokePath (glass, juce::PathStrokeType (0.8f));

    // Sockel (dunkles Bakelit)
    auto base = area.reduced (2.0f, 1.0f).removeFromBottom (6.0f);
    g.setColour (juce::Colour (0xff3a2418));
    g.fillRoundedRectangle (base, 2.0f);

    // Heizer: Sinus-Zickzack
    juce::Path heater;
    const float y = area.getCentreY() - 2.0f;
    const float amp = 2.6f;
    const int segments = 24;

    for (int i = 0; i <= segments; ++i)
    {
        const float t = (float) i / (float) segments;
        const float x = area.getX() + 4.0f + (area.getWidth() - 8.0f) * t;
        const float yy = y + std::sin (t * 2.0f * juce::MathConstants<float>::twoPi) * amp;

        if (i == 0)
            heater.startNewSubPath (x, yy);
        else
            heater.lineTo (x, yy);
    }

    if (on)
    {
        g.setColour (juce::Colour (0xffff4d00).withAlpha (0.90f));
        g.strokePath (heater, juce::PathStrokeType (1.8f));
        g.setColour (juce::Colour (0xffffe0b0));
        g.strokePath (heater, juce::PathStrokeType (0.8f));
    }
    else
    {
        g.setColour (juce::Colour (0xff59606c));
        g.strokePath (heater, juce::PathStrokeType (1.0f));
    }
}
} // namespace

void SectionPanel::paint (juce::Graphics& g)
{
    // Titel separat zeichnen, damit die Enable-Schalter rechts im Titelbereich
    // nicht mit dem Text kollidieren (bei 2+ Schaltern relevant).
    CERNLookAndFeel::drawSectionPanel (g, 0, 0, getWidth(), getHeight(), {});

    if (upperTitle.isEmpty())
        return;

    int titleRight = getWidth() - 12;

    if (! enableToggles.empty())
    {
        const int toggleAreaWidth = (int) enableToggles.size() * enableToggleWidth
                                  + ((int) enableToggles.size() - 1) * enableToggleGap;
        titleRight = juce::jmin (titleRight, getWidth() - padding - toggleAreaWidth - 8);
    }

    const juce::Rectangle<int> titleArea (12, 6, juce::jmax (0, titleRight - 12), 20);

    // Bei der Röhren-Stufe: gluehendes Vakuumröhrchen-Badge statt der LED.
    const bool isTube = (title == "Röhren-Stufe");

    if (isTube && ! enableToggles.empty())
    {
        const bool tubeOn = enableToggles.front()->getToggleState();
        drawTubeBadge (g, juce::Rectangle<float> ((float) titleArea.getX(), 1.0f,
                                                  20.0f, 26.0f), tubeOn);
    }
    else
    {
        // Rote LED + Titel: Signal-Rot nur als Akzent, Text bleibt Off-White.
        g.setColour (CERNLookAndFeel::accentColour);
        g.fillEllipse ((float) titleArea.getX(), (float) titleArea.getY() + 5.0f, 6.0f, 6.0f);
        g.setColour (CERNLookAndFeel::accentBrightColour.withAlpha (0.35f));
        g.drawEllipse ((float) titleArea.getX() - 1.5f, (float) titleArea.getY() + 3.5f, 9.0f, 9.0f, 1.0f);
    }

    g.setColour (CERNLookAndFeel::textColour);
    g.setFont (CERNLookAndFeel::getSectionTitleFont());
    g.drawText (upperTitle, titleArea.withTrimmedLeft (isTube ? 26 : 14),
                juce::Justification::left, true);
}

void SectionPanel::resized()
{
    auto area = getLocalBounds().reduced (padding);
    area.removeFromTop (titleHeight - padding);

    // Modul-Ein/Aus-Schalter oben rechts im Titelbereich
    int toggleX = getWidth() - padding - enableToggleWidth;

    for (auto& toggle : enableToggles)
    {
        toggle->setBounds (toggleX, 5, enableToggleWidth, enableToggleHeight);
        toggleX -= enableToggleWidth + enableToggleGap;
    }

    if (controls.empty())
        return;

    const int rh = rowHeightForWidth (getWidth());
    const int cellWidth =
        (area.getWidth() - (columns - 1) * rowGap) / columns;

    for (size_t i = 0; i < controls.size(); ++i)
    {
        const int row = (int) i / columns;
        const int col = (int) i % columns;

        const int cellX = area.getX() + col * (cellWidth + rowGap);
        const int cellY = area.getY() + row * (rh + rowGap);

        // Auswahl/Toggle bleiben kompakt in der Zelle; nur Regler fuellen
        // die volle Hoehe. Sonst wirken ComboBoxen wie leere grosse Felder.
        if (auto* combo = dynamic_cast<juce::ComboBox*> (controls[i].component.get()))
        {
            combo->setBounds (cellX + 4, cellY + (rh - 30) / 2,
                              juce::jmax (1, cellWidth - 8), 30);
        }
        else if (auto* toggle = dynamic_cast<juce::ToggleButton*> (controls[i].component.get()))
        {
            toggle->setBounds (cellX + 6, cellY + (rh - 26) / 2,
                               juce::jmax (1, cellWidth - 12), 26);
        }
        else
        {
            controls[i].component->setBounds (cellX, cellY, cellWidth, rh);
        }
    }
}

void SectionPanel::refresh()
{
    for (auto& control : controls)
    {
        if (control.knob != nullptr)
            control.knob->refresh();

        if (control.fader != nullptr)
            control.fader->refresh();
    }
}
