#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
/** MaterialReverb — 8x8-FDN-Reverb (§4.7, SND-101…116, E4/E15).

    - 8 Delay-Zeilen mit teilerfremden Primzahl-Längen (1103, 1277, 1427, 1583,
      1721, 1871, 2017, 2179 Samples bei 48 kHz), skaliert mit Sample-Rate und
      reverbSize; Längenänderungen werden über 20 ms geglättet (SND-116)
    - Verlustfreie 8x8-Hadamard-Matrix (1/sqrt(8), orthonormal) im Rückkopplungspfad
    - RT60 (reverbDecay in ms) -> Feedback-Gain pro Zeile mit material-spezifischen
      RT60/Dämpfungs-Kurven (kurze vs. lange Zeilen) statt gemeinsamem Netzwerk
    - Eingangs-Diffusion: 4 Allpässe 7/11/17/23 ms, g = 0.5, L/R getrennt (SND-102)
      + mindestens 1 Allpass pro Delay-Zeile im Feedbackpfad (stabile Mischung)
    - Material-spezifischer Tonfilter/EQ vor dem FDN (HP/LP/Tilt je Material)
    - Delay-Modulation: ±0.3 % bei 0.2 Hz, Zeilen phasenversetzt (SND-103)
    - Pre-Delay (0–150 ms, identisch L/R) als Ringbuffer
    - Damping: One-Pole-Lowpass pro Zeile im Feedbackpfad + materialabhängige Kurve
    - LowCut (One-Pole-HP) und HighCut (One-Pole-LP) pro Zeile im Feedbackpfad
    - DC-Blocker pro Zeile (korrekt, auch bei Freeze/Gate)
    - Stereo: Injektion L -> Zeilen 0/2/4/6, R -> 1/3/5/7 (SND-113),
      Auslese L = 0/2/4/6, R = 1/3/5/7
    - Material 1..6 (Wood/Metal/Plastic/Concrete/Glass/Stone) mit klar
      unterschiedlichen RT60-/Filter-Kurven und pre-FDN-Timbre (physikalisch
      nachvollziehbar relativiert über Dichte/Härte/Absorption -> Filter/RT60)
    - Freeze (Feedback bleibt stehen, kein Input) und Gate 10–500 ms (SND-112)
    - Mix (SND-105): Wet*mix als Send/Return-Gain
    - Bypass: Material 0 oder mix = 0 -> Ausgang 0. Pre-Delay läuft weiter,
      Feedback-Zustand wird beim Re-Enable geleert
    - NaN/Inf-Guard pro Zeile, Denormal-Flush (< 1e-20)
    - Keine Allokation, keine Locks, keine Prints in process()
*/
class MaterialReverb
{
public:
    MaterialReverb() = default;

    /** Struktur-Abfragen für Tests/Diagnose (SND-104). */
    static constexpr int getNumLines() noexcept { return 8; }
    static constexpr int getNumInputDiffusers() noexcept { return 4; }

