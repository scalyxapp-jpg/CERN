#pragma once

#include <cmath>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
/** Higgs Field Engine (§9).

    Modelliert eine 3D-Potentialfläche (Mexican Hat). Die Parameter Phi/Theta
    bestimmen einen Punkt auf der Fläche; die abgeleiteten Größen modulieren
    das Audiosignal (Sustain/Inertia, Präsenz/Yukawa, Resonator/Boson).

    Symmetry-Gating: coeff = baseCoeff * higgsSymmetry. Bei Symmetry=0 kein
    Sustain-Effekt (Inertia ohne Wirkung), bei 1 volle Wirkung.
*/
struct HiggsParams
{
    bool  enabled        = false;
    float cursorPhi      = 0.5f;   // 0..1
    float cursorTheta    = 180.0f; // 0..360 deg
    float fieldStrength  = 0.5f;   // 0..1
    float vev            = 0.7f;   // 0.2..0.95
    float selfCoupling   = 0.5f;   // 0..1 (lambda)
    float inertia        = 0.5f;   // 0..1 (Sustain)
    float yukawaBoost    = 0.3f;   // 0..1
    float bosonHz        = 98.0f;  // 40..400 Hz (SND-119/120: 2x49)
    float bosonQ         = 8.0f;   // 1..40 (SND-121/126)
    float symmetry       = 0.5f;   // 0..1
    float decay          = 0.5f;   // 0..1 (dämpft den Boson-Resonator)
    float mix            = 0.10f;  // 0..1 Wet-Pegel (SND-122)
    float lfoRate        = 0.5f;   // 0.1..2 Hz (SND-118)
    float lfoDepth       = 0.25f;  // 0..1 (SND-118)
};

class HiggsFieldEngine
{
public:
    HiggsFieldEngine() = default;

    void prepare (double sampleRate)
    {
        this->sampleRate = sampleRate;

        // SND-123: DC-Blocker bei 8 Hz (vorher 38 Hz).
        const float sr = (float) juce::jmax (1.0, sampleRate);
        dcCoeff = juce::jlimit (0.9f, 0.99999f,
                                1.0f - juce::MathConstants<float>::twoPi * 8.0f / sr);
        reset();
    }

    void reset()
    {
        phiState = 0.0f;
        thetaState = 0.0f;
        lfoPhase = 0.0f;
        dcX1L = dcY1L = dcX1R = dcY1R = 0.0f;
        z1L = z2L = z1R = z2R = 0.0f;
    }

    /** Setzt die Parameter (einmal pro Block, vom Smoothing). */
    void setParams (const HiggsParams& p)
    {
        params = p;

        // Boson-Resonator (Bandpass) aus Boson Hz/Q konfigurieren.
        const float sr   = (float) juce::jmax (1.0, sampleRate);
        const float freq = juce::jlimit (20.0f, 0.45f * (float) sr, params.bosonHz);
        const float q    = juce::jlimit (0.1f, 60.0f, params.bosonQ);
        const float w0   = 2.0f * (float) juce::MathConstants<float>::pi * freq / (float) sr;
        const float alpha = std::sin (w0) / (2.0f * q);
        const float a0 = 1.0f + alpha;

        b0 =  alpha / a0;
        b1 =  0.0f;
        b2 = -alpha / a0;
        a1 = (-2.0f * std::cos (w0)) / a0;
        a2 = (1.0f - alpha) / a0;
    }

