#include "LayeringStage.h"

namespace
{
    constexpr float kTwoPi = 6.283185307179586f;

    inline float onePoleCoeff (float hz, double fs) noexcept
    {
        const float maxHz = 0.45f * (float) fs;
        return std::min (0.9f, std::max (1.0e-5f, kTwoPi * std::min (hz, maxHz) / (float) fs));
    }

    inline float timeCoeff (float ms, double fs) noexcept
    {
        const float seconds = std::max (1.0e-5f, ms * 0.001f);
        return std::exp (-1.0f / (seconds * (float) fs));
    }
}

//==============================================================================
void LayeringStage::prepare (double newSampleRate)
{
    sampleRate = (newSampleRate > 1000.0 ? newSampleRate : 48000.0);
    reset();
}

void LayeringStage::reset()
{
    lowPhase = 0.0f;
    lowEnv = 0.0f;
    lowPeak = 0.0f;
    lowInAttack = false;

    highPhase = 0.0f;
    highEnv = 0.0f;
    highPeak = 0.0f;
    highInAttack = false;
    highDelayCounter = highDelaySamples;

    hpState = 0.0f;
    lpState = 0.0f;
}

//==============================================================================
void LayeringStage::setParams (const Params& newParams)
{
    // Nur beim Ausschalten leeren: ein bereits gefeuerter Trigger (der im
    // selben Block vor dem naechsten setParams liegt) darf nicht verloren
    // gehen, wenn das Layering gerade erst eingeschaltet wurde.
    if (! newParams.enabled && params.enabled)
        reset();

    params = newParams;
    updateDerived();
}

void LayeringStage::updateDerived()
{
    info.enabled = params.enabled;

    if (! params.enabled)
    {
        baseTrim = 1.0f;
        lowWeight = 0.0f;
        highWeight = 0.0f;
        info.lowGainDb = -120.0f;
        info.highGainDb = -120.0f;
        info.baseTrimDb = 0.0f;
        return;
    }

    const float f0 = std::min (140.0f, std::max (20.0f, params.fundamentalHz));

    // ------------------------------------------------------------------
    // Layer 1 "DEPTH": Grundton-Stuetzung, lange Huelle.
    // Unter 55 Hz bleibt der Layer auf dem Grundton (auf TV-Boxen waere die
    // Oktave darunter nicht mehr hoerbar); darueber legt er eine Oktave
    // tiefer eine zusaetzliche Tragschicht.
    // ------------------------------------------------------------------
    lowRatio = (f0 >= 55.0f) ? 0.5f : 1.0f;
    info.lowHz = std::max (14.0f, f0 * lowRatio);

    // Attack aus dem Body-Attack (etwas weicher, damit kein Zweit-Klick
    // entsteht), Decay aus dem Sub-Decay (traegt bis in den Rumble).
    // M10: Decay = subDecay * 0.9 (vorher 1.05) - der Depth-Layer ist eine
    // Duplikation des Sub-Oszillators; mit dem laengeren Decay schwingt er
    // im Sub nach und verdeckt den Sub. 0.9 laesst ihn etwas frueher
    // auslaufen als der Sub-Oszillator.
    info.lowAttackMs = std::min (18.0f, std::max (1.5f, params.bodyAttackMs * 0.8f));
    info.lowDecayMs  = std::min (1400.0f, std::max (200.0f, params.subDecayMs * 0.9f));

    // Pegel: getragen von Sub-Level, leicht angehoben durch Drive.
    float lowW = 0.080f + 0.100f * clamp01 (params.subLevel)
                       + 0.030f * clamp01 (params.driveAmount);

    // Kleiner 2. Oberton: macht den Layer auch auf kleinen Speakern
    // (TV/Bluetooth) hoerbar, ohne ihn metallisch zu machen.
    harmonic = 0.10f + 0.10f * clamp01 (params.driveAmount);

    // ------------------------------------------------------------------
    // Layer 2 "PRESENCE": kurzer, bandbegrenzter Textur-Layer.
    // Bandlage folgt Click-Tone, Laenge dem Click-Decay, Delay wenige ms
    // (Definition statt Doppelklick). Dichte steuert Rausch-/Ton-Anteil.
    // ------------------------------------------------------------------
    info.highHz = std::min (7200.0f, std::max (1200.0f,
                                               1400.0f + 4200.0f * clamp01 (params.clickTone)));
    info.highAttackMs = std::min (4.0f, std::max (0.6f,
                                                  0.6f + params.clickDecayMs * 0.03f));
    info.highDecayMs  = std::min (110.0f, std::max (14.0f, params.clickDecayMs * 2.4f));
    info.highDelayMs  = std::min (9.0f, std::max (0.6f,
                                                  0.6f + params.clickDecayMs * 0.12f));

    float highW = 0.055f + 0.100f * clamp01 (params.clickLevel);

    info.density = clamp01 (0.28f + 0.50f * clamp01 (params.clickDistortion)
                                 + 0.22f * clamp01 (params.driveAmount));

    // Hohe Dichte = mehr Rausch-Textur, niedrige = mehr tonaler Anteil.
    toneMix = std::min (0.75f, std::max (0.10f, 0.62f - 0.45f * info.density));

    // ------------------------------------------------------------------
    // Pegelneutralitaet.
    // Der Depth-Layer liegt auf demselben Grundton wie die Basis und summiert
    // sich daher *kohaerent* (nicht nur energetisch). Deshalb wird der
    // Basis-Trim mit einem teil-kohaerenten Budget gerechnet:
    //     T = 0.985 / sqrt(1 + k1^2 + k2^2 + 1.0*k1)
    // Der 1.0-Term bildet die Phasen-Kohaerenz des Depth-Layers ab, der
    // 0.985-Faktor ist eine kleine Sicherheits-Reserve (Messung ueber alle
    // Factory-Presets: Layering bleibt innerhalb +0.7 dB).
    // Ergebnis: RMS bleibt praktisch unveraendert (kein "nur lauter"),
    // waehrend das Spektrum nach unten (Depth) und oben (Presence) breiter wird.
    // ------------------------------------------------------------------
    lowW  = std::min (0.26f, lowW);
    highW = std::min (0.18f, highW);

    // Layering Amount (0–200 %): skaliert beide Layer-Pegel. Die Kompensation
    // wird mit denselben Gewichten gerechnet, bleibt also mitgezogen.
    const float amount = std::min (2.0f, std::max (0.0f, params.amountPct * 0.01f));
    info.amountPct = amount * 100.0f;

    lowW  *= amount;
    highW *= amount;

    if (lowW + highW <= 1.0e-6f)
    {
        // Amount 0 %: exakt transparent (kein Trim, keine Layer).
        baseTrim   = 1.0f;
        lowWeight  = 0.0f;
        highWeight = 0.0f;
        info.lowGainDb  = -120.0f;
        info.highGainDb = -120.0f;
        info.baseTrimDb = 0.0f;

        lowAttackCoeff  = timeCoeff (info.lowAttackMs, sampleRate);
        lowDecayCoeff   = timeCoeff (info.lowDecayMs, sampleRate);
        highAttackCoeff = timeCoeff (info.highAttackMs, sampleRate);
        highDecayCoeff  = timeCoeff (info.highDecayMs, sampleRate);
        highDelaySamples = std::max (0, (int) std::lround (info.highDelayMs * 0.001f
                                                              * (float) sampleRate));
        return;
    }

    const float sumSq = lowW * lowW + highW * highW + 1.0f * lowW;
    baseTrim = 0.985f / std::sqrt (1.0f + sumSq);

    lowWeight  = lowW  * baseTrim * (params.tubeStageOn ? 0.72f : 1.0f);
    highWeight = highW * baseTrim;

    info.lowGainDb  = 20.0f * std::log10 (std::max (1.0e-6f, lowWeight));
    info.highGainDb = 20.0f * std::log10 (std::max (1.0e-6f, highWeight));
    info.baseTrimDb = 20.0f * std::log10 (std::max (1.0e-6f, baseTrim));

    // Koeffizienten (blockweise, keine Transzendenten im Sample-Loop).
    lowAttackCoeff  = timeCoeff (info.lowAttackMs, sampleRate);
    lowDecayCoeff   = timeCoeff (info.lowDecayMs, sampleRate);
    highAttackCoeff = timeCoeff (info.highAttackMs, sampleRate);
    highDecayCoeff  = timeCoeff (info.highDecayMs, sampleRate);

    highDelaySamples = std::max (0, (int) std::lround (info.highDelayMs * 0.001f
                                                          * (float) sampleRate));
}

//==============================================================================
void LayeringStage::trigger (float velocity)
{
    const float vel = clamp01 (velocity);

    lowPeak = vel;
    lowEnv = 0.0f;
    lowInAttack = true;
    lowPhase = 0.0f;

    highPeak = vel;
    highEnv = 0.0f;
    highInAttack = true;
    highPhase = 0.0f;
    highDelayCounter = 0;

    hpState = 0.0f;
    lpState = 0.0f;

    // Neuer, aber deterministischer Rausch-Startwert pro Hit.
    noiseState = noiseState * 1664525u + 1013904223u;
}

//==============================================================================
void LayeringStage::process (float instantaneousFundamentalHz, float& lowOut, float& highOut)
{
    lowOut = 0.0f;
    highOut = 0.0f;

    if (! params.enabled)
        return;

    const float fs = (float) sampleRate;

    // ------------------------------------------------------------------
    // Layer 1: DEPTH
    // ------------------------------------------------------------------
    if (lowInAttack)
    {
        lowEnv += (lowPeak - lowEnv) * lowAttackCoeff;

        if (lowEnv >= lowPeak * 0.99f)
        {
            lowEnv = lowPeak;
            lowInAttack = false;
        }
    }
    else
    {
        lowEnv *= lowDecayCoeff;

        if (lowEnv < 1.0e-5f)
            lowEnv = 0.0f;
    }

    if (lowEnv > 0.0f)
    {
        // Exakt dieselbe Momentanfrequenz wie die Basis (Pitch-Env + Glide).
        const float freq = std::min (0.45f * fs,
                                     std::max (10.0f, instantaneousFundamentalHz * lowRatio));
        lowPhase += kTwoPi * freq / fs;

        if (lowPhase >= kTwoPi)
            lowPhase -= kTwoPi;

        const float fundamental = std::sin (lowPhase);

        if (! std::isfinite (fundamental))
        {
            lowPhase = 0.0f;
            lowEnv = 0.0f;
        }
        else
        {
            const float shape = fundamental + harmonic * std::sin (lowPhase * 2.0f);
            lowOut = shape * lowEnv * lowWeight;
        }
    }

    // ------------------------------------------------------------------
    // Layer 2: PRESENCE
    // ------------------------------------------------------------------
    if (highDelayCounter < highDelaySamples)
    {
        ++highDelayCounter;
        return;
    }

    if (highInAttack)
    {
        highEnv += (highPeak - highEnv) * highAttackCoeff;

        if (highEnv >= highPeak * 0.99f)
        {
            highEnv = highPeak;
            highInAttack = false;
        }
    }
    else
    {
        highEnv *= highDecayCoeff;

        if (highEnv < 1.0e-5f)
            highEnv = 0.0f;
    }

    if (highEnv <= 0.0f)
        return;

    // Bandlage folgt der Modulation leicht (sqrt-Tracking), damit auch der
    // Praesenz-Layer mit der Pitch-Bewegung atmet.
    const float f0 = std::max (20.0f, params.fundamentalHz);
    const float tracking = std::sqrt (std::min (4.0f, std::max (0.25f,
                                                                instantaneousFundamentalHz / f0)));
    const float centreHz = std::min (0.20f * fs,
                                     std::max (300.0f, info.highHz * tracking));

    const float aHp = onePoleCoeff (centreHz * 0.6f, sampleRate);
    const float aLp = onePoleCoeff (centreHz * 1.6f, sampleRate);

    // Deterministisches Rauschen (-1..1).
    noiseState = noiseState * 1664525u + 1013904223u;
    const float noise = (float) ((noiseState >> 8) & 0xFFFFFFu) * (2.0f / 16777215.0f) - 1.0f;

    hpState += aHp * (noise - hpState);
    const float highPassed = noise - hpState;
    lpState += aLp * (highPassed - lpState);
    const float banded = lpState;

    highPhase += kTwoPi * centreHz / fs;

    if (highPhase >= kTwoPi)
        highPhase -= kTwoPi;

    const float tonal = std::sin (highPhase);

    if (! std::isfinite (banded) || ! std::isfinite (tonal))
    {
        hpState = 0.0f;
        lpState = 0.0f;
        highPhase = 0.0f;
        highEnv = 0.0f;
        return;
    }

    highOut = (toneMix * tonal + (1.0f - toneMix) * banded) * highEnv * highWeight;
}
