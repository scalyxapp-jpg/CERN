#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/** Envelope-Editor fuer die Pitch-Envelope (Attack/Decay/Depth/Curve).

    Zeichnet den tatsaechlichen Verlauf der DSP-Huellkurve (PitchEnvelope):
    Attack mit Curve-Kennlinie, exponentieller Decay (-40 dB nach der Decay-Zeit)
    und die Depth-Auslenkung in Halbtoenen (SND-171).
*/
class EnvelopeEditor : public juce::Component
{
public:
    EnvelopeEditor();
    ~EnvelopeEditor() override = default;

    /** Rohwerte: attackMs/decayMs in Millisekunden, depth 0..2, curve -1..1. */
    void setParameters (float attackMs, float decayMs, float depth, float curve);

    void paint (juce::Graphics& g) override;

private:
    /** Attack-Kennlinie, identisch zu PitchEnvelope::process(). */

    void rebuildInfoText();

    float attackMs { 0.2f };
    float decayMs  { 60.0f };
    float depth    { 0.8f };
    float curve    { 0.0f };

    /** Drei Voreinstellungen der Info-Zeile (voll / mittel / kurz), damit der
        Text bei schmalem Display nicht abgeschnitten wird. */
    juce::String infoFull;
    juce::String infoMedium;
    juce::String infoShort;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (EnvelopeEditor)
};
