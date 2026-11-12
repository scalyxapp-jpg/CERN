#include "MixerView.h"
#include "GUI/CERNFader.h"
#include "GUI/ParamKnob.h"
#include "GUI/Theme/CERNLookAndFeel.h"

//==============================================================================
/** Ein Kanalstreifen: Name, 4 EQ-Regler, Fader, Mute + Peak-Meter. */
class MixerView::ChannelStrip : public juce::Component,
                               public juce::SettableTooltipClient
{
public:
    ChannelStrip (juce::AudioProcessorValueTreeState& apvts, int channelIndex)
        : apvts (apvts), channel (channelIndex)
    {
        setTitle (MixerStage::channelName (channelIndex));
        setDescription (juce::String (MixerStage::channelName (channelIndex)) + " mixer channel");

        nameLabel.setText (MixerStage::channelName (channelIndex), juce::dontSendNotification);
        nameLabel.setJustificationType (juce::Justification::centred);
        nameLabel.setFont (CERNLookAndFeel::getNameFont());
        nameLabel.setColour (juce::Label::textColourId,
                             CERNLookAndFeel::textColour.withAlpha (0.9f));
        nameLabel.setMinimumHorizontalScale (0.6f);
        nameLabel.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (nameLabel);

        // 4 EQ-Baender: LOW / MID-LOW / MID / HIGH.
        static const char* bandNames[4] { "LOW", "MID-LOW", "MID", "HIGH" };
        static const char* bandParams[4] { "Low", "MidLow", "Mid", "High" };

        for (int b = 0; b < 4; ++b)
        {
            auto knob = std::make_unique<ParamKnob> (
                apvts, ParameterIDs::mixerId (channel, bandParams[b]), bandNames[b]);
            addAndMakeVisible (*knob);
            eqKnobs.push_back (std::move (knob));
        }

        fader = std::make_unique<CERNFader> (
            apvts, ParameterIDs::mixerId (channel, "Gain"), "LEVEL");
        addAndMakeVisible (*fader);

        muteButton.setClickingTogglesState (true);
        muteButton.setButtonText ("MUTE");
        muteButton.setTooltip ("Mute this channel");
        muteButton.setWantsKeyboardFocus (false);
        muteButton.onClick = [this] { repaint(); };
        addAndMakeVisible (muteButton);

        muteAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
            apvts, ParameterIDs::mixerId (channel, "Mute"), muteButton);

        // Sidechain-Anzeige: Quelle des Ducks (oder leer).
        const int source = MixerStage::sidechainSource (channel);
        sidechainLabel.setText (source >= 0
                                    ? juce::String ("SC ") + MixerStage::channelName (source)
                                    : juce::String(),
                                juce::dontSendNotification);
        sidechainLabel.setText ("SIDE-CHAIN", juce::dontSendNotification);
    sidechainLabel.setJustificationType (juce::Justification::centred);
        sidechainLabel.setFont (CERNLookAndFeel::getLabelFont());
        sidechainLabel.setColour (juce::Label::textColourId, CERNLookAndFeel::accentBrightColour);
        sidechainLabel.setMinimumHorizontalScale (0.6f);
        sidechainLabel.setInterceptsMouseClicks (false, false);
        addAndMakeVisible (sidechainLabel);
    }

    void paint (juce::Graphics& g) override
    {
        const auto area = getLocalBounds().toFloat();

        CERNLookAndFeel::drawWell (g, area, 5.0f);

        // Peak-Meter (rechts) als schmale 3D-Saeule.
        if (peakDb > -60.0f)
        {
            const float norm = juce::jlimit (0.0f, 1.0f, (peakDb + 60.0f) / 60.0f);
            auto meterArea = area;
            const float h = meterArea.getHeight() * norm;
            const auto meterRect = meterArea.removeFromRight (6.0f)
                                      .withHeight (h)
                                      .withY (meterArea.getBottom() - h);

            g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.85f));
            g.fillRect (meterRect);
        }

        if (muteButton.getToggleState())
        {
            g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.10f));
            g.fillRoundedRectangle (area, 5.0f);
        }
    }

    void resized() override
    {
        auto area = getLocalBounds();

        nameLabel.setBounds (area.removeFromTop (16));
        sidechainLabel.setBounds (area.removeFromTop (13));

        // 4 EQ-Baender als 2x2-Block oberhalb des Faders.
        const int knobSize = juce::jmin (42, (area.getWidth() - 14) / 2);
        const int knobRowH = knobSize + ParamKnob::nameHeight + ParamKnob::valueHeight;

        for (int b = 0; b < 4; ++b)
        {
            auto& knob = *eqKnobs[(size_t) b];
            const int row = b / 2;
            const int col = b % 2;
            const int x = area.getX() + 4 + col * (knobSize + 4);
            const int y = area.getY() + row * knobRowH;
            knob.setBounds (x, y, knobSize, knobRowH);
        }

        area.removeFromTop (2 * knobRowH + 6);

        // MUTE-Button unten, Fader in der Restflaeche darüber.
        const int buttonH = 20;
        muteButton.setBounds (area.removeFromBottom (buttonH).reduced (8, 0));

        fader->setBounds (area.reduced (6, 2));  // Restflaeche ueber MUTE
    }

    void setPeakDb (float value) noexcept
    {
        if (peakDb == value)
            return;

        peakDb = value;
        repaint();
    }

    void refresh()
    {
        for (auto& knob : eqKnobs)
            knob->refresh();

        fader->refresh();
    }