    void prepare (double sampleRate, int maxBlockSize)
    {
        juce::ignoreUnused (maxBlockSize);

        this->sampleRate = juce::jmax (1000.0, sampleRate);
        const float fs = (float) this->sampleRate;

        const int maxDelaySamples = (int) (this->sampleRate * 0.150) + 16;

        delayLines.assign ((size_t) kNumLines, std::vector<float> ((size_t) maxDelaySamples, 0.0f));
        writePos.fill (0);
        currentDelay.fill (1000.0f);
        targetDelay.fill (1000.0f);

        // ---- Eingangs-Diffusion (SND-102): 7/11/17/23 ms, g = 0.5 ----
        static constexpr float diffusionMs[kNumDiffusers] { 7.0f, 11.0f, 17.0f, 23.0f };
        diffusionL.assign ((size_t) kNumDiffusers, std::vector<float>());
        diffusionR.assign ((size_t) kNumDiffusers, std::vector<float>());
        for (int d = 0; d < kNumDiffusers; ++d)
        {
            const int len = juce::jmax (8, (int) std::lround ((double) this->sampleRate * diffusionMs[d] / 1000.0));
            diffusionL[(size_t) d].assign ((size_t) len, 0.0f);
            diffusionR[(size_t) d].assign ((size_t) len, 0.0f);
            diffusionDelay[(size_t) d] = len - 1;
        }
        diffWritePosL.fill (0);
        diffWritePosR.fill (0);

        // ---- Per-Zeilen-Diffusion: mindestens 1 Allpass pro Delay-Zeile ----
        static constexpr int lineAllpassBase[kNumLines] { 149, 211, 263, 293, 331, 379, 421, 463 };
        for (int i = 0; i < kNumLines; ++i)
        {
            const int len = juce::jmax (8, (int) std::lround ((double) lineAllpassBase[i] * (this->sampleRate / 48000.0)));
            lineDiffusion[(size_t) i].assign ((size_t) len, 0.0f);
            lineDiffDelay[(size_t) i] = len - 1;
            lineDiffWrite[(size_t) i] = 0;
        }

        preDelayBufferL.assign ((size_t) maxDelaySamples, 0.0f);
        preDelayBufferR.assign ((size_t) maxDelaySamples, 0.0f);
        preDelayWritePos = 0;
        preDelaySamples = 0;

        processedActive = false;
        parametersActive = false;
        pendingTailClear = false;
        snapCoefficients = true;

        coeffSmooth = 1.0f - std::exp (-1.0f / (0.02f * fs));
        sizeSmooth  = 1.0f - std::exp (-1.0f / (0.02f * fs));
        gateSmooth  = 1.0f - std::exp (-1.0f / (0.002f * fs));
        modInc      = kModRateHz / fs;
        modPhase    = 0.0f;

        gateGain = 1.0f;
        gateHoldSamples = 0;
        frozen = false;

        clearTails();
        clearDiffusion();
        preToneHpL = preToneHpR = 0.0f;
        preToneLpL = preToneLpR = 0.0f;

        setParameters (0, 30.0f, 1200.0f, 0.5f, 0.3f, 0.15f, 150.0f, 6000.0f);
    }

    void reset()
    {
        clearTails();
        clearDiffusion();
        std::fill (preDelayBufferL.begin(), preDelayBufferL.end(), 0.0f);
        std::fill (preDelayBufferR.begin(), preDelayBufferR.end(), 0.0f);
        preDelayWritePos = 0;
        preToneHpL = preToneHpR = 0.0f;
        preToneLpL = preToneLpR = 0.0f;
        processedActive = false;
        parametersActive = false;
        pendingTailClear = false;
        snapCoefficients = true;
        gateGain = 1.0f;
        gateHoldSamples = 0;
        modPhase = 0.0f;
    }

