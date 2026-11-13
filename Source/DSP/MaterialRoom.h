#pragma once

#include <algorithm>
#include <array>
#include <cmath>
#include <utility>
#include <vector>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
/** MaterialRoom — Material-Raum (4-Tap-FDN) + Sub-Grumble (MATERIAL_ROOM_SPEC.md).

    - Materialtabelle Water/Wood/Plastic/Steel/Concrete/Fabric mit c, rho,
      Z = rho*c, R, alpha_normal, alpha_room (Spec §1).
    - 4 Taps mit Verhaeltnissen [1.0, 1.37, 1.79, 2.21] * (L / c); L =
      materialRoomSize in Metern, c = Schallgeschwindigkeit des Materials.
    - Stereo-Taps mit +/-0,3 % Versatz (M2: +/-3 % erzeugten in Mono ~180°
      Phasendrehung im Präsenzband); Hadamard-4x4-Mischung im Rueckkopplungspfad.
    - Feedback pro Material: clamp(0.55 + 0.44*(1 - alpha_room), 0.55, 0.995).
    - Damping: One-Pole-LP pro Tap, fc = clamp(12000 - 9000*alpha_room, 1500, 18000)
      * (1 + 0.35*rho/7850), begrenzt auf 0.45*fs.
    - 10 Hz DC-Blocker im Wet-Pfad, zusaetzlich 2nd-Order-Highpass bei 100 Hz
      auf dem Wet: verhindert Sub-Antiphase/Cancellation am Dry (Dry bleibt
      immer voll). Der 100-Hz-Cut schuetzt bewusst den 20–100-Hz-Druck (MR-090).
    - Materialbreite als Mid/Side auf dem Wet (MR-013: 0.5 = neutral,
      Seiten-Faktor gekappt auf max 1.75 bei Width 1.0).
    - Layer Kick / Rumble / Both bestimmt, welche Eingaenge in den Wet-Tap gehen.
    - Das Modul liefert ausschliesslich Wet (Send/Return). Das Dry-Signal wird
      nie abgesenkt, subtrahiert oder gefiltert — kein Druckverlust.
    - Grumble: Sub-Oszillator 20–120 Hz (Sine/Triangle/Square), Huellkurve folgt
      dem Kick-/Rumble-Input (Peak, Attack ~5 ms, Decay grumbleDecay), Pan-LFO
      von panA nach panB (Rate 0.05–8 Hz, Tiefe, Sine/Triangle/Square/Saw).
      Der Grumble ist vom Room-Mix unabhaengig (eigener Pegel); mix = 0 und
      grumbleLevel = 0 -> exakt 0. Er wird dem Wet-Return additiv zugemischt,
      nie vom Dry abgezogen.
      M1: Die Pan-Verteilung wird mit 1/(cos+sin) normiert - die Mono-Summe
      (L+R) ist damit konstant pegelneutral und atmet nicht mit dem Pan-LFO.
      MR-035: bei starkem Kick-Transienten (Env-Retrigger von < 20 % auf
      >= 80 %) wird die Sub-Oszillator-Phase deterministisch per LCG neu
      gezogen (fester Seed, nicht pro Sample).
    - Keine Allokation, keine Locks, keine Prints in process(); prepare allokiert.
    - NaN/Inf-Guards, Denormal-Flush, Sample-Rates 44.1–192 kHz.
*/
class MaterialRoom
{
public:
    MaterialRoom() = default;

    /** MR-013: Mid/Side-Faktor fuer die Wet-Breite. 0.5 = neutral (1.0),
        daruber gekappt: 1.0f + (width-0.5f)*1.5f, max 1.75 bei width = 1.0. */
    static float widthSideGain (float width) noexcept
    {
        return width <= 0.5f ? width * 2.0f
                             : 1.0f + (width - 0.5f) * 1.5f;
    }

    static constexpr int kNumTaps = 4;
    static constexpr int kNumMaterials = 6;

    //==============================================================================
    /** Physikalische Materialkennwerte (Spec §1). */
    struct MaterialInfo
    {
        const char* name { "" };
        float speedOfSound { 0.0f };   // c [m/s]
        float density { 0.0f };        // rho [kg/m3]
        float impedance { 0.0f };      // Z = rho*c [Rayl]
        float reflection { 0.0f };     // R = (Z - Z_air) / (Z + Z_air)
        float alphaNormal { 0.0f };    // 1 - R^2
        float alphaRoom { 0.0f };      // NRC / alpha_mid
    };

