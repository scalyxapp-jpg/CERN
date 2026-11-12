#pragma once

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters/ParameterIDs.h"
#include "DSP/PatchGraph.h"
#include "DSP/ParameterPatchGraph.h"

//==============================================================================
/** PresetManager gemäß §10.
 *
 *  Factory-Presets (eingebettete XML-Snapshots) + User-Presets (XML-Datei).
 *  Load/Save laufen über den APVTS-State; der modulare Audio- und Parameter-Patch
 *  werden zusätzlich als <Patch>- und <ParameterPatch>-Elemente im Preset/State
 *  gespeichert und beim Laden wiederhergestellt.
 */
class PresetManager
{
public:
    /** Ein Eintrag des Preset-Browsers (SND-165).
     *
     *  `category` gruppiert die Einträge im Browser: "FACTORY" für Init und die
     *  eingebetteten Presets, "USER" für User-Dateien im Basisordner und sonst
     *  der Name des Unterordners (Ordner-Browser).
     */
    struct Entry
    {
        enum class Kind { init, factory, user };

        Kind kind { Kind::factory };
        juce::String name;
        juce::String category;
        int factoryIndex { -1 };
        juce::File file;
    };

    explicit PresetManager (juce::AudioProcessorValueTreeState& apvts,
                            PatchGraph& patchGraph,
                            ParameterPatchGraph& parameterPatchGraph);

    void prepare (double sampleRate);

    // Factory-Presets laden (von Index)
    bool loadFactory (int index);

    /** Neutrales Init-Preset (Preset 0, SND-166): Parameter-Defaults,
        Default-Audio-Patch, keine Parameter-CV-Kabel. */
    bool loadInit();

    // User-Preset laden/save (Datei)
    bool loadUser (const juce::File& file);
    bool saveUser (const juce::File& file) const;

    /** Ordner der User-Presets (Save-As-Ziel, Quelle des Browsers). */
    static juce::File getUserPresetDirectory();

    /** Alter Speicherort von SND-164 (user-preset.xml im C.E.R.N-Ordner). */
    static juce::File getLegacyUserPresetFile();

    /** Alle Browser-Einträge: Init, Factory, User-Dateien (inkl. Unterordner).
        Liest das Dateisystem; nur vom Message-Thread aufrufen. */
    juce::Array<Entry> getPresetEntries() const;

    /** Löscht eine User-Preset-Datei innerhalb des Preset-Ordners. */
    bool deleteUserPreset (const juce::File& file) const;

    // Liste der Factory-Preset-Namen (ohne Init, Index-kompatibel)
    juce::StringArray getFactoryPresetNames() const;
    int getNumFactoryPresets() const;

    // Aktuelles Preset speichern/laden als XML-String (SND-167 A/B)
    juce::String getCurrentStateXml() const;
    bool setCurrentStateXml (const juce::String& xml);

private:
    juce::AudioProcessorValueTreeState& apvts;
    PatchGraph& patch;
    ParameterPatchGraph& parameterPatch;

    /** Schützt die eingebetteten Preset-Listen; alle Zugriffe laufen vom
        Message-Thread (kein Audio-Thread-Zugriff). */
    mutable juce::CriticalSection lock;

    juce::StringArray factoryPresetNames;
    juce::StringArray factoryPresetXml; // eingebettete XML-Snapshots
    juce::String initPresetXml;         // neutrales Init-Preset (SND-166)

    void initFactoryPresets();
    void buildInitPreset();

    static void addUserEntries (juce::Array<Entry>& entries, const juce::File& directory,
                                const juce::String& category);

    /** Setzt alle 12 Module-Enable-Parameter im kopierten State auf `enabled`. */
    static void applyModuleEnableDefaults (juce::ValueTree& state, bool enabled);

    bool applyXml (const juce::XmlElement& xml);
};