    void setParameters (int material,
                        float preDelayMs,
                        float decayMs,
                        float size,
                        float damping,
                        float mix,
                        float lowCutHz,
                        float highCutHz)
    {
        this->material   = juce::jlimit (0, 6, material);
        this->preDelayMs = juce::jlimit (0.0f, 150.0f, preDelayMs);
        this->decayMs    = juce::jlimit (20.0f, 10000.0f, decayMs);
        this->size       = juce::jlimit (0.0f, 1.0f, size);
        this->damping    = juce::jlimit (0.0f, 1.0f, damping);
        this->mix        = juce::jlimit (0.0f, 1.0f, mix);
        this->lowCutHz   = juce::jlimit (0.0f, 20000.0f, lowCutHz);
        this->highCutHz  = juce::jlimit (0.0f, 20000.0f, highCutHz);

        const float fs = (float) sampleRate;
        const MaterialProfile profile = profileFor (this->material);

        const int pdSize = (int) preDelayBufferL.size();
        const int maxPreDelay = juce::jmax (0, pdSize - 1);
        preDelaySamples = juce::jlimit (0, maxPreDelay,
                                        (int) (sampleRate * this->preDelayMs / 1000.0));

        // SND-101: RT60-Kalibrierung +2 dB
        const float baseDecaySec = juce::jlimit (0.02f, 10.0f,
                                                 this->decayMs * profile.decayMul / 1000.0f)
                                 * kRt60Calibration;

        const float sizeValue = juce::jlimit (0.0f, 1.0f, this->size * profile.sizeMul);
        const float dampingV  = applyDampingOffset (this->damping, profile.dampingOffset);

        const float srScale = fs / 48000.0f;
        const float sizeScale = 0.5f + sizeValue * 1.5f; // 0.5 .. 2.0

        // Material-spezifische RT60/Dämpfungs-Kurven: pro Zeile abhängig von Länge
        // (kurze Zeilen ~ frühe/hohe Moden, lange ~ später Tail).
        for (int i = 0; i < kNumLines; ++i)
        {
            const int bufSize = (i < (int) delayLines.size()) ? (int) delayLines[(size_t) i].size() : 0;
            const float wanted = juce::jmax (8.0f, (float) baseDelays[i] * srScale * sizeScale);
            const float maxDelay = (float) (bufSize > 8 ? bufSize - 4 : (int) wanted);
            const float clamped = juce::jlimit (8.0f, juce::jmax (8.0f, maxDelay), wanted);

            targetDelay[(size_t) i] = clamped;

            const float normLen = (float) (baseDelays[i] - baseDelays[0])
                                / (float) (baseDelays[kNumLines - 1] - baseDelays[0]); // 0..1
            const float rt60Line = baseDecaySec
                                 * std::pow (juce::jmax (0.1f, profile.rt60Curve), (normLen - 0.5f) * 0.9f)
                                 * (0.85f + 0.35f * sizeValue);
            const float g = std::pow (10.0f, -3.0f * clamped / (juce::jmax (0.02f, rt60Line) * fs));
            targetFeedbackGain[(size_t) i] = juce::jlimit (0.0f, 0.9995f, g);

            // Dämpfungskurve: kürzere Zeilen werden materialabhängig stärker gedämpft
            const float dampShape = 1.0f + (1.0f - normLen) * (profile.dampingCurve - 1.0f) * 0.8f;
            const float lineDamp = juce::jlimit (0.0f, 1.0f, dampingV * juce::jmax (0.25f, dampShape));
            const float dampHz = juce::jlimit (20.0f, 0.45f * fs, 16000.0f - lineDamp * 15200.0f);
            targetDampCoeffLine[(size_t) i] = onePoleCoeff (dampHz, fs);
        }

        // Globale Filterkoeffizienten (für Kompatibilität/geglättete Basis)
        const float dampHz = juce::jlimit (20.0f, 0.45f * fs, 16000.0f - dampingV * 15200.0f);
        targetDampCoeff = onePoleCoeff (dampHz, fs);

        hpActive = this->lowCutHz > 0.5f;
        const float lowCut = juce::jlimit (20.0f, 0.45f * fs,
                                           juce::jmax (20.0f, this->lowCutHz) * profile.lowCutMul);
        targetHpCoeff = onePoleCoeff (lowCut, fs);

        const float minHighCut = juce::jmin (lowCut * 1.2f, 0.45f * fs);
        const float highCut = juce::jlimit (minHighCut, 0.45f * fs,
                                            juce::jmax (minHighCut, this->highCutHz) * profile.highCutMul);
        lpActive = highCut < 0.45f * fs;
        targetLpCoeff = onePoleCoeff (highCut, fs);

        // ---- Material-spezifischer Tonfilter/EQ vor dem FDN ----
        const float preHpHz = juce::jlimit (20.0f, 0.45f * fs, profile.preHpHz);
        const float preLpHz = juce::jlimit (preHpHz + 20.0f, 0.45f * fs, profile.preLpHz);
        targetPreHpCoeff = onePoleCoeff (preHpHz, fs);
        targetPreLpCoeff = onePoleCoeff (preLpHz, fs);
        targetPreTilt = juce::jlimit (0.0f, 1.0f, profile.preTilt);

        lineDiffGain = juce::jlimit (0.1f, 0.65f, profile.lineDiffGain);

        if (snapCoefficients)
        {
            feedbackGain = targetFeedbackGain;
            dampCoeffLine = targetDampCoeffLine;
            dampCoeff    = targetDampCoeff;
            hpCoeff      = targetHpCoeff;
            lpCoeff      = targetLpCoeff;
            preHpCoeff   = targetPreHpCoeff;
            preLpCoeff   = targetPreLpCoeff;
            preTilt      = targetPreTilt;
            currentDelay = targetDelay;
            snapCoefficients = false;
        }

        const bool nowActive = this->material != 0 && this->mix > 0.0f;
        if (nowActive && ! parametersActive)
            pendingTailClear = true;
        parametersActive = nowActive;
    }

