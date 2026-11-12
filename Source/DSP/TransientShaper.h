#pragma once

#include <cmath>
#include <utility>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
/** Transient-Shaper (SND-175).

    Zwei Hüllkurven-Detektoren auf dem Stereo-Maximum:
      fast: 0.5 ms Attack / 30 ms Release  (Anschlag)
      slow: 15 ms Attack / 200 ms Release  (Körper)

    Die Differenz (in dB) entscheidet, ob ein Anschlag (fast > slow) oder ein
    Ausschwingen (slow > fast) vorliegt. Daraus wird eine Verstärkung gebildet:

      attackDb  * clamp(diff / 6 dB, 0..1)   für diff > 0  (Anschlag)
      sustainDb * clamp(-diff / 6 dB, 0..1)  für diff < 0  (Körper/Tail)

    Bereich: 0..+12 dB pro Anteil, Gesamt-Gain auf 0..+12 dB begrenzt. Die
    Verstärkung wird mit 1 ms geglättet (kein Zipper, kein Überschwingen der
    Hüllkurven). attackDb = sustainDb = 0 ergibt exakt Bypass (Gain 1).
    Beide Kanäle werden mit demselben Gain verarbeitet (Stereo-Link), es gibt
    keine Latenz. Header-only, keine Allokationen.
*/
class TransientShaper
{
public:
    TransientShaper() = default;

    void prepare (double newSampleRate)
    {
        sampleRate = juce::jmax (1.0, newSampleRate);

        fastAttackCoeff  = onePoleCoeff (0.5f);
        fastReleaseCoeff = onePoleCoeff (30.0f);
        slowAttackCoeff  = onePoleCoeff (15.0f);
        slowReleaseCoeff = onePoleCoeff (200.0f);
        gainCoeff        = onePoleCoeff (1.0f);

        reset();
    }

    void reset()
    {
        fastEnv = 0.0f;
        slowEnv = 0.0f;
        smoothedGainDb = 0.0f;
        lastTransientDb = 0.0f;
    }

    /** Anschlag-Anhebung in dB (0..12). */
    void setAttackDb (float db)
    {
        attackDb = juce::jlimit (0.0f, 12.0f, std::isfinite (db) ? db : 0.0f);
    }

    /** Sustain-/Körper-Anhebung in dB (0..12). */
    void setSustainDb (float db)
    {
        sustainDb = juce::jlimit (0.0f, 12.0f, std::isfinite (db) ? db : 0.0f);
    }

    float getAttackDb() const { return attackDb; }
    float getSustainDb() const { return sustainDb; }

    /** Zuletzt angewendeter Gain in dB (Meter/UI). */
    float getLastGainDb() const { return smoothedGainDb; }

    /** Zuletzt gemessene Transient-Differenz in dB (>0 Anschlag, <0 Tail). */
    float getLastTransientDb() const { return lastTransientDb; }

    /** Verarbeitet ein Stereo-Sample-Paar und gibt (L, R) zurück. */
    std::pair<float,float> process (float l, float r)
    {
        const float in = juce::jmax (std::abs (l), std::abs (r));

        fastEnv = follow (fastEnv, in, fastAttackCoeff, fastReleaseCoeff);
        slowEnv = follow (slowEnv, in, slowAttackCoeff, slowReleaseCoeff);

        float targetGainDb = 0.0f;

        if (attackDb > 0.0f || sustainDb > 0.0f)
        {
            const float fastDb = juce::Decibels::gainToDecibels (fastEnv + 1.0e-9f);
            const float slowDb = juce::Decibels::gainToDecibels (slowEnv + 1.0e-9f);

            const float diffDb = fastDb - slowDb;
            lastTransientDb = diffDb;

            if (diffDb > 0.0f)
                targetGainDb = attackDb * juce::jlimit (0.0f, 1.0f, diffDb / 6.0f);
            else
                targetGainDb = sustainDb * juce::jlimit (0.0f, 1.0f, -diffDb / 6.0f);

            targetGainDb = juce::jlimit (0.0f, 12.0f, targetGainDb);
        }
        else
        {
            lastTransientDb = 0.0f;
        }

        smoothedGainDb = follow (smoothedGainDb, targetGainDb, gainCoeff, gainCoeff);

        float gain = juce::Decibels::decibelsToGain (smoothedGainDb);

        if (! std::isfinite (gain) || gain < 0.0f || gain > 8.0f)
            gain = 1.0f; // +18 dB ist die Obergrenze des Designs; sonst Bypass

        return { l * gain, r * gain };
    }

private:
    float onePoleCoeff (float timeMs) const
    {
        return std::exp (-1.0f / juce::jmax (1.0f, timeMs * 0.001f * (float) sampleRate));
    }

    static float follow (float env, float target, float attackCoeff, float releaseCoeff)
    {
        return (target > env)
            ? attackCoeff  * env + (1.0f - attackCoeff)  * target
            : releaseCoeff * env + (1.0f - releaseCoeff) * target;
    }

    double sampleRate { 48000.0 };
    float attackDb { 0.0f };
    float sustainDb { 0.0f };
    float fastEnv { 0.0f };
    float slowEnv { 0.0f };
    float smoothedGainDb { 0.0f };
    float lastTransientDb { 0.0f };
    float fastAttackCoeff { 0.0f };
    float fastReleaseCoeff { 0.0f };
    float slowAttackCoeff { 0.0f };
    float slowReleaseCoeff { 0.0f };
    float gainCoeff { 0.0f };
};
