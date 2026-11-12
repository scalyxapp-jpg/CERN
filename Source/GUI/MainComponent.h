#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>
#include <juce_gui_basics/juce_gui_basics.h>

#include <array>
#include <functional>
#include <memory>
#include <vector>

#include "GUI/EnvelopeEditor.h"
#include "GUI/HiggsField3D.h"
#include "GUI/LogoComponent.h"
#include "GUI/MixerView.h"
#include "GUI/PatchView.h"
#include "GUI/SectionPanel.h"
#include "GUI/SpectrumAnalyzer.h"
#include "GUI/WaveformDisplay.h"
#include "PluginProcessor.h"

//==============================================================================
/** Haupt-GUI-Komponente des CERN-Editors.

 *  Layout:
 *    - HeaderBar (66 px): Plugin-Name, PATCH (inkl. Kabel-LED), Trigger,
 *      Audio-Setup (nur Standalone), Preset-Auswahl, Save/Load
 *    - MacroStrip (76 px): 5 Performance-Makros TUNE/PUNCH/BOOM/DIRT/SPACE
 *    - Linke Spalte (scrollbar): 18 Sektionen mit dynamischer Hoehe
 *    - Rechte Spalte: Waveform + Spectrum oben, Pitch Envelope (links) + Higgs (66 %) unten,
 *      Meter-Strip (LUFS + GR-History, SND-170) unten ueber die volle Breite
 *    - FooterSequencer (216 px): Transport, BPM/Swing/Steps, 16 Gates
 *      + 16 Velocities + 16 Step-Pitch-Slider
 *
 *  Shortcuts (SND-173): Space = Play/Stop, T = Trigger, Left/Right = Preset.
 *  Undo/Redo (SND-168): Ctrl+Z, Ctrl+Shift+Z bzw. Ctrl+Y.
 *  Texteingaben (z. B. BPM) behalten ihre Tasten.
 */
class MainComponent : public juce::Component, public juce::Timer
{
public:
    MainComponent (CERNAudioProcessor& processor,
                   juce::AudioDeviceManager* audioDeviceManager = nullptr);
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    bool keyPressed (const juce::KeyPress& key) override;

    /** GUI-Thread: liest die atomaren Display-Buffers und aktualisiert Displays. */
    void timerCallback() override;

private:
    class HeaderBar;
    class FooterSequencer;
    class MacroPanel;
    class MeterDisplay;

    void addSection (const juce::String& title, const std::vector<juce::String>& paramIds,
                     const std::vector<juce::String>& enableIds = {});
    void updateDisplays();
    float rawValue (const juce::String& parameterId);
    void setPatchMode (bool on);

    /** Front-Ansicht / Patchbay / Mixer (Tab-Umschaltung im Header). */
    enum class ViewMode { front, patchbay, mixer };
    void setViewMode (ViewMode mode);
    ViewMode viewMode { ViewMode::front };

    /** SND-168: Undo/Redo ueber den UndoManager des APVTS. */
    void performUndo();
    void performRedo();

    CERNAudioProcessor& processor;
    juce::AudioDeviceManager* audioDeviceManager;

    std::unique_ptr<HeaderBar> header;
    std::unique_ptr<FooterSequencer> footer;
    std::unique_ptr<MacroPanel> macroPanel;
    std::unique_ptr<MeterDisplay> meterDisplay;

    std::vector<std::unique_ptr<SectionPanel>> sections;

    WaveformDisplay waveformDisplay;
    SpectrumAnalyzer spectrumAnalyzer;
    EnvelopeEditor envelopeEditor;
    std::unique_ptr<HiggsField3D> higgsField;

    /** Zielpuffer fuer copyGainReductionHistory (GUI-Thread, vorallokiert). */
    std::array<float, (size_t) CERNAudioProcessor::DisplayBuffers::grHistorySize>
        grHistoryScratch {};

    juce::Viewport sectionViewport;
    juce::Component sectionContainer;

    std::unique_ptr<PatchView> patchView;
    std::unique_ptr<MixerView> mixerView;
    bool patchMode { false };

    // FFT fuer den Spectrum-Analyzer
    static constexpr int fftOrder = 12;
    static constexpr int fftSize  = 1 << fftOrder;

    juce::dsp::FFT fft { fftOrder };
    juce::dsp::WindowingFunction<float> fftWindow {
        (size_t) fftSize, juce::dsp::WindowingFunction<float>::hann, true };

    std::vector<float> fftBuffer;
    std::vector<float> spectrumBars;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};

