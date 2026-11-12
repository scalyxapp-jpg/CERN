#include "StandaloneApp.h"
#include "GUI/AudioDeviceDialog.h"

//==============================================================================
// Eigenes Fenster: das Schliessen des Fensters beendet die App.
// (juce::DocumentWindow macht beim Standard-closeButtonPressed() NICHTS ->
//  das X wirkte vorher nur als Assert und die App blieb laufen.)
class StandaloneWindow : public juce::DocumentWindow
{
public:
    StandaloneWindow (const juce::String& name, juce::Colour backgroundColour)
        : juce::DocumentWindow (name, backgroundColour, juce::DocumentWindow::allButtons) {}

    void closeButtonPressed() override
    {
        juce::JUCEApplicationBase::quit();
    }

    bool keyPressed (const juce::KeyPress& key) override
    {
        const bool cmd = key.getModifiers().isCommandDown();

        if (cmd && (key.getKeyCode() == 'Q' || key.getKeyCode() == 'q'))
        {
            juce::JUCEApplicationBase::quit();
            return true;
        }

        return juce::DocumentWindow::keyPressed (key);
    }
};

//==============================================================================
// Standalone-State: APVTS + Patch sind engine-relative; Audio-Device bleibt separat.
static juce::File getStandaloneStateFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
        .getChildFile ("C.E.R.N")
        .getChildFile ("standalone_state.xml");
}

// Autosave-Intervall: rettet die Session auch bei einem Crash.
static constexpr int autosaveIntervalMs = 30000;

void CERNStandaloneApp::initialise (const juce::String& /*commandLine*/)
{
    // Audio-Device-Manager initialisieren (0 Eingaenge, 2 Ausgaenge).
    // Gespeicherte Einstellungen (Geraet, Sample-Rate, Buffer) aus der User-Config
    // laden; falls das gespeicherte Geraet nicht mehr da ist, faellt JUCE auf den
    // Default zurueck (selectDefaultDeviceOnFailure = true).
    juce::AudioDeviceManager::AudioDeviceSetup setup;
    auto savedState = CERNAudioSettings::loadSettingsXml();
    deviceManager.initialise (0, 2, savedState.get(), true, juce::String(), &setup);

    // Prozessor erstellen. Das Vorbereiten uebernimmt der AudioProcessorPlayer
    // mit den tatsaechlichen Geraeteeinstellungen (kein manuelles prepareToPlay).
    processor = std::make_unique<CERNAudioProcessor>();

    // WICHTIG: AudioProcessorPlayer als Callback registrieren.
    // Ohne addAudioCallback laeuft processBlock nie -> kein Ton, Sequencer tot.
    // setProcessor() vor addAudioCallback(): das Geraet ist bereits offen, der
    // Player wird beim Callback-Add einmalig mit dessen Settings vorbereitet.
    processorPlayer.setProcessor (processor.get());
    deviceManager.addAudioCallback (&processorPlayer);

    // MIDI-Eingaenge aktivieren: alle vorhandenen Geraete einschalten und den
    // AudioProcessorPlayer als Callback fuer alle aktivierten Eingaenge
    // registrieren (leerer Identifier = alle). Ohne das reagiert der
    // Standalone nicht auf MIDI-Noten.
    for (const auto& midiDevice : juce::MidiInput::getAvailableDevices())
        deviceManager.setMidiInputDeviceEnabled (midiDevice.identifier, true);

    deviceManager.addMidiInputDeviceCallback ({}, &processorPlayer);

    // Kein Geraet ist kein Crash: ohne Ausgabe laeuft die App weiter (Editor
    // bleibt bedienbar), es kommt nur kein Ton. Fuer diesen Fall einmalig mit
    // plausiblen Ersatzwerten vorbereiten (setup.* kann 0 sein).
    if (deviceManager.getCurrentAudioDevice() == nullptr)
    {
        const double fallbackSampleRate = (setup.sampleRate >= 8000.0) ? setup.sampleRate : 48000.0;
        const int    fallbackBufferSize = juce::jlimit (256, 512,
                                                        setup.bufferSize > 0 ? setup.bufferSize : 512);
        processor->prepareToPlay (fallbackSampleRate, fallbackBufferSize);

        DBG ("Kein Audio-Ausgabegeraet verfuegbar - Standalone laeuft ohne Audio ("
             << fallbackSampleRate << " Hz / " << fallbackBufferSize << " Samples).");
    }

    // Standalone-State (Parameter + Patch) erst NACH setProcessor/Prepare laden,
    // damit ein etwaiges reset() beim Vorbereiten den Patch nicht ueberschreibt.
    const auto stateFile = getStandaloneStateFile();

    if (stateFile.existsAsFile())
    {
        const auto xml = stateFile.loadFileAsString();

        // Korrupte/unlesbare Datei: Factory-State behalten, einmal loggen,
        // Start nicht abbrechen.
        if (xml.isEmpty() || ! processor->getPresetManager().setCurrentStateXml (xml))
            DBG ("Standalone-State konnte nicht geladen werden: " << stateFile.getFullPathName());
    }

    // Editor erstellen (mit DeviceManager -> "AUDIO"-Button im Header)
    editor = std::make_unique<CERNAudioProcessorEditor> (*processor, &deviceManager);

    // Fenster erstellen
    window = std::make_unique<StandaloneWindow> (
        "C.E.R.N Standalone",
        juce::Desktop::getInstance().getDefaultLookAndFeel()
            .findColour (juce::ResizableWindow::backgroundColourId));

    window->setContentOwned (editor.release(), true);
    window->setResizable (true, false);
    window->setResizeLimits (700, 500, 1600, 1000);
    window->centreWithSize (1100, 750);
    window->setVisible (true);

    // Autosave: alle 30 s Parameter + Patch sichern (gleiche Datei wie shutdown).
    startTimer (autosaveIntervalMs);
}

