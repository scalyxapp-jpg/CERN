#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/** 3D-Fader fuer einen APVTS-Parameter (vertikal), inkl. Name und Wert.

    Wird fuer Mix-/Breiten-Parameter im Sektions-Grid verwendet, wo ein
    Drehregler zu klein waere. Die Bindung laeuft ueber einen normalen
    SliderAttachment – es gibt keine separate Parameter-Logik.
*/
class CERNFader : public juce::Component,
                  public juce::SettableTooltipClient
{
public:
    static constexpr int nameHeight  = 15;
    static constexpr int valueHeight = 14;

    CERNFader (juce::AudioProcessorValueTreeState& apvts,
               const juce::String& parameterId,
               const juce::String& displayName);

    ~CERNFader() override;

    void resized() override;
    void paint (juce::Graphics& g) override;

    /** Aktualisiert die Wertanzeige (z. B. nach Preset-Wechsel). */
    void refresh();

    juce::Slider& getSlider() noexcept { return slider; }

private:
    /** Slider, der Hover-Zustaende sofort neu zeichnet (3D-Highlight). */
    class HoverSlider : public juce::Slider
    {
    public:
        using juce::Slider::Slider;

        void mouseEnter (const juce::MouseEvent& e) override
        {
            juce::Slider::mouseEnter (e);
            repaint();
        }

        void mouseExit (const juce::MouseEvent& e) override
        {
            juce::Slider::mouseExit (e);
            repaint();
        }
    };

    void updateValueText();

    juce::AudioProcessorValueTreeState& apvts;
    juce::String parameterId;
    juce::String lastValueText;
    float lastNormalisedValue = -1.0f;

    HoverSlider slider;
    juce::Label nameLabel;
    juce::Label valueLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CERNFader)
};