    /** Zuletzt berechnete Koeffizienten (fuer Tests und Anzeige). */
    struct MaterialCoefficients
    {
        MaterialInfo material {};
        float feedbackGain { 0.0f };
        float dampingHz { 0.0f };
        std::array<float, kNumTaps> tapDelaySeconds {};
        std::array<float, kNumTaps> tapDelaySamplesL {};
        std::array<float, kNumTaps> tapDelaySamplesR {};
    };

    static MaterialInfo materialInfo (int index) noexcept
    {
        switch (juce::jlimit (0, kNumMaterials - 1, index))
        {
            // Z in Rayl = rho*c (Spec-Tabelle: 1.477 MRayl = 1 477 040 Rayl).
            case 0:  return { "Water",    1480.0f,  998.0f,  1477040.0f, 0.99944f, 0.00110f, 0.01f };
            case 1:  return { "Wood",     3960.0f,  700.0f,  2772000.0f, 0.99970f, 0.00060f, 0.10f };
            case 2:  return { "Plastic",  2750.0f, 1180.0f,  3245000.0f, 0.99975f, 0.00050f, 0.05f };
            case 3:  return { "Steel",    5940.0f, 7850.0f, 46629000.0f, 0.99998f, 0.00004f, 0.02f };
            case 4:  return { "Concrete", 3700.0f, 2400.0f,  8880000.0f, 0.99991f, 0.00018f, 0.09f };
            default: return { "Fabric",    250.0f,  100.0f,    25000.0f, 0.96750f, 0.06400f, 0.60f };
        }
    }

    const MaterialCoefficients& getLastMaterialCoefficients() const noexcept { return lastCoeffs; }

    //==============================================================================
    void prepare (double newSampleRate, int maxBlockSize)
    {
        juce::ignoreUnused (maxBlockSize);

        sampleRate = juce::jlimit (8000.0, 384000.0, newSampleRate);
        const float fs = (float) sampleRate;

        // Pufferlaenge aus maximalem Tap-Delay: 2.21 * 30 m / 250 m/s * 1.03 (+8 Samples).
        for (int t = 0; t < kNumTaps; ++t)
        {
            delayL[(size_t) t].assign ((size_t) bufferLengthFor (kTapRatios[t] * (1.0f + kStereoSpread), fs), 0.0f);
            delayR[(size_t) t].assign ((size_t) bufferLengthFor (kTapRatios[t] * (1.0f + kStereoSpread), fs), 0.0f);
        }

        reset();
        updateCoefficients();
    }

    void reset()
    {
        for (int t = 0; t < kNumTaps; ++t)
        {
            std::fill (delayL[(size_t) t].begin(), delayL[(size_t) t].end(), 0.0f);
            std::fill (delayR[(size_t) t].begin(), delayR[(size_t) t].end(), 0.0f);
            writePosL[(size_t) t] = 0;
            writePosR[(size_t) t] = 0;
            dampStateL[(size_t) t] = 0.0f;
            dampStateR[(size_t) t] = 0.0f;
        }

        dcX1L = dcY1L = dcX1R = dcY1R = 0.0f;
        lowCutLpL = lowCutLpR = 0.0f;
        lowCutLpL2 = lowCutLpR2 = 0.0f;
        grumblePhase = 0.0f;
        panPhase = 0.0f;
        grumbleEnv = 0.0f;
        panSlewState = 0.0f;
        lcgState = 0x2545F491u;   // fester Seed -> deterministische Phasenfolge
        envRetriggerArmed = true;
        processedActive = false;
    }

