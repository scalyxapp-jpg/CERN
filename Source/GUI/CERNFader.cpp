#include "CERNFader.h"
#include "GUI/Theme/CERNLookAndFeel.h"

//==============================================================================
CERNFader::CERNFader (juce::AudioProcessorValueTreeState& apvtsRef,
                      const juce::String& parameterIdIn,
                      const juce::String& displayNameIn)
    : apvts (apvtsRef), parameterId (parameterIdIn)
{
    slider.setSliderStyle (juce::Slider::LinearVertical);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setMouseDragSensitivity (180);
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, parameterId, slider);

    setTitle (displayNameIn);
    setDescription (displayNameIn + " fader");
    slider.setTitle (displayNameIn);
    slider.setName (displayNameIn);

    nameLabel.setText (displayNameIn, juce::dontSendNotification);
    nameLabel.setJustificationType (juce::Justification::centred);
    nameLabel.setFont (CERNLookAndFeel::getNameFont());
    nameLabel.setColour (juce::Label::textColourId,
                         CERNLookAndFeel::textColour.withAlpha (0.82f));
    nameLabel.setMinimumHorizontalScale (0.55f);
    nameLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (nameLabel);

    valueLabel.setJustificationType (juce::Justification::centred);
    valueLabel.setFont (CERNLookAndFeel::getValueFont());
    valueLabel.setColour (juce::Label::textColourId, CERNLookAndFeel::accentBrightColour);
    valueLabel.setInterceptsMouseClicks (false, false);
    addAndMakeVisible (valueLabel);

    slider.onValueChange = [this] { updateValueText(); };
    updateValueText();
}

CERNFader::~CERNFader() = default;

//==============================================================================
void CERNFader::updateValueText()
{
    auto* parameter = apvts.getParameter (parameterId);

    if (parameter == nullptr)
        return;

    const float normalisedValue = parameter->getValue();

    if (normalisedValue == lastNormalisedValue)
        return;

    lastNormalisedValue = normalisedValue;

    auto text = parameter->getText (normalisedValue, 8);

    if (text.isEmpty())
        text = juce::String (slider.getValue(), 2);

    if (text != lastValueText)
    {
        lastValueText = text;
        valueLabel.setText (text, juce::dontSendNotification);
        setTooltip (nameLabel.getText() + ": " + text);
    }
}

void CERNFader::refresh()
{
    updateValueText();
}

void CERNFader::paint (juce::Graphics& g)
{
    // Dezenter Sockel hinter dem Fader: gibt dem Regler einen Hardware-Rahmen.
    const auto area = getLocalBounds().toFloat();
    CERNLookAndFeel::drawWell (g, area.reduced (nameHeight + 2.0f, 2.0f), 4.0f);
}

void CERNFader::resized()
{
    auto area = getLocalBounds();

    nameLabel.setBounds (area.removeFromTop (nameHeight));
    valueLabel.setBounds (area.removeFromBottom (valueHeight));

    const int faderWidth = juce::jmin (area.getWidth() - 10, 34);
    const int x = area.getX() + (area.getWidth() - faderWidth) / 2;
    slider.setBounds (x, area.getY() + 2, faderWidth, juce::jmax (0, area.getHeight() - 4));
}
