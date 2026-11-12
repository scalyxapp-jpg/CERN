#pragma once

#include <cmath>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
/** Pitch-Envelope (TR-909/808 Style).

    Wird nach einem Trigger gestartet und liefert einen Frequenz-Multiplikator
    (1.0 = keine Auslenkung). Attack und Decay sind unabhängig; der Decay ist
    exponentiell (Abfall auf -40 dB nach `decayMs`), die Kurve wirkt auf beide
    Segmente.
*/
class PitchEnvelope
{
public:
    PitchEnvelope() = default;

    void prepare (double sampleRate)
    {
        this->sampleRate = sampleRate;
        reset();
    }

    void reset()
    {
        position = 0.0f;
        envelopeValue = 0.0f;
        isActive = false;
    }

    void trigger()
    {
        position = 0.0f;
        envelopeValue = 0.0f;
        isActive = true;
    }

    /** Liefert den pitchRatio-Multiplikator für das aktuelle Sample.
        attack/decay in ms, depth 0..2 (0..+24 Halbtöne), curve -1..1. */
    float process (float attackMs, float decayMs, float depth, float curve)
    {
        if (! isActive)
        {
            envelopeValue = 0.0f;
            return 1.0f;
        }

        const float attackSamples = juce::jmax (1.0f, attackMs * 0.001f * (float) sampleRate);
        const float decaySamples  = juce::jmax (1.0f, decayMs  * 0.001f * (float) sampleRate);
        const float clampedCurve  = juce::jlimit (-1.0f, 1.0f, curve);

        if (position < attackSamples)
        {
            const float u = juce::jlimit (0.0f, 1.0f, position / attackSamples);

            // Kurve formt den Attack: <0 schneller Anstieg, >0 späterer Anstieg.
            envelopeValue = (clampedCurve < 0.0f)
                ? 1.0f - std::pow (1.0f - u, 1.0f + 2.0f * -clampedCurve)
                : (clampedCurve > 0.0f ? std::pow (u, 1.0f + 2.0f * clampedCurve) : u);

            position += 1.0f;
        }
        else
        {
            // Exponentieller Decay: -40 dB nach genau `decayMs`.
            const float u = (position - attackSamples) / decaySamples;
            const float decayRate = std::pow (2.0f, clampedCurve); // 0.5..2

            envelopeValue = std::exp (-4.605f * u * decayRate);
            position += 1.0f;

            if (envelopeValue <= 0.01f)
            {
                envelopeValue = 0.0f;
                isActive = false;
            }
        }

        // depth 0..2 -> 0..+24 Halbtöne (exponentiell, musikalisch).
        return std::exp2 (juce::jmax (0.0f, depth) * envelopeValue);
    }

    /** Normierte Hüllkurve 0..1 des letzten process()-Aufrufs (für Body-Tracking). */
    float getEnvelopeValue() const { return envelopeValue; }

    bool isActive { false };

private:
    double sampleRate { 48000.0 };
    float position { 0.0f };       ///< Sample-Position seit dem Trigger
    float envelopeValue { 0.0f };  ///< 0..1
};
