#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <vector>

#include "GUI/CERNFader.h"
#include "GUI/ParamKnob.h"
#include "GUI/PowerSwitch.h"

//==============================================================================
/** Eine Sektion mit Titel und einem Grid aus Parameter-Controls.

    Der Control-Typ wird aus dem Parameter abgeleitet:
      - AudioParameterBool   -> ToggleButton (Schalter)
      - AudioParameterChoice -> ComboBox (Auswahl)
      - sonst                -> ParamKnob (Drehregler)

    Die Höhe richtet sich nach der Anzahl der Controls, damit kein Control
    abgeschnitten wird.
*/
class SectionPanel : public juce::Component
{
public:
    static constexpr int columns     = 4;
    static constexpr int rowHeight   = 104;
    static constexpr int rowGap      = 8;
    static constexpr int titleHeight = 30;
    static constexpr int padding     = 10;

    /** Zeilenhoehe in Abhaengigkeit der Sektionsbreite: bei grossen Fenstern
        (>=1400 px Gesamtbreite) sollen die Knobs mitwachsen, damit die
        Sektionen nicht "luftig-duenn" wirken. Bei kleinen Fenstern unveraendert. */
    static int rowHeightForWidth (int sectionWidth)
    {
        if (sectionWidth >= 740)
            return 132;
        if (sectionWidth >= 620)
            return 118;
        return rowHeight;
    }

    static constexpr int enableToggleWidth  = 48;
    static constexpr int enableToggleHeight = 22;
    static constexpr int enableToggleGap    = 5;

    SectionPanel (juce::AudioProcessorValueTreeState& apvts,
                  const juce::String& title,
                  const std::vector<juce::String>& parameterIds,
                  const std::vector<juce::String>& enableParameterIds = {});

    ~SectionPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** Aktualisiert alle Wert-Anzeigen (nach Preset-Wechsel). */
    void refresh();

    static int getPreferredHeight (int numControls, int sectionWidth);

    int getPreferredHeight (int sectionWidth) const
    {
        return getPreferredHeight ((int) controls.size(), sectionWidth);
    }

private:
    struct Control
    {
        std::unique_ptr<juce::Component> component;
        ParamKnob* knob = nullptr;
        CERNFader* fader = nullptr;
    };

    void createControl (juce::AudioProcessorValueTreeState& apvts,
                        const juce::String& parameterId);

    /** Mix-/Breiten-Parameter bekommen einen 3D-Fader statt eines Knobs. */
    static bool prefersFader (const juce::String& parameterId);

    void createEnableToggle (juce::AudioProcessorValueTreeState& apvts,
                            const juce::String& parameterId);

    juce::String title;
    juce::String upperTitle;
    std::vector<Control> controls;

    std::vector<std::unique_ptr<PowerSwitch>> enableToggles;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> enableAttachments;

    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ComboBoxAttachment>> comboAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> buttonAttachments;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SectionPanel)
};
