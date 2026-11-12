#include "PluginEditor.h"

//==============================================================================
CERNAudioProcessorEditor::CERNAudioProcessorEditor (CERNAudioProcessor& p,
                                                                juce::AudioDeviceManager* deviceManagerToShow)
    : AudioProcessorEditor (&p), processor (p), mainComponent (p, deviceManagerToShow)
{
    addAndMakeVisible (mainComponent);
    setSize (1100, 750);
}

CERNAudioProcessorEditor::~CERNAudioProcessorEditor() = default;

void CERNAudioProcessorEditor::paint (juce::Graphics& g)
{
    g.fillAll (juce::Colour (0xff1a0a0a));
}

void CERNAudioProcessorEditor::resized()
{
    mainComponent.setBounds (getLocalBounds());
}