    void setParameters (int material,
                        float sizeM,
                        float mix,
                        int layer,
                        float width,
                        float grumbleLevel_,
                        float grumbleFreq_,
                        float grumbleRate_,
                        float grumbleDepth_,
                        float panA_,
                        float panB_,
                        int grumbleWave_,
                        int grumblePanWave_,
                        float grumbleDecay_)
    {
        materialIndex = juce::jlimit (0, kNumMaterials - 1, material);
        roomSizeM     = juce::jlimit (1.0f, 30.0f, sizeM);
        mixAmount     = juce::jlimit (0.0f, 1.0f, mix);
        layerMode     = juce::jlimit (0, 2, layer);
        roomWidth     = juce::jlimit (0.0f, 1.0f, width);

        grumbleLevel  = juce::jlimit (0.0f, 1.0f, grumbleLevel_);
        grumbleFreq   = juce::jlimit (20.0f, 120.0f, grumbleFreq_);
        grumbleRate   = juce::jlimit (0.05f, 8.0f, grumbleRate_);
        grumbleDepth  = juce::jlimit (0.0f, 1.0f, grumbleDepth_);
        panA          = juce::jlimit (-1.0f, 1.0f, panA_);
        panB          = juce::jlimit (-1.0f, 1.0f, panB_);
        grumbleWave   = juce::jlimit (0, 2, grumbleWave_);
        panWave       = juce::jlimit (0, 3, grumblePanWave_);
        grumbleDecay  = juce::jlimit (0.1f, 2.0f, grumbleDecay_);

        updateCoefficients();
    }

    //==============================================================================
    /** Verarbeitet ein Stereo-Sample-Paar. Gibt ausschliesslich Wet zurueck
        (kein Dry, keine Pegelabsenkung des Busses). */
    std::pair<float,float> process (float kickL, float kickR, float rumbleL, float rumbleR)
    {
        if ((int) delayL[0].size() < 8)
            return { 0.0f, 0.0f };

        kickL   = sanitise (kickL);
        kickR   = sanitise (kickR);
        rumbleL = sanitise (rumbleL);
        rumbleR = sanitise (rumbleR);

        // mix = 0: Room-Wet ist stumm. Der Tail wird beim Uebergang verworfen,
        // damit beim Aufdrehen keine alten Reste ausbrechen. Der Grumble bleibt
        // jedoch unabhaengig vom Room-Mix aktiv (eigener Pegel).
        const bool roomActive = mixAmount > 0.0f;

        float wetL = 0.0f;
        float wetR = 0.0f;

        if (roomActive)
        {
            if (! processedActive)
            {
                clearTails();
                processedActive = true;
            }

            // ---- Layer-Auswahl: nur die gewaehlten Inputs speisen den Wet-Tap ----
            const float inL = layerInput (kickL, rumbleL);
            const float inR = layerInput (kickR, rumbleR);

            wetL = processChannel (delayL, writePosL, dampStateL, delaySamplesL, inL);
            wetR = processChannel (delayR, writePosR, dampStateR, delaySamplesR, inR);

            // ---- 10 Hz DC-Blocker im Wet-Pfad ----
            const float blockedL = wetL - dcX1L + dcCoeff * dcY1L;
            const float blockedR = wetR - dcX1R + dcCoeff * dcY1R;

            dcX1L = wetL;
            dcX1R = wetR;
            dcY1L = flushDenormal (blockedL);
            dcY1R = flushDenormal (blockedR);

            wetL = std::isfinite (blockedL) ? blockedL : 0.0f;
            wetR = std::isfinite (blockedR) ? blockedR : 0.0f;

            // ---- Materialbreite: Mid/Side auf dem Wet (0.5 = neutral, 1.0 -> 1.75, MR-013) ----
            const float widthFactor = widthSideGain (roomWidth);
            const float mid  = 0.5f * (wetL + wetR);
            const float side = 0.5f * (wetL - wetR) * widthFactor;
            wetL = mid + side;
            wetR = mid - side;

            // ---- Wet-Highpass (2nd order): schützt 20–100 Hz vor Druckverlust/Antiphase (MR-090) ----
            const float lp1L = lowCutLpL  + lowCutCoeff * (wetL - lowCutLpL);
            const float lp1R = lowCutLpR  + lowCutCoeff * (wetR - lowCutLpR);
            const float hp1L = wetL - lp1L;
            const float hp1R = wetR - lp1R;
            const float lp2L = lowCutLpL2 + lowCutCoeff * (hp1L - lowCutLpL2);
            const float lp2R = lowCutLpR2 + lowCutCoeff * (hp1R - lowCutLpR2);
            lowCutLpL  = flushDenormal (lp1L);
            lowCutLpR  = flushDenormal (lp1R);
            lowCutLpL2 = flushDenormal (lp2L);
            lowCutLpR2 = flushDenormal (lp2R);
            wetL = hp1L - lp2L;
            wetR = hp1R - lp2R;
        }
        else if (processedActive)
        {
            clearTails();
            processedActive = false;
        }

        // ---- Sub-Grumble: eigener Pegel, unabhaengig vom Room-Mix ----
        float grumbleL = 0.0f;
        float grumbleR = 0.0f;

        if (grumbleLevel > 0.0f)
            addGrumble (grumbleL, grumbleR, kickL, kickR, rumbleL, rumbleR);

        // ---- Wet-Send + Grumble (Dry wird nie angefasst) ----
        float outL = wetL * mixAmount + grumbleL;
        float outR = wetR * mixAmount + grumbleR;

        if (! std::isfinite (outL)) outL = 0.0f;
        if (! std::isfinite (outR)) outR = 0.0f;

        return { outL, outR };
    }

private:
    static constexpr float kTapRatios[kNumTaps] { 1.0f, 1.37f, 1.79f, 2.21f };
    static constexpr float kStereoSpread = 0.003f;       // +/-0,3 % L/R-Versatz (M2)
    static constexpr float kMaxRoomSizeM = 30.0f;        // Parameter-Obergrenze
    static constexpr float kMinSpeedOfSound = 250.0f;    // Fabric (langsamstes c)
    static constexpr float kDenormalThreshold = 1.0e-20f;
    static constexpr float kInputClip = 32.0f;
    static constexpr float kDcBlockerHz = 10.0f;
    static constexpr float kWetLowCutHz = 100.0f;  // Wet-Only Low-Cut: schützt 20–100 Hz vor Druckverlust/Antiphase (MR-090)
    static constexpr float kGrumbleAttackSeconds = 0.005f;
    static constexpr float kTwoPi = 6.28318530718f;

