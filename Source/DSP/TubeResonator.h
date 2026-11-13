#pragma once

#include <array>
#include <cmath>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
/** Tube / Hollow Resonator (Stufe 3).

    Drei gekoppelte Bandpass-Moden (1x / 2.7x / 5.4x der Resonanzfrequenz,
    -0 / -9 / -15 dB) bilden den Hohlkörper. `hollowMetallic` fügt eine
    pegelneutrale Sättigung der Modussumme hinzu (metallische Obertöne,
    Kleinsignalverstärkung exakt 1.0).

    Der Ausgang ist pegelneutral zum eigenen Eingang: bei level = 1 und
    coupling = 1 beträgt die Verstärkung 1.0 (kein x4 mehr) - der Zweig wird
    parallel zur Membran zu mischen und soll sie nicht übersteuern.
*/
class TubeResonator
{
public:
    TubeResonator() = default;

    void prepare (double sampleRate)
    {
        this->sampleRate = sampleRate;
        reset();
    }

    void reset()
    {
        states = {};
        cachedFreq = -1.0f;
        cachedQ = -1.0f;
        cachedSampleRate = 0.0;
    }

    /** Verarbeitet ein Sample durch die Resonanz-Struktur.
        Gibt den gefilterten Output zurück (parallel zur Membran zu mischen). */
    float process (float input, float freq, float q, float levelIn, float coupling, float hollowMetallic)
    {
        freq = juce::jlimit (20.0f, 8000.0f, freq);
        q    = juce::jlimit (0.5f, 20.0f, q);

        if (freq != cachedFreq || q != cachedQ || sampleRate != cachedSampleRate)
        {
            for (int i = 0; i < numModes; ++i)
            {
                const float modeFreq = juce::jlimit (20.0f, 0.45f * (float) sampleRate,
                                                     freq * modeRatios[(size_t) i]);
                const float omega = juce::MathConstants<float>::twoPi * modeFreq / (float) sampleRate;
                const float sinW = std::sin (omega);
                const float cosW = std::cos (omega);
                const float alpha = sinW / (2.0f * q);

                coefficients[(size_t) i].b0 = alpha;
                coefficients[(size_t) i].b2 = -alpha;
                coefficients[(size_t) i].a0 = 1.0f + alpha;
                coefficients[(size_t) i].a1 = -2.0f * cosW;
                coefficients[(size_t) i].a2 = 1.0f - alpha;
            }

            cachedFreq = freq;
            cachedQ = q;
            cachedSampleRate = sampleRate;
        }

        float out = 0.0f;

        for (int i = 0; i < numModes; ++i)
        {
            const auto& c = coefficients[(size_t) i];
            auto& s = states[(size_t) i];

            const float w = (input - c.a1 * s.y1 - c.a2 * s.y2) / c.a0;
            const float band = c.b0 * w + c.b2 * s.y2;
            s.y2 = s.y1;
            s.y1 = w;

            if (! std::isfinite (band))
            {
                s = {};
                continue;
            }

            out += band * modeGains[(size_t) i];
        }

        // Metallischer Hohlklang: Sättigung der Modussumme mit Slope 1
        // (Kleinsignalverstärkung exakt 1.0, Asymptote 1/k).
        if (hollowMetallic > 0.0f)
        {
            const float k = 1.0f + 2.0f * juce::jlimit (0.0f, 1.0f, hollowMetallic);
            const float saturated = std::tanh (out * k) / k;
            out += (saturated - out) * juce::jlimit (0.0f, 1.0f, hollowMetallic);
        }

        // Ausgang = eigener Eingang x levelIn x coupling (Slope 1).
        // Die drei Bandpässe sind RBJ-Konstant-Peak (jeder Modus 0 dB
        // Peak-Gain); ihre Gewichte summieren sich zu 1.533 (-0/-9/-15 dB).
        // tubeNorm = 1 / 1.533 normiert diesen Modus-Summen-Faktor weg, damit
        // der Zweig bei level = 1, coupling = 1 die Unit-Verstärkung hat
        // (vorher: x4 - der Zweig war 4-6x lauter als die Membrane).
        return out * levelIn * coupling * tubeNorm;
    }

private:
    static constexpr int numModes = 3;

    static constexpr std::array<float, numModes> modeRatios { 1.0f, 2.7f, 5.4f };
    // -0 dB / -9 dB / -15 dB
    static constexpr std::array<float, numModes> modeGains { 1.0f, 0.355f, 0.178f };

    /** Normierung des Modus-Summen-Faktors auf den Einheitspegel:
        1 / (1.0 + 0.355 + 0.178) = 0.6525. Die internen Modusgewichte
        bleiben 1.0 / 0.355 / 0.178 (-0/-9/-15 dB); ihre Wirkung ist damit
        0.652 / 0.232 / 0.116 (Summe = 1.0). */
    static constexpr float tubeNorm = 1.0f / (1.0f + 0.355f + 0.178f);

    struct Coefficients
    {
        float b0 { 0.0f }, b2 { 0.0f }, a0 { 1.0f }, a1 { 0.0f }, a2 { 0.0f };
    };

    struct State
    {
        float y1 { 0.0f }, y2 { 0.0f };
    };

    std::array<Coefficients, numModes> coefficients {};
    std::array<State, numModes> states {};

    double sampleRate { 48000.0 };
    double cachedSampleRate { 0.0 };
    float cachedFreq { -1.0f };
    float cachedQ { -1.0f };
};
