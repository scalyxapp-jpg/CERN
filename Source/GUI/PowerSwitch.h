#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/** Deutlich sichtbarer Power-/On/Off-Schalter für Module und Parameter.
 *  Zeigt immer ON oder OFF inklusive Zustandshinweisen. */
class PowerSwitch : public juce::ToggleButton
{
public:
    PowerSwitch();
    explicit PowerSwitch (const juce::String& name);

    void paint (juce::Graphics& g) override;

private:
    void drawState (juce::Graphics& g);
};