    //==============================================================================
    void updateCoefficients() noexcept
    {
        const float fs = (float) sampleRate;
        const MaterialInfo info = materialInfo (materialIndex);

        // Spec §2: feedback = clamp(0.55 + 0.44*(1 - alpha_room), 0.55, 0.995).
        const float feedback = juce::jlimit (0.55f, 0.995f,
                                             0.55f + 0.44f * (1.0f - info.alphaRoom));

        // Damping: Absorption bestimmt die Grunddaempfung, hohe Dichte hellt
        // metallische/deichte Materialien auf (Steel bleibt metallisch, Fabric
        // bleibt dunkel/absorbierend). Keine Daempfung ueber die Dichte allein.
        float dampingHz = juce::jlimit (1500.0f, 18000.0f,
                                        12000.0f - 9000.0f * info.alphaRoom);
        dampingHz *= 1.0f + 0.35f * (info.density / 7850.0f);
        dampingHz = juce::jmin (dampingHz, 0.45f * fs);

        feedbackGain = feedback;
        dampCoeff    = onePoleCoeff (dampingHz, fs);
        dcCoeff      = std::exp (-kTwoPi * kDcBlockerHz / fs);
        lowCutCoeff  = onePoleCoeff (kWetLowCutHz, fs);

        lastCoeffs = {};
        lastCoeffs.material     = info;
        lastCoeffs.feedbackGain = feedback;
        lastCoeffs.dampingHz    = dampingHz;

        for (int t = 0; t < kNumTaps; ++t)
        {
            const float seconds = kTapRatios[t] * roomSizeM / info.speedOfSound;
            lastCoeffs.tapDelaySeconds[(size_t) t] = seconds;

            const int lenL = (int) delayL[(size_t) t].size();
            const int lenR = (int) delayR[(size_t) t].size();

            const float sL = seconds * (1.0f - kStereoSpread) * fs;
            const float sR = seconds * (1.0f + kStereoSpread) * fs;

            delaySamplesL[(size_t) t] = lenL > 8 ? juce::jlimit (4.0f, (float) (lenL - 3), sL) : sL;
            delaySamplesR[(size_t) t] = lenR > 8 ? juce::jlimit (4.0f, (float) (lenR - 3), sR) : sR;

            lastCoeffs.tapDelaySamplesL[(size_t) t] = delaySamplesL[(size_t) t];
            lastCoeffs.tapDelaySamplesR[(size_t) t] = delaySamplesR[(size_t) t];
        }

        grumblePhaseInc   = grumbleFreq / fs;
        panPhaseInc       = grumbleRate / fs;
        grumbleAttack     = 1.0f - std::exp (-1.0f / (kGrumbleAttackSeconds * fs));
        panSlewCoeff      = 1.0f - std::exp (-1.0f / (0.005f * fs));
        // -60 dB ueber grumbleDecay Sekunden.
        grumbleDecayCoeff = std::exp (-6.907755f / (grumbleDecay * fs));
    }