//==============================================================================
/** Meter-Strip (SND-170): Kurzzeit-LUFS und GR-History.

    Zeigt den 400-ms-LUFS-Wert (BS.1770) und den Verlauf der
    Kompressor-Gain-Reduction (0 dB oben, 24 dB Reduktion unten).
    Alle Anzeige-Strings und Pfade werden ausserhalb von paint() aufgebaut;
    paint() selbst allokiert nichts.
*/
class MainComponent::MeterDisplay : public juce::Component,
                                    public juce::SettableTooltipClient
{
public:
    MeterDisplay();

    void paint (juce::Graphics& g) override;
    void resized() override;

    /** Aktualisiert den LUFS-Wert (nur bei Aenderung -> repaint). */
    void setShortTermLufs (float lufs);

    /** Aktualisiert den GR-Verlauf (aeltester -> neuester Wert). */
    void setGainReductionHistory (const float* values, int numValues);

    /** Kurze Statusmeldung (z. B. "UNDO") fuer ~1.6 s. */
    void showStatus (const juce::String& message);

    /** Blendet eine abgelaufene Statusmeldung aus (GUI-Timer). */
    void tickStatus (double nowMs);

private:
    static constexpr int maxHistory =
        CERNAudioProcessor::DisplayBuffers::grHistorySize;
    static constexpr float minGrDb = 0.0f;   ///< keine Reduktion (oben)
    static constexpr float maxGrDb = 24.0f;  ///< maximale dargestellte Reduktion (unten)

    void rebuildGraphPath();

    juce::Rectangle<float> graphArea;
    juce::Path graphPath;
    juce::Path graphFill;

    std::array<float, (size_t) maxHistory> history {};
    int numHistory { 0 };

    float currentLufs { 1.0f };  // unmoeglicher Startwert -> erster Aufruf formatiert
    float currentGr { 0.0f };
    juce::String lufsText { "-inf" };
    juce::String grText { "0.0 dB" };
    juce::String statusText;
    juce::String hintText { "CTRL+Z UNDO  /  CTRL+SHIFT+Z REDO" };
    juce::String hintTextShort { "CTRL+Z / CTRL+Y" };
    double statusUntilMs { 0.0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MeterDisplay)
};

//==============================================================================
/** HeaderBar: Plugin-Name, PATCH-Umschalter (mit Kabel-LED), Trigger-Button,
    Preset-Auswahl (inkl. Modified-Marker), Save/Load,
    optionaler Audio-Setup-Button (nur Standalone),
    RND (SND-184), RENDER (SND-183) und MORPH-Regler (SND-185). */
class MainComponent::HeaderBar : public juce::Component,
                                 private juce::AudioProcessorValueTreeState::Listener
{
public:
    explicit HeaderBar (CERNAudioProcessor& p,
                        juce::AudioDeviceManager* deviceManager = nullptr);
    ~HeaderBar() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    std::function<void()> onPatchToggle;
    std::function<void()> onMixerToggle;
    void setPatchActive (bool active);

    /** Mixer-Tab aktiv? Dann zeigt der Button "FRONT". */
    void setMixerActive (bool active);

    /** MORPH-Regler sichtbar (beide A/B-Slots belegt)? */
    bool isMorphVisible() const { return morphSlider.isVisible(); }

    /** Zweizeilige Kopfzeile: bei schmalen Fenstern oder wenn MORPH bei
        mittleren Breiten sonst die Preset-Combo abschneiden wuerde. */
    static bool useTwoRows (int width, bool morphVisible)
    {
        return width < 980 || (morphVisible && width < 1160);
    }

    /** Anzahl aktiver Kabel (Patch + Parameter-Patch) fuer die Front-LED. */
    void setCableCount (int count);

    /** Shortcut-Ziele (SND-173): Trigger und Preset vor/zurueck. */
    void trigger();
    void selectRelativePreset (int delta);

    /** Statusmeldung an den Meter-Strip (z. B. "RANDOMIZED (tuning safe)"). */
    std::function<void(const juce::String&)> onStatus;

private:
    /** Kleine LED, die aktive Kabel auf der Frontseite signalisiert (SND-158). */
    class CableLed : public juce::Component,
                     public juce::SettableTooltipClient
    {
    public:
        void setCableCount (int count);
        void paint (juce::Graphics& g) override;

    private:
        int cableCount { 0 };
        juce::String labelText { "CV 0" };
    };

    void triggerClicked();
    void presetChanged();
    void savePresetAs();
    void savePresetFile (const juce::File& file);
    void writePreset (const juce::File& file);
    void deletePreset();
    void loadPresetFromDisk();

    void loadUserPreset (const juce::File& file);
    void loadPresetEntry (const PresetManager::Entry& entry);

    /** Baut die Combo aus PresetManager::getPresetEntries() neu auf (SND-165). */
    void rebuildPresetList();
    int findPresetItemId (const PresetManager::Entry& entry) const;
    void setCurrentEntry (const PresetManager::Entry& entry);
    void applyPresetDisplayText();

    /** A/B (SND-167): zwei vollständige States inkl. Patches. */
    void toggleAB();
    void copyAB();
    void updateABDisplay();

    /** SND-184: konservative Zufallsvariation (Pitch-Parameter unveraendert). */
    void randomizeSound();

    /** SND-183: einen Takt offline als WAV nach ~/Documents rendern. */
    void renderBounce();

    /** SND-185: Interpoliert A/B (0..100 %) live in die APVTS. */
    void applyMorph (float morphPercent);

    /** Blendet den Morph-Regler nur ein, wenn beide A/B-Slots belegt sind. */
    void updateMorphVisibility();

    /** true, wenn beide A/B-Slots gespeicherte States enthalten. */
    bool bothABSlotsStored() const;

    void parameterChanged (const juce::String& parameterId, float newValue) override;

    CERNAudioProcessor& processor;
    juce::AudioDeviceManager* audioDeviceManager;

    LogoComponent logo;
    juce::Label presetLabel;
    juce::TextButton patchButton { "PATCHBAY" };
    juce::TextButton mixerButton { "MIXER" };
    juce::TextButton triggerButton { "TRIGGER" };
    juce::TextButton audioButton { "AUDIO" };
    juce::ComboBox presetSelector;
    juce::TextButton saveButton { "SAVE" };
    juce::TextButton loadButton { "LOAD" };
    juce::TextButton deleteButton { "DEL" };
    juce::TextButton abButton { "A" };
    juce::TextButton copyButton { "A>B" };
    juce::TextButton rndButton { "RND" };
    juce::TextButton renderButton { "RENDER" };
    juce::Slider morphSlider { juce::Slider::LinearHorizontal, juce::Slider::TextBoxRight };
    juce::Label morphLabel;
    CableLed cableLed;

    std::unique_ptr<juce::FileChooser> fileChooser;

    juce::Array<PresetManager::Entry> presetEntries; // Browser-Reihenfolge
    juce::Array<int> presetEntryIds;                 // Combo-IDs, parallel zu presetEntries

    int currentPresetId { 0 };          // Combo-ID des aktuellen Presets; 0 = keins
    int displayedPresetId { 0 };        // Combo-ID, deren Text zuletzt markiert wurde
    PresetManager::Entry currentPreset; // Identität des aktuellen Presets (SND-172)
    bool hasCurrentPreset { false };
    juce::String currentPresetName;     // Anzeigename (auch fuer User-Presets)
    bool presetModified { false };
    bool suppressModifiedTracking { false };
    bool listenersAttached { false };

    // A/B-Speicher (SND-167): vollständiger Plugin-State je Slot.
    std::array<juce::String, 2> abStateXml;
    std::array<bool, 2> abModified { false, false };
    int activeABSlot { 0 };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HeaderBar)
};