    /** SND-112: Freeze hält den Hall stehen (Feedback = 1, kein neuer Input). */
    void setFreeze (bool shouldFreeze) noexcept { frozen = shouldFreeze; }

    /** SND-112: Gate 0 = aus, sonst 10–500 ms Haltezeit nach dem letzten Input. */
    void setGateMs (float ms) noexcept
    {
        gateMs = (ms <= 0.0f) ? 0.0f : juce::jlimit (10.0f, 500.0f, ms);
        if (gateMs <= 0.0f)
            gateHoldSamples = 0;
    }

    float getMix() const { return mix; }

    /** SND-111: effektive RT60 in Sekunden (Material-Mul + Kalibrierung). */
    static float effectiveDecaySeconds (int material, float decayMs) noexcept
    {
        const float decaySec = juce::jlimit (0.02f, 10.0f,
                                             juce::jlimit (20.0f, 10000.0f, decayMs)
                                                 * profileFor (material).decayMul / 1000.0f);
        return decaySec * kRt60Calibration;
    }

    /** Grobe RT60-Schätzung für Tests/Analyse (Material + Size + Damping + Kurve). */
    static float estimatedRt60Seconds (int material, float decayMs, float size, float damping) noexcept
    {
        const auto p = profileFor (material);
        const float sizeValue = juce::jlimit (0.0f, 1.0f, juce::jlimit (0.0f, 1.0f, size) * p.sizeMul);
        const float dampingV  = applyDampingOffset (juce::jlimit (0.0f, 1.0f, damping), p.dampingOffset);
        float rt60 = effectiveDecaySeconds (material, decayMs) * (0.82f + 0.38f * sizeValue);
        rt60 *= (1.0f - 0.40f * dampingV);
        rt60 *= (0.88f + 0.30f * (p.rt60Curve - 1.0f));
        return juce::jlimit (0.02f, 12.0f, rt60);
    }

