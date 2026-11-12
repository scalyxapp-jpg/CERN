#pragma once

#include <array>
#include <functional>
#include <vector>

#include <juce_gui_basics/juce_gui_basics.h>

#include <juce_audio_processors/juce_audio_processors.h>

#include "DSP/PatchGraph.h"
#include "DSP/ParameterPatchGraph.h"
#include "GUI/ParameterPatchView.h"
#include "GUI/PowerSwitch.h"
#include "Parameters/ParameterIDs.h"

//==============================================================================
/** Rückseiten-/Patch-Ansicht: hardware-artige Rückwand mit Modulen, Buchsen,
    Kabeln und freiem Re-Patching (Eurorack-Stil). Das Signal-Routing wirkt
    direkt auf die DSP-Engine (PatchGraph), daher hat jedes Umstecken sofort
    hörbaren Einfluss auf das Ausgangssignal. */
class PatchView : public juce::Component,
                  private juce::Timer
{
public:
    explicit PatchView (PatchGraph& patchGraph, ParameterPatchGraph& parameterPatchGraph,
                        juce::AudioProcessorValueTreeState& apvts);
    ~PatchView() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseUp   (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

private:
    enum class ViewMode { Audio, Parameter };

    void timerCallback() override;
    void setMode (ViewMode mode);

    // --- Geometrie / Hit-Tests ---
    juce::Rectangle<float> moduleBounds (ModuleIndex module) const;
    juce::Point<float> jackCentre (Jack jack) const;

    /** Anzahl der Buchsen eines Moduls in einer Richtung (allokationsfrei). */
    static int jackCountFor (ModuleIndex module, bool isInput) noexcept;

    /** Beschriftung der Buchse; mehrdeutige "IN"/"OUT" werden unterschieden. */
    static const char* jackLabel (Jack jack) noexcept;
    int findJackAt (juce::Point<float> pos, float radius = 14.0f) const;
    int findCableAt (juce::Point<float> pos, float threshold = 7.0f) const;

    // --- Zeichenroutinen ---
    void drawPanelBackground (juce::Graphics& g);
    void drawModule (juce::Graphics& g, ModuleIndex module);
    /** Titelplatte eines Moduls; wird nach den Kabeln erneut gezeichnet,
        damit keine Leitung den Modultitel unlesbar macht. */
    void drawModuleTitle (juce::Graphics& g, ModuleIndex module,
                          juce::Rectangle<float> moduleBox);
    void drawJack (juce::Graphics& g, Jack jack, bool highlight, bool target);
    void drawCable (juce::Graphics& g, juce::Point<float> from, juce::Point<float> to,
                    juce::Colour colour, bool selected, bool pending = false) const;
    void drawPendingCable (juce::Graphics& g);
    void drawHintBar (juce::Graphics& g);

    juce::AudioProcessorValueTreeState& parameters;
    PatchGraph& patch;
    ParameterPatchGraph& parameterPatch;

    juce::TextButton audioPatchButton { "AUDIO ROUTING" };
    juce::TextButton parameterPatchButton { "PARAMETER CV" };
    juce::TextButton resetButton { "STANDARD PATCH" };
    juce::TextButton clearButton { "ALLE LOESCHEN" };

    std::unique_ptr<ParameterPatchView> parameterView;
    ViewMode mode { ViewMode::Audio };

    std::array<std::unique_ptr<PowerSwitch>, (size_t) ModuleIndex::ModuleCount> modulePowerToggles {};
    std::array<std::unique_ptr<juce::AudioProcessorValueTreeState::ButtonAttachment>, (size_t) ModuleIndex::ModuleCount> modulePowerAttachments {};

    // Interaktionszustand
    bool pending { false };
    Jack pendingFrom { Jack::MembraneOut };
    juce::Point<float> mousePos;
    int hoverJack { -1 };
    int selectedCable { -1 };
    bool dragMoved { false };
    juce::Point<float> mouseDownPos;

    float dashPhase { 0.0f };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PatchView)
};