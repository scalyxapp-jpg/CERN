#pragma once

#include <cmath>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
/** Click / Beater Transient (Stufe 5).

    Zwei Quellen, die über `clickNoiseMix` gemischt werden:
    - Noise-Burst 0.5..2 ms (Hochpass 1.5 kHz) für den Schlag-Anteil
    - Resonanz-Tick bei 1.2..6.5 kHz (Tone) für den metallischen Anteil

    Das Schläger-Material setzt Tick-Decay und Noise-Anteil
    (Soft 8 ms / 70 %, Medium 4 ms / 50 %, Hard 2 ms / 30 %).

    `clickNoiseMix` (Default 0.5, 0..1) ist über setNoiseMix() vom Engine
    setzbar; das Material verschiebt den Wert um ±0.2.
*/
class ClickGenerator
{
public:
    ClickGenerator() = default;

    void prepare (double sampleRate)
    {
        this->sampleRate = sampleRate;
        noiseHpCoeff = std::exp (-juce::MathConstants<float>::twoPi
                                 * 1500.0f / (float) sampleRate);
        reset();
    }

    void reset()
    {
        tickEnv = 0.0f;
        tickPhase = 0.0f;
        burstEnv = 0.0f;
        burstSamplesLeft = 0;
        noiseLp = 0.0f;
        rngState = 0x2545F491u;
        cachedDecayMs = -1.0f;
        cachedMaterial = -1;
        tickCoeff = 0.0f;
        velClickScale = 1.0f;
        isActive = false;
    }

    void trigger (float velocity)
    {
        trigger (velocity, 0.0f);
    }

    /** Velocity -> Click-Pegel (SND-177): level wird mit
        (1 + velToClick * (velocity - 1)) multipliziert, geklemmt >= 0.
        velToClick = 0 deaktiviert die Kopplung (Faktor 1). */
    void trigger (float velocity, float velToClick)
    {
        const float v = juce::jlimit (0.0f, 1.0f, velocity);
        const float amount = juce::jlimit (0.0f, 1.0f, velToClick);
        velClickScale = juce::jmax (0.0f, 1.0f + amount * (v - 1.0f));

        tickEnv = v;
        burstEnv = 1.0f;
        tickPhase = 0.0f;
        noiseLp = 0.0f;
        burstSamplesLeft = (int) (0.002 * sampleRate); // max. 2 ms Noise-Burst
        isActive = true;
    }

    /** Anteil des Noise-Bursts am Click (0 = nur Tick, 1 = nur Noise).
        Default 0.5 (Wert im Material verschoben: Soft +0.2 / Medium 0.0 /
        Hard -0.2). Der Wert ist bewusst als Engine-setzbares Member
        ausgelegt (setNoiseMix/getNoiseMix), damit ein Nutzerparameter
        (APVTS) direkt durchgereicht werden kann.
        REVIEW-TODO (Paket B/C): Die Engine ruft setNoiseMix() aktuell nicht
        auf - der Parameter muss im Parameter-Layout ergaenzt und hier
        verdrahtet werden. */
    void setNoiseMix (float mix) { clickNoiseMix = juce::jlimit (0.0f, 1.0f, mix); }
    float getNoiseMix() const { return clickNoiseMix; }