    /** Verarbeitet ein Stereo-Sample-Paar und gibt (L, R) zurück.
        Deaktiviert = exakte Passthrough. Aktiv = wet-only: das Trockensignal
        läuft im Patch am Higgs vorbei (SND-117). */
    std::pair<float,float> process (float l, float r)
    {
        if (! params.enabled)
            return { l, r };

        const float sr = (float) juce::jmax (1.0, sampleRate);
        constexpr float twoPi = juce::MathConstants<float>::twoPi;

        // ---- LFO: langsame Bewegung auf Phi/Theta (SND-118) ----
        lfoPhase += juce::jlimit (0.0f, 2.0f, params.lfoRate) / sr;

        if (lfoPhase >= 1.0f)
            lfoPhase -= std::floor (lfoPhase);

        const float lfo   = std::sin (twoPi * lfoPhase);
        const float depth = juce::jlimit (0.0f, 1.0f, params.lfoDepth);

        const float targetPhi = juce::jlimit (0.0f, 1.0f,
                                              params.cursorPhi + depth * 0.5f * lfo);

        float targetTheta = params.cursorTheta + depth * 90.0f * std::cos (twoPi * lfoPhase);
        targetTheta -= 360.0f * std::floor (targetTheta / 360.0f);   // 0..360

        // Phi/Theta-Zustand langsam verfolgen (Trägheit)
        const float inertiaGated = params.inertia * params.symmetry; // symmetry-gated (coeff = baseCoeff * gate)
        const float follow = 0.05f + 0.95f * (1.0f - inertiaGated);

        phiState += (targetPhi - phiState) * follow;

        float dTheta = targetTheta - thetaState;

        if (dTheta > 180.0f)  dTheta -= 360.0f;   // kürzester Weg über den Wrap
        if (dTheta < -180.0f) dTheta += 360.0f;

        thetaState += dTheta * follow;
        thetaState -= 360.0f * std::floor (thetaState / 360.0f);

        // Feld-Modulation: Mexican-Hat-Abweichung vom VEV
        const float field = std::sin (twoPi * phiState)
                          * std::cos (thetaState * (float) juce::MathConstants<float>::pi / 180.0f);
        const float deviation = field - params.vev;

        // SND-124: Abweichung auf -1..+1 normieren, Präsenz auf ±3 dB begrenzen.
        const float posRange = juce::jmax (1.0e-3f, 1.0f - params.vev);
        const float negRange = juce::jmax (1.0e-3f, 1.0f + params.vev);
        const float devNorm = juce::jlimit (-1.0f, 1.0f,
                                            deviation >= 0.0f ? deviation / posRange
                                                              : deviation / negRange);

        const float yukawa = std::pow (10.0f, params.yukawaBoost * 3.0f * devNorm / 20.0f);

        float outL = l * yukawa;
        float outR = r * yukawa;

        // Einfacher Resonator-Effekt (moduliert Gain leicht)
        const float resp = 1.0f + devNorm * params.fieldStrength * 0.15f;
        outL *= resp;
        outR *= resp;

        // DC-Blocker am Wet-Ausgang (8 Hz)
        outL = applyDcBlocker (outL, dcX1L, dcY1L);
        outR = applyDcBlocker (outR, dcX1R, dcY1R);

        // Boson-Resonator (Bandpass bei bosonHz, Q) auf den Eingang
        // Transposed Direct Form II: y = b0*x + z1; z1 = b1*x - a1*y + z2;
        // z2 = b2*x - a2*y. (b1 = 0, aber vollständig geschrieben.)
        const float yL = b0 * l + z1L;
        const float yR = b0 * r + z1R;
        z1L = b1 * l - a1 * yL + z2L;
        z2L = b2 * l - a2 * yL;
        z1R = b1 * r - a1 * yR + z2R;
        z2R = b2 * r - a2 * yR;

        // Decay dämpft den Resonator (höherer Decay = kürzeres Nachklingen).
        const float decayDamp = 1.0f - 0.5f * juce::jlimit (0.0f, 1.0f, params.decay);
        const float bosonAmount = params.selfCoupling * 0.5f * decayDamp;
        outL += bosonAmount * yL;
        outR += bosonAmount * yR;

        // SND-117: wet-only. Mix ist der Pegel des Nasssignals; Dry läuft im
        // Patch am Higgs vorbei, daher kein Dry-Anteil im Return.
        const float wet = juce::jlimit (0.0f, 1.0f, params.mix);
        return { outL * wet, outR * wet };
    }

    float getMix() const { return params.mix; }

private:
    /** Echtes Ein-Pol-Hochpass (y = x - x1 + R*y1), R aus dcCoeff. */
    float applyDcBlocker (float x, float& x1, float& y1)
    {
        const float y = x - x1 + dcCoeff * y1;
        x1 = x;
        y1 = y;
        return y;
    }

    double sampleRate { 48000.0 };
    HiggsParams params;

    float phiState { 0.0f };
    float thetaState { 0.0f };
    float lfoPhase { 0.0f };
    float dcCoeff { 0.99895f };
    float dcX1L { 0.0f }, dcY1L { 0.0f };
    float dcX1R { 0.0f }, dcY1R { 0.0f };

    // Boson-Bandpass (direkt als Biquad, ohne Allokation im Audio-Thread)
    float b0 { 0.0f }, b1 { 0.0f }, b2 { 0.0f }, a1 { 0.0f }, a2 { 0.0f };
    float z1L { 0.0f }, z2L { 0.0f }, z1R { 0.0f }, z2R { 0.0f };
};