    /** Verarbeitet ein Stereo-Sample-Paar. Gibt Wet*mix zurück (SND-105, Send/Return-Gain). */
    std::pair<float,float> process (float inputL, float inputR)
    {
        const int pdSize = (int) preDelayBufferL.size();
        if (pdSize < 2 || (int) delayLines.size() < kNumLines)
            return { 0.0f, 0.0f };

        if (! std::isfinite (inputL)) inputL = 0.0f;
        if (! std::isfinite (inputR)) inputR = 0.0f;
        inputL = juce::jlimit (-32.0f, 32.0f, inputL);
        inputR = juce::jlimit (-32.0f, 32.0f, inputR);

        // ---- Pre-Delay läuft immer weiter (auch im Bypass) ----
        preDelayBufferL[(size_t) preDelayWritePos] = inputL;
        preDelayBufferR[(size_t) preDelayWritePos] = inputR;

        int pdRead = preDelayWritePos - preDelaySamples;
        if (pdRead < 0) pdRead += pdSize;

        const float pdL = preDelayBufferL[(size_t) pdRead];
        const float pdR = preDelayBufferR[(size_t) pdRead];

        if (++preDelayWritePos >= pdSize) preDelayWritePos = 0;

        // ---- SND-102: Eingangs-Diffusion (4 Allpässe, L/R getrennt) ----
        float diffL = pdL;
        float diffR = pdR;
        for (int d = 0; d < kNumDiffusers; ++d)
        {
            diffL = processAllpass (diffusionL[(size_t) d], diffWritePosL[(size_t) d],
                                    diffusionDelay[(size_t) d], kDiffusionGain, diffL);
            diffR = processAllpass (diffusionR[(size_t) d], diffWritePosR[(size_t) d],
                                    diffusionDelay[(size_t) d], kDiffusionGain, diffR);
        }

        // ---- Material-spezifischer Tonfilter/EQ vor dem FDN ----
        preHpCoeff += (targetPreHpCoeff - preHpCoeff) * coeffSmooth;
        preLpCoeff += (targetPreLpCoeff - preLpCoeff) * coeffSmooth;
        preTilt    += (targetPreTilt    - preTilt)    * coeffSmooth;
        diffL = applyPreTone (diffL, preToneHpL, preToneLpL);
        diffR = applyPreTone (diffR, preToneHpR, preToneLpR);

        const bool wantActive = material != 0 && mix > 0.0f;
        if (! wantActive)
        {
            if (processedActive)
            {
                clearTails();
                clearDiffusion();
            }
            processedActive = false;
            return { 0.0f, 0.0f };
        }

        if (pendingTailClear || ! processedActive)
        {
            clearTails();
            clearDiffusion();
            pendingTailClear = false;
            processedActive = true;
        }

        // ---- SND-112: Gate ----
        const float inLevel = juce::jmax (std::abs (inputL), std::abs (inputR));
        if (gateMs > 0.0f)
        {
            if (inLevel > kGateThreshold)
                gateHoldSamples = (int) (gateMs * 0.001f * (float) sampleRate);
            else if (gateHoldSamples > 0)
                --gateHoldSamples;

            const float targetGate = (gateHoldSamples > 0) ? 1.0f : 0.0f;
            gateGain += (targetGate - gateGain) * gateSmooth;
            gateGain = flushDenormal (gateGain);
        }
        else
        {
            gateGain = 1.0f;
            gateHoldSamples = 0;
        }

        // ---- Koeffizienten- und Size-Glättung, samplesweise (SND-116) ----
        for (int i = 0; i < kNumLines; ++i)
        {
            feedbackGain[(size_t) i] += (targetFeedbackGain[(size_t) i] - feedbackGain[(size_t) i]) * coeffSmooth;
            dampCoeffLine[(size_t) i] += (targetDampCoeffLine[(size_t) i] - dampCoeffLine[(size_t) i]) * coeffSmooth;
            currentDelay[(size_t) i] += (targetDelay[(size_t) i] - currentDelay[(size_t) i]) * sizeSmooth;
        }
        hpCoeff += (targetHpCoeff - hpCoeff) * coeffSmooth;
        lpCoeff += (targetLpCoeff - lpCoeff) * coeffSmooth;
        dampCoeff += (targetDampCoeff - dampCoeff) * coeffSmooth;

        modPhase += modInc;
        if (modPhase >= 1.0f) modPhase -= 1.0f;

        // ---- Taps lesen, filter, DC-Block, Diffusion pro Zeile ----
        std::array<float, kNumLines> taps {};
        for (int i = 0; i < kNumLines; ++i)
        {
            auto& line = delayLines[(size_t) i];
            const int bufSize = (int) line.size();

            const float lfo = std::sin (juce::MathConstants<float>::twoPi
                                        * (modPhase + (float) i / (float) kNumLines));
            const float delaySamples = juce::jlimit (4.0f, (float) (bufSize - 3),
                                                     currentDelay[(size_t) i] * (1.0f + kModDepth * lfo));

            float tap = readFractional (line, writePos[(size_t) i], delaySamples);
            if (! std::isfinite (tap)) { clearLine (i); tap = 0.0f; }

            if (frozen)
            {
                hpState[(size_t) i]   = tap;
                dampState[(size_t) i] = tap;
                hiState[(size_t) i]   = tap;
            }
            else
            {
                if (hpActive)
                {
                    hpState[(size_t) i] += hpCoeff * (tap - hpState[(size_t) i]);
                    tap -= hpState[(size_t) i];
                }

                dampState[(size_t) i] += dampCoeffLine[(size_t) i] * (tap - dampState[(size_t) i]);
                tap = dampState[(size_t) i];

                if (lpActive)
                {
                    hiState[(size_t) i] += lpCoeff * (tap - hiState[(size_t) i]);
                    tap = hiState[(size_t) i];
                }
            }

            // DC-Blocker pro Zeile (korrekt/stabil), auch bei Freeze
            const float dcOut = tap - dcX1[(size_t) i] + kDcBlockerR * dcY1[(size_t) i];
            dcX1[(size_t) i] = tap;
            dcY1[(size_t) i] = dcOut;
            tap = dcOut;

            // Mindestens 1 Allpass pro Zeile im Feedback/Tap-Pfad
            tap = processAllpass (lineDiffusion[(size_t) i], lineDiffWrite[(size_t) i],
                                  lineDiffDelay[(size_t) i], lineDiffGain, tap);

            taps[(size_t) i] = flushDenormal (tap);
        }

        // ---- Verlustfreie 8x8-Hadamard-Matrix ----
        std::array<float, kNumLines> mixed = taps;
        for (int stride = 1; stride < kNumLines; stride <<= 1)
            for (int base = 0; base < kNumLines; base += 2 * stride)
                for (int j = 0; j < stride; ++j)
                {
                    const float a = mixed[(size_t) (base + j)];
                    const float b = mixed[(size_t) (base + j + stride)];
                    mixed[(size_t) (base + j)]          = a + b;
                    mixed[(size_t) (base + j + stride)] = a - b;
                }
        for (int i = 0; i < kNumLines; ++i) mixed[(size_t) i] *= kHadamardScale;

        // ---- Injektion + Rückkopplung, zurückschreiben ----
        for (int i = 0; i < kNumLines; ++i)
        {
            auto& line = delayLines[(size_t) i];
            const int bufSize = (int) line.size();
            const int wPos = writePos[(size_t) i];

            const bool isLeft = (i % 2) == 0;
            const float input = isLeft ? diffL : diffR;
            const float injected = frozen ? 0.0f : 0.5f * input * injectSign[i];

            const float lineGain = frozen ? 1.0f : feedbackGain[(size_t) i] * gateGain;
            float value = mixed[(size_t) i] * lineGain + injected;

            if (! std::isfinite (value)) { clearLine (i); value = 0.0f; }

            value = juce::jlimit (-4.0f, 4.0f, value); // stabile Mischung, kein Clipping-Aufbau
            line[(size_t) wPos] = flushDenormal (value);
            writePos[(size_t) i] = (wPos + 1 >= bufSize) ? 0 : wPos + 1;
        }

        float wetL = 0.25f * (taps[0] + taps[2] + taps[4] + taps[6]);
        float wetR = 0.25f * (taps[1] + taps[3] + taps[5] + taps[7]);
        if (! frozen) { wetL *= gateGain; wetR *= gateGain; }

        if (! std::isfinite (wetL)) wetL = 0.0f;
        if (! std::isfinite (wetR)) wetR = 0.0f;

        return { wetL * mix, wetR * mix };
    }

private:
    static constexpr int kNumLines = 8;
    static constexpr int kNumDiffusers = 4;
    static constexpr float kDenormalThreshold = 1.0e-20f;
    static constexpr float kHadamardScale = 0.35355339f;   // 1/sqrt(8)
    static constexpr float kModRateHz = 0.2f;              // SND-103
    static constexpr float kModDepth = 0.003f;             // ±0.3 %
    static constexpr float kDiffusionGain = 0.5f;          // SND-102
    static constexpr float kDcBlockerR = 0.995f;           // DC-Blocker pro Zeile
    static constexpr float kGateThreshold = 1.0e-4f;       // SND-112
    static constexpr float kRt60Calibration = 1.07978f;    // SND-101: +2 dB