    //==============================================================================
    /** 4-Tap-FDN eines Kanals: Taps lesen, daempfen, Hadamard-mischen, zurueckschreiben. */
    float processChannel (std::array<std::vector<float>, kNumTaps>& lines,
                          std::array<int, kNumTaps>& writePos,
                          std::array<float, kNumTaps>& dampState,
                          const std::array<float, kNumTaps>& delaySamples,
                          float input) noexcept
    {
        std::array<float, kNumTaps> taps {};

        for (int t = 0; t < kNumTaps; ++t)
        {
            float tap = readFractional (lines[(size_t) t], writePos[(size_t) t],
                                        delaySamples[(size_t) t]);

            if (! std::isfinite (tap))
            {
                std::fill (lines[(size_t) t].begin(), lines[(size_t) t].end(), 0.0f);
                dampState[(size_t) t] = 0.0f;
                tap = 0.0f;
            }

            // One-Pole-Damping pro Tap im Rueckkopplungspfad.
            float state = dampState[(size_t) t] + dampCoeff * (tap - dampState[(size_t) t]);
            state = flushDenormal (state);
            dampState[(size_t) t] = state;
            taps[(size_t) t] = state;
        }

        // Verlustfreie 4x4-Hadamard-Mischung (Faktor 0.5 = orthonormal).
        const std::array<float, kNumTaps> mixed
        {
            0.5f * (taps[0] + taps[1] + taps[2] + taps[3]),
            0.5f * (taps[0] - taps[1] + taps[2] - taps[3]),
            0.5f * (taps[0] + taps[1] - taps[2] - taps[3]),
            0.5f * (taps[0] - taps[1] - taps[2] + taps[3])
        };

        for (int t = 0; t < kNumTaps; ++t)
        {
            auto& line = lines[(size_t) t];
            const int len = (int) line.size();
            const int wPos = writePos[(size_t) t];

            float value = mixed[(size_t) t] * feedbackGain + 0.5f * input;

            if (! std::isfinite (value))
            {
                std::fill (line.begin(), line.end(), 0.0f);
                dampState[(size_t) t] = 0.0f;
                value = 0.0f;
            }

            line[(size_t) wPos] = flushDenormal (value);
            writePos[(size_t) t] = (wPos + 1 >= len) ? 0 : wPos + 1;
        }

        return 0.5f * (taps[0] + taps[1] + taps[2] + taps[3]);
    }

