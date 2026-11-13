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

    Nichtlinearität (von-Kármán) moduliert **pro Mode** die Phasengeschwindigkeit
    proportional zum Quadrat der aktuellen Modus-Amplitude (Frequenzmodulation
    ∝ Amplitude², wie bei echten Membranen). Die FM wirkt direkt auf die
    Phasendifferenz (inkrementell), nicht auf eine neu berechnete
    Oszillatorfrequenz - dadurch entstehen beim Trigger/Reset keine Sprünge.
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
        // Masse -> Gewicht des Grundmodus 1.2..0.6 + Dämpfungsfaktor 1.6..0.6,
        // Tension -> Dämpfungsfaktor 1.5..0.7.
        const float radiusNorm  = juce::jlimit (0.0f, 1.0f, (radius - 0.05f) / 0.45f);
        const float massNorm    = juce::jlimit (0.0f, 1.0f, (mass - 0.1f) / 1.9f);
        const float tensionNorm = juce::jlimit (0.0f, 1.0f, tension);

        const float modeSpread  = 0.6f + 1.0f * radiusNorm;
        const float dampingMul  = (1.6f - 1.0f * massNorm) * (1.5f - 0.8f * tensionNorm);

        // Gewicht des Grundmodus: Masse 0.1 kg -> 1.2, 2.0 kg -> 0.6.
        // Das Fundamental ist dadurch der dominanteste Modus (bei Masse 2.0 kg
        // gleichauf mit Mode 1), statt der leiseste (SND-031 / Review H1).
        const float weight0 = 1.2f - 0.6f * massNorm;

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
        const float clampedNonlinearity = juce::jlimit (0.0f, 2.0f, nonlinearity);

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

            // Nichtlinearität pro Mode (von-Kármán): Frequenzmodulation ∝
            // Amplitude². Die interne Modusamplitude (m.amplitude, ohne
            // Normalisierung) startet bei velocity * modeWeight, also bei ≈ 1.0
            // für Mode 0 => bei nonlinearity = 1 genau +2 Halbtöne; die
            // schwachen Obertonmoden bekommen durch das Quadrat deutlich
            // weniger (Mode 1: 0.6² => +0.65 Halbtöne).
            const float ampSquared = m.amplitude * m.amplitude;
            const float fm = juce::jmin (kMaxFrequencyModulation,
                                         clampedNonlinearity * kNonlinearityGain * ampSquared);

            // Inkrementell auf die Phase: die FM darf beim Trigger/Reset keine
            // Frequenzsprünge erzeugen.
            m.phase += omega * (1.0f + fm);

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

        // Die alte Summen-Sättigung (tanh(out*(1+nonlinearity))) entfällt:
        // Die Nichtlinearität wirkt jetzt pro Mode auf die Frequenz, nicht mehr
        // als Kompression der Gesamtsumme.

        return sum;
    }

    bool isActive { false };

private:
    static constexpr int maxModes = 16;

    /** +2 Halbtöne bei nonlinearity = 1 und Modusamplitude 1:
        2^(2/12) - 1 = 0.12246. */
    static constexpr float kNonlinearityGain = 0.12246f;

    /** Deckel der Frequenzmodulation (max. +3.9 Halbtöne) - verhindert
        Aliasing/Instabilität bei sehr lauten Moden. */
    static constexpr float kMaxFrequencyModulation = 0.25f;

    /** Bessel-Nullstellen-Verhältnisse einer kreisförmigen Membran (SND-032). */
    /** Nullstellen der Bessel-Funktion J1 (eingespannte Kreismembran).
        Review M9: bis 16 Modi erweitert -> Membraninhalt bis 5,83*f0
        (bei f0 = 46,25 Hz also bis ~270 Hz, vorher nur 188 Hz). */
    static constexpr std::array<float, maxModes> besselRatios {
        1.0f, 1.593f, 2.135f, 2.295f, 2.653f, 2.917f,
        3.155f, 3.500f, 3.598f, 3.647f, 3.994f, 4.059f,
        4.501f, 5.331f, 5.536f, 5.827f
    };

    /** Gewichtete Modus-Amplituden (SND-033). */
    static constexpr std::array<float, maxModes> modeWeights {
        1.0f, 0.6f, 0.35f, 0.25f, 0.2f, 0.16f,
        0.13f, 0.11f, 0.09f, 0.08f, 0.07f, 0.06f,
        0.055f, 0.05f, 0.045f, 0.04f
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