    static constexpr int baseDelays[kNumLines] { 1103, 1277, 1427, 1583, 1721, 1871, 2017, 2179 };
    static constexpr float injectSign[kNumLines] { 1.0f, -1.0f, -1.0f, 1.0f, 1.0f, -1.0f, -1.0f, 1.0f };

    struct MaterialProfile
    {
        const char* name;
        float decayMul;
        float sizeMul;
        float dampingOffset;
        float lowCutMul;
        float highCutMul;
        float preHpHz;       // pre-FDN Tonfilter
        float preLpHz;
        float preTilt;       // 0..1 (hell/klingend)
        float dampingCurve;  // >1 stärkere HF-Dämpfung auf kurzen Zeilen
        float rt60Curve;     // >1 längerer Tail auf langen Zeilen
        float lineDiffGain;  // Allpass-Gain pro Zeile
    };

    static MaterialProfile profileFor (int mat) noexcept
    {
        // 1..6 = Wood/Metal/Plastic/Concrete/Glass/Stone (Layout-konform).
        // Physikalisch relativiert (Dichte/Härte/Absorption -> RT60/Filter):
        switch (mat)
        {
            case 1:  return { "Wood",     1.00f, 0.90f,  0.20f, 1.10f, 0.70f, 120.0f,  5200.0f, 0.45f, 1.25f, 1.00f, 0.45f };
            case 2:  return { "Metal",    1.65f, 1.20f, -0.30f, 0.70f, 1.80f,  60.0f, 16000.0f, 0.90f, 0.70f, 1.25f, 0.60f };
            case 3:  return { "Plastic",  0.55f, 0.75f,  0.10f, 1.50f, 0.85f, 180.0f,  6000.0f, 0.50f, 1.10f, 0.85f, 0.40f };
            case 4:  return { "Concrete", 1.35f, 1.30f,  0.05f, 0.80f, 0.80f,  80.0f,  7000.0f, 0.55f, 1.00f, 1.30f, 0.55f };
            case 5:  return { "Glass",    0.85f, 0.95f, -0.35f, 1.80f, 1.90f, 140.0f, 14000.0f, 0.85f, 0.80f, 0.95f, 0.50f };
            case 6:  return { "Stone",    1.10f, 1.15f,  0.30f, 1.00f, 0.55f, 100.0f,  4200.0f, 0.35f, 1.35f, 1.10f, 0.45f };
            default: return { "None",     1.00f, 1.00f,  0.00f, 1.00f, 1.00f,  20.0f, 20000.0f, 0.50f, 1.00f, 1.00f, 0.50f };
        }
    }

