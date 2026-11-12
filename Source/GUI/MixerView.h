#pragma once

#include <array>
#include <memory>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "DSP/MixerStage.h"
#include "Parameters/ParameterIDs.h"

//==============================================================================
/** Mixer-Ansicht: ein Kanalstreifen pro klang-erzeugendem Modul.

    Jeder Streifen zeigt den Modulnamen, vier EQ-Regler (LOW / MID-LOW /
    MID / HIGH), einen Lautstaerke-Fader und einen MUTE-Schalter. Die
    Sidechain-Beziehungen (Comp -> FX, Click -> Sub) werden als rote
    Sidechain-Klammer angezeigt, damit die automatischen Ducking-Beziehungen
    im GUI sichtbar sind.

    Alle Regler schreiben direkt in die APVTS (SliderAttachment /
    ButtonAttachment) - die DSP-Seite liest die Werte pro Block.
*/
class MixerView : public juce::Component,
                  private juce::Timer
{
public:
    explicit MixerView (juce::AudioProcessorValueTreeState& apvts,
                        const MixerStage& mixer);
    /** Kopierieren ist gesperrt (JUCE_DECLARE_NON_COPYABLE). */
    ~MixerView() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    class ChannelStrip;

    void timerCallback() override;

    juce::AudioProcessorValueTreeState& parameters;
    const MixerStage& mixer;

    std::vector<std::unique_ptr<ChannelStrip>> strips;
    juce::Viewport viewport;
    juce::Component stripContainer;

    /** Globaler Sidechain-Tiefen-Regler (0..1). */
    juce::Slider sidechainSlider { juce::Slider::RotaryHorizontalVerticalDrag,
                                   juce::Slider::NoTextBox };
    juce::Label sidechainLabel;
    juce::Label sidechainValue;
    juce::Label titleLabel;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> sidechainAttachment;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerView)
};
