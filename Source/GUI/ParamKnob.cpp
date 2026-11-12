#include "ParamKnob.h"
#include "GUI/Theme/CERNLookAndFeel.h"
#include "Parameters/ParameterIDs.h"

#include <algorithm>
#include <array>
#include <cmath>

//==============================================================================
// Scale-Snap (SND-132): identisch zur Engine-Logik in KickSynthesisEngine.cpp
// (dort datei-lokal; GUI braucht dieselbe Abbildung für die Note+Cents-Anzeige).
// Nur die Note-Logik, keine Frequenz-Kurve: f0 -> nächster Skalen-Ton,
// Step-Offsets relativ zur gesnappten f0-Note.
namespace
{
constexpr std::array<int, 7> naturalMinorPitchClasses { 0, 2, 3, 5, 7, 8, 10 };

bool isPitchClassInScale (int pitchClass, int keyRoot, int scaleIndex)
{
    if (scaleIndex != 1) // 0 = Chromatisch
        return true;

    const int relative = ((pitchClass - keyRoot) % 12 + 12) % 12;
    return std::find (naturalMinorPitchClasses.begin(),
                      naturalMinorPitchClasses.end(), relative)
           != naturalMinorPitchClasses.end();
}

/** Naechstgelegene Note der Scale (bei Gleichstand abwaerts). */
int snapNoteToScale (int noteNumber, int keyRoot, int scaleIndex)
{
    if (isPitchClassInScale (noteNumber, keyRoot, scaleIndex))
        return noteNumber;

    for (int distance = 1; distance <= 6; ++distance)
    {
        if (isPitchClassInScale (noteNumber - distance, keyRoot, scaleIndex))
            return noteNumber - distance;

        if (isPitchClassInScale (noteNumber + distance, keyRoot, scaleIndex))
            return noteNumber + distance;
    }

    return noteNumber;
}

float snapFundamentalToScale (float hz, int keyRoot, int scaleIndex)
{
    if (scaleIndex != 1 || hz <= 0.0f)
        return hz;

    const float note = 69.0f + 12.0f * std::log2 (hz / 440.0f);
    const int snapped = snapNoteToScale ((int) std::lround (note), keyRoot, scaleIndex);
    return 440.0f * std::pow (2.0f, (float) (snapped - 69) / 12.0f);
}
} // namespace

namespace TuningDisplay
{
juce::String noteNameWithCents (float hz)
{
    if (! std::isfinite (hz) || hz <= 0.0f)
        return "-";

    const float midiNote = 69.0f + 12.0f * std::log2 (hz / 440.0f);
    int nearest = (int) std::lround (midiNote);
    int cents = (int) std::lround ((midiNote - (float) nearest) * 100.0f);

    if (cents >= 100)
    {
        ++nearest;
        cents -= 100;
    }
    else if (cents <= -100)
    {
        --nearest;
        cents += 100;
    }

    const auto name = juce::MidiMessage::getMidiNoteName (nearest, true, true, 4);

    if (cents == 0)
        return name;

    // Unicode-Minus (U+2212) wie im SOUND_REVIEW-Beispiel "F#1 −3 ct".
    const auto sign = juce::String::fromUTF8 (cents > 0 ? " +" : " \xe2\x88\x92");
    return name + sign + juce::String (std::abs (cents)) + " ct";
}

bool isPitchedParameter (const juce::String& parameterId)
{
    return parameterId == ParameterIDs::fundamentalHz
        || parameterId.startsWith (ParameterIDs::stepPrefixPitch);
}

juce::String pitchedValueText (juce::AudioProcessorValueTreeState& apvts,
                               const juce::String& parameterId)
{
    const auto* fundamentalParam = apvts.getRawParameterValue (ParameterIDs::fundamentalHz);

    if (fundamentalParam == nullptr)
        return {};

    const float fundamentalHz = fundamentalParam->load();

    // Key/Scale (SND-132) lesen: die Anzeige zeigt den gesnappten Wert.
    int keyRoot = 0;
    int scale   = 0;

    if (const auto* keyRootParam = apvts.getRawParameterValue (ParameterIDs::keyRoot))
        keyRoot = juce::jlimit (0, 11, (int) std::lround (keyRootParam->load()));

    if (const auto* scaleParam = apvts.getRawParameterValue (ParameterIDs::scale))
        scale = juce::jlimit (0, 1, (int) std::lround (scaleParam->load()));

    const float snappedF0 = snapFundamentalToScale (fundamentalHz, keyRoot, scale);

    if (parameterId == ParameterIDs::fundamentalHz)
        return noteNameWithCents (snappedF0);

    if (! parameterId.startsWith (ParameterIDs::stepPrefixPitch))
        return {};

    const auto* stepParam = apvts.getRawParameterValue (parameterId);

    if (stepParam == nullptr)
        return {};

    const float semitones = stepParam->load();
    const int baseNote = (int) std::lround (69.0f + 12.0f * std::log2 (juce::jmax (1.0e-6f, snappedF0) / 440.0f));
    const int snappedNote = snapNoteToScale (baseNote + (int) std::lround (semitones),
                                             keyRoot, scale);
    return noteNameWithCents (440.0f * std::pow (2.0f, (float) (snappedNote - 69) / 12.0f));
}
} // namespace TuningDisplay