    static float applyDampingOffset (float damping, float offset) noexcept
    {
        const float off = juce::jlimit (-1.0f, 1.0f, offset);
        const float blended = off >= 0.0f ? damping + off * (1.0f - damping)
                                          : damping * (1.0f + off);
        return juce::jlimit (0.0f, 1.0f, blended);
    }

    static float onePoleCoeff (float cutoffHz, float fs) noexcept
    {
        const float fc = juce::jlimit (5.0f, 0.45f * fs, cutoffHz);
        return 1.0f - std::exp (-2.0f * juce::MathConstants<float>::pi * fc / fs);
    }

    static float flushDenormal (float v) noexcept
    {
        return std::abs (v) < kDenormalThreshold ? 0.0f : v;
    }

    static float readFractional (const std::vector<float>& line, int wPos, float delaySamples) noexcept
    {
        const int bufSize = (int) line.size();
        float readPos = (float) wPos - delaySamples;
        while (readPos < 0.0f) readPos += (float) bufSize;
        const int i0 = (int) readPos;
        const float frac = readPos - (float) i0;
        const int i1 = (i0 + 1 >= bufSize) ? 0 : i0 + 1;
        return line[(size_t) i0] + frac * (line[(size_t) i1] - line[(size_t) i0]);
    }

    static float processAllpass (std::vector<float>& buf, int& wPos, int delaySamples, float gain, float in) noexcept
    {
        const int size = (int) buf.size();
        if (size < 4) return in;
        const int d = juce::jlimit (1, size - 1, delaySamples);
        int readPos = wPos - d;
        if (readPos < 0) readPos += size;
        const float delayed = buf[(size_t) readPos];
        const float stored  = in + gain * delayed;
        const float out     = delayed - gain * stored;
        buf[(size_t) wPos] = flushDenormal (stored);
        wPos = (wPos + 1 >= size) ? 0 : wPos + 1;
        return flushDenormal (out);
    }

    float applyPreTone (float in, float& hpState, float& lpState) const noexcept
    {
        // HP dann LP vor FDN + sanfte Tilt-Gewichtung (materialabhängig)
        hpState += preHpCoeff * (in - hpState);
        const float hpOut = in - hpState;
        lpState += preLpCoeff * (hpOut - lpState);
        const float lpOut = lpState;
        const float bright = hpOut * preTilt + lpOut * (1.0f - preTilt);
        return flushDenormal (bright);
    }

