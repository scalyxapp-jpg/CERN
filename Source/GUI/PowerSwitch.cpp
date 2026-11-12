#include "PowerSwitch.h"
#include "GUI/Theme/CERNLookAndFeel.h"

PowerSwitch::PowerSwitch()
{
    setClickingTogglesState (true);
    setTitle ("Power");
    setDescription ("Toggle module power on or off");
}

PowerSwitch::PowerSwitch (const juce::String& name)
{
    setName (name);
    setClickingTogglesState (true);
    setTitle (name);
    setDescription (name + " on/off");
    setTooltip (name + " on/off");
}

void PowerSwitch::paint (juce::Graphics& g)
{
    drawState (g);
}

void PowerSwitch::drawState (juce::Graphics& g)
{
    const bool on = getToggleState();
    const bool hover = isMouseOverOrDragging();
    auto bounds = getLocalBounds().toFloat().reduced (1.0f);

    // 3D-Gehaeuse: versenkt, bei ON mit rotem Innenlicht.
    CERNLookAndFeel::drawWell (g, bounds, 5.0f);

    if (on)
    {
        juce::ColourGradient lit (CERNLookAndFeel::accentColour.withAlpha (0.55f),
                                  bounds.getX(), bounds.getY(),
                                  CERNLookAndFeel::accentColour.withAlpha (0.12f),
                                  bounds.getX(), bounds.getBottom(), false);
        g.setGradientFill (lit);
        g.fillRoundedRectangle (bounds.reduced (1.5f), 4.0f);

        g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.85f));
        g.drawRoundedRectangle (bounds, 5.0f, 1.2f);
    }
    else
    {
        g.setColour (CERNLookAndFeel::edgeColour.withAlpha (hover ? 0.75f : 0.45f));
        g.drawRoundedRectangle (bounds, 5.0f, 1.0f);
    }

    if (hover)
    {
        g.setColour (juce::Colours::white.withAlpha (0.06f));
        g.fillRoundedRectangle (bounds.reduced (1.0f), 4.5f);
    }

    // LED: rot wenn ON, sonst dunkles Glas.
    const auto led = juce::Rectangle<float> (bounds.getX() + 5.0f,
                                             bounds.getCentreY() - 4.0f, 8.0f, 8.0f);
    juce::ColourGradient ledGradient (on ? CERNLookAndFeel::accentBrightColour
                                         : juce::Colour (0xff2a1416),
                                      led.getCentreX(), led.getY(),
                                      on ? CERNLookAndFeel::accentColour.darker (0.4f)
                                         : juce::Colour (0xff120c0d),
                                      led.getCentreX(), led.getBottom(), false);
    g.setGradientFill (ledGradient);
    g.fillEllipse (led);

    if (on)
    {
        g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.30f));
        g.drawEllipse (led.expanded (2.5f), 2.0f);
    }

    g.setColour (juce::Colours::white.withAlpha (on ? 0.60f : 0.12f));
    g.fillEllipse (led.getX() + 2.0f, led.getY() + 1.5f, 3.0f, 2.0f);

    // Text
    g.setColour (on ? CERNLookAndFeel::textColour
                    : CERNLookAndFeel::textColour.withAlpha (0.55f));
    g.setFont (juce::Font (juce::FontOptions (10.0f, juce::Font::bold)));
    g.drawText (on ? "ON" : "OFF",
                bounds.withLeft (led.getRight() + 4.0f),
                juce::Justification::centred);
}
