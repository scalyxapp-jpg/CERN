#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/** Ein einzelner Parameter-Knob mit Name (oben) und formatiertem Wert (unten).

    Der Regler selbst ist ein Kind-Component, damit Name und Wert ausserhalb
    des Drehbereichs liegen und nicht von der Knopfgrafik überdeckt werden.
*/
//==============================================================================
/** Note + Cents-Anzeige für Frequenz-/Pitch-Parameter (SND-131). */
namespace TuningDisplay
{
    /** Hz -> "F#1 -3 ct"; 0/ungültig -> "-". */
    juce::String noteNameWithCents (float hz);

    /** true für fundamentalHz und stepPitch_* (Note + Cents statt Hz). */
    bool isPitchedParameter (const juce::String& parameterId);

    /** Note+Cents-Text für Pitched-Parameter, sonst leer. */
    juce::String pitchedValueText (juce::AudioProcessorValueTreeState& apvts,
                                   const juce::String& parameterId);
}

//==============================================================================
class ParamKnob : public juce::Component,
                  public juce::SettableTooltipClient
{public:
    static constexpr int nameHeight  = 19;
    static constexpr int valueHeight = 18;

    ParamKnob (juce::AudioProcessorValueTreeState& apvts,
               const juce::String& parameterId,
               const juce::String& displayName);

    ~ParamKnob() override;

    void resized() override;
    void paint (juce::Graphics& g) override;

    /** Aktualisiert den angezeigten Wert (z. B. nach Preset-Wechsel). */
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
    juce::String displayName;
    juce::String lastValueText;
    juce::String lastTooltip;
    float lastNormalisedValue = -1.0f;

    HoverSlider slider { juce::Slider::RotaryHorizontalVerticalDrag,
                         juce::Slider::NoTextBox };
    juce::Label nameLabel;
    juce::Label valueLabel;

    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> attachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ParamKnob)
};