    void clearLine (int index)
    {
        if (index < 0 || index >= kNumLines) return;
        std::fill (delayLines[(size_t) index].begin(), delayLines[(size_t) index].end(), 0.0f);
        std::fill (lineDiffusion[(size_t) index].begin(), lineDiffusion[(size_t) index].end(), 0.0f);
        lineDiffWrite[(size_t) index] = 0;
        writePos[(size_t) index]  = 0;
        hpState[(size_t) index]   = 0.0f;
        dampState[(size_t) index] = 0.0f;
        hiState[(size_t) index]   = 0.0f;
        dcX1[(size_t) index]      = 0.0f;
        dcY1[(size_t) index]      = 0.0f;
    }

    void clearDiffusion()
    {
        for (auto& b : diffusionL) std::fill (b.begin(), b.end(), 0.0f);
        for (auto& b : diffusionR) std::fill (b.begin(), b.end(), 0.0f);
        diffWritePosL.fill (0);
        diffWritePosR.fill (0);
    }

    void clearTails()
    {
        for (int i = 0; i < kNumLines; ++i) clearLine (i);
    }

    double sampleRate { 48000.0 };

    std::vector<std::vector<float>> delayLines;
    std::array<int, kNumLines> writePos {};
    std::array<float, kNumLines> currentDelay {};
    std::array<float, kNumLines> targetDelay {};

    std::vector<std::vector<float>> diffusionL;
    std::vector<std::vector<float>> diffusionR;
    std::array<int, kNumDiffusers> diffusionDelay {};
    std::array<int, kNumDiffusers> diffWritePosL {};
    std::array<int, kNumDiffusers> diffWritePosR {};

    // 1 Allpass pro Delay-Zeile
    std::array<std::vector<float>, kNumLines> lineDiffusion {};
    std::array<int, kNumLines> lineDiffDelay {};
    std::array<int, kNumLines> lineDiffWrite {};
    float lineDiffGain { 0.5f };

    // DC-Blocker pro Zeile
    std::array<float, kNumLines> dcX1 {};
    std::array<float, kNumLines> dcY1 {};

    std::vector<float> preDelayBufferL;
    std::vector<float> preDelayBufferR;
    int preDelayWritePos { 0 };
    int preDelaySamples { 0 };

    int material { 0 };
    std::array<float, kNumLines> feedbackGain {};
    std::array<float, kNumLines> targetFeedbackGain {};
    std::array<float, kNumLines> dampCoeffLine {};
    std::array<float, kNumLines> targetDampCoeffLine {};

    float dampCoeff { 0.0f };
    float targetDampCoeff { 0.0f };
    float hpCoeff { 0.0f };
    float targetHpCoeff { 0.0f };
    float lpCoeff { 0.0f };
    float targetLpCoeff { 0.0f };
    float coeffSmooth { 0.001f };
    float sizeSmooth { 0.001f };
    float gateSmooth { 0.01f };
    bool  hpActive { false };
    bool  lpActive { false };

    std::array<float, kNumLines> hpState {};
    std::array<float, kNumLines> dampState {};
    std::array<float, kNumLines> hiState {};

    // Pre-FDN Material-Tonfilter
    float preHpCoeff { 0.0f }, targetPreHpCoeff { 0.0f };
    float preLpCoeff { 1.0f }, targetPreLpCoeff { 1.0f };
    float preTilt { 0.5f }, targetPreTilt { 0.5f };
    float preToneHpL { 0.0f }, preToneHpR { 0.0f };
    float preToneLpL { 0.0f }, preToneLpR { 0.0f };

    float modPhase { 0.0f };
    float modInc { 0.0f };

    bool  frozen { false };
    float gateMs { 0.0f };
    int   gateHoldSamples { 0 };
    float gateGain { 1.0f };

    bool processedActive { false };
    bool parametersActive { false };
    bool pendingTailClear { false };
    bool snapCoefficients { true };

    float mix { 0.15f };
    float preDelayMs { 30.0f };
    float decayMs { 1200.0f };
    float size { 0.5f };
    float damping { 0.3f };
    float lowCutHz { 150.0f };
    float highCutHz { 6000.0f };
};
