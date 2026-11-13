#pragma once

#include <cmath>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
/** Pitch-Envelope (TR-909/808 Style).

    Wird nach einem Trigger gestartet und liefert einen Frequenz-Multiplikator
    (1.0 = keine Auslenkung).

    TR-909/808: Tonfall von oben (2^depth) auf f0.
    Der Sweep startet bei `2^depth` (Maximum) und faellt exponentiell auf 1.0
    (= f0). `depth` ist in Oktaven (0.3 ~ +3,8 st ... 3.0 ~ +36 st).

    - `attackMs` = Haltezeit auf dem Maximum (kein Anstieg - der Ton startet
      bereits auf `2^depth`).
    - `decayMs`  = Abfallzeit; der Sweep erreicht nach ~1x `decayMs` den Wert
      0.01. `curve` (-1..1) skaliert die Abfallrate (`2^curve` = 0.5 ... 2).
    - Die Kurvenform fuer die Anzeige (Envelope-Editor) berechnet die GUI
      separat (GUI/EnvelopeEditor.cpp) - sie hat keinen Einfluss auf den Ton.

    `getEnvelopeValue()` liefert die Sweep-Form: 1.0 am Trigger -> 0 am Ende
    (frueher: 0 -> 1 -> 0, Attack-Form). Die Engine nutzt sie fuer das
    Body-Gleiten; die Sweep-Form passt zur Tonfall-Richtung.
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
        sweep = 1.0f;
        isActive = false;
    }

    void trigger()
    {
        position = 0.0f;
        sweep = 1.0f;
        isActive = true;
    }

    /** Liefert den pitchRatio-Multiplikator für das aktuelle Sample.
        attack/decay in ms, depth 0..3 (Oktaven), curve -1..1. */
    float process (float attackMs, float decayMs, float depth, float curve)
    {
        if (! isActive)
        {
            sweep = 0.0f;
            return 1.0f;
        }

        const float attackSamples = juce::jmax (1.0f, attackMs * 0.001f * (float) sampleRate);
        const float decaySamples  = juce::jmax (1.0f, decayMs  * 0.001f * (float) sampleRate);
        const float clampedCurve  = juce::jlimit (-1.0f, 1.0f, curve);
        const float clampedDepth  = juce::jlimit (0.0f, 3.0f, depth);

        if (position < attackSamples)
        {
            // Phase 1: Ton haelt auf dem Maximum (2^depth). "Attack" ist die
            // Haltezeit vor dem Abfall - es gibt keinen Anstieg.
            sweep = 1.0f;

            position += 1.0f;
        }
        else
        {
            // Phase 2: exponentieller Abfall (Sweep 1 -> 0).
            // -40 dB nach ~1x decaySamples (bei curve = 0).
            const float u = (position - attackSamples) / decaySamples;
            const float decayRate = std::pow (2.0f, clampedCurve); // 0.5..2

            sweep = std::exp (-4.605f * u * decayRate);
            position += 1.0f;

            if (sweep <= 0.01f)
            {
                sweep = 0.0f;
                isActive = false;
            }
        }

        // ratio(t) = 1 + (2^depth - 1) * sweep
        //   t = 0     : 2^depth (Tonfall-Maximum)
        //   am Ende   : 1.0 (Zieltrequenz f0)
        return 1.0f + (std::exp2 (clampedDepth) - 1.0f) * sweep;
    }

    /** Sweep-Form des letzten process()-Aufrufs: 1.0 am Trigger -> 0 am Ende
        (0 = Zielton erreicht). Wird von der Engine fuer das Body-Gleiten
        genutzt und entspricht der Tonfall-Richtung (oben -> f0). */
    float getEnvelopeValue() const { return sweep; }

    bool isActive { false };

private:
    double sampleRate { 48000.0 };
    float position { 0.0f };   ///< Sample-Position seit dem Trigger
    float sweep { 1.0f };      ///< Sweep-Form 1.0 (Maximum) -> 0.0 (f0)
};
