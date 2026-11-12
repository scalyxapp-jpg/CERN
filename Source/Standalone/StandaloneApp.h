#pragma once

#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_events/juce_events.h>
#include <juce_gui_basics/juce_gui_basics.h>
#include "PluginProcessor.h"
#include "PluginEditor.h"

//==============================================================================
/** CERN Standalone-Anwendung. */
class CERNStandaloneApp : public juce::JUCEApplicationBase,
                                private juce::Timer
{
public:
    CERNStandaloneApp() = default;

    const juce::String getApplicationName() override       { return "C.E.R.N Standalone"; }
    const juce::String getApplicationVersion() override    { return "1.0.0"; }
    bool moreThanOneInstanceAllowed() override             { return true; }

    void initialise (const juce::String& commandLine) override;
    void shutdown() override;
    void anotherInstanceStarted (const juce::String& commandLine) override;
    void systemRequestedQuit() override;
    void suspended() override;
    void resumed() override;
    void unhandledException (const std::exception* exception,
                             const juce::String& sourceFilename,
                             int lineNumber) override;

private:
    void timerCallback() override;

    /** Sichert Parameter + Patch in die Standalone-State-Datei (still, ohne Log-Spam). */
    void saveStateToFile();

    std::unique_ptr<juce::DocumentWindow> window;
    std::unique_ptr<CERNAudioProcessor> processor;
    std::unique_ptr<CERNAudioProcessorEditor> editor;
    juce::AudioDeviceManager deviceManager;
    juce::AudioProcessorPlayer processorPlayer;
    bool stateSaveErrorLogged = false;
};