//==============================================================================
ParamKnob::ParamKnob (juce::AudioProcessorValueTreeState& apvtsRef,
                      const juce::String& parameterIdIn,
                      const juce::String& displayNameIn)
    : apvts (apvtsRef), parameterId (parameterIdIn), displayName (displayNameIn)
{
    slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
    slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
    slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                juce::MathConstants<float>::pi * 2.75f,
                                true);
    slider.setMouseDragSensitivity (160);
    addAndMakeVisible (slider);

    attachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, parameterId, slider);

    setTitle (displayName);
    setDescription (displayName + " parameter");
    slider.setTitle (displayName);
    slider.setName (displayName);
    slider.setDescription (displayName + " parameter");

    nameLabel.setText (displayName, juce::dontSendNotification);
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

ParamKnob::~ParamKnob() = default;

void ParamKnob::updateValueText()
{
    // getValue() liefert den normalisierten Wert; AudioProcessorParameter::getText
    // erwartet ebenfalls einen normalisierten Wert (JUCE 9).
    auto* parameter = apvts.getParameter (parameterId);
    const float normalisedValue = parameter != nullptr ? parameter->getValue() : -1.0f;
    const bool pitched = TuningDisplay::isPitchedParameter (parameterId);

    if (! pitched && parameter != nullptr && normalisedValue == lastNormalisedValue)
        return;

    lastNormalisedValue = normalisedValue;

    juce::String text;

    if (pitched)
        text = TuningDisplay::pitchedValueText (apvts, parameterId);

    if (text.isEmpty() && parameter != nullptr)
        text = parameter->getText (normalisedValue, 8);

    if (text.isEmpty())
        text = juce::String (slider.getValue(), 2);

    if (text != lastValueText)
    {
        lastValueText = text;
        valueLabel.setText (text, juce::dontSendNotification);
    }

    const juce::String tooltip = displayName + ": " + text;

    if (tooltip != lastTooltip)
    {
        lastTooltip = tooltip;
        setTooltip (tooltip);
        slider.setTooltip (tooltip);
    }
}

void ParamKnob::refresh()
{
    updateValueText();
}

void ParamKnob::paint (juce::Graphics& g)
{
    // Wert-Chip: versenkte Anzeige unter dem Regler (Display-Look).
    const auto chip = juce::Rectangle<float> (0.0f, (float) (getHeight() - valueHeight),
                                              (float) getWidth(), (float) valueHeight);

    if (chip.getWidth() < 4.0f)
        return;

    CERNLookAndFeel::drawWell (g, chip.reduced (2.0f, 1.0f), 3.0f);

    // Feine rote Statuslinie links am Chip.
    g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.75f));
    g.fillRoundedRectangle (chip.getX() + 3.0f, chip.getY() + 3.0f,
                            2.0f, juce::jmax (0.0f, chip.getHeight() - 6.0f), 1.0f);
}

void ParamKnob::resized()
{
    auto area = getLocalBounds();

    nameLabel.setBounds (area.removeFromTop (nameHeight));
    valueLabel.setBounds (area.removeFromBottom (valueHeight));

    const int side = juce::jmin (area.getWidth(), area.getHeight());
    const int x = area.getX() + (area.getWidth() - side) / 2;
    const int y = area.getY() + (area.getHeight() - side) / 2;
    slider.setBounds (x, y, side, side);
}
