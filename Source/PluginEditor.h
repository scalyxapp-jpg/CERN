#pragma once

#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "GUI/MainComponent.h"

//==============================================================================
/** CERN Plugin Editor.
 *
 *  Hostet die MainComponent und bietet das VST3-Editor-Fenster.
 */
class CERNAudioProcessorEditor : public juce::AudioProcessorEditor
{
public:
    // deviceManagerToShow: nur der Standalone übergibt einen DeviceManager,
    // damit der Editor den "AUDIO"-Button zeigen kann (im VST3 ist er null).
    explicit CERNAudioProcessorEditor (CERNAudioProcessor&,
                                             juce::AudioDeviceManager* deviceManagerToShow = nullptr);
    ~CERNAudioProcessorEditor() override;

    void paint (juce::Graphics&) override;
    void resized() override;

private:
    CERNAudioProcessor& processor;
    MainComponent mainComponent;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CERNAudioProcessorEditor)
};