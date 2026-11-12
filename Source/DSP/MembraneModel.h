#pragma once

#include <array>
#include <cmath>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
/** Physikalisches Membran-Modell (modale Synthese einer kreisförmigen Membran).

    Die Modusfrequenzen folgen den Besselfunktions-Nullstellen einer kreisförmigen
    Membran (1 / 1.593 / 2.135 / 2.295 / 2.653 ...). Die Grundfrequenz kommt aus
    fundamentalHz; Tension/Radius/Mass formen das Timbre (Modus-Spreizung,
    Grundton-Gewicht, Dämpfung), nicht mehr die Stimmung.

    Nichtlinearität (von-Kármán) moduliert die Phasengeschwindigkeit proportional
    zur aktuellen Modus-Amplitude (Pitch-Glide), statt einen festen Frequenz-
    Offset zu addieren.
*/
class MembraneModel
{
public:
    MembraneModel() = default;

    void prepare (double sampleRate)
    {
        this->sampleRate = sampleRate;
        reset();
    }

    void reset()
    {
        for (auto& m : modes)
        {
            m.phase = 0.0f;
            m.amplitude = 0.0f;
        }

        isActive = false;
        cachedModes = -1;
        cachedMassNorm = -1.0f;
        normalization = 1.0f;
    }

    /** Einen Anschlag auslösen (Velocity 0..1). */
    void trigger (float velocity)
    {
        const float vel = juce::jlimit (0.0f, 1.0f, velocity);

        for (int i = 0; i < maxModes; ++i)
        {
            modes[(size_t) i].amplitude = vel * modeWeights[(size_t) i];
            modes[(size_t) i].phase = 0.0f;
        }

        isActive = vel > 0.0f;
    }

    /** Ein Sample verarbeiten. Gibt das Mono-Membran-Signal zurück. */
    float process (float tension, float radius, float mass, float damping,
                   float modesCount, float fundamentalHz,
                   float nonlinearity, float stiffness)
    {
        if (! isActive)
            return 0.0f;

        const int numModes = (int) juce::jlimit (1.0f, (float) maxModes, modesCount);

        // Timbre-Mapping (SND-031): Radius -> Modus-Spreizung 0.6..1.6,
        // Masse -> Gewicht des Grundmodus 0.9..0.2 + Dämpfungsfaktor 1.6..0.6,
        // Tension -> Dämpfungsfaktor 1.5..0.7.
        const float radiusNorm  = juce::jlimit (0.0f, 1.0f, (radius - 0.05f) / 0.45f);
        const float massNorm    = juce::jlimit (0.0f, 1.0f, (mass - 0.1f) / 1.9f);
        const float tensionNorm = juce::jlimit (0.0f, 1.0f, tension);

        const float modeSpread  = 0.6f + 1.0f * radiusNorm;
        const float dampingMul  = (1.6f - 1.0f * massNorm) * (1.5f - 0.8f * tensionNorm);

        // Gewicht des Grundmodus: Masse 0.1 kg -> 0.9, 2.0 kg -> 0.2 (SND-031).
        const float weight0 = 0.9f - 0.7f * massNorm;

        // Gewichtssumme nur bei Parameteränderung neu normieren (blockkonstant).
        if (numModes != cachedModes || std::abs (massNorm - cachedMassNorm) > 1.0e-6f)
        {
            float weightSum = weight0;

            for (int i = 1; i < numModes; ++i)
                weightSum += modeWeights[(size_t) i];

            normalization = 1.0f / juce::jmax (1.0e-6f, weightSum);

            cachedModes = numModes;
            cachedMassNorm = massNorm;
        }

        const float f = juce::jlimit (10.0f, 2000.0f, fundamentalHz);

        const float dampingScale = 0.0018f + 0.051f * juce::jlimit (0.0f, 1.0f, damping);

        float sum = 0.0f;

        for (int i = 0; i < numModes; ++i)
        {
            auto& m = modes[(size_t) i];
            const float modeFreq = f * besselRatios[(size_t) i]
                                     * (1.0f + 0.15f * juce::jlimit (0.0f, 1.0f, stiffness) * (float) i)
                                     * (1.0f + (modeSpread - 1.0f) * (besselRatios[(size_t) i] - 1.0f));

            const float omega = juce::MathConstants<float>::twoPi * modeFreq / (float) sampleRate;

            // Physikalisch: höhere Modi klingen wegen des omega-Faktors schneller ab.
            const float decay = std::exp (-dampingScale * dampingMul * omega);
            m.amplitude *= decay;

            const float amplitude = m.amplitude * normalization
                                      * ((i == 0) ? weight0 : 1.0f);

            // Pitch-Glide: Phasengeschwindigkeit steigt mit der Modus-Amplitude
            // (SND-034) — kein fester Frequenz-Offset mehr.
            m.phase += omega * (1.0f + nonlinearity * amplitude * 0.5f);

            if (m.phase >= juce::MathConstants<float>::twoPi)
                m.phase -= juce::MathConstants<float>::twoPi;

            sum += std::sin (m.phase) * amplitude;
        }

        // Alle Moden unterhalb der Hörbarkeit -> Modell deaktivieren.
        if (std::abs (modes[0].amplitude * normalization * weight0) < 1.0e-5f)
        {
            isActive = false;
            return 0.0f;
        }

        float out = sum;

        if (nonlinearity > 0.0f)
            out = std::tanh (out * (1.0f + nonlinearity));

        return out;
    }

    bool isActive { false };

private:
    static constexpr int maxModes = 12;

    /** Bessel-Nullstellen-Verhältnisse einer kreisförmigen Membran (SND-032). */
    static constexpr std::array<float, maxModes> besselRatios {
        1.0f, 1.593f, 2.135f, 2.295f, 2.653f, 2.917f,
        3.155f, 3.500f, 3.598f, 3.647f, 3.994f, 4.059f
    };

    /** Gewichtete Modus-Amplituden (SND-033). */
    static constexpr std::array<float, maxModes> modeWeights {
        1.0f, 0.6f, 0.35f, 0.25f, 0.2f, 0.16f,
        0.13f, 0.11f, 0.09f, 0.08f, 0.07f, 0.06f
    };

    struct Mode
    {
        float phase { 0.0f };
        float amplitude { 0.0f };
    };

    std::array<Mode, maxModes> modes {};
    double sampleRate { 48000.0 };

    int   cachedModes { -1 };
    float cachedMassNorm { -1.0f };
    float normalization { 1.0f };
};