    //==============================================================================
    void addGrumble (float& wetL, float& wetR,
                     float kickL, float kickR, float rumbleL, float rumbleR) noexcept
    {
        // Huellkurve folgt Kick + Rumble (Peak ueber beide Kanaele), unabhaengig
        // vom Layer-Modus: der Sub-Grumble soll auch bei Layer = Rumble auf den
        // Kick-Transienten reagieren.
        float peak = 0.0f;
        peak = juce::jmax (peak, juce::jmax (std::abs (kickL), std::abs (kickR)));
        peak = juce::jmax (peak, juce::jmax (std::abs (rumbleL), std::abs (rumbleR)));

        // MR-035: starker Kick-Transient -> neue, deterministische LCG-Phase
        // (nicht pro Sample). Retrigger nur, wenn die Huellkurve von unter
        // ~20 % auf ueber ~80 % steigt (z. B. 0.01 -> 0.85); schwache Peaks
        // loesen keinen Phasensprung aus.
        const float prevEnv = grumbleEnv;

        if (peak > grumbleEnv)
            grumbleEnv += grumbleAttack * (peak - grumbleEnv);
        else
            grumbleEnv *= grumbleDecayCoeff;

        if (prevEnv < 0.2f)
            envRetriggerArmed = true;

        if (envRetriggerArmed && grumbleEnv >= 0.8f)
        {
            lcgState = lcgState * 1664525u + 1013904223u;   // klassischer LCG (Numerical Recipes)
            grumblePhase = (float) (lcgState >> 8) * (1.0f / 16777216.0f);
            envRetriggerArmed = false;
        }

        grumbleEnv = flushDenormal (grumbleEnv);

        if (! std::isfinite (grumbleEnv))
            grumbleEnv = 0.0f;

        const float osc = oscillatorValue (grumbleWave, grumblePhase);

        // Pan-LFO: Mitte zwischen A/B, Tiefe skaliert den Schwung (0 = Mitte, 1 = voller A→B-Schwung).
        // Slew-Limit (~5 ms) verhindert Pegelsprünge bei Square/Saw-Pan-LFOs.
        const float lfoSigned  = lfoValue (panWave, panPhase) * 2.0f - 1.0f;
        const float panMid     = 0.5f * (panA + panB);
        const float panAmp     = 0.5f * (panB - panA) * grumbleDepth;
        const float panTarget  = juce::jlimit (-1.0f, 1.0f, panMid + panAmp * lfoSigned);
        panSlewState += panSlewCoeff * (panTarget - panSlewState);
        panSlewState = flushDenormal (panSlewState);
        const float pan        = juce::jlimit (-1.0f, 1.0f, panSlewState);
        const float angle = (pan + 1.0f) * 0.25f * juce::MathConstants<float>::pi;
        const float amp = osc * grumbleEnv * grumbleLevel;

        // M1 (Mono-Pegelneutralitaet): cos+sin normieren. Ohne diese Normierung
        // liefert jeder Kanal bei Pan 0 nur 0.71*amp => die Mono-Summe ist
        // +3 dB lauter und der Pan-LFO moduliert die Mono-Summe (Sub "atmet").
        // Mit der Normierung ist L+R = amp konstant (pan-unabhaengig), das
        // Kanal-Stereo-Bild (cos:sin) bleibt exakt erhalten.
        const float panSum = juce::jmax (1.0e-3f, std::cos (angle) + std::sin (angle));

        wetL += amp * std::cos (angle) / panSum;
        wetR += amp * std::sin (angle) / panSum;

        advancePhase (grumblePhase, grumblePhaseInc);
        advancePhase (panPhase, panPhaseInc);
    }

    static float oscillatorValue (int wave, float phase) noexcept
    {
        switch (wave)
        {
            case 1:  return 4.0f * std::abs (phase - 0.5f) - 1.0f;           // Triangle -1..1
            case 2:  return phase < 0.5f ? 1.0f : -1.0f;                     // Square
            default: return std::sin (kTwoPi * phase);                       // Sine
        }
    }

    static float lfoValue (int wave, float phase) noexcept
    {
        switch (wave)
        {
            case 1:  return 1.0f - std::abs (2.0f * phase - 1.0f);           // Triangle 0..1
            case 2:  return phase < 0.5f ? 1.0f : 0.0f;                      // Square 0/1
            case 3:  return phase;                                           // Saw 0..1
            default: return 0.5f + 0.5f * std::sin (kTwoPi * phase);         // Sine 0..1
        }
    }

    static void advancePhase (float& phase, float increment) noexcept
    {
        phase += increment;

        if (phase >= 1.0f)
            phase -= std::floor (phase);
    }

    //==============================================================================
    static int bufferLengthFor (float ratio, float fs) noexcept
    {
        const int len = (int) std::ceil (fs * ratio * kMaxRoomSizeM / kMinSpeedOfSound) + 8;
        return juce::jmax (64, len);
    }

    static float onePoleCoeff (float cutoffHz, float fs) noexcept
    {
        const float fc = juce::jlimit (5.0f, 0.45f * fs, cutoffHz);
        return 1.0f - std::exp (-kTwoPi * fc / fs);
    }

    static float flushDenormal (float v) noexcept
    {
        return std::abs (v) < kDenormalThreshold ? 0.0f : v;
    }