    /** tone: 0..1 (Base-Tick 1.2..6.5 kHz, SND-060).
        pitchRatio: nativer Pitch-Faktor des Ticks (SND-134); der Engine
        übergibt den 30-%-getrackten Step-Pitch (trackGlideRatio). 1 = aus.
        pitchEnvRatio: Momentanverhältnis der Pitch-Envelope (Sweep-Form:
        1.0 = Tonfall-Maximum, 1.0 am Ende = Zielton; 1.0 = kein Sweep).
        Der Tick folgt dem Sweep mit 30 % (SND-134-Semistik R^0.3), damit der
        Click bei einem Pitch-Sweep mitzieht (Strohhalm-Effekt).
        REVIEW-TODO (Paket B): Die Engine muss pitchEnvRatio übergeben
        (z. B. aus pitchEnv.getEnvelopeValue()); ohne Übergabe bleibt der Tick
        unmoduliert (Default 1.0 = Verhalten wie bisher). */
    float process (float level, float decayMs, float tone, float distortion,
                   int beaterMaterial, float pitchRatio = 1.0f,
                   float pitchEnvRatio = 1.0f)
    {
        if (! isActive)
            return 0.0f;

        if (decayMs != cachedDecayMs || beaterMaterial != cachedMaterial)
        {
            // Material: Soft 8 ms / Medium 4 ms / Hard 2 ms bei Default-Decay 4 ms.
            const float materialScale = (beaterMaterial == 0) ? 2.0f
                                      : (beaterMaterial == 2) ? 0.5f : 1.0f;
            const float decay = juce::jmax (1.0f, decayMs * materialScale * 0.001f * (float) sampleRate);
            tickCoeff = std::exp (-1.0f / decay);

            cachedDecayMs = decayMs;
            cachedMaterial = beaterMaterial;
        }

        tickEnv *= tickCoeff;

        if (tickEnv < 1.0e-4f && burstSamplesLeft <= 0)
        {
            tickEnv = 0.0f;
            burstEnv = 0.0f;
            isActive = false;
            return 0.0f;
        }

        // Resonanz-Tick: abklingender Sinus bei 1.2..6.5 kHz (Base), nativ um
        // pitchRatio gestuft (SND-134, kein Tone-Remapping mehr).
        const float baseTickFreq = 1200.0f + 5300.0f * juce::jlimit (0.0f, 1.0f, tone);
        const float pitch = juce::jlimit (0.1f, 12.0f, std::isfinite (pitchRatio) ? pitchRatio : 1.0f);

        // Tick folgt dem Pitch-Sweep der Envelope mit 30 % (R^0.3) - der Click
        // zieht beim Tonfall mit (Strohhalm-Effekt).
        const float envRatio = juce::jlimit (0.1f, 12.0f, std::isfinite (pitchEnvRatio) ? pitchEnvRatio : 1.0f);
        const float tickPitch = pitch * std::pow (envRatio, 0.3f);

        const float tickFreq = juce::jlimit (20.0f, 18000.0f, baseTickFreq * tickPitch);
        const float tickOmega = juce::MathConstants<float>::twoPi * tickFreq / (float) sampleRate;
        tickPhase += tickOmega;

        if (tickPhase >= juce::MathConstants<float>::twoPi)
            tickPhase -= juce::MathConstants<float>::twoPi;

        const float tick = std::sin (tickPhase) * tickEnv;

        // Noise-Burst: weiss, Hochpass 1.5 kHz (komplementäres One-Pole-Paar).
        float noise = 0.0f;

        if (burstSamplesLeft > 0)
        {
            --burstSamplesLeft;

            rngState ^= rngState << 13;
            rngState ^= rngState >> 17;
            rngState ^= rngState << 5;
            const float white = (float) (rngState & 0xFFFFFFu) / 8388608.0f - 1.0f; // -1..1

            noiseLp += (1.0f - noiseHpCoeff) * (white - noiseLp);
            noise = (white - noiseLp) * burstEnv;

            // Burst-Hüllkurve: schneller Abfall (~0.5 ms tau), hart bei 2 ms aus.
            burstEnv *= 0.97f;
        }

        // Material verschiebt den Noise-Anteil um +-0.2 um den Nutzerwert.
        const float materialNoiseOffset = (beaterMaterial == 0) ? 0.2f
                                        : (beaterMaterial == 2) ? -0.2f : 0.0f;
        const float mix = juce::jlimit (0.0f, 1.0f, clickNoiseMix + materialNoiseOffset);

        float x = mix * noise + (1.0f - mix) * tick;

        // Verzerrung pegelneutral: Ceiling bleibt 1.0 (SND-064).
        if (distortion > 0.0f)
        {
            const float k = 1.0f + 3.0f * juce::jlimit (0.0f, 1.0f, distortion);
            x = std::tanh (x * k) / std::tanh (k);
        }

        if (! std::isfinite (x))
        {
            isActive = false;
            return 0.0f;
        }

        return x * level * velClickScale;
    }

    bool isActive { false };

private:
    double sampleRate { 48000.0 };

    float clickNoiseMix { 0.5f }; ///< 0.5 -> Material liefert exakt 70/50/30 %
    float velClickScale { 1.0f }; ///< SND-177: (1 + velToClick*(vel-1)), >= 0
    float tickEnv { 0.0f };
    float tickPhase { 0.0f };
    float burstEnv { 0.0f };
    float noiseLp { 0.0f };
    float noiseHpCoeff { 0.8f };
    int   burstSamplesLeft { 0 };
    juce::uint32 rngState { 0x2545F491u };

    float cachedDecayMs { -1.0f };
    int   cachedMaterial { -1 };
    float tickCoeff { 0.0f };
};