private:
    juce::AudioProcessorValueTreeState& apvts;
    int channel;

    juce::Label nameLabel;
    juce::Label sidechainLabel;
    std::vector<std::unique_ptr<ParamKnob>> eqKnobs;
    std::unique_ptr<CERNFader> fader;
    juce::TextButton muteButton;
    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> muteAttachment;

    float peakDb { -120.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (ChannelStrip)
};

//==============================================================================
MixerView::MixerView (juce::AudioProcessorValueTreeState& apvtsRef,
                        const MixerStage& mixerRef)
    : parameters (apvtsRef), mixer (mixerRef)
{
    for (int c = 0; c < MixerStage::numChannels; ++c)
    {
        auto strip = std::make_unique<ChannelStrip> (parameters, c);
        stripContainer.addAndMakeVisible (*strip);  // muss IM Viewport liegen
        strips.push_back (std::move (strip));
    }

    viewport.setViewedComponent (&stripContainer, false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    sidechainSlider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                         juce::MathConstants<float>::pi * 2.75f, true);
    sidechainSlider.setMouseDragSensitivity (160);
    sidechainSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    sidechainSlider.setTooltip ("Global depth of the automatic sidechain "
                                "(Comp -> Room/Reverb/Mat-Room, Click -> Sub)");
    addAndMakeVisible (sidechainSlider);

    sidechainLabel.setText ("SIDE-CHAIN", juce::dontSendNotification);
    sidechainLabel.setJustificationType (juce::Justification::centred);
    sidechainLabel.setFont (CERNLookAndFeel::getNameFont());
    sidechainLabel.setColour (juce::Label::textColourId,
                              CERNLookAndFeel::textColour.withAlpha (0.8f));
    sidechainLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (sidechainLabel);

    sidechainValue.setJustificationType (juce::Justification::centredLeft);
    sidechainValue.setFont (CERNLookAndFeel::getValueFont());
    sidechainValue.setColour (juce::Label::textColourId,
                              CERNLookAndFeel::accentBrightColour);
    sidechainValue.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (sidechainValue);

    titleLabel.setText ("MIXER", juce::dontSendNotification);
    titleLabel.setJustificationType (juce::Justification::centredLeft);
    titleLabel.setFont (CERNLookAndFeel::getSectionTitleFont());
    titleLabel.setColour (juce::Label::textColourId, CERNLookAndFeel::accentBrightColour);
    titleLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (titleLabel);

    sidechainAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        parameters, ParameterIDs::mixerSidechainDepth, sidechainSlider);

    auto refreshSidechainValue = [this]
    {
        if (auto* p = parameters.getParameter (ParameterIDs::mixerSidechainDepth))
        {
            const auto text = p->getText (p->getValue(), 8);
            sidechainValue.setText (text.isEmpty()
                                       ? juce::String (sidechainSlider.getValue(), 2)
                                       : text,
                                   juce::dontSendNotification);
        }
    };

    sidechainSlider.onValueChange = refreshSidechainValue;
    refreshSidechainValue();

    startTimerHz (20);
}

MixerView::~MixerView()
{
    stopTimer();
}

void MixerView::timerCallback()
{
    for (int c = 0; c < (int) strips.size(); ++c)
        strips[(size_t) c]->setPeakDb (mixer.getPeakDb (c));
}

void MixerView::paint (juce::Graphics& g)
{
    CERNLookAndFeel::drawBackdrop (g, getLocalBounds().toFloat());
}

void MixerView::resized()
{
    auto area = getLocalBounds();

    // Kopfzeile: Titel + Sidechain-Regler links, Kanalstreifen darunter.
    const int headH = 66;
    auto head = area.removeFromTop (headH);

    titleLabel.setBounds (head.removeFromLeft (86).reduced (6, 8));
    sidechainLabel.setBounds (head.removeFromLeft (92).reduced (4, 8));
    sidechainSlider.setBounds (head.removeFromLeft (56).reduced (6, 8));
    sidechainValue.setBounds (head.removeFromLeft (52).reduced (2, 8));

    viewport.setBounds (area);

    // Adaptive Streifenbreite: bei breiten Fenstern passen alle 13 Streifen
    // nebeneinander; bei schmalen wird horizontal gescrollt (Mindestbreite 96 px,
    // damit die 4 EQ-Knobs lesbar bleiben).
    const int numStrips = juce::jmax (1, (int) strips.size());
    const int visibleW = juce::jmax (1, viewport.getWidth() - viewport.getScrollBarThickness());
    const int stripW = juce::jlimit (96, 132, visibleW / numStrips);

    stripContainer.setSize (stripW * numStrips, juce::jmax (1, viewport.getHeight()));

    for (int c = 0; c < numStrips; ++c)
        strips[(size_t) c]->setBounds (c * stripW, 0, stripW, stripContainer.getHeight());
}