    static float sanitise (float v) noexcept
    {
        if (! std::isfinite (v))
            return 0.0f;

        return juce::jlimit (-kInputClip, kInputClip, v);
    }

    static float readFractional (const std::vector<float>& line, int writePos,
                                 float delaySamples) noexcept
    {
        const int len = (int) line.size();

        if (len < 4)
            return 0.0f;

        float readPos = (float) writePos - juce::jlimit (1.0f, (float) (len - 2), delaySamples);

        while (readPos < 0.0f)
            readPos += (float) len;

        const int i0 = (int) readPos;
        const float frac = readPos - (float) i0;
        const int i1 = (i0 + 1 >= len) ? 0 : i0 + 1;

        return line[(size_t) i0] + frac * (line[(size_t) i1] - line[(size_t) i0]);
    }

    // Layer 0 = Kick, 1 = Rumble, 2 = Both (Summe der gewaehlten Inputs).
    float layerInput (float kick, float rumble) const noexcept
    {
        switch (layerMode)
        {
            case 0:  return kick;
            case 1:  return rumble;
            default: return kick + rumble;
        }
    }

    void clearTails()
    {
        for (int t = 0; t < kNumTaps; ++t)
        {
            std::fill (delayL[(size_t) t].begin(), delayL[(size_t) t].end(), 0.0f);
            std::fill (delayR[(size_t) t].begin(), delayR[(size_t) t].end(), 0.0f);
            writePosL[(size_t) t]  = 0;
            writePosR[(size_t) t]  = 0;
            dampStateL[(size_t) t] = 0.0f;
            dampStateR[(size_t) t] = 0.0f;
        }

        dcX1L = dcY1L = dcX1R = dcY1R = 0.0f;
        lowCutLpL = lowCutLpR = 0.0f;
        lowCutLpL2 = lowCutLpR2 = 0.0f;
    }

    //==============================================================================
    double sampleRate { 48000.0 };

    std::array<std::vector<float>, kNumTaps> delayL {};
    std::array<std::vector<float>, kNumTaps> delayR {};
    std::array<int, kNumTaps>   writePosL {};
    std::array<int, kNumTaps>   writePosR {};
    std::array<float, kNumTaps> dampStateL {};
    std::array<float, kNumTaps> dampStateR {};
    std::array<float, kNumTaps> delaySamplesL {};
    std::array<float, kNumTaps> delaySamplesR {};

    float feedbackGain { 0.946f };
    float dampCoeff { 0.5f };
    float dcCoeff { 0.999f };
    float lowCutCoeff { 0.96f };
    float lowCutLpL { 0.0f }, lowCutLpR { 0.0f };
    float lowCutLpL2 { 0.0f }, lowCutLpR2 { 0.0f };
    float dcX1L { 0.0f }, dcY1L { 0.0f }, dcX1R { 0.0f }, dcY1R { 0.0f };

    // Parameter (Defaults laut Spec §3).
    int   materialIndex { 1 };      // Wood
    float roomSizeM { 8.0f };
    float mixAmount { 0.0f };
    int   layerMode { 2 };          // Both
    float roomWidth { 0.5f };

    float grumbleLevel { 0.0f };
    float grumbleFreq { 45.0f };
    float grumbleRate { 0.5f };
    float grumbleDepth { 0.5f };
    float panA { -0.5f };
    float panB { 0.5f };
    int   grumbleWave { 0 };        // Sine
    int   panWave { 0 };            // Sine
    float grumbleDecay { 0.5f };

    // Grumble-Laufzeit
    float panSlewState { 0.0f };
    float panSlewCoeff { 0.01f };

    float grumblePhase { 0.0f };
    float grumblePhaseInc { 0.0f };
    float panPhase { 0.0f };
    float panPhaseInc { 0.0f };
    float grumbleEnv { 0.0f };
    float grumbleAttack { 0.01f };
    float grumbleDecayCoeff { 0.999f };

    // MR-035: LCG-Zustand + Retrigger-Flag fuer die deterministische
    // Phasenrandomisierung bei starken Kick-Transienten.
    unsigned int lcgState { 0x2545F491u };
    bool envRetriggerArmed { true };

    bool processedActive { false };
    MaterialCoefficients lastCoeffs {};
};