//==============================================================================
/** FooterSequencer: Transport, Tempo-Parameter und 16-Step-Editor
    (Gates + Velocity + Step-Pitch). */
class MainComponent::FooterSequencer : public juce::Component, public juce::Timer
{
public:
    explicit FooterSequencer (CERNAudioProcessor& p);
    ~FooterSequencer() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void timerCallback() override;

    /** Shortcut-Ziel (SND-173): Play/Stop umschalten. */
    void togglePlayback();

private:
    CERNAudioProcessor& processor;

    juce::Label sectionLabel;
    juce::TextButton playButton { "PLAY" };
    juce::Label bpmLabel;
    juce::Label swingLabel;
    juce::Label stepsLabel;
    juce::Label bpmInput;      // Editierbares Textfeld für BPM
    juce::Slider swingSlider;
    juce::Slider stepsSlider;

    std::vector<std::unique_ptr<juce::ToggleButton>> gateButtons;
    std::vector<std::unique_ptr<juce::Slider>> velocitySliders;
    std::vector<std::unique_ptr<juce::Slider>> pitchSliders;

    std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment> playAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> swingAttachment;
    std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment> stepsAttachment;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>> gateAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> velocityAttachments;
    std::vector<std::unique_ptr<juce::AudioProcessorValueTreeState::SliderAttachment>> pitchAttachments;

    int currentStep { 0 };
    bool playing { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (FooterSequencer)
};

//==============================================================================
/** MacroPanel: 5 Performance-Makros (SND-162).

    Die Makros sind reine UI-Aggregatoren: sie schreiben mehrere bestehende
    Parameter (Mapping siehe MainComponent.cpp). Eigene APVTS-Parameter fuer die
    Makro-Stellungen (Persistenz/Automation) fehlen noch und muessen in
    ParameterLayout/DSP nachgezogen werden.
*/
class MainComponent::MacroPanel : public juce::Component
{
public:
    static constexpr int numMacros = 5;

    explicit MacroPanel (CERNAudioProcessor& p);
    ~MacroPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    struct Macro
    {
        juce::String name;
        juce::String description;
        juce::Slider slider;
        juce::Label nameLabel;
        juce::Label valueLabel;
    };

    void applyMacro (int index, float value);
    void updateValueLabel (int index, float value);

    CERNAudioProcessor& processor;
    std::array<Macro, numMacros> macros;
    juce::String upperTitle { "MACROS" };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MacroPanel)
};