void CERNStandaloneApp::timerCallback()
{
    saveStateToFile();
}

void CERNStandaloneApp::saveStateToFile()
{
    if (processor == nullptr)
        return;

    const auto xml = processor->getPresetManager().getCurrentStateXml();

    if (xml.isEmpty())
        return;

    const auto stateFile = getStandaloneStateFile();
    stateFile.getParentDirectory().createDirectory();

    // Fehler nur einmal loggen, sonst spammt jeder Tick das Log.
    if (! stateFile.replaceWithText (xml) && ! stateSaveErrorLogged)
    {
        stateSaveErrorLogged = true;
        DBG ("Standalone-State konnte nicht gespeichert werden: " << stateFile.getFullPathName());
    }
}

void CERNStandaloneApp::shutdown()
{
    // Timer zuerst stoppen: kein Autosave mehr waehrend des Teardowns.
    stopTimer();

    // Standalone-State (Parameter + Patch) vor dem Audio-Teardown sichern
    saveStateToFile();

    // Audio-Settings sichern (falls zwischenzeitlich noch nicht gespeichert)
    CERNAudioSettings::saveSettings (deviceManager);

    // Callbacks genau einmal abmelden, sonst greift der Audio-Callback ggf. auf
    // den bereits zerstoerten Prozessor zu. setProcessor(nullptr) gibt die
    // Prozessor-Ressourcen frei.
    deviceManager.removeMidiInputDeviceCallback ({}, &processorPlayer);
    deviceManager.removeAudioCallback (&processorPlayer);
    processorPlayer.setProcessor (nullptr);

    window = nullptr;
    editor = nullptr;
    processor = nullptr;

    deviceManager.closeAudioDevice();
}

void CERNStandaloneApp::systemRequestedQuit()
{
    juce::JUCEApplicationBase::quit();
}

void CERNStandaloneApp::anotherInstanceStarted (const juce::String& /*commandLine*/)
{
}

void CERNStandaloneApp::suspended()
{
}

void CERNStandaloneApp::resumed()
{
}

void CERNStandaloneApp::unhandledException (const std::exception* /*exception*/,
                                                  const juce::String& /*sourceFilename*/,
                                                  int /*lineNumber*/)
{
}

//==============================================================================
START_JUCE_APPLICATION (CERNStandaloneApp)
