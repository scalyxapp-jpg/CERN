#pragma once

#include <juce_audio_devices/juce_audio_devices.h>
#include <juce_audio_utils/juce_audio_utils.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include "GUI/Theme/CERNLookAndFeel.h"

//==============================================================================
/** Audio-Geräteeinstellungen für den Standalone-Build.

    Persistiert die Auswahl (Gerät, Sample-Rate, Buffer-Grösse, Kanäle) in
    ~/.config/C.E.R.N/audio_settings.xml und stellt sie als modalen Dialog
    (juce::AudioDeviceSelectorComponent) zur Verfügung.

    Wird nur im Standalone genutzt: im VST3-Plugin gibt es kein
    AudioDeviceManager, der Header kompiliert dort aber problemlos
    (showDialog/saveSettings werden dann schlicht nicht aufgerufen).
 */
namespace CERNAudioSettings
{
    /** Pfad der Settings-Datei. */
    inline juce::File getSettingsFile()
    {
        return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
                   .getChildFile ("C.E.R.N")
                   .getChildFile ("audio_settings.xml");
    }

    /** Gespeicherte Settings laden (nullptr wenn nicht vorhanden/ungültig). */
    inline std::unique_ptr<juce::XmlElement> loadSettingsXml()
    {
        const auto file = getSettingsFile();

        if (! file.existsAsFile())
            return nullptr;

        return std::unique_ptr<juce::XmlElement> (juce::parseXML (file));
    }

    /** XML aus dem aktuellen Gerätezustand bauen (Fallback, wenn JUCE noch
        kein explizites Setup gespeichert hat - z. B. erster Start ohne
        jemals geänderte Einstellung). Format kompatibel zu
        AudioDeviceManager::initialise() / DEVICESETUP. */
    inline std::unique_ptr<juce::XmlElement> stateXmlFromCurrentSetup (juce::AudioDeviceManager& deviceManager)
    {
        const auto setup = deviceManager.getAudioDeviceSetup();

        auto xml = std::make_unique<juce::XmlElement> ("DEVICESETUP");

        xml->setAttribute ("deviceType",            deviceManager.getCurrentAudioDeviceType());
        xml->setAttribute ("audioOutputDeviceName", setup.outputDeviceName);
        xml->setAttribute ("audioInputDeviceName",  setup.inputDeviceName);
        xml->setAttribute ("audioDeviceRate",       setup.sampleRate);
        xml->setAttribute ("audioDeviceBufferSize", setup.bufferSize);

        if (! setup.useDefaultInputChannels)
            xml->setAttribute ("audioDeviceInChans",  setup.inputChannels.toString (2));

        if (! setup.useDefaultOutputChannels)
            xml->setAttribute ("audioDeviceOutChans", setup.outputChannels.toString (2));

        return xml;
    }

    /** Aktuellen Gerätezustand dauerhaft speichern. */
    inline bool saveSettings (juce::AudioDeviceManager& deviceManager)
    {
        const auto file = getSettingsFile();
        file.getParentDirectory().createDirectory();

        std::unique_ptr<juce::XmlElement> xml (deviceManager.createStateXml());

        if (xml == nullptr)
            xml = stateXmlFromCurrentSetup (deviceManager);

        return file.replaceWithText (xml->toString());
    }

    /** Modal ausgeführtes Einstellungsfenster (Gerät, Sample-Rate, Buffer). */
    inline void showDialog (juce::AudioDeviceManager& deviceManager);

    //==============================================================================
    /** Interner Container: speichert die Settings beim Schliessen (Destruktor). */
    class SettingsContent : public juce::Component
    {
    public:
        explicit SettingsContent (juce::AudioDeviceManager& deviceManagerToUse)
            : deviceManager (deviceManagerToUse),
              selector (deviceManagerToUse,
                        0, 0,     // keine Audio-Eingänge nötig
                        2, 2,     // Stereo-Ausgang
                        false,    // keine MIDI-Input-Optionen
                        false,    // keine MIDI-Output-Auswahl
                        true,     // Kanäle als Stereo-Paare
                        false)    // Sample-Rate + Buffer immer sichtbar (kein versteckter Button)
        {
            static CERNLookAndFeel lookAndFeel;
            setLookAndFeel (&lookAndFeel);

            addAndMakeVisible (selector);
            setSize (600, 620);
        }

        ~SettingsContent() override
        {
            // Einstellungen auch dann persistent machen, wenn das Fenster
            // per X / Alt+F4 / Escape geschlossen wird.
            CERNAudioSettings::saveSettings (deviceManager);
            setLookAndFeel (nullptr);
        }

        void paint (juce::Graphics& g) override
        {
            g.fillAll (CERNLookAndFeel::panelColour);
        }

        void resized() override
        {
            selector.setBounds (getLocalBounds().reduced (10));
        }

    private:
        juce::AudioDeviceManager& deviceManager;
        juce::AudioDeviceSelectorComponent selector;

        JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (SettingsContent)
    };

    inline void showDialog (juce::AudioDeviceManager& deviceManager)
    {
        juce::DialogWindow::LaunchOptions options;

        options.content.setOwned (new SettingsContent (deviceManager));
        options.dialogTitle                 = "C.E.R.N - Audio Setup";
        options.dialogBackgroundColour      = CERNLookAndFeel::panelColour;
        options.escapeKeyTriggersCloseButton = true;
        options.useNativeTitleBar           = true;
        options.resizable                   = false;

        options.launchAsync();
    }
}
