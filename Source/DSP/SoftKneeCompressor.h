#pragma once

#include <cmath>
#include <utility>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
/** Soft-Knee-Compressor (ersetzt juce::dsp::Compressor).

    Detektor: Peak (Stereo-Link über maximales Envelope beider Kanäle).
    Schwelle: compThreshold (dB), Verhältnis: compRatio, Attack/Release (ms).
    Soft-Knee im dB-Bereich: compKnee (0–12 dB). Unter threshold − knee/2 keine
    Kompression (Ratio 1), darüber volle Kompression, dazwischen quadratischer
    weicher Übergang. Makeup-Gain: compMakeup (dB).

    Auto-Makeup (SND-088): mit setAutoMakeupEnabled (true) folgt die Verstärkung
    der aktuellen Gain Reduction; compMakeup wirkt dann als Trim-Offset.
    Die Kompensation läuft über einen langsamen Follower (50 ms Attack /
    400 ms Release), weil eine instantane Kompensation die Kompression exakt
    aufheben würde (out == in). Default: aus, Verhalten unverändert.

    Dry/Wet (SND-090/SND-180): mix 0..1 als letzter process()-Parameter,
    0 = trocken, 1 = voll komprimiert, dazwischen lineare Überblendung.
    Default 1, damit bestehende Aufrufer unverändert bleiben.

    GR-Meter über getGainReductionDb(). Lookahead wird vom Engine gesteuert
    (hier: einfache Verzögerung nicht zwingend, Envelope sofort angewendet).
    NaN/Inf-Schutz auf den Gain-Faktor; bei ungültigem Gain werden beide Kanäle
    stummgeschaltet.
*/
class SoftKneeCompressor
{
public:
    SoftKneeCompressor() = default;

    void prepare (double newSampleRate)
    {
        sampleRate = juce::jmax (1.0, newSampleRate);

        // Koeffizienten für den Auto-Makeup-Follower (einmal pro prepare).
        autoMakeupAttackCoeff  = onePoleCoeff (50.0f);
        autoMakeupReleaseCoeff = onePoleCoeff (400.0f);

        reset();
    }

    void reset()
    {
        envelopeL = 0.0f;
        envelopeR = 0.0f;
        gainReductionDb = 0.0f;
        autoMakeupDb = 0.0f;
    }

    void setSidechainEnabled (bool enabled) { sidechainEnabled = enabled; }
    void setSidechainEnvelope (float envL, float envR)
    {
        sidechainEnvL = envL;
        sidechainEnvR = envR;
    }

    /** Auto-Makeup = aktuelle (geglättete) Gain Reduction (SND-088). */
    void setAutoMakeupEnabled (bool enabled)
    {
        if (autoMakeupEnabled != enabled)
            autoMakeupDb = 0.0f; // Neustart ohne Sprung

        autoMakeupEnabled = enabled;
    }

    bool isAutoMakeupEnabled() const { return autoMakeupEnabled; }

    /** Aktuell angewendeter Auto-Makeup-Anteil in dB (für Meter/UI). */
    float getAutoMakeupDb() const { return autoMakeupDb; }

    /** Verarbeitet ein Stereo-Sample-Paar und gibt (L, R) zurück.
        mix: 0 = trocken, 1 = komprimiert (Default 1 = bisheriges Verhalten).
    */
    std::pair<float,float> process (float l, float r,
                                    float thresholdDb, float ratio,
                                    float attackMs, float releaseMs,
                                    float kneeDb, float makeupDb,
                                    float mix = 1.0f)
    {
        thresholdDb = juce::jlimit (-90.0f, 0.0f, thresholdDb);
        ratio       = juce::jlimit (1.0f, 100.0f, ratio);
        kneeDb      = juce::jlimit (0.0f, 24.0f, kneeDb);

        // Sidechain-Envelope bevorzugen, falls aktiv
        float envL = sidechainEnabled ? sidechainEnvL : std::abs (l);
        float envR = sidechainEnabled ? sidechainEnvR : std::abs (r);

        // Peak-Detektor mit Attack/Release (Koeffizienten pro Sample)
        const float attackCoeff  = std::exp (-1.0f / juce::jmax (1.0f, attackMs  * 0.001f * (float) sampleRate));
        const float releaseCoeff = std::exp (-1.0f / juce::jmax (1.0f, releaseMs * 0.001f * (float) sampleRate));

        auto follow = [] (float env, float target, float attackC, float releaseC)
        {
            return (target > env)
                ? attackC  * env + (1.0f - attackC)  * target
                : releaseC * env + (1.0f - releaseC) * target;
        };

        envelopeL = follow (envelopeL, envL, attackCoeff, releaseCoeff);
        envelopeR = follow (envelopeR, envR, attackCoeff, releaseCoeff);

        // Stereo-Link: maximales Envelope
        const float env = juce::jmax (envelopeL, envelopeR);

        // Gain Reduktion in dB mit Soft-Knee
        const float envDb = juce::Decibels::gainToDecibels (env + 1.0e-9f);

        float grDb = 0.0f;
        const float kneeLow  = thresholdDb - kneeDb * 0.5f;
        const float kneeHigh = thresholdDb + kneeDb * 0.5f;

        if (envDb <= kneeLow)
        {
            grDb = 0.0f; // keine Kompression
        }
        else if (envDb >= kneeHigh)
        {
            grDb = (envDb - thresholdDb) * (1.0f - 1.0f / ratio);
        }
        else
        {
            // Quadratischer weicher Übergang über den Knee
            const float x = (envDb - kneeLow) / kneeDb;
            const float linDb = (envDb - thresholdDb) * (1.0f - 1.0f / ratio);
            grDb = linDb * x * x; // weicher Einstieg
        }

        gainReductionDb = grDb;

        // Auto-Makeup folgt der GR träge (siehe Klassenkommentar).
        float appliedMakeupDb = makeupDb;

        if (autoMakeupEnabled)
        {
            const float coeff = (grDb > autoMakeupDb) ? autoMakeupAttackCoeff
                                                      : autoMakeupReleaseCoeff;
            autoMakeupDb = coeff * autoMakeupDb + (1.0f - coeff) * grDb;
            appliedMakeupDb += autoMakeupDb;
        }

        // Gain-Faktor (linear), NaN-Schutz
        float gain = juce::Decibels::decibelsToGain (-grDb + appliedMakeupDb);
        if (! std::isfinite (gain) || gain < 0.0f)
            gain = 0.0f;

        const float wetL = l * gain;
        const float wetR = r * gain;

        // Dry/Wet (SND-090/SND-180)
        const float m = std::isfinite (mix) ? juce::jlimit (0.0f, 1.0f, mix) : 1.0f;

        if (m >= 1.0f)
            return { wetL, wetR };

        if (m <= 0.0f)
            return { l, r };

        return { l + (wetL - l) * m, r + (wetR - r) * m };
    }

    float getGainReductionDb() const { return gainReductionDb; }

private:
    float onePoleCoeff (float timeMs) const
    {
        return std::exp (-1.0f / juce::jmax (1.0f, timeMs * 0.001f * (float) sampleRate));
    }

    double sampleRate { 48000.0 };
    float envelopeL { 0.0f };
    float envelopeR { 0.0f };
    float gainReductionDb { 0.0f };
    bool sidechainEnabled { false };
    float sidechainEnvL { 0.0f };
    float sidechainEnvR { 0.0f };
    bool autoMakeupEnabled { false };
    float autoMakeupDb { 0.0f };
    float autoMakeupAttackCoeff { 0.0f };
    float autoMakeupReleaseCoeff { 0.0f };
};
