#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/** Grafisches C.E.R.N-Logo fuer den Header.

    Links das generierte Emblem (Ring + roter Kollisionspunkt, freigestellt),
    rechts die Wortmarke als Vektor-Typografie: weisse Buchstaben, die Punkte
    als rote Quadrate. Kein Text-Label, kein AI-Schriftbild – dadurch bleibt
    die Wortmarke bei jeder Groesse scharf und korrekt lesbar.

    Accessibility: Titel/Beschreibung sind "C.E.R.N".
*/
class LogoComponent : public juce::Component
{
public:
    LogoComponent();

    void paint (juce::Graphics& g) override;

    /** Bevorzugte Breite fuer eine gegebene Hoehe (Header-Layout). */
    static int preferredWidthForHeight (int height);

private:
    juce::Image emblem;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (LogoComponent)
};
