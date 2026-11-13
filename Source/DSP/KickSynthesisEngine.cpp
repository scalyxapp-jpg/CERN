#include "KickSynthesisEngine.h"

#include <juce_audio_formats/juce_audio_formats.h>

#include <algorithm>
#include <limits>

//==============================================================================
namespace
{
// Key/Scale-Snap (SND-132): Chromatisch ist identisch, natuerliches Moll
// verwendet die Stammtoene 0/2/3/5/7/8/10 relativ zum Key-Root.
constexpr std::array<int, 7> naturalMinorPitchClasses { 0, 2, 3, 5, 7, 8, 10 };

bool isPitchClassInScale (int pitchClass, int keyRoot, int scaleIndex)
{
    if (scaleIndex != 1) // 0 = Chromatisch
        return true;

    const int relative = ((pitchClass - keyRoot) % 12 + 12) % 12;
    return std::find (naturalMinorPitchClasses.begin(),
                      naturalMinorPitchClasses.end(), relative)
           != naturalMinorPitchClasses.end();
}

/** Naechstgelegene Note der Scale (bei Gleichstand abwaerts). */
int snapNoteToScale (int noteNumber, int keyRoot, int scaleIndex)
{
    if (isPitchClassInScale (noteNumber, keyRoot, scaleIndex))
        return noteNumber;

    for (int distance = 1; distance <= 6; ++distance)
    {
        if (isPitchClassInScale (noteNumber - distance, keyRoot, scaleIndex))
            return noteNumber - distance;

        if (isPitchClassInScale (noteNumber + distance, keyRoot, scaleIndex))
            return noteNumber + distance;
    }

    return noteNumber;
}

float snapFundamentalToScale (float hz, int keyRoot, int scaleIndex)
{
    if (scaleIndex != 1 || hz <= 0.0f)
        return hz;

    const float note = 69.0f + 12.0f * std::log2 (hz / 440.0f);
    const int snapped = snapNoteToScale ((int) std::lround (note), keyRoot, scaleIndex);
    return 440.0f * std::pow (2.0f, (float) (snapped - 69) / 12.0f);
}

/** Deterministischer Hash -> vorzeichenbehafteter Pseudo-Zufallswert in [-1, 1]. */
float signedHash01 (uint32_t seed)
{
    seed ^= seed >> 16;
    seed *= 0x7FEB352Du;
    seed ^= seed >> 15;
    seed *= 0x846CA68Bu;
    seed ^= seed >> 16;
    return (float) (seed & 0x00FFFFFFu) / 8388608.0f - 1.0f;
}

/** SND-181: nur die Side-Komponente der FX-Tails skalieren (Mid bleibt unverändert). */
inline void applyFxWidth (float width, float& l, float& r)
{
    const float mid  = 0.5f * (l + r);
    const float side = 0.5f * (l - r) * width;
    l = mid + side;
    r = mid - side;
}
} // namespace


//==============================================================================
KickSynthesisEngine::KickSynthesisEngine (juce::AudioProcessorValueTreeState& apvts)
    : parameters (apvts)
{
    // Parameter-Zeiger einmal cachen: kein dynamic_cast/getParameter pro Block.
    cacheParameterPointers();
}

void KickSynthesisEngine::cacheParameterPointers()
{
    cachedParameters.fill (nullptr);

    const auto& ids = ParameterIDs::getAllIDs();
    const int count = juce::jmin ((int) ids.size(), maxCachedParameters);

    for (int i = 0; i < count; ++i)
        cachedParameters[(size_t) i] = dynamic_cast<juce::RangedAudioParameter*> (
            parameters.getParameter (ids[(size_t) i]));
}

//==============================================================================
void KickSynthesisEngine::prepare (double sampleRate, int samplesPerBlock)
{
    this->sampleRate = sampleRate;

    membrane.prepare (sampleRate);
    pitchEnv.prepare (sampleRate);
    click.prepare (sampleRate);
    tube.prepare (sampleRate);
    compressor.prepare (sampleRate);
    reverb.prepare (sampleRate, samplesPerBlock);
    materialRoom.prepare (sampleRate, samplesPerBlock);
    higgs.prepare (sampleRate);
    mixer.prepare (sampleRate);
    layering.prepare (sampleRate);

    layerTapLow.assign ((size_t) juce::jmax (1, samplesPerBlock), 0.0f);
    layerTapHigh.assign ((size_t) juce::jmax (1, samplesPerBlock), 0.0f);

    const size_t tapSize = (size_t) juce::jmax (1, samplesPerBlock);
    moduleTapMembrane.assign (tapSize, 0.0f);
    moduleTapBody.assign (tapSize, 0.0f);
    moduleTapSub.assign (tapSize, 0.0f);
    moduleTapClick.assign (tapSize, 0.0f);

    // Room: 4-Tap-Netzwerk, max. 600 ms Tap-Abstand (SND-091..100).
    roomBufferSize = juce::jmax (8, (int) std::ceil (sampleRate * 0.65) + 4);
    roomDelayL.assign ((size_t) roomBufferSize, 0.0f);
    roomDelayR.assign ((size_t) roomBufferSize, 0.0f);

    // SND-179: 2x-Halfband-FIR der Drive-/Clip-Stufen entwerfen.
    driveUp.prepare();
    driveDown.prepare();
    for (int i = 0; i < 2; ++i)
    {
        clipUp[i].prepare();
        clipDown[i].prepare();
    }

    // SND-175: Transient-Shaper (Detector-Zeiten aus der Samplerate).
    transientShaper.prepare (sampleRate);

    // DC-Blocker im Room bei 2 Hz (SND-099), statt 38 Hz.
    roomDcCoeff = juce::jlimit (0.9f, 0.99999f,
                                std::exp (-juce::MathConstants<float>::twoPi * 2.0f
                                          / (float) sampleRate));

    cacheParameterPointers();
    reset();
}

void KickSynthesisEngine::reset()
{
    membrane.reset();
    pitchEnv.reset();
    click.reset();
    tube.reset();
    compressor.reset();
    reverb.reset();
    materialRoom.reset();
    higgs.reset();
    mixer.reset();
    layering.reset();

    // Sub
    subPhase = 0.0f;
    subEnv = 0.0f;
    subOmega = 0.0f;
    subOmegaBase = 0.0f;
    subDt = 0.0f;
    subDtBase = 0.0f;
    subTriState = 0.0f;
    subDecayCoeff = 0.0f;
    triggerPitchSemitones = 0.0f;
    glideRatio = 1.0f;
    glideTargetRatio = 1.0f;
    glideCoeff = 1.0f;
    glideSamplesLeft = 0;
    trackGlideRatio = 1.0f;
    trackGlideTargetRatio = 1.0f;
    trackGlideCoeff = 1.0f;
    hasTriggerPitch = false;

    // Body-Resonator
    bodyPhase = 0.0f;
    bodyEnv = 0.0f;
    bodyAttackCoeff = 0.0f;
    bodyDecayCoeff = 0.0f;
    bodyOmega = 0.0f;
    bodyOmegaBase = 0.0f;
    bodyPeak = 0.0f;
    bodyInAttack = false;
    bodyBpX1 = 0.0f;
    bodyBpX2 = 0.0f;
    bodyBpY1 = 0.0f;
    bodyBpY2 = 0.0f;

    // Room
    roomFeedback = 0.0f;
    roomSendGain = 0.0f;
    roomPos = 0;
    roomToneLpL = 0.0f;
    roomToneLpR = 0.0f;
    roomToneHpXPrevL = 0.0f;
    roomToneHpYPrevL = 0.0f;
    roomToneHpXPrevR = 0.0f;
    roomToneHpYPrevR = 0.0f;
    roomFbXPrevL = 0.0f;
    roomFbYPrevL = 0.0f;
    roomFbXPrevR = 0.0f;
    roomFbYPrevR = 0.0f;
    roomHpX1L = 0.0f;
    roomHpX2L = 0.0f;
    roomHpY1L = 0.0f;
    roomHpY2L = 0.0f;
    roomHpX1R = 0.0f;
    roomHpX2R = 0.0f;
    roomHpY1R = 0.0f;
    roomHpY2R = 0.0f;
    std::fill (roomDelayL.begin(), roomDelayL.end(), 0.0f);
    std::fill (roomDelayR.begin(), roomDelayR.end(), 0.0f);

    // Drive
    driveOutDcXPrev = 0.0f;
    driveOutDcYPrev = 0.0f;
    driveLowLpState = 0.0f;
    driveHpLpState = 0.0f;
    driveHpCoeff = 0.0f;
    driveSplitCoeff = 0.0f;
    driveDcCoeff = 0.0f;
    drivePreHpActive = false;
    drivePeak = 0.0f;
    driveOutputLevelDb = -100.0f;
    driveLatencyActive = false;

    // 2x-Oversampling (SND-179)
    driveUp.reset();
    driveDown.reset();
    for (int i = 0; i < 2; ++i)
    {
        clipUp[i].reset();
        clipDown[i].reset();
    }
    oversampleEnabled = false;
    driveLowDelay.fill (0.0f);
    driveLowDelayPos = 0;

    // Transient-Shaper (SND-175)
    transientShaper.reset();

    // Ducking (SND-174)
    duckDepthDb = 0.0f;
    duckAttackCoeff = 0.0f;
    duckReleaseCoeff = 0.0f;
    duckEnv = 0.0f;
    membraneBusSample = 0.0f;
    subBusSample = 0.0f;

    // Humanize / Velocity->Click (SND-177/178)
    humanizeCounter = 0;
    humanizeTimingCounter = 0;
    humanizeAmount = 0.0f;
    velocityToClick = 0.0f;

    // Envelope-Follower (SND-156)
    envFollowerValue = 0.0f;
    envFollowAttackCoeff = 0.0f;
    envFollowReleaseCoeff = 0.0f;

    // FX-Breite (SND-181)
    fxWidthValue = 1.0f;

    // Clip-GR Metering (SND-006/020)
    clipGainReductionDb = 0.0f;
    clipGainReductionMaxDb = 0.0f;
    clipMinGain = 1.0f;

    // EQ
    eqShelfX1 = 0.0f;
    eqShelfX2 = 0.0f;
    eqShelfY1 = 0.0f;
    eqShelfY2 = 0.0f;
    eqHighShelfState = 0.0f;
    eqHighpassState = 0.0f;
    eqPunchX1 = 0.0f;
    eqPunchX2 = 0.0f;
    eqPunchY1 = 0.0f;
    eqPunchY2 = 0.0f;

    // Output
    outGainCurrent = 0.0f; // dB
    outPanLCurrent = 1.0f;
    outPanRCurrent = 1.0f;
    outputRampInitialized = false;

    monoBelowState = 0.0f;
    monoBelowCoeffValue = 1.0f;
    monoBelowActive = false;
    outputWidthValue = 1.0f;
    clipCeiling = 1.0f;
    clipActive = false;

    limiterGain = 1.0f;
    limiterPeakEnv = 0.0f;
    limiterCeiling = 1.0f;
    limiterMinGain = 1.0f;
    limiterGainReductionDb = 0.0f;
    limiterGainReductionMaxDb = 0.0f;
    limiterPrevL = 0.0f;
    limiterPrev2L = 0.0f;
    limiterPrevR = 0.0f;
    limiterPrev2R = 0.0f;
    limiterHistoryValid = false;
    blockTruePeak = 0.0f;
    truePeakDb = -100.0f;

    // Trigger-Queue (Audio-Thread)
    triggerCount = 0;

    // Beim Reset gilt "alles war an": kein künstlicher 0->1-Reset im ersten Block.
    previousEnableState = ModuleEnableState {};

    tailCounter = 0;
    isActiveFlag.store (false, std::memory_order_release);
    parameterModulationPrepared = false;
}

//==============================================================================
void KickSynthesisEngine::startPitchGlide (float targetSemitones)
{
    const float targetRatio = std::pow (2.0f, targetSemitones / 12.0f);
    const float targetTrackRatio = std::pow (targetRatio, 0.3f); // SND-134

    const int glideSamples = (int) std::lround (blockGlideMs * 0.001f * (float) sampleRate);

    glideTargetRatio = targetRatio;
    trackGlideTargetRatio = targetTrackRatio;

    if (glideSamples <= 0 || ! hasTriggerPitch)
    {
        // Kein Glide (oder erster Trigger): Ziel sofort setzen.
        glideRatio = targetRatio;
        trackGlideRatio = targetTrackRatio;
        glideSamplesLeft = 0;
    }
    else
    {
        // Portamento: multiplikativer Schritt pro Sample = linear in Halbtoenen.
        glideSamplesLeft = glideSamples;
        glideCoeff = std::pow (targetRatio / juce::jmax (1.0e-6f, glideRatio),
                               1.0f / (float) glideSamples);
        trackGlideCoeff = std::pow (targetTrackRatio / juce::jmax (1.0e-6f, trackGlideRatio),
                                    1.0f / (float) glideSamples);
    }

    hasTriggerPitch = true;
}

void KickSynthesisEngine::trigger (float velocity, float pitchSemitones)
{
    // SND-178: Humanize – deterministisches ±3 ct Pitch- und ±1 dB Level-Jitter,
    // proportional zu "humanize". Pro Trigger, nicht pro Sample.
    const uint32_t humanizeSeed = ++humanizeCounter * 2654435761u;
    const float pitchJitter = signedHash01 (humanizeSeed) * 0.03f * humanizeAmount;
    const float levelJitterDb = signedHash01 (humanizeSeed ^ 0x00ABCDEFu) * 1.0f * humanizeAmount;
    velocity = juce::jlimit (0.0f, 1.0f,
                             velocity * juce::Decibels::decibelsToGain (levelJitterDb));

    // Key/Scale-Snap (SND-132): Step-/MIDI-Offset auf die Scale des Key-Roots
    // beziehen; die absolute Note ist die Note von f0 + Offset.
    const int snappedNote = snapNoteToScale (baseNoteNumber + (int) std::lround (pitchSemitones),
                                             blockKeyRoot, blockScale);
    const float snappedSemitones = (float) (snappedNote - baseNoteNumber);

    // Expliziter Velocity->Pitch-Anteil (SND-030): 0 = aus, 1 = -12 st bei Vel 0.
    triggerPitchSemitones = snappedSemitones
                          + velocityToPitch * (juce::jlimit (0.0f, 1.0f, velocity) - 1.0f) * 12.0f
                          + pitchJitter;

    startPitchGlide (triggerPitchSemitones);

    membrane.trigger (velocity);
    pitchEnv.trigger();
    click.trigger (velocity, velocityToClick);
    layering.trigger (velocity);

    // Sub-Phase: 0 bei Ratio 1.0, pi/2 bei 0.5 (SND-051).
    subPhase = (subRatio < 0.75f) ? juce::MathConstants<float>::halfPi : 0.0f;
    subTriState = 0.0f;
    // B5: der Sub startet mit dem vollen Velocity-Pegel (vorher 0.8x, d. h.
    // der Spitzenpegel lag 20 % unter dem eingestellten Level).
    subEnv = juce::jlimit (0.0f, 1.0f, velocity);

    // Body-Resonator: Start bei pi/2 (Cosinus-Phase, kein Klick) und Level
    // direkt aus der Velocity (kein fester 0.6-Faktor mehr, SND-047).
    bodyPhase = juce::MathConstants<float>::halfPi;
    bodyEnv = 0.0f;
    bodyPeak = juce::jlimit (0.0f, 1.0f, velocity);
    bodyInAttack = true;

    isActiveFlag.store (true, std::memory_order_release);
    tailCounter = (int) (tailLengthSeconds * (float) sampleRate);
}

void KickSynthesisEngine::enqueueTrigger (float velocity, float pitchSemitones, int sampleOffset)
{
    if (triggerCount >= triggerQueueSize)
        return; // Queue voll: Event verwerfen (kein Blocking im Audio-Thread).

    // SND-178: Humanize Timing-Jitter ±1 ms, proportional zu "humanize".
    // Gilt nur für sample-genaue Events (Sequencer/MIDI); der GUI-Direct-
    // Trigger-Fallback feuert unverändert bei Offset 0.
    if (humanizeAmount > 0.0f)
    {
        const uint32_t seed = ++humanizeTimingCounter * 2654435761u;
        const float jitterSamples = signedHash01 (seed) * 0.001f * humanizeAmount
                                    * (float) sampleRate;
        sampleOffset = juce::jmax (0, sampleOffset + (int) std::lround (jitterSamples));
    }

    TriggerEvent& event = triggerQueue[(size_t) triggerCount];
    event.velocity = juce::jlimit (0.0f, 1.0f, velocity);
    event.pitchSemitones = pitchSemitones;
    event.sampleOffset = juce::jmax (0, sampleOffset);

    ++triggerCount;
}

bool KickSynthesisEngine::consumeTriggerEvents (int sample, float velSensitivity)
{
    bool fired = false;
    // Sequencer- und MIDI-Events können unsortiert eingefügt worden sein
    // (MIDI wird vor dem Sequencer gelesen). Deshalb pro Sample das früheste
    // fällige Event suchen und konsumieren, statt FIFO-Reihenfolge anzunehmen.
    while (triggerCount > 0)
    {
        int bestIndex = -1;
        int bestOffset = 0;

        for (int k = 0; k < triggerCount; ++k)
        {
            const auto& candidate = triggerQueue[(size_t) k];

            if (candidate.sampleOffset <= sample
                && (bestIndex < 0 || candidate.sampleOffset < bestOffset))
            {
                bestIndex = k;
                bestOffset = candidate.sampleOffset;
            }
        }

        if (bestIndex < 0)
            break;

        const TriggerEvent event = triggerQueue[(size_t) bestIndex];

        // Kompaktieren (max. 16 Einträge, keine Allokation).
        for (int k = bestIndex; k < triggerCount - 1; ++k)
            triggerQueue[(size_t) k] = triggerQueue[(size_t) k + 1];

        --triggerCount;

        const float velocity = 1.0f - velSensitivity * (1.0f - event.velocity);
        trigger (velocity, event.pitchSemitones);
        fired = true;
    }

    // Message-Thread-Fallback (GUI-Button, triggerNote): Offset 0.
    if (sample == 0)
    {
        float velocity = 1.0f;
        float pitchSemitones = 0.0f;

        if (consumePendingTrigger (velocity, pitchSemitones))
        {
            trigger (1.0f - velSensitivity * (1.0f - velocity), pitchSemitones);
            fired = true;
        }
    }

    return fired;
}

//==============================================================================
void KickSynthesisEngine::updateSubOscillator (float tunedFundamental, float decayMs,
                                               float detuneCents)
{
    const float detune = std::pow (2.0f, detuneCents / 1200.0f);

    // Ratio 1.0 = Grundton (SND-049), 0.5 = Sub-Oktave.
    const float subFreq = juce::jlimit (20.0f, 400.0f, tunedFundamental * subRatio) * detune;
    const float subFreqClamped = juce::jlimit (1.0f, 0.45f * (float) sampleRate, subFreq);

    subOmega = juce::MathConstants<float>::twoPi * subFreqClamped / (float) sampleRate;
    subDt = subOmega / juce::MathConstants<float>::twoPi;
    subOmegaBase = subOmega;
    subDtBase = subDt;
    subDecayCoeff = std::exp (-1.0f / juce::jmax (1.0f, decayMs * 0.001f * (float) sampleRate));
}

void KickSynthesisEngine::updateBodyOscillator (float tunedFundamental, float tuning,
                                                float decayMs, float attackMs)
{
    // B4: Der Body ist ein Shell-Band bei tunedFundamental * bodyTuning
    // (bodyTuning = 2.0 => 92,5 Hz bei f0 = 46,25 Hz). Die bisherige
    // +3-Cent-Detune entfaellt: bei bodyTuning = 2 liegt das Band nicht mehr
    // auf dem Fundamental (kein Phasen-Lock mehr noetig).
    const float bodyFreq = juce::jlimit (15.0f, 2000.0f,
                                         tunedFundamental * juce::jlimit (0.5f, 2.0f, tuning));
    const float bodyFreqClamped = juce::jlimit (1.0f, 0.45f * (float) sampleRate, bodyFreq);

    bodyOmega = juce::MathConstants<float>::twoPi * bodyFreqClamped / (float) sampleRate;
    bodyOmegaBase = bodyOmega;
    bodyDecayCoeff = std::exp (-1.0f / juce::jmax (1.0f, decayMs * 0.001f * (float) sampleRate));
    bodyAttackCoeff = std::exp (-1.0f / juce::jmax (1.0f, attackMs * 0.001f * (float) sampleRate));

    // REVIEW-TODO (Paket B): Gewunschter Parameter "bodyBandQ" (0.7..3.0,
    //   Default 1.4) - die Bandbreite des Shell-Bandes ist hier fest.
    // Shell-Resonator: 2-poliger RBJ-Bandpass (Q = 1.4, 0 dB Peak-Gain).
    // -3-dB-Bandbreite = bodyFreq / Q (bei 92,5 Hz => 66 Hz => 66..119 Hz).
    const float w0 = juce::MathConstants<float>::twoPi * bodyFreqClamped / (float) sampleRate;
    constexpr float bodyBandpassQ = 1.4f;
    const float alpha = std::sin (w0) / (2.0f * bodyBandpassQ);
    const float a0 = 1.0f + alpha;

    bodyBpB0 = alpha / a0;
    bodyBpB1 = 0.0f;
    bodyBpB2 = -alpha / a0;
    bodyBpA1 = -2.0f * std::cos (w0) / a0;
    bodyBpA2 = (1.0f - alpha) / a0;
}

//==============================================================================
float KickSynthesisEngine::applyDcBlocker (float x, float& xPrev, float& yPrev, float coeff)
{
    const float c = juce::jlimit (0.9f, 0.99999f, coeff);
    float y = x - xPrev + c * yPrev;
    xPrev = x;
    yPrev = y;
    return y;
}

//==============================================================================
void KickSynthesisEngine::updateEQ (float rumbleHP, float subFreq, float subGain,
                                    float punchFreq, float punchGain,
                                    float airFreq, float airGain, float fundamentalHz)
{
    auto coeff = [this] (float fc)
    {
        const float clamped = juce::jlimit (10.0f, 0.45f * (float) sampleRate, fc);
        return std::exp (-2.0f * (float) juce::MathConstants<float>::pi
                         * clamped / (float) sampleRate);
    };

    // Ranges (SND-082): Rumble 15..120, Sub 20..200, Punch 500..8000 Hz.
    eqRumbleCoeff = coeff (juce::jlimit (15.0f, 120.0f, rumbleHP));
    eqAirCoeff = coeff (airFreq);

    // ---- B1: Sub-Band = RBJ-Low-Shelf (Slope S = 1.0, Q0 = 1/sqrt(2)) ----
    // Der Shelf laeuft DIREKT auf x0 (kein paralleles Band, keine
    // Phasendrehung, kein Makeup). Damit ist der Regler ehrlich:
    // +3 dB auf f0 => +3 dB am Fundamental, 0 dB => bit-transparent.
    //
    // RBJ-Koeffizienten (Audio EQ Cookbook, "Low Shelf"):
    //   A     = 10^(G_dc/40),  alpha = sin(w0)/2 * sqrt((A + 1/A)*(1/S - 1) + 2)
    //   fuer S = 1:  alpha = sin(w0)/sqrt(2) = sin(w0)/(2*Q0), Q0 = 0.7071
    //   (G_dc = asymptotischer Gain = 2 * eqSubGain, siehe unten)
    //   b0 =  A*((A+1) - (A-1)*cos(w0) + 2*sqrt(A)*alpha)
    //   b1 =  2*A*((A-1) - (A+1)*cos(w0))
    //   b2 =  A*((A+1) - (A-1)*cos(w0) - 2*sqrt(A)*alpha)
    //   a0 =  (A+1) + (A-1)*cos(w0) + 2*sqrt(A)*alpha
    //   a1 = -2*((A-1) + (A+1)*cos(w0))
    //   a2 =  (A+1) + (A-1)*cos(w0) - 2*sqrt(A)*alpha
    //
    // Transparenz-Beweis (G = 0 dB => A = 1):
    //   b0 = 2 + 2*alpha        a0 = 2 + 2*alpha   -> b0 = a0
    //   b1 = -4*cos(w0)              a1 = -4*cos(w0)    -> b1 = a1
    //   b2 = 2 - 2*alpha        a2 = 2 - 2*alpha   -> b2 = a2
    // Nach der Normalisierung (a0) ist der Filter damit exakt die Identitaet
    // (H(z) = (b0 + b1 z^-1 + b2 z^-2)/(a0 + a1 z^-1 + a2 z^-2) = 1).
    //
    // REVIEW-TODO (Paket B): Gewunschter Parameter "eqSubMode"
    //   (0 = Low-Shelf wie hier, 1 = Bell/Peak bei f0). Der Shelf erfuellt die
    //   Forderung "+3 dB => +3 dB am Fundamental" nur, indem sein asymptotischer
    //   Gain auf 2*eqSubGain gesetzt wird (Sub-Oktave dann +5,8 dB bei +3 dB
    //   Einstellung). Mit einem Bell-Filter waere der Gain genau bei f0 = der
    //   Einstellung und die Sub-Oktave unberuehrt.
    // Gain-Abbildung: Der RBJ-Low-Shelf liefert am Eckpunkt f0 per Konstruktion
    // die Haelfte des asymptotischen Gains (klassische Analog-Shelf-Kurve,
    // unabhaengig von S). Damit der Regler an f0 ehrlich ist, wird der
    // asymptotische Gain auf 2*eqSubGain gesetzt: bei f0 ergibt sich exakt
    // eqSubGain dB (+3 dB => +3,00 dB am Fundamental, +/-0,01 dB), unterhalb
    // von f0 waechst der Gain sanft bis zum Doppelten (Sub-Oktave).
    const float subTarget = eqSubTrack ? fundamentalHz : subFreq;
    const float shelfHz = juce::jlimit (20.0f, 200.0f, subTarget);
    const float shelfGainDb = juce::jlimit (-12.0f, 12.0f, subGain);
    const float A = std::pow (10.0f, shelfGainDb / 20.0f);
    const float w0 = juce::MathConstants<float>::twoPi
                   * juce::jlimit (20.0f, 0.45f * (float) sampleRate, shelfHz)
                   / (float) sampleRate;
    const float cosW0 = std::cos (w0);
    const float sinW0 = std::sin (w0);
    const float alpha = sinW0 * 0.70710678f; // S = 1.0  =>  Q0 = 1/sqrt(2)
    const float twoSqrtAAlpha = 2.0f * std::sqrt (A) * alpha;

    const float b0 = A * ((A + 1.0f) - (A - 1.0f) * cosW0 + twoSqrtAAlpha);
    const float b1 = 2.0f * A * ((A - 1.0f) - (A + 1.0f) * cosW0);
    const float b2 = A * ((A + 1.0f) - (A - 1.0f) * cosW0 - twoSqrtAAlpha);
    const float a0 = (A + 1.0f) + (A - 1.0f) * cosW0 + twoSqrtAAlpha;
    const float a1 = -2.0f * ((A - 1.0f) + (A + 1.0f) * cosW0);
    const float a2 = (A + 1.0f) + (A - 1.0f) * cosW0 - twoSqrtAAlpha;

    eqShelfB0 = b0 / a0;
    eqShelfB1 = b1 / a0;
    eqShelfB2 = b2 / a0;
    eqShelfA1 = a1 / a0;
    eqShelfA2 = a2 / a0;

    // Punch-Band als Biquad-Peak (BPF, 0 dB Peak-Gain, Q 1.0 = 0.7..1.2,
    // SND-080): RBJ-Koeffizienten einmal pro Block.
    const float punchHz = juce::jlimit (500.0f, 8000.0f, punchFreq);
    const float punchW0 = 2.0f * (float) juce::MathConstants<float>::pi
                        * juce::jlimit (20.0f, 0.45f * (float) sampleRate, punchHz)
                        / (float) sampleRate;
    const float punchQ = 1.0f;
    const float punchAlpha = std::sin (punchW0) / (2.0f * punchQ);
    const float punchA0 = 1.0f + punchAlpha;

    eqPunchB0 = punchAlpha / punchA0;
    eqPunchB1 = 0.0f;
    eqPunchB2 = -punchAlpha / punchA0;
    eqPunchA1 = -2.0f * std::cos (punchW0) / punchA0;
    eqPunchA2 = (1.0f - punchAlpha) / punchA0;

    // Punch-/Air-Baender bleiben additive Baesse (unveraendert, ohne Makeup):
    // der Linear-Anteil ist der Boost ueber 1. Clamp verhindert
    // Pegel-Entgleisungen bei extremen Reglerstellungen (eq<gain> ist auf
    // -12..+12 dB begrenzt -> -0.75..+2.98 linear).
    eqPunchGainLin = juce::jlimit (-0.9f, 3.0f, juce::Decibels::decibelsToGain (punchGain) - 1.0f);
    eqAirGainLin   = juce::jlimit (-0.9f, 3.0f, juce::Decibels::decibelsToGain (airGain)   - 1.0f);
}

float KickSynthesisEngine::processEQSample (float x)
{
    if (! std::isfinite (x))
        x = 0.0f;

    // Rumble-Highpass zuerst (komplementäres First-Order-Paar).
    const float rumbleLp = eqHighpassState + (1.0f - eqRumbleCoeff) * (x - eqHighpassState);
    eqHighpassState = rumbleLp;
    const float x0 = x - rumbleLp;

    // B1: Der Low-Shelf laeuft DIREKT auf x0 (kein paralleles Sub-Band).
    // Gain 0 dB => b0 = a0, b1 = a1, b2 = a2 => bit-transparent.
    // Punch: Biquad-Peak (Direct Form I).
    const float punchBand = eqPunchB0 * x0 + eqPunchB1 * eqPunchX1 + eqPunchB2 * eqPunchX2
                          - eqPunchA1 * eqPunchY1 - eqPunchA2 * eqPunchY2;
    eqPunchX2 = eqPunchX1;
    eqPunchX1 = x0;
    eqPunchY2 = eqPunchY1;
    eqPunchY1 = punchBand;

    eqHighShelfState += (1.0f - eqAirCoeff) * (x0 - eqHighShelfState);
    const float airBand = x0 - eqHighShelfState;

    // Low-Shelf (Direct Form I) auf dem Direktsignal.
    const float shelfBand = eqShelfB0 * x0 + eqShelfB1 * eqShelfX1 + eqShelfB2 * eqShelfX2
                          - eqShelfA1 * eqShelfY1 - eqShelfA2 * eqShelfY2;
    eqShelfX2 = eqShelfX1;
    eqShelfX1 = x0;
    eqShelfY2 = eqShelfY1;
    eqShelfY1 = shelfBand;

    // y = Shelf(x0) + Punch + Air. Kein Makeup: der Shelf arbeitet
    // phasentreu auf dem Direktsignal, Punch/Air sind additive Baesse und
    // wirken energieerhoehend (eine Kompensation waere hier nicht noetig,
    // weil der Shelf das Nutzsignal nicht anhebt, sondern nur die Tiefen
    // anhebt - die additiven Baender heben den Gesamtpegel an).
    float y = shelfBand + eqPunchGainLin * punchBand + eqAirGainLin * airBand;

    if (! std::isfinite (y))
    {
        // Ein NaN/Inf darf die Filterzustände nicht dauerhaft vergiften.
        eqShelfX1 = 0.0f;
        eqShelfX2 = 0.0f;
        eqShelfY1 = 0.0f;
        eqShelfY2 = 0.0f;
        eqPunchX1 = 0.0f;
        eqPunchX2 = 0.0f;
        eqPunchY1 = 0.0f;
        eqPunchY2 = 0.0f;
        eqHighShelfState = 0.0f;
        eqHighpassState = 0.0f;
        y = 0.0f;
    }

    return y;
}

//==============================================================================
namespace
{
/** PolyBLEP-Rest einer Sprungstelle (t, dt normiert in 0..1). */
inline float polyBlep (float t, float dt) noexcept
{
    if (dt <= 0.0f)
        return 0.0f;

    if (t < dt)
    {
        const float u = t / dt;
        return u + u - u * u - 1.0f;
    }

    if (t > 1.0f - dt)
    {
        const float u = (t - 1.0f) / dt;
        return u * u + u + u + 1.0f;
    }

    return 0.0f;
}
} // namespace

//==============================================================================
float KickSynthesisEngine::processSub (float subLevel, int subShape)
{
    subEnv *= subDecayCoeff;

    if (subEnv < 1.0e-4f)
        subEnv = 0.0f;

    if (subEnv <= 0.0f)
        return 0.0f;

    subPhase += subOmega;

    if (subPhase >= juce::MathConstants<float>::twoPi)
        subPhase -= juce::MathConstants<float>::twoPi;

    // Normierte Phase für die Bandbegrenzung (PolyBLEP).
    const float t = subPhase / juce::MathConstants<float>::twoPi;

    float s = 0.0f;

    if (subShape == 0)
    {
        s = std::sin (subPhase);
    }
    else if (subShape == 1)
    {
        // Bandbegrenztes Dreieck: integrierter PolyBLEP-Rechteck (SND-052).
        float square = (t < 0.5f) ? 1.0f : -1.0f;
        square += polyBlep (t, subDt);
        square -= polyBlep (std::fmod (t + 0.5f, 1.0f), subDt);

        subTriState += 2.0f * subDt * square;
        subTriState *= 0.9995f; // DC-Leck, verhindert Weglaufen des Integrators
        subTriState = juce::jlimit (-1.5f, 1.5f, subTriState);

        s = subTriState;
    }
    else
    {
        // Bandbegrenzter Rechteck (PolyBLEP, SND-052).
        s = (t < 0.5f) ? 1.0f : -1.0f;
        s += polyBlep (t, subDt);
        s -= polyBlep (std::fmod (t + 0.5f, 1.0f), subDt);
    }

    if (! std::isfinite (s))
    {
        subPhase = 0.0f;
        subTriState = 0.0f;
        subEnv = 0.0f;
        return 0.0f;
    }

    return s * subEnv * subLevel;
}

float KickSynthesisEngine::processBody (float bodyLevel, float bodyRatio)
{
    // Eigener Resonator (Body). Bewusst NICHT die bereits abklingende Membran
    // skalieren: eigener Oszillator, eigene Hüllkurve.
    if (bodyInAttack)
    {
        bodyEnv += (bodyPeak - bodyEnv) * bodyAttackCoeff;

        if (bodyEnv >= bodyPeak * 0.99f)
        {
            bodyEnv = bodyPeak;
            bodyInAttack = false;
        }
    }
    else
    {
        bodyEnv *= bodyDecayCoeff;
    }

    if (bodyEnv < 1.0e-5f)
    {
        bodyEnv = 0.0f;
        return 0.0f;
    }

    // B4: Der Body gleitet mit 0.4x der Pitch-Envelope:
    // bodyRatio = 1 + 0.4*(pitchRatio - 1). Verwendet wird ausschliesslich
    // der Multiplikator pitchRatio (kein getEnvelopeValue()).
    bodyPhase += bodyOmega * juce::jlimit (0.5f, 2.0f, bodyRatio);

    if (bodyPhase >= juce::MathConstants<float>::twoPi)
        bodyPhase -= juce::MathConstants<float>::twoPi;

    const float s = std::sin (bodyPhase);

    if (! std::isfinite (s))
    {
        bodyPhase = 0.0f;
        bodyEnv = 0.0f;
        bodyInAttack = false;
        return 0.0f;
    }

    // Anregung des Shell-Resonators (Oszillator ohne Hüllkurve: die
    // Hüllkurve wird erst auf den Resonator-Ausgang gelegt, damit die
    // Gruppenlaufzeit des Bandpasses (~1/(pi*BW) = 4,8 ms bei 92,5 Hz)
    // den Attack nicht verspätet).
    const float exciter = s;

    // 2-poliger RBJ-Bandpass (Q = 1.4, 0 dB Peak-Gain) bei bodyFreq.
    const float shell = bodyBpB0 * exciter + bodyBpB1 * bodyBpX1 + bodyBpB2 * bodyBpX2
                      - bodyBpA1 * bodyBpY1 - bodyBpA2 * bodyBpY2;
    bodyBpX2 = bodyBpX1;
    bodyBpX1 = exciter;
    bodyBpY2 = bodyBpY1;
    bodyBpY1 = shell;

    // 75 % Resonator + 25 % Sinus auf bodyFreq: der Sinus-Anteil verhindert,
    // dass der Body bei tiefem bodyTuning (schmales Band) zu duenn wird.
    return (0.75f * shell + 0.25f * exciter) * bodyEnv * bodyLevel;
}


float KickSynthesisEngine::processDrive (float x, float amount, float mix, int character)
{
    if (! std::isfinite (x))
        x = 0.0f;

    // Vollständig trocken: bit-transparent (keine Filter, kein DC-Blocker).
    if (mix <= 0.0f)
    {
        driveHpLpState = 0.0f;
        driveLowLpState = 0.0f;
        driveOutDcXPrev = 0.0f;
        driveOutDcYPrev = 0.0f;
        driveUp.reset();
        driveDown.reset();
        driveLowDelay.fill (0.0f);
        driveLowDelayPos = 0;
        return x;
    }

    float wet = x;

    if (amount > 0.0f)
    {
        // Band-Split (SND-014): Low bleibt clean, nur das High-Band wird
        // gesättigt. Komplementäres One-Pole-Paar -> phasenkohärent.
        driveLowLpState += (1.0f - driveSplitCoeff) * (x - driveLowLpState);
        const float low = driveLowLpState;
        float high = x - low;

        // Optionaler Pre-HP im gesättigten Zweig (SND-019: 0 = aus).
        if (drivePreHpActive)
        {
            driveHpLpState += (1.0f - driveHpCoeff) * (high - driveHpLpState);
            high -= driveHpLpState;
        }

        const float drive = 1.0f + amount * 8.0f;

        // ---- B2: Slope-1-Normierung (Kleinsignalverstaerkung = 1.0) ----
        // Vorher: tanh(k*x) * 1/tanh(k)  ->  Steigung k im Ursprung
        //         (bei amount 0,4 => k = 4,2 => +12,5 dB Kleinsignal).
        // Jetzt:  tanh(k*x) / k           ->  Steigung 1 im Ursprung.
        // Alle fuenf Charaktere (Tube/Diode/Fuzz, WaveFold, Alias) werden so
        // normiert, dass ein sehr kleines Eingangssignal unveraendert
        // durchgeht. Der Drive faerbt damit, statt zu verstaerken.
        const float kTube     = drive;                  // Character 0
        const float kDiodePos = drive * 0.8f;            // Character 1 (asymmetrisch)
        const float kDiodeNeg = drive * 1.2f;
        const float kFuzz     = drive * 2.0f;            // Character 2 (Fuzz)
        const float kFold     = 1.0f + amount * 4.0f;   // Character 3 (WaveFold)
        const float aliasSteps   = 1.0f + amount * 15.0f; ///< Character 4 (Alias)
        const float aliasPreGain = 4.0f;
        const float aliasStep = 1.0f / (aliasPreGain * aliasSteps); // Rasterweite

        auto saturate = [kTube, kDiodePos, kDiodeNeg, kFuzz, kFold,
                         aliasStep, character] (float s) -> float
        {
            switch (character)
            {
                case 0: // Tube
                    return std::tanh (s * kTube) / kTube;

                case 1: // Diode (asymmetrisch, beide Aeste mit Steigung 1)
                    return ((s > 0.0f) ? std::tanh (s * kDiodePos) / kDiodePos
                                      : std::tanh (s * kDiodeNeg) / kDiodeNeg);

                case 2: // Fuzz: haertere Kennlinie, gleiche Steigung 1 (SND-016)
                    return std::tanh (s * kFuzz) / kFuzz;

                case 3: // Wave Fold: amplitudenunabhaengige Schwelle (SND-017)
                {
                    // Die Dreiecksfunktion T(u) hat im Ursprung die Steigung 1
                    // und den Wertebereich [-1, 1]. Mit T(k*s)/k bleibt die
                    // Steigung bezueglich s bei 1 (transparent fuer kleine
                    // Signale); der Ausgangsbereich schrumpft auf 1/k.
                    // REVIEW-TODO (Paket B): Gewunschter Parameter
                    //   "driveMakeup" (0..+12 dB) - der WaveFold-Charakter ist
                    //   bei hohem amount dadurch deutlich leiser (bei amount 1
                    //   nur noch +-0,2 statt +-1,0 Ausgangsbereich).
                    float y = std::fmod (s * kFold + 1.0f, 4.0f);
                    if (y < 0.0f)
                        y += 4.0f;
                    const float triangle = (y < 2.0f) ? (y - 1.0f) : (3.0f - y);
                    return triangle / kFold;
                }

                case 4: // Alias: Rasterquantisierung (SND-018)
                {
                    // Quantisierung auf das Raster step. Unterhalb der halben
                    // Rasterbreite bleibt das Signal unveraendert (Steigung 1),
                    // darueber wird quantisiert. Die Ueberblendung (smoothstep)
                    // vermeidet Spruenge am Schwellenpunkt.
                    const float quantised = std::floor (s / aliasStep + 0.5f) * aliasStep;
                    const float magnitude = std::abs (s);
                    const float blend = juce::jlimit (0.0f, 1.0f,
                        (magnitude - 0.5f * aliasStep) / (1.5f * aliasStep));
                    const float w = blend * blend * (3.0f - 2.0f * blend);
                    return s + w * (quantised - s);
                }

                default:
                    return s;
            }
        };

        // 2x-Oversampling des Saturators (SND-179): eigenes Halfband-Pfad
        // (StreamingTwoX, 31 Taps) statt Halbschritt-Mittelung. Der Low-Zweig
        // wird um dieselbe Latenz verzögert, sonst kämmt das 150-Hz-Crossover.
        float saturated;

        if (oversampleEnabled)
        {
            const auto oversampled = driveUp.upsample (high);
            const float s0 = saturate (oversampled.first);
            const float s1 = saturate (oversampled.second);
            saturated = driveDown.downsample (s0, s1);
        }
        else
        {
            saturated = saturate (high);
        }

        float lowPath = low;

        if (oversampleEnabled)
        {
            driveLowDelay[(size_t) driveLowDelayPos] = low;

            int readPos = driveLowDelayPos - StreamingTwoX::totalLatencySamples;

            while (readPos < 0)
                readPos += (int) driveLowDelay.size();

            lowPath = driveLowDelay[(size_t) readPos];
            driveLowDelayPos = (driveLowDelayPos + 1) % (int) driveLowDelay.size();
        }

        const float sampled = saturated;

        // DC-Blocker nur auf dem gesättigten Zweig (~10 Hz statt 38 Hz, SND-013).
        const float saturatedDcFree = applyDcBlocker (sampled, driveOutDcXPrev,
                                                      driveOutDcYPrev, driveDcCoeff);
        wet = lowPath + saturatedDcFree;

        if (! std::isfinite (wet))
        {
            wet = 0.0f;
            driveOutDcXPrev = 0.0f;
            driveOutDcYPrev = 0.0f;
        }
    }

    drivePeak = juce::jmax (drivePeak, std::abs (wet));

    return x * (1.0f - mix) + wet * mix;
}

void KickSynthesisEngine::updateRoom (float level, float decayMs, float rumbleTone, float bpm)
{
    // 4-Tap-Set 41/53/67/79 ms (B6): kuerzere, dichtere Taps => mehr "Raum",
    // weniger "Echo", hoehere Nullstellenfrequenzen (weniger Sub-Wummern).
    // Bei Sync-Modi wird das Set so skaliert, dass der letzte Tap genau eine
    // Notendauer lang ist (SND-093).
    static constexpr float baseTapMs[4] = { 41.0f, 53.0f, 67.0f, 79.0f };
    static constexpr float maxTapMs = 600.0f; // Delay-Line-Grenze (roomBufferSize)

    // Tap-Gains auf Summe 1.0 normieren (B6). Vorher: 1.0/0.8/0.65/0.52
    // (Summe 2.97) => der Room-Wet lag 9,4 dB ueber dem Send-Signal.
    static constexpr float rawTapGain[4] = { 1.0f, 0.80f, 0.65f, 0.52f };
    constexpr float tapGainSum = rawTapGain[0] + rawTapGain[1]
                               + rawTapGain[2] + rawTapGain[3]; // 2.97

    for (int t = 0; t < 4; ++t)
        roomTapGain[(size_t) t] = rawTapGain[t] / tapGainSum;

    float tapScale = 1.0f;

    if (roomSync != RoomSync::free)
    {
        const float beatMs = 60000.0f / juce::jlimit (20.0f, 300.0f, bpm);
        const float noteMs = (roomSync == RoomSync::sixteenth)    ? beatMs * 0.25f
                           : (roomSync == RoomSync::eighth)       ? beatMs * 0.5f
                                                                  : beatMs * 0.75f; // dotted 1/8
        tapScale = juce::jlimit (0.1f, 8.0f, noteMs / baseTapMs[3]);
    }

    const int maxSamples = juce::jmax (4, roomBufferSize - 2);
    auto toSamples = [this, maxSamples] (float ms)
    {
        const int samples = (int) std::lround (ms * 0.001f * (float) sampleRate);
        return juce::jlimit (1, maxSamples, samples);
    };

    for (int t = 0; t < 4; ++t)
    {
        const float tapMs = juce::jmin (maxTapMs, baseTapMs[t] * tapScale);

        roomTapSamplesL[(size_t) t] = toSamples (tapMs);

        // R-Tap-Paare um 8.3 % versetzt -> dekorelliertes Stereobild (SND-100).
        roomTapSamplesR[(size_t) t] = toSamples (juce::jmin (maxTapMs, tapMs * 1.083f));
    }

    // Send = Level allein (SND-092), Pegel sqrt-kalibriert (SND-097).
    roomSendGain = std::sqrt (juce::jlimit (0.0f, 1.0f, level));

    // Decay 50..2000 ms (SND-098); Feedback wirkt einmal pro Tap-Periode.
    const float decaySec = juce::jlimit (0.05f, 2.0f, decayMs * 0.001f);
    roomFeedback = juce::jlimit (0.0f, 0.995f,
                                 std::exp (-6.907755f * (float) roomTapSamplesL[3]
                                           / (decaySec * (float) sampleRate)));

    // SND-096: roomRumble ist jetzt ein Tone-Regler im Send (kein Input-Gain).
    // < 0.5 = Lowpass 200..8000 Hz, > 0.5 = Highpass 60..400 Hz.
    const float tone = juce::jlimit (0.0f, 1.0f, rumbleTone);
    roomToneLowpass = tone < 0.5f;

    const float toneHz = roomToneLowpass
        ? 200.0f * std::pow (40.0f, tone * 2.0f)
        : 60.0f * std::pow (6.6667f, (tone - 0.5f) * 2.0f);

    roomToneCoeff = std::exp (-juce::MathConstants<float>::twoPi
                              * juce::jlimit (10.0f, 0.45f * (float) sampleRate, toneHz)
                              / (float) sampleRate);

    // ---- B6: Sub-Cut im Room-Send (2. Ordnung, 70 Hz) ----
    // REVIEW-TODO (Paket B): Gewunschter Parameter "roomSubCutHz" (20..200 Hz,
    //   Default 70) - die Sub-Cut-Frequenz ist hier fest verdrahtet.
    // Ohne diesen Cut laufen vier verzögerte 46-Hz-Kopien in den Raum
    // (Kammfilter-Nullen bei 27,8/55,6/83,3 Hz => "Wummern"). Der Dry-Pfad
    // bleibt unangetastet; nur der Send wird beschnitten.
    {
        constexpr float roomSubCutHz = 70.0f;
        constexpr float roomSubCutQ = 0.70710678f; // Butterworth
        const float w0 = juce::MathConstants<float>::twoPi
                       * juce::jlimit (20.0f, 0.45f * (float) sampleRate, roomSubCutHz)
                       / (float) sampleRate;
        const float cosW0 = std::cos (w0);
        const float alpha = std::sin (w0) / (2.0f * roomSubCutQ);
        const float a0 = 1.0f + alpha;

        roomHpB0 = ((1.0f + cosW0) * 0.5f) / a0;
        roomHpB1 = (-(1.0f + cosW0)) / a0;
        roomHpB2 = ((1.0f + cosW0) * 0.5f) / a0;
        roomHpA1 = (-2.0f * cosW0) / a0;
        roomHpA2 = (1.0f - alpha) / a0;
    }
}

StereoMix KickSynthesisEngine::processRoomRumble (const StereoMix& in)
{
    if (roomBufferSize < 8 || roomDelayL.empty() || roomDelayR.empty())
        return {};

    float sendL = std::isfinite (in.l) ? in.l : 0.0f;
    float sendR = std::isfinite (in.r) ? in.r : 0.0f;

    // Tone im Send (SND-096).
    if (roomToneLowpass)
    {
        roomToneLpL += (1.0f - roomToneCoeff) * (sendL - roomToneLpL);
        roomToneLpR += (1.0f - roomToneCoeff) * (sendR - roomToneLpR);
        sendL = roomToneLpL;
        sendR = roomToneLpR;
    }
    else
    {
        const float hpL = roomToneCoeff * (roomToneHpYPrevL + sendL - roomToneHpXPrevL);
        roomToneHpXPrevL = sendL;
        roomToneHpYPrevL = std::isfinite (hpL) ? hpL : 0.0f;

        const float hpR = roomToneCoeff * (roomToneHpYPrevR + sendR - roomToneHpXPrevR);
        roomToneHpXPrevR = sendR;
        roomToneHpYPrevR = std::isfinite (hpR) ? hpR : 0.0f;

        sendL = roomToneHpYPrevL;
        sendR = roomToneHpYPrevR;
    }

    // B6: Sub-Cut (2. Ordnung, 70 Hz) direkt auf dem Send-Signal. Der
    // Grundton (46 Hz) darf nicht in den Raum laufen, sonst hoert man vier
    // verzögerte Tief-Kopien (Kammfilter-Nullen, "Wummern").
    {
        const float hpL = roomHpB0 * sendL + roomHpB1 * roomHpX1L + roomHpB2 * roomHpX2L
                        - roomHpA1 * roomHpY1L - roomHpA2 * roomHpY2L;
        roomHpX2L = roomHpX1L;
        roomHpX1L = sendL;
        roomHpY2L = roomHpY1L;
        roomHpY1L = std::isfinite (hpL) ? hpL : 0.0f;
        sendL = roomHpY1L;

        const float hpR = roomHpB0 * sendR + roomHpB1 * roomHpX1R + roomHpB2 * roomHpX2R
                        - roomHpA1 * roomHpY1R - roomHpA2 * roomHpY2R;
        roomHpX2R = roomHpX1R;
        roomHpX1R = sendR;
        roomHpY2R = roomHpY1R;
        roomHpY1R = std::isfinite (hpR) ? hpR : 0.0f;
        sendR = roomHpY1R;
    }

    // Taps lesen (vor dem Schreiben): 4 Taps pro Kanal, per-Tap-Gain.
    float tapL = 0.0f;
    float tapR = 0.0f;
    float lastL = 0.0f;
    float lastR = 0.0f;

    for (int t = 0; t < 4; ++t)
    {
        int readL = roomPos - roomTapSamplesL[(size_t) t];

        if (readL < 0)
            readL += roomBufferSize;

        int readR = roomPos - roomTapSamplesR[(size_t) t];

        if (readR < 0)
            readR += roomBufferSize;

        const float valueL = roomDelayL[(size_t) readL];
        const float valueR = roomDelayR[(size_t) readR];

        tapL += roomTapGain[(size_t) t] * valueL;
        tapR += roomTapGain[(size_t) t] * valueR;

        if (t == 3)
        {
            lastL = valueL;
            lastR = valueR;
        }
    }

    // Feedback nur über den letzten Tap, DC-geblockt bei 2 Hz (SND-099).
    const float feedbackL = applyDcBlocker (lastL, roomFbXPrevL, roomFbYPrevL, roomDcCoeff)
                          * roomFeedback;
    const float feedbackR = applyDcBlocker (lastR, roomFbXPrevR, roomFbYPrevR, roomDcCoeff)
                          * roomFeedback;

    float writeL = sendL + feedbackL;
    float writeR = sendR + feedbackR;

    // Ein einzelnes NaN/Inf darf die Delay-Line nicht vergiften.
    if (! std::isfinite (writeL))
    {
        writeL = 0.0f;
        roomToneLpL = 0.0f;
        roomFbXPrevL = 0.0f;
        roomFbYPrevL = 0.0f;
    }

    if (! std::isfinite (writeR))
    {
        writeR = 0.0f;
        roomToneLpR = 0.0f;
        roomFbXPrevR = 0.0f;
        roomFbYPrevR = 0.0f;
    }

    roomDelayL[(size_t) roomPos] = writeL;
    roomDelayR[(size_t) roomPos] = writeR;
    roomPos = (roomPos + 1) % roomBufferSize;

    return { tapL * roomSendGain, tapR * roomSendGain };
}
//==============================================================================
namespace
{
/** Kubische Hermite-Interpolation auf dem Intervall [p1, p2] (B7).

    Tangenten:  m1 = (p2 - p0)/2      (zentrale Differenz an p1)
                m2 = 2*p2 - 3*p1 + p0 (zweite Differenz an p2)
   Fuer ein lineares Signal geht die Kurve in die lineare Interpolation
    ueber, fuer ein sinusfoermiges Signal ist sie zweite Ordnung genau.
    t = 0 liefert p1, t = 1 liefert p2. */
inline float hermiteInterp (float p1, float p2,
                             float m1, float m2, float t) noexcept
{
    const float t2 = t * t;
    const float t3 = t2 * t;
    const float h00 =  2.0f * t3 - 3.0f * t2 + 1.0f;
    const float h10 =        t3 - 2.0f * t2 + t;
    const float h01 = -2.0f * t3 + 3.0f * t2;
    const float h11 =        t3 -       t2;
    return h00 * p1 + h10 * m1 + h01 * p2 + h11 * m2;
}
} // namespace

//==============================================================================
void KickSynthesisEngine::processOutputStage (float& l, float& r, float gainDb,
                                              float panL, float panR)
{
    // -1 dBFS-Kalibrierung (SND-009): 0 dB outputGain endet bei ~-1 dBFS Peak.
    static constexpr float calibrationGain = 0.8912509f; // -1 dB

    // Gain
    const float g = juce::Decibels::decibelsToGain (gainDb);
    l *= g;
    r *= g;

    // Mono-Below (komplementäres First-Order-Paar, phasenkohärent):
    // low = LP(side), high = side - low. Die Tiefen bleiben im Mid erhalten,
    // nur der Side-Anteil oberhalb der Grenzfrequenz bleibt stereo.
    if (monoBelowActive)
    {
        const float mid  = (l + r) * 0.5f;
        const float side = (l - r) * 0.5f;

        monoBelowState = monoBelowCoeffValue * monoBelowState
                       + (1.0f - monoBelowCoeffValue) * side;

        if (! std::isfinite (monoBelowState))
            monoBelowState = 0.0f;

        const float highSide = side - monoBelowState;

        l = mid + highSide * outputWidthValue;
        r = mid - highSide * outputWidthValue;
    }

    // Soft-Clip vor Pan/Limiter (SND-007).
    // B3: Slope-1-Softclipper  y = x / sqrt(1 + (x/ceiling)^2)
    //   -> Kleinsignalverstaerkung exakt 1.0, Asymptote +-ceiling, stetig.
    //   "outputClip" steuert NUR das Ceiling (1.0 - 0.5*clip); es gibt
    //   keinen parameterabhaengigen Gain mehr (vorher: +7,7 dB Makeup).
    // SND-179: bei "oversampleEnabled" 2x Halfband-FIR (31 Taps) je Kanal,
    // sonst direkte Auswertung. Die FIR-Latenz (15 Samples) ist L/R-identisch.
    if (clipActive)
    {
        auto softClip = [this] (float x) -> float
        {
            const float t = x / clipCeiling;
            return x / std::sqrt (1.0f + t * t);
        };

        auto processClip = [&softClip] (float x, StreamingTwoX& up, StreamingTwoX& down,
                                        bool oversample) -> float
        {
            if (! oversample)
                return softClip (x);

            const auto twoSamples = up.upsample (x);
            return down.downsample (softClip (twoSamples.first),
                                    softClip (twoSamples.second));
        };

        const float inL = l;
        const float inR = r;
        l = processClip (l, clipUp[0], clipDown[0], oversampleEnabled);
        r = processClip (r, clipUp[1], clipDown[1], oversampleEnabled);

        // SND-006/020: Clip-GR = 1 - |y|/|x| (in dB: -20*log10(|y|/|x|)),
        // clipMinGain = Minimum der Gain-Reduktion im Block.
        // Der Slope-1-Clipper ist deterministisch: |y|/|x| = 1/sqrt(1+(x/C)^2).
        // Der Gain-Faktor ist monoton fallend - das Block-Minimum entspricht
        // damit dem groessten |x| im Block. Die Auswertung der Kennlinie ist
        // exakt die vom Clipper erzeugte Daempfung und frei von Latenz- und
        // Transient-Artefakten des 2x-Halfband-Pfades (15 Samples Latenz).
        auto gainRatio = [this] (float in)
        {
            const float magnitude = std::abs (in);

            if (magnitude < 1.0e-4f)
                return 1.0f;

            const float t = magnitude / clipCeiling;
            return 1.0f / std::sqrt (1.0f + t * t);
        };

        clipMinGain = juce::jmin (clipMinGain,
                                  juce::jmin (gainRatio (inL), gainRatio (inR)));
    }

    // ---- B7: True-Peak-Detektor (4x interpoliert, stereo-linked) ----
    // Das Intervall zwischen dem vorigen und dem aktuellen Sample wird mit
    // einer kubischen Hermite-Interpolation verfeinert (t = 0.25/0.5/0.75),
    // also 4 Punkte pro Sample. Damit werden Inter-Sample-Peaks erkannt,
    // die ein reiner Sample-Peak-Detektor uebersieht (Club-Anlagen mit
    // True-Peak-Limitern wuerden sonst doppelt limitieren).
    // Hinweis: der Detektor kennt das Intervall erst beim Sample n (eine
    // Sample Verzoegerung); der Peak-Hold (2 ms) haelt die Reduktion.
    // REVIEW-TODO (Paket B): Gewunschter Parameter "limiterLookahead"
    //   (0 = aus, 1 = 1 Sample Vorlauf). Ohne Vorlauf kann das erste Sample
    //   eines Transients das Ceiling um den Interpollationsfehler ueberschreiten.
    float truePeak = juce::jmax (std::abs (l), std::abs (r));

    if (limiterHistoryValid)
    {
        const float m1L = 0.5f * (l - limiterPrev2L);
        const float m2L = 2.0f * l - 3.0f * limiterPrevL + limiterPrev2L;
        const float m1R = 0.5f * (r - limiterPrev2R);
        const float m2R = 2.0f * r - 3.0f * limiterPrevR + limiterPrev2R;

        for (int k = 1; k < 4; ++k)
        {
            const float t = (float) k * 0.25f;
            truePeak = juce::jmax (truePeak, std::abs (
                hermiteInterp (limiterPrevL, l, m1L, m2L, t)));
            truePeak = juce::jmax (truePeak, std::abs (
                hermiteInterp (limiterPrevR, r, m1R, m2R, t)));
        }
    }

    limiterPrev2L = limiterPrevL;
    limiterPrevL = l;
    limiterPrev2R = limiterPrevR;
    limiterPrevR = r;
    limiterHistoryValid = true;

    blockTruePeak = juce::jmax (blockTruePeak, truePeak);

    // Limiter: Peak-Hold-Envelope + Instant-Attack + Release.
    // Stereo-linked über max(|L|,|R|) (SND-008) - hier auf dem True-Peak.
    const float peak = truePeak;

    if (! std::isfinite (peak))
    {
        l = 0.0f;
        r = 0.0f;
        limiterGain = 1.0f;
        limiterPeakEnv = 0.0f;
        return;
    }

    // Peak-Hold: das Maximum der letzten ~2 ms bleibt stehen, damit der Gain
    // nicht pro Sample zwischen den Nulldurchgängen auf- und zuschnappt.
    limiterPeakEnv = juce::jmax (peak, limiterPeakEnv * limiterPeakDecay);

    const float needed = (limiterPeakEnv > limiterCeiling)
        ? limiterCeiling / limiterPeakEnv
        : 1.0f;

    if (needed < limiterGain)
        limiterGain += (needed - limiterGain) * limiterAttackCoeff;   // schnell
    else
        limiterGain += (needed - limiterGain) * limiterReleaseCoeff;  // langsam

    // Garantie: kein Overshoot über das Ceiling (Instant-Attack-Obergrenze).
    if (limiterGain > needed)
        limiterGain = needed;

    if (! std::isfinite (limiterGain) || limiterGain < 0.0f)
    {
        limiterGain = 1.0f;
        limiterPeakEnv = 0.0f;
    }

    limiterMinGain = juce::jmin (limiterMinGain, limiterGain);

    l *= limiterGain;
    r *= limiterGain;

    // Kalibrierung nach dem Limiter: garantiert -1 dBFS Peak (SND-009).
    l *= calibrationGain;
    r *= calibrationGain;

    // Pan erst nach Clip/Limiter (SND-007): Balance-Gesetz, Gains <= 1,
    // damit das Ceiling nicht durch Pan überschritten werden kann.
    l *= panL;
    r *= panR;
}

//==============================================================================
void KickSynthesisEngine::applyEnableTransitions (const ModuleEnableState& current)
{
    auto rising = [] (bool now, bool& previous)
    {
        const bool edge = now && ! previous;
        previous = now;
        return edge;
    };

    // Quellen/Sends: Reset beim Wiedereinschalten verhindert Ghost-Hits und
    // stehengebliebene Tails. Serie-Prozessoren: Bypass, beim Einschalten
    // werden die Filterzustände geleert (kein Knall aus alten Zuständen).
    if (rising (current.membrane, previousEnableState.membrane))
        membrane.reset();

    if (rising (current.tube, previousEnableState.tube))
        tube.reset();

    if (rising (current.pitchEnv, previousEnableState.pitchEnv))
        pitchEnv.reset();

    if (rising (current.click, previousEnableState.click))
        click.reset();

    if (rising (current.sub, previousEnableState.sub))
    {
        subPhase = 0.0f;
        subEnv = 0.0f;
        subTriState = 0.0f;
    }

    if (rising (current.body, previousEnableState.body))
    {
        bodyPhase = 0.0f;
        bodyEnv = 0.0f;
        bodyPeak = 0.0f;
        bodyInAttack = false;
        bodyBpX1 = 0.0f;
        bodyBpX2 = 0.0f;
        bodyBpY1 = 0.0f;
        bodyBpY2 = 0.0f;
    }

    if (rising (current.drive, previousEnableState.drive))
    {
        driveOutDcXPrev = 0.0f;
        driveOutDcYPrev = 0.0f;
        driveHpLpState = 0.0f;
        driveLowLpState = 0.0f;
        driveUp.reset();
        driveDown.reset();
        driveLowDelay.fill (0.0f);
        driveLowDelayPos = 0;
    }

    if (rising (current.eq, previousEnableState.eq))
    {
        eqShelfX1 = 0.0f;
        eqShelfX2 = 0.0f;
        eqShelfY1 = 0.0f;
        eqShelfY2 = 0.0f;
        eqPunchX1 = 0.0f;
        eqPunchX2 = 0.0f;
        eqPunchY1 = 0.0f;
        eqPunchY2 = 0.0f;
        eqHighShelfState = 0.0f;
        eqHighpassState = 0.0f;
    }

    if (rising (current.comp, previousEnableState.comp))
        compressor.reset();

    if (rising (current.room, previousEnableState.room))
    {
        roomPos = 0;
        roomToneLpL = 0.0f;
        roomToneLpR = 0.0f;
        roomToneHpXPrevL = 0.0f;
        roomToneHpYPrevL = 0.0f;
        roomToneHpXPrevR = 0.0f;
        roomToneHpYPrevR = 0.0f;
        roomFbXPrevL = 0.0f;
        roomFbYPrevL = 0.0f;
        roomFbXPrevR = 0.0f;
        roomFbYPrevR = 0.0f;
        roomHpX1L = 0.0f;
        roomHpX2L = 0.0f;
        roomHpY1L = 0.0f;
        roomHpY2L = 0.0f;
        roomHpX1R = 0.0f;
        roomHpX2R = 0.0f;
        roomHpY1R = 0.0f;
        roomHpY2R = 0.0f;
        std::fill (roomDelayL.begin(), roomDelayL.end(), 0.0f);
        std::fill (roomDelayR.begin(), roomDelayR.end(), 0.0f);
    }

    if (rising (current.reverb, previousEnableState.reverb))
        reverb.reset();

    if (rising (current.materialRoom, previousEnableState.materialRoom))
        materialRoom.reset();

    if (rising (current.higgs, previousEnableState.higgs))
        higgs.reset();
}
//==============================================================================
void KickSynthesisEngine::prepareParameterModulation (ParameterSmoothing& smoothing,
                                                       int samplesPerBlock)
{
    const auto& ids = ParameterIDs::getAllIDs();
    const int count = static_cast<int> (ids.size());

    if (count <= 0 || count > static_cast<int> (modulatedParameterValues.size()))
    {
        parameterModulationPrepared = true;
        return;
    }

    std::array<float, ParameterPatchGraph::maxParameterCount> baseNormalised {};
    std::array<float, ParameterPatchGraph::maxParameterCount> effectiveNormalised {};
    std::array<float, ParameterPatchGraph::maxParameterCount> effectiveValues {};

    // Gecachte Zeiger statt dynamic_cast + getParameter(String) pro Block.
    auto rangedParameter = [this] (int index) -> juce::RangedAudioParameter*
    {
        if (! juce::isPositiveAndBelow (index, maxCachedParameters))
            return nullptr;
        return cachedParameters[(size_t) index];
    };

    for (int i = 0; i < count; ++i)
    {
        // Basis ist der rohe Parameter-Zielwert (nicht der geglättete/bereits
        // modulierte Wert) -> keine blockübergreifende CV-Rückkopplung.
        const float baseValue = smoothing.getTarget (i);
        if (auto* parameter = rangedParameter (i))
            baseNormalised[(size_t) i] = parameter->convertTo0to1 (baseValue);
        else
            baseNormalised[(size_t) i] = 0.5f;

        effectiveNormalised[(size_t) i] = baseNormalised[(size_t) i];
    }

    parameterPatchGraph.fillSnapshot (parameterSnapshot);

    const int snapshotCount = parameterSnapshot.connectionCount;
    const int orderCount    = juce::jmin (count, parameterSnapshot.parameterCount);
    const int blockSamples  = juce::jmax (1, samplesPerBlock);

    // Virtuelle CV-Quellen (LFO) einmal pro Block weiterdrehen (SND-156/157).
    parameterPatchGraph.advanceVirtualSources ((float) sampleRate, blockSamples);

    // Kabel-CV vorab berechnen: amount, Polarität, Rate (1/64-Hold) und Slew
    // stecken in cableCv (SND-152/153/154/155). Der Pre-Pass liest die
    // Basiswerte, damit verkettete Kabel stabil bleiben.
    std::array<float, ParameterPatchGraph::maxConnectionCount> cableCv {};
    const float bpm = smoothing.getById (ParameterIDs::sequencerBpm);

    parameterPatchGraph.prepareCableCv (parameterSnapshot, baseNormalised, count,
                                        (float) sampleRate, blockSamples, bpm, cableCv);

    // Topologische Auswertung: fertig prozessierte Kabel-CV summieren.
    for (int step = 0; step < orderCount; ++step)
    {
        const int target = parameterSnapshot.order[(size_t) step];

        if (target < 0 || target >= count)
            continue;

        float sum = 0.0f;

        for (int c = 0; c < snapshotCount; ++c)
        {
            const auto& connection = parameterSnapshot.connections[(size_t) c];

            if (connection.sourceParam < 0 || connection.targetParam < 0)
                continue;
            if (connection.targetParam >= count)
                continue;

            // Enable-Flags stammen aus demselben Snapshot -> keine Locks pro Kabel.
            if (! parameterSnapshot.sourceEnabled[(size_t) connection.sourceParam] ||
                ! parameterSnapshot.targetEnabled[(size_t) connection.targetParam])
                continue;

            if (connection.targetParam == target)
                sum += cableCv[(size_t) c];
        }

        const float target01 = baseNormalised[(size_t) target] + sum;

        // SND-161: optionaler 0..1-Wrap statt Clamp.
        effectiveNormalised[(size_t) target] = parameterSnapshot.wrapEnabled
            ? ParameterPatchGraph::wrapNormalised (target01)
            : juce::jlimit (0.0f, 1.0f, target01);
    }

    for (int i = 0; i < count; ++i)
    {
        if (auto* parameter = rangedParameter (i))
            effectiveValues[(size_t) i] =
                parameter->convertFrom0to1 (effectiveNormalised[(size_t) i]);
        else
            effectiveValues[(size_t) i] = baseNormalised[(size_t) i];
    }

    modulatedParameterValues = effectiveValues;
    smoothing.applyModulatedValues (effectiveValues.data(), count);
    parameterModulationPrepared = true;
}
//==============================================================================
void KickSynthesisEngine::render (juce::AudioBuffer<float>& buffer, int numSamples,
                                  ParameterSmoothing& smoothing)
{
    const int numChannels = buffer.getNumChannels();
    const bool isStereo = numChannels >= 2;

    if (! parameterModulationPrepared)
        prepareParameterModulation (smoothing, numSamples);

    // ---- Mixer-Parameter (einmal pro Block) ----
    mixer.updateParameters (smoothing);

    // ---- Einmal pro Block: Parameter lesen (Reihenfolge gemäß §4) ----
    const float membraneTension      = smoothing.getById (ParameterIDs::membraneTension);
    const float membraneRadius       = smoothing.getById (ParameterIDs::membraneRadius);
    const float membraneMass         = smoothing.getById (ParameterIDs::membraneMass);
    const float membraneDamping      = smoothing.getById (ParameterIDs::membraneDamping);
    const float membraneModes        = smoothing.getById (ParameterIDs::membraneModes);
    const float fundamentalHz        = smoothing.getById (ParameterIDs::fundamentalHz);
    const float membraneNonlinearity = smoothing.getById (ParameterIDs::membraneNonlinearity);
    const float membraneStiffness    = smoothing.getById (ParameterIDs::membraneStiffness);

    const float tubeResonanceFreq    = smoothing.getById (ParameterIDs::tubeResonanceFreq);
    const float tubeResonanceQ       = smoothing.getById (ParameterIDs::tubeResonanceQ);
    const float tubeResonanceLevel   = smoothing.getById (ParameterIDs::tubeResonanceLevel);
    const float tubeCoupling         = smoothing.getById (ParameterIDs::tubeCoupling);
    const float hollowMetallic       = smoothing.getById (ParameterIDs::hollowMetallic);

    const float pitchEnvAttack       = smoothing.getById (ParameterIDs::pitchEnvAttack);
    const float pitchEnvDecay        = smoothing.getById (ParameterIDs::pitchEnvDecay);
    const float pitchEnvDepth        = smoothing.getById (ParameterIDs::pitchEnvDepth);
    const float pitchEnvCurve        = smoothing.getById (ParameterIDs::pitchEnvCurve);

    const float clickLevel           = smoothing.getById (ParameterIDs::clickLevel);
    const float clickNoiseMix        = smoothing.getById (ParameterIDs::clickNoiseMix);
    const float clickDecay           = smoothing.getById (ParameterIDs::clickDecay);
    const float clickTone            = smoothing.getById (ParameterIDs::clickTone);
    const float clickDistortion      = smoothing.getById (ParameterIDs::clickDistortion);
    const int   clickBeaterMaterial  = (int) smoothing.getUnsmoothedById (ParameterIDs::clickBeaterMaterial);

    const float bodyLevel            = smoothing.getById (ParameterIDs::bodyLevel);
    const float bodyAttack           = smoothing.getById (ParameterIDs::bodyAttack);
    const float bodyDecay            = smoothing.getById (ParameterIDs::bodyDecay);
    const float bodyTuning           = smoothing.getById (ParameterIDs::bodyTuning);

    const float subLevel             = smoothing.getById (ParameterIDs::subLevel);
    const float subDecay             = smoothing.getById (ParameterIDs::subDecay);
    const float subDetune            = smoothing.getById (ParameterIDs::subDetune);
    const int   subShape             = (int) smoothing.getUnsmoothedById (ParameterIDs::subShape);

    const float driveAmount          = smoothing.getById (ParameterIDs::driveAmount);
    const float driveMix             = smoothing.getById (ParameterIDs::driveMix);
    const int   driveCharacter       = (int) smoothing.getUnsmoothedById (ParameterIDs::driveCharacter);
    const float drivePreHP           = smoothing.getById (ParameterIDs::drivePreHP);

    const float eqSubFreq            = smoothing.getById (ParameterIDs::eqSubFreq);
    const float eqSubGain            = smoothing.getById (ParameterIDs::eqSubGain);
    const float eqPunchFreq          = smoothing.getById (ParameterIDs::eqPunchFreq);
    const float eqPunchGain          = smoothing.getById (ParameterIDs::eqPunchGain);
    const float eqAirFreq            = smoothing.getById (ParameterIDs::eqAirFreq);
    const float eqAirGain            = smoothing.getById (ParameterIDs::eqAirGain);
    const float eqRumbleHP           = smoothing.getById (ParameterIDs::eqRumbleHP);

    const float compThreshold        = smoothing.getById (ParameterIDs::compThreshold);
    const float compRatio            = smoothing.getById (ParameterIDs::compRatio);
    const float compAttack           = smoothing.getById (ParameterIDs::compAttack);
    const float compRelease          = smoothing.getById (ParameterIDs::compRelease);
    const float compKnee             = smoothing.getById (ParameterIDs::compKnee);
    const float compMakeup           = smoothing.getById (ParameterIDs::compMakeup);

    const float outputClip           = smoothing.getById (ParameterIDs::outputClip);
    const float outputLimiter        = smoothing.getById (ParameterIDs::outputLimiter);
    const float outputWidth          = smoothing.getById (ParameterIDs::outputWidth);
    const float outputMonoBelow      = smoothing.getById (ParameterIDs::outputMonoBelow);

    const float roomLevel            = smoothing.getById (ParameterIDs::roomLevel);
    const float roomDecay            = smoothing.getById (ParameterIDs::roomDecay);
    const float roomRumble           = smoothing.getById (ParameterIDs::roomRumble);
    const float sequencerBpm         = smoothing.getById (ParameterIDs::sequencerBpm);

    const int   reverbMaterial       = (int) smoothing.getUnsmoothedById (ParameterIDs::reverbMaterial);
    const bool  reverbFreeze         = smoothing.getUnsmoothedById (ParameterIDs::reverbFreeze) > 0.5f;
    const float reverbGate           = smoothing.getUnsmoothedById (ParameterIDs::reverbGate);
    const float reverbPreDelay       = smoothing.getById (ParameterIDs::reverbPreDelay);
    const float reverbDecay          = smoothing.getById (ParameterIDs::reverbDecay);
    const float reverbSize           = smoothing.getById (ParameterIDs::reverbSize);
    const float reverbDamping        = smoothing.getById (ParameterIDs::reverbDamping);
    const float reverbMix            = smoothing.getById (ParameterIDs::reverbMix);
    const float reverbLowCut         = smoothing.getById (ParameterIDs::reverbLowCut);
    const float reverbHighCut        = smoothing.getById (ParameterIDs::reverbHighCut);

    const float velocitySensitivity  = juce::jlimit (0.0f, 1.0f,
                                                     smoothing.getById (ParameterIDs::velocitySensitivity));

    // ---- Feature-Findings (SND-068/134/174/175/177/178/179/181) ----
    const float transientAttackDb   = smoothing.getById (ParameterIDs::transientAttack);
    const float transientSustainDb  = smoothing.getById (ParameterIDs::transientSustain);
    const float duckDepth           = juce::jlimit (0.0f, 24.0f, smoothing.getById (ParameterIDs::duckDepth));
    const float duckReleaseMs       = juce::jlimit (20.0f, 500.0f, smoothing.getById (ParameterIDs::duckRelease));
    const bool  tubeTrackParam      = smoothing.getUnsmoothedById (ParameterIDs::tubeTrack) > 0.5f;
    const bool  oversampleParam     = smoothing.getUnsmoothedById (ParameterIDs::oversampleEnabled) > 0.5f;
    const float fxWidthParam        = juce::jlimit (0.0f, 1.0f, smoothing.getById (ParameterIDs::fxWidth));
    const float humanizeParam       = juce::jlimit (0.0f, 1.0f, smoothing.getById (ParameterIDs::humanize));
    const float velToClickParam     = juce::jlimit (0.0f, 1.0f, smoothing.getById (ParameterIDs::velToClick));
    const float envFollowAttack     = juce::jlimit (0.1f, 1000.0f,
                                                    smoothing.getById (ParameterIDs::envFollowAttack));
    const float envFollowRelease    = juce::jlimit (1.0f, 2000.0f,
                                                    smoothing.getById (ParameterIDs::envFollowRelease));

    // ---- Globales Tuning (SND-128/129/132/135) ----
    // rootNote ist die Note von f0 (MIDI-Mapping in PluginProcessor); Key/Scale
    // snappen f0 und die Step-/MIDI-Pitch-Offsets. Choice/Bools ungesmoothed.
    blockKeyRoot = juce::jlimit (0, 11, (int) std::lround (
        smoothing.getUnsmoothedById (ParameterIDs::keyRoot)));
    blockScale = juce::jlimit (0, 1, (int) std::lround (
        smoothing.getUnsmoothedById (ParameterIDs::scale)));
    blockGlideMs = juce::jlimit (0.0f, 200.0f, smoothing.getById (ParameterIDs::glideMs));
    velocityToPitch = juce::jlimit (0.0f, 1.0f, smoothing.getById (ParameterIDs::velToPitch));

    // SND-056: Sub-Ratio als sichtbarer Schalter (0.5 / 1.0), ungesmoothed.
    setSubRatio (smoothing.getUnsmoothedById (ParameterIDs::subRatio));

    // ---- Feature-Block-Konstanten ----
    // SND-068: Tube-Resonanz folgt f0 nur mit aktiviertem "tubeTrack".
    tubeTracking = tubeTrackParam;

    // SND-179: 2x-Oversampling für Drive-Sättigung + Soft-Clip (Halfband-FIR).
    oversampleEnabled = oversampleParam;

    // SND-181: FX-Wet-Breite (nur Room/Reverb/MaterialRoom-Tails).
    fxWidthValue = fxWidthParam;

    // SND-177/178: Velocity->Click und Humanize-Menge pro Block.
    velocityToClick = velToClickParam;
    humanizeAmount = humanizeParam;

    // SND-156: Envelope-Follower-Zeitkonstanten aus der APVTS (Attack/Release).
    envFollowAttackCoeff = std::exp (-1.0f / juce::jmax (1.0f, envFollowAttack * 0.001f * (float) sampleRate));
    envFollowReleaseCoeff = std::exp (-1.0f / juce::jmax (1.0f, envFollowRelease * 0.001f * (float) sampleRate));

    // SND-175: Transient-Shaper im Bus vor Drive/Comp (Attack/Sustain 0..+12 dB).
    transientShaper.setAttackDb (transientAttackDb);
    transientShaper.setSustainDb (transientSustainDb);

    // SND-174: internes Sidechain/Ducking (Quelle: Kick/Sub-Signal, ~2 ms Attack).
    duckDepthDb = duckDepth;
    duckAttackCoeff = std::exp (-1.0f / juce::jmax (1.0f, 0.002f * (float) sampleRate));
    duckReleaseCoeff = std::exp (-1.0f / juce::jmax (1.0f, duckReleaseMs * 0.001f * (float) sampleRate));

    clipMinGain = 1.0f;
    clipGainReductionDb = 0.0f;

    baseFundamentalHz = snapFundamentalToScale (fundamentalHz, blockKeyRoot, blockScale);
    baseNoteNumber = (int) std::lround (69.0f + 12.0f * std::log2 (baseFundamentalHz / 440.0f));

    // ---- Automatisches Layering ----
    // Kein eigener Regler ausser dem Ein/Aus: alle Layer-Werte werden aus den
    // bestehenden Modul-Parametern abgeleitet (Laenge/Attack/Decay/Pegel/
    // M5: Rausch-/Tick-Anteil des Clicks (0 = reiner Tick, 1 = reines Rausch).
    click.setNoiseMix (clickNoiseMix);

    // Dichte/Bandlage). Bool ungesmoothed, damit der Schalter nicht nachzieht.
    {
        const bool layeringOn = smoothing.getUnsmoothedById (ParameterIDs::layeringEnabled) > 0.5f;

        LayeringStage::Params lp;
        lp.enabled         = layeringOn;
        lp.amountPct       = smoothing.getById (ParameterIDs::layeringAmount);
        lp.fundamentalHz   = baseFundamentalHz;
        lp.subLevel        = subLevel;
        lp.subDecayMs      = subDecay;
        lp.bodyAttackMs    = bodyAttack;
        lp.bodyDecayMs     = bodyDecay;
        lp.clickLevel      = clickLevel;
        lp.clickDecayMs    = clickDecay;
        lp.clickTone       = clickTone;
        lp.clickDistortion = clickDistortion;
        lp.driveAmount     = driveAmount;
        layering.setParams (lp);
    }

    // ---- Modul-Ein/Aus ----
    // Semantik (konsistent): Quellen/Sends (Membrane, Body, Tube, Sub, Click,
    // Room, Reverb, Higgs) werden stummgeschaltet; Serie-Prozessoren (Drive, EQ,
    // Comp) werden gebypasst; PitchEnv moduliert nichts; Output schaltet den
    // Ausgang stumm. Ein deaktiviertes Modul beeinflusst den Ausgang damit nie.
    ModuleEnableState enableState;
    enableState.membrane = smoothing.getById (ParameterIDs::membraneEnabled) > 0.5f;
    enableState.tube     = smoothing.getById (ParameterIDs::tubeEnabled)     > 0.5f;
    enableState.pitchEnv = smoothing.getById (ParameterIDs::pitchEnvEnabled) > 0.5f;
    enableState.click    = smoothing.getById (ParameterIDs::clickEnabled)    > 0.5f;
    enableState.body     = smoothing.getById (ParameterIDs::bodyEnabled)     > 0.5f;
    enableState.sub      = smoothing.getById (ParameterIDs::subEnabled)      > 0.5f;
    enableState.drive    = smoothing.getById (ParameterIDs::driveEnabled)    > 0.5f;
    enableState.eq       = smoothing.getById (ParameterIDs::eqEnabled)       > 0.5f;
    enableState.comp     = smoothing.getById (ParameterIDs::compEnabled)     > 0.5f;
    enableState.room     = smoothing.getById (ParameterIDs::roomEnabled)     > 0.5f;
    enableState.reverb   = smoothing.getById (ParameterIDs::reverbEnabled)   > 0.5f;
    enableState.higgs    = smoothing.getById (ParameterIDs::higgsEnabled)    > 0.5f;
    enableState.materialRoom = smoothing.getById (ParameterIDs::materialRoomEnabled) > 0.5f;

    const bool membraneEnabled = enableState.membrane;
    const bool tubeEnabled     = enableState.tube;
    const bool pitchEnvEnabled = enableState.pitchEnv;
    const bool clickEnabled    = enableState.click;
    const bool bodyEnabled     = enableState.body;
    const bool subEnabled      = enableState.sub;
    const bool driveEnabled    = enableState.drive;
    const bool eqEnabled       = enableState.eq;
    const bool compEnabled     = enableState.comp;
    const bool outputEnabled   = smoothing.getById (ParameterIDs::outputEnabled) > 0.5f;

    // 0->1-Flanken: Modulzustände leeren (kein Ghost-Hit, kein alter Tail).
    applyEnableTransitions (enableState);

    // EQ-Koeffizienten einmal pro Block. Das Sub-Band trackt immer f0
    // (SND-073 funktional erfüllt; "eqSubTrack" ist kein APVTS-Parameter).
    updateEQ (eqRumbleHP, eqSubFreq, eqSubGain, eqPunchFreq, eqPunchGain,
              eqAirFreq, eqAirGain, baseFundamentalHz);

    // Room: 4 Taps + Send-Pegel/Tone einmal pro Block (SND-091..100).
    // Hinweis: "roomSync" ist derzeit fix RoomSync::free; SND-093 ist ohne
    // APVTS-Parameter nur als Engine-API vorhanden (PARTIAL, siehe TODO.md).
    updateRoom (roomLevel, roomDecay, roomRumble, sequencerBpm);

    // Reverb-Parameter
    reverb.setParameters (reverbMaterial, reverbPreDelay, reverbDecay, reverbSize,
                          reverbDamping, reverbMix, reverbLowCut, reverbHighCut);
    reverb.setFreeze (reverbFreeze);
    reverb.setGateMs (reverbGate);

    // Higgs-Parameter (process() prüft params.enabled intern)
    HiggsParams higgsParams;
    higgsParams.enabled       = enableState.higgs;
    higgsParams.cursorPhi     = smoothing.getById (ParameterIDs::higgsCursorPhi);
    higgsParams.cursorTheta   = smoothing.getById (ParameterIDs::higgsCursorTheta);
    higgsParams.fieldStrength = smoothing.getById (ParameterIDs::higgsFieldStrength);
    higgsParams.vev           = smoothing.getById (ParameterIDs::higgsVev);
    higgsParams.selfCoupling  = smoothing.getById (ParameterIDs::higgsSelfCoupling);
    higgsParams.inertia       = smoothing.getById (ParameterIDs::higgsInertia);
    higgsParams.yukawaBoost   = smoothing.getById (ParameterIDs::higgsYukawaBoost);
    higgsParams.bosonHz       = smoothing.getById (ParameterIDs::higgsBosonHz);
    higgsParams.bosonQ        = smoothing.getById (ParameterIDs::higgsBosonQ);
    higgsParams.symmetry      = smoothing.getById (ParameterIDs::higgsSymmetry);
    higgsParams.decay         = smoothing.getById (ParameterIDs::higgsDecay);
    higgsParams.mix           = smoothing.getById (ParameterIDs::higgsMix);
    higgsParams.lfoRate       = smoothing.getById (ParameterIDs::higgsRate);
    higgsParams.lfoDepth      = smoothing.getById (ParameterIDs::higgsDepth);
    higgs.setParams (higgsParams);

    materialRoom.setParameters (
        (int) smoothing.getUnsmoothedById (ParameterIDs::materialRoomMaterial),
        smoothing.getById (ParameterIDs::materialRoomSize),
        smoothing.getById (ParameterIDs::materialRoomMix),
        (int) smoothing.getUnsmoothedById (ParameterIDs::materialRoomLayer),
        smoothing.getById (ParameterIDs::materialRoomWidth),
        smoothing.getById (ParameterIDs::grumbleLevel),
        smoothing.getById (ParameterIDs::grumbleFreq),
        smoothing.getById (ParameterIDs::grumbleRate),
        smoothing.getById (ParameterIDs::grumbleDepth),
        smoothing.getById (ParameterIDs::grumblePanA),
        smoothing.getById (ParameterIDs::grumblePanB),
        (int) smoothing.getUnsmoothedById (ParameterIDs::grumbleWave),
        (int) smoothing.getUnsmoothedById (ParameterIDs::grumblePanWave),
        smoothing.getById (ParameterIDs::grumbleDecay));

    // ---- Virtuelle CV-Quellen aus der APVTS (SND-154/156/157) ----
    // LFO-Rate/-Tiefe und die Makros 1-5 sind echte, automatisierbare Parameter.
    // Der Parameter-Patch fuehrt sie als Laufzeit-Spiegel: pro Block aus der
    // APVTS gelesen, damit Host-Automation, Presets und das Makro-Panel
    // dieselbe Quelle benutzen. Der Graph selbst bleibt allokationsfrei
    // (Atomics); die IDs der Makros werden einmalig gebaut, nicht pro Block.
    parameterPatchGraph.setLfoRateHz (smoothing.getById (ParameterIDs::lfoRate));
    parameterPatchGraph.setLfoDepth  (smoothing.getById (ParameterIDs::lfoDepth));

    {
        static const auto macroIds = []
        {
            std::array<juce::String, 5> ids {};

            for (int i = 0; i < (int) ids.size(); ++i)
                ids[(size_t) i] = ParameterIDs::macro (i + 1);

            return ids;
        }();

        for (int m = 0; m < (int) macroIds.size(); ++m)
            parameterPatchGraph.setMacroValue (m, smoothing.getById (macroIds[(size_t) m]));
    }

    // ---- Globales Tuning: MIDI-/Step-Pitch auf die Grundfrequenz ----
    float tunedFundamental = baseFundamentalHz * glideRatio;

    // ---- Block-Konstanten (keine Transzendenten pro Sample) ----
    updateSubOscillator (baseFundamentalHz, subDecay, subDetune);
    updateBodyOscillator (baseFundamentalHz, bodyTuning, bodyDecay, bodyAttack);

    {
        // Drive: 150-Hz-Crossover (Band-Split) + optionaler Pre-HP 60..500 Hz
        // (drivePreHP 0 = aus, SND-019) + DC-Blocker ~10 Hz (SND-013).
        driveSplitCoeff = std::exp (-juce::MathConstants<float>::twoPi
                                    * 150.0f / (float) sampleRate);

        const float preHp = juce::jlimit (0.0f, 1.0f, drivePreHP);
        drivePreHpActive = preHp > 0.001f;
        const float driveHpHz = 60.0f + 440.0f * preHp;
        driveHpCoeff = std::exp (-juce::MathConstants<float>::twoPi
                                 * driveHpHz / (float) sampleRate);

        driveDcCoeff = std::exp (-juce::MathConstants<float>::twoPi
                                 * 10.0f / (float) sampleRate);

        // B7: Der Drive-Pfad bringt 15 Samples Latenz mit (2x-Halfband),
        // aber nur wenn er tatsaechlich laeuft (Bypass = 0 Samples).
        driveLatencyActive = driveEnabled && driveAmount > 0.0f && driveMix > 0.0f;
    }

    {
        // SND-088 (Auto-Makeup = aktuelle GR) / SND-090 (Dry/Wet):
        // IDs "compAutoMakeup" und "compMix" werden pro Block gelesen.
        compressorAutoMakeup = smoothing.getUnsmoothedById (ParameterIDs::compAutoMakeup) > 0.5f;
        compressorMix        = juce::jlimit (0.0f, 1.0f,
                                             smoothing.getById (ParameterIDs::compMix));
        compressor.setAutoMakeupEnabled (compressorAutoMakeup);
    }

    {
        // Mono-Below: eine Grenzfrequenz pro Block, komplementäres Paar.
        monoBelowActive = outputMonoBelow > 0.0f;

        if (monoBelowActive)
        {
            const float fc = juce::jlimit (5.0f, 0.45f * (float) sampleRate, outputMonoBelow);
            monoBelowCoeffValue = std::exp (-juce::MathConstants<float>::twoPi
                                            * fc / (float) sampleRate);
        }
        else
        {
            monoBelowCoeffValue = 1.0f;
        }

        outputWidthValue = juce::jlimit (0.0f, 1.0f, outputWidth);
    }

    {
        // B3: Soft-Clip mit Slope 1. "outputClip" steuert NUR das Ceiling
        // (0 => 1.0 = praktisch transparent, 1 => 0.5 = -6 dB). Es gibt
        // keinen parameterabhaengigen Gain mehr (vorher: tanh-Makeup mit
        // +7,7 dB Kleinsignalverstaerkung bei outputClip 0.35).
        const float clipAmt = juce::jlimit (0.0f, 1.0f, outputClip);
        clipActive  = clipAmt > 0.0f;
        clipCeiling = 1.0f - 0.5f * clipAmt;

        // B3: Limiter-Ceiling 0..3 dB unter dem Clip-Ceiling. Der Limiter ist
        // die letzte Instanz und muss auch dann greifen koennen, wenn der
        // Clip bereits aktiv ist (vorher: identisches Ceiling, der Limiter
        // war ein No-Op: -0,045 dB bei amount 0,15).
        const float limiterAmt = juce::jlimit (0.0f, 1.0f, outputLimiter);
        limiterCeiling = clipCeiling * juce::Decibels::decibelsToGain (-3.0f * limiterAmt);

        limiterAttackCoeff = std::exp (-1.0f / juce::jmax (1.0f, 0.001f * (float) sampleRate));
        limiterReleaseCoeff = std::exp (-1.0f / juce::jmax (1.0f, 0.120f * (float) sampleRate));
        limiterPeakDecay   = std::exp (-1.0f / juce::jmax (1.0f, 0.002f * (float) sampleRate));

        limiterMinGain = 1.0f;
        drivePeak = 0.0f;
        blockTruePeak = 0.0f;

        if (! clipActive)
        {
            for (int i = 0; i < 2; ++i)
            {
                clipUp[i].reset();
                clipDown[i].reset();
            }
        }

        // Der Limiter ist die bindende Instanz: sein Ceiling liegt unter dem
        // Clip-Ceiling (SND-010, invertiert: der Clip darf das Limiter-Ceiling
        // nicht uebersteigen - deshalb begrenzt der Limiter zusaetzlich).
        jassert (limiterCeiling <= clipCeiling);
    }

    // ---- Patch-Snapshot: EIN Lock, keine Allokation im Sample-Loop ----
    patchGraph.fillSnapshot (patchSnapshot);
    const int connectionCount = patchSnapshot.connectionCount;
    const int moduleOrderCount = (int) ModuleIndex::ModuleCount;

    // ---- OutputGain/Pan: blockweise Rampe ----
    const float outGainTarget = smoothing.getById (ParameterIDs::outputGain);
    const float outPanTarget  = juce::jlimit (-1.0f, 1.0f, smoothing.getById (ParameterIDs::outputPan));

    // Balance-Pan (nach Clip/Limiter, SND-007): pan=-1 -> L=1/R=0,
    // 0 -> L=R=1, +1 -> L=0/R=1. Gains bleiben <= 1, damit das
    // Limiter-Ceiling durch das Panning nicht überschritten werden kann.
    const float targetPanL = juce::jmin (1.0f, 1.0f - juce::jmax (0.0f, outPanTarget));
    const float targetPanR = juce::jmin (1.0f, 1.0f + juce::jmin (0.0f, outPanTarget));

    if (! outputRampInitialized)
    {
        outGainCurrent = outGainTarget;
        outPanLCurrent = targetPanL;
        outPanRCurrent = targetPanR;
        outputRampInitialized = true;
    }

    const float outGainStep = (outGainTarget - outGainCurrent) / (float) juce::jmax (1, numSamples);
    const float panLStep    = (targetPanL - outPanLCurrent) / (float) juce::jmax (1, numSamples);
    const float panRStep    = (targetPanR - outPanRCurrent) / (float) juce::jmax (1, numSamples);

    // ---- Sample-Loop (graph-basiertes modulares Routing) ----
    float blockPeak = 0.0f; ///< Envelope-Follower-Quelle (SND-156)

    for (int i = 0; i < numSamples; ++i)
    {
        outGainCurrent += outGainStep;
        outPanLCurrent += panLStep;
        outPanRCurrent += panRStep;

        // Sample-genaue Trigger (Sequencer/MIDI) an ihrem Offset; der
        // Message-Thread-Fallback (GUI/triggerNote) landet bei Offset 0.
        if (consumeTriggerEvents (i, velocitySensitivity))
        {
            // Pitch hängt am Trigger: Grundwerte neu ableiten; die Glide-Rampe
            // läuft direkt darunter sample-genau auf den neuen Zielwert.
            updateSubOscillator (baseFundamentalHz, subDecay, subDetune);
            updateBodyOscillator (baseFundamentalHz, bodyTuning, bodyDecay, bodyAttack);
        }

        // SND-135: Portamento als multiplikativer Pitch-Schritt pro Sample
        // (linear in Halbtönen, keine Transzendenten im Sample-Loop).
        if (glideSamplesLeft > 0)
        {
            glideRatio *= glideCoeff;
            trackGlideRatio *= trackGlideCoeff;

            if (--glideSamplesLeft == 0)
            {
                glideRatio = glideTargetRatio;
                trackGlideRatio = trackGlideTargetRatio;
            }
        }

        tunedFundamental = baseFundamentalHz * glideRatio;

        // Pitch-Envelope läuft pro Sample, ausgeschaltet = kein Glide.
        const float pitchRatio = pitchEnvEnabled
            ? pitchEnv.process (pitchEnvAttack, pitchEnvDecay, pitchEnvDepth, pitchEnvCurve)
            : 1.0f;

        // B4: Der Body gleitet mit 0.4x der Pitch-Env:
        //       bodyRatio = 1 + 0.4*(pitchRatio - 1)
        // B5: Der Sub gleitet mit 0.5x der Pitch-Env:
        //       subRatioPitch = 1 + 0.5*(pitchRatio - 1)
        // Ausgewertet wird ausschliesslich der Multiplikator pitchRatio
        // (die Semantik von getEnvelopeValue() wird parallel geaendert).
        const float bodyPitchRatio = 1.0f + 0.4f * (pitchRatio - 1.0f);
        const float subPitchRatio  = 1.0f + 0.5f * (pitchRatio - 1.0f);

        subOmega = subOmegaBase * glideRatio * subPitchRatio;
        subDt = subDtBase * glideRatio * subPitchRatio;
        bodyOmega = bodyOmegaBase * glideRatio;

        std::array<StereoMix, (int) Jack::JackCount> jacks {};

        auto sumTo = [this, &jacks, connectionCount] (Jack inJack) -> StereoMix
        {
            StereoMix mix;

            for (int c = 0; c < connectionCount; ++c)
            {
                const auto& connection = patchSnapshot.connections[(size_t) c];

                if (connection.to == inJack)
                {
                    mix.l += jacks[(size_t) connection.from].l;
                    mix.r += jacks[(size_t) connection.from].r;
                }
            }
            return mix;
        };
        auto meanIn = [&] (Jack inJack) -> float
        {
            const auto m = sumTo (inJack);
            return monoAverage (m);
        };

        for (int order = 0; order < moduleOrderCount; ++order)
        {
            const int mod = patchSnapshot.order[(size_t) order];

            switch (static_cast<ModuleIndex> (mod))
            {
                case ModuleIndex::Membrane:
                {
                    float membraneSample = 0.0f;

                    if (membraneEnabled)
                        membraneSample = membrane.process (membraneTension, membraneRadius, membraneMass,
                                                           membraneDamping, membraneModes,
                                                           tunedFundamental * pitchRatio,
                                                           membraneNonlinearity, membraneStiffness);

                    // Body ist ein eigener Resonator (Quelle mit eigenem Enable).
                    float bodySample = 0.0f;

                    if (bodyEnabled)
                        bodySample = processBody (bodyLevel, bodyPitchRatio);

                    // Mixer: Membran und Body sind getrennte Kanaele
                    // (je eigener Lautstaerke + 4-Band-EQ im Mixdown).
                    membraneSample = mixer.processMono ((int) MixerStage::Channel::Membrane, membraneSample);
                    bodySample = mixer.processMono ((int) MixerStage::Channel::Body, bodySample);

                    // ---- Automatisches 2-Layer-Layering ----
                    // Die Layer sitzen am Membrane-Bus: damit laufen sie durch
                    // exakt dieselben Module (Tube/Drive/EQ/Comp) und dieselben
                    // Modulationen wie die Basis. Equal-Power-Trim auf
                    // Membrane+Body haelt die Energie konstant (kein Overgain).
                    float layerLow = 0.0f;
                    float layerHigh = 0.0f;

                    if (layering.isEnabled())
                    {
                        layering.process (tunedFundamental * pitchRatio, layerLow, layerHigh);

                        const float trim = layering.getBaseTrim();
                        membraneSample *= trim;
                        bodySample *= trim;
                    }

                    if (i < (int) layerTapLow.size())
                    {
                        layerTapLow[(size_t) i] = layerLow;
                        layerTapHigh[(size_t) i] = layerHigh;
                    }

                    // Modul-Taps fuer das Waveform-Overlay (Review §7.3).
                    if (i < (int) moduleTapMembrane.size())
                    {
                        moduleTapMembrane[(size_t) i] = membraneSample;
                        moduleTapBody[(size_t) i] = bodySample;
                    }

                    float membraneBus = membraneSample + bodySample + layerLow + layerHigh;

                    if (! std::isfinite (membraneBus))
                        membraneBus = 0.0f;

                    membraneBusSample = membraneBus; // SND-174: Duck-Quelle (|Membrane+Body|)
                    jacks[(size_t) Jack::MembraneOut] = { membraneBus, membraneBus };
                    break;
                }

                case ModuleIndex::Tube:
                {
                    const float in = meanIn (Jack::TubeIn);

                    // Tube-Resonanz trackt f0 (SND-068; nur mit "tubeTrack") und den
                    // Step-Pitch zu 30 % (SND-134); 120 Hz = Ratio 2.0 bei 60 Hz.
                    const float tubeFreq = tubeTracking
                        ? juce::jlimit (20.0f, 3000.0f,
                                        tubeResonanceFreq * (baseFundamentalHz / 60.0f)
                                            * trackGlideRatio)
                        : tubeResonanceFreq;

                    float tubeSample = tubeEnabled
                        ? tube.process (in, tubeFreq, tubeResonanceQ,
                                        tubeResonanceLevel, tubeCoupling, hollowMetallic)
                        : 0.0f;

                    if (! std::isfinite (tubeSample))
                        tubeSample = 0.0f;

                    tubeSample = mixer.processMono ((int) MixerStage::Channel::Tube, tubeSample);
                    jacks[(size_t) Jack::TubeOut] = { tubeSample, tubeSample };
                    break;
                }

                case ModuleIndex::Sub:
                {
                    float subSample = subEnabled ? processSub (subLevel, subShape) : 0.0f;
                    subSample = mixer.processMono ((int) MixerStage::Channel::Sub, subSample);
                    subBusSample = subSample; // SND-174: Duck-Quelle (|Sub|)

                    // SND-174 / Review M3: Der globale Duck wirkt jetzt NUR auf
                    // die FX (Room/Higgs/Reverb/MaterialRoom) - nicht mehr auf
                    // den trockenen Kick (vorher duckte sich der Kick selbst
                    // und bekam eine Hoehle im eigenen Sustain).
                    {
                        const float duckSource = std::abs (membraneBusSample) + std::abs (subBusSample);
                        const float duckTarget = juce::jlimit (0.0f, 1.0f, duckSource);
                        duckEnv = (duckTarget > duckEnv)
                            ? duckAttackCoeff * duckEnv + (1.0f - duckAttackCoeff) * duckTarget
                            : duckReleaseCoeff * duckEnv + (1.0f - duckReleaseCoeff) * duckTarget;
                        duckGainCurrent = juce::Decibels::decibelsToGain
                            (-duckDepthDb * juce::jlimit (0.0f, 1.0f, duckEnv));
                    }
                    if (i < (int) moduleTapSub.size())
                        moduleTapSub[(size_t) i] = subSample;

                    jacks[(size_t) Jack::SubOut] = { subSample, subSample };
                    break;
                }

                case ModuleIndex::Click:
                {
                    // SND-134: Click-Tick folgt dem Step-Pitch nativ (30 % getrackt).
                    float clickSample = clickEnabled
                        ? click.process (clickLevel, clickDecay, clickTone,
                                         clickDistortion, clickBeaterMaterial,
                                         trackGlideRatio, pitchRatio)
                        : 0.0f;

                    if (! std::isfinite (clickSample))
                        clickSample = 0.0f;

                    clickSample = mixer.processMono ((int) MixerStage::Channel::Click, clickSample);
                    if (i < (int) moduleTapClick.size())
                        moduleTapClick[(size_t) i] = clickSample;

                    jacks[(size_t) Jack::ClickOut] = { clickSample, clickSample };
                    break;
                }

                case ModuleIndex::Drive:
                {
                    const float in = meanIn (Jack::DriveIn);

                    // SND-175: Transient-Shaper im Bus vor Drive/Comp. Sitzt am
                    // Drive-Eingang (vor Drive und allen nachgeschalteten
                    // Stufen); Attack/Sustain in dB, 0 = exakt Bypass. Die
                    // Erkennung läuft auf dem Mono-Bus (Stereo-Link identisch).
                    const float bus = transientShaper.process (in, in).first;

                    float driveSample = driveEnabled
                        ? processDrive (bus, driveAmount, driveMix, driveCharacter)
                        : bus; // Bypass: geformtes Signal unverändert durchleiten.

                    driveSample = mixer.processMono ((int) MixerStage::Channel::Drive, driveSample);
                    jacks[(size_t) Jack::DriveOut] = { driveSample, driveSample };
                    break;
                }

                case ModuleIndex::Eq:
                {
                    const float in = meanIn (Jack::EqIn);
                    float eqSample = eqEnabled ? processEQSample (in) : in;
                    eqSample = mixer.processMono ((int) MixerStage::Channel::Eq, eqSample);
                    jacks[(size_t) Jack::EqOut] = { eqSample, eqSample };
                    break;
                }

                case ModuleIndex::Compressor:
                {
                    const auto inMix = sumTo (Jack::CompIn);
                    StereoMix compOut = inMix; // Bypass: beide Kanäle unverändert.

                    if (compEnabled)
                    {
                        const auto compResult = compressor.process (inMix.l, inMix.r,
                                                                    compThreshold, compRatio,
                                                                    compAttack, compRelease,
                                                                    compKnee, compMakeup,
                                                                    compressorMix);
                        compOut.l = compResult.first;
                        compOut.r = compResult.second;
                    }

                    mixer.processStereo ((int) MixerStage::Channel::Compressor, compOut.l, compOut.r);
                    jacks[(size_t) Jack::CompOut] = compOut;
                    break;
                }

                case ModuleIndex::Room:
                {
                    // Stereo-Send (Tap-Paare L/R, SND-100); deaktiviert = stumm.
                    // SND-181: FX-Wet-Breite wirkt nur auf den Room-Tail.
                    const auto inMix = sumTo (Jack::RoomIn);
                    StereoMix roomWet = enableState.room
                        ? processRoomRumble (inMix)
                        : StereoMix {};
                    applyFxWidth (fxWidthValue, roomWet.l, roomWet.r);
                    roomWet.l *= duckGainCurrent;
                    roomWet.r *= duckGainCurrent;
                    mixer.processStereo ((int) MixerStage::Channel::Room, roomWet.l, roomWet.r);

                    jacks[(size_t) Jack::RoomOut] = roomWet;
                    break;
                }

                case ModuleIndex::Higgs:
                {
                    // Wet-only: deaktiviert liefert keinen Beitrag zum Ausgang
                    // (sonst würde das trockene Signal doppelt summiert).
                    StereoMix higgsOut { 0.0f, 0.0f };

                    if (enableState.higgs)
                    {
                        const auto mix = sumTo (Jack::HiggsIn);
                        const auto higgsResult = higgs.process (mix.l, mix.r);
                        higgsOut = { higgsResult.first * duckGainCurrent,
                                     higgsResult.second * duckGainCurrent };
                    }

                    mixer.processStereo ((int) MixerStage::Channel::Higgs, higgsOut.l, higgsOut.r);
                    jacks[(size_t) Jack::HiggsOut] = higgsOut;
                    break;
                }

                case ModuleIndex::Reverb:
                {
                    const auto mix = sumTo (Jack::ReverbIn);
                    // SND-181: FX-Wet-Breite wirkt nur auf den Reverb-Tail.
                    auto revResult = enableState.reverb
                        ? reverb.process (mix.l, mix.r)
                        : std::make_pair (0.0f, 0.0f);
                    applyFxWidth (fxWidthValue, revResult.first, revResult.second);
                    revResult.first *= duckGainCurrent;
                    revResult.second *= duckGainCurrent;
                    mixer.processStereo ((int) MixerStage::Channel::Reverb, revResult.first, revResult.second);

                    jacks[(size_t) Jack::ReverbOut] = { revResult.first, revResult.second };
                    break;
                }

                case ModuleIndex::MaterialRoom:
                {
                    const auto kick   = sumTo (Jack::MaterialRoomKickIn);
                    const auto rumble = sumTo (Jack::MaterialRoomRumbleIn);
                    // SND-181: FX-Wet-Breite wirkt nur auf den MaterialRoom-Tail.
                    auto wet = enableState.materialRoom
                        ? materialRoom.process (kick.l, kick.r, rumble.l, rumble.r)
                        : std::make_pair (0.0f, 0.0f);
                    applyFxWidth (fxWidthValue, wet.first, wet.second);
                    wet.first *= duckGainCurrent;
                    wet.second *= duckGainCurrent;
                    mixer.processStereo ((int) MixerStage::Channel::MaterialRoom, wet.first, wet.second);

                    jacks[(size_t) Jack::MaterialRoomOut] = { wet.first, wet.second };
                    break;
                }

                case ModuleIndex::OutputModule:
                {
                    auto mix = sumTo (Jack::OutputIn);
                    float outL = outputEnabled ? mix.l : 0.0f;
                    float outR = outputEnabled ? mix.r : 0.0f;

                    // Master-Kanal des Mixers (EQ + Trim) vor der Output-Stufe.
                    mixer.processStereo ((int) MixerStage::Channel::Output, outL, outR);

                    // SND-174 / Review M3: Das Ducking wird im Sub-Fall berechnet
                    // und dort auf die FX-Ausgaenge angewendet - der trockene Kick
                    // bleibt unberuehrt (kein Self-Ducking mehr).

                    processOutputStage (outL, outR, outGainCurrent, outPanLCurrent, outPanRCurrent);

                    if (! std::isfinite (outL)) outL = 0.0f;
                    if (! std::isfinite (outR)) outR = 0.0f;

                    jacks[(size_t) Jack::OutputOut] = { outL, outR };
                    break;
                }

                default:
                    break;
            }
        }

        // ---- Finalen Ausgang schreiben ----
        const auto& out = jacks[(size_t) Jack::OutputOut];
        blockPeak = juce::jmax (blockPeak, juce::jmax (std::abs (out.l), std::abs (out.r)));
        buffer.setSample (0, i, out.l);

        if (isStereo)
        {
            buffer.setSample (1, i, out.r);
        }
        else
        {
            // Mono-Summe: Balance-Pan ist bei Center 1.0 pro Kanal, also
            // liefert (l+r)*0.5 wieder das mittige Mono-Signal.
            buffer.setSample (0, i, (out.l + out.r) * 0.5f);
        }

        // Tail-Counter: nie unter 0 laufen (kein signed overflow).
        if (isActiveFlag.load (std::memory_order_acquire))
        {
            if (tailCounter > 0)
                --tailCounter;

            if (tailCounter == 0 && std::abs (out.l) < 1.0e-4f && std::abs (out.r) < 1.0e-4f)
                isActiveFlag.store (false, std::memory_order_release);
        }
    }

    // Envelope-Follower fuer die naechste Block-CV (SND-156): geglaetteter
    // Ausgangs-Peak mit den APVTS-Zeitkonstanten "envFollowAttack/Release".
    {
        const float target = juce::jlimit (0.0f, 1.0f, blockPeak);
        const float coeff = (target > envFollowerValue) ? envFollowAttackCoeff
                                                        : envFollowReleaseCoeff;
        envFollowerValue = coeff * envFollowerValue + (1.0f - coeff) * target;

        if (! std::isfinite (envFollowerValue))
            envFollowerValue = 0.0f;

        parameterPatchGraph.setEnvelopeFollowerLevel (envFollowerValue);
    }

    // ---- Metering (SND-005/020) ----
    driveOutputLevelDb = (drivePeak > 1.0e-6f)
        ? 20.0f * std::log10 (drivePeak)
        : -100.0f;

    limiterGainReductionDb = (limiterMinGain < 1.0f)
        ? -20.0f * std::log10 (juce::jmax (1.0e-6f, limiterMinGain))
        : 0.0f;

    limiterGainReductionMaxDb = juce::jmax (limiterGainReductionMaxDb, limiterGainReductionDb);

    clipGainReductionDb = (clipMinGain < 1.0f)
        ? -20.0f * std::log10 (juce::jmax (1.0e-6f, clipMinGain))
        : 0.0f;

    clipGainReductionMaxDb = juce::jmax (clipGainReductionMaxDb, clipGainReductionDb);

    // B7: True-Peak des Blocks in dBFS (4x interpoliert, stereo-linked).
    truePeakDb = (blockTruePeak > 1.0e-6f)
        ? 20.0f * std::log10 (blockTruePeak)
        : -100.0f;

    // Sicherheitsnetz: Events mit Offset außerhalb des Blocks (sollte nicht
    // vorkommen) gehen nicht verloren, sondern feuern am Blockende.
    if (triggerCount > 0)
        (void) consumeTriggerEvents (std::numeric_limits<int>::max(), velocitySensitivity);

    // Der Block-Snapshot gilt nur für diesen Render-Aufruf.
    parameterModulationPrepared = false;
}

//==============================================================================
int KickSynthesisEngine::getLatencyInSamples() const
{
    // B7: Latenz des Signalpfades. Ohne "oversampleEnabled" ist der Pfad
    // sample-genau. Mit Oversampling kommen zwei 2x-Halfband-Pfade hintereinander
    // (Drive 15 Samples, Clip 15 Samples) => 30 Samples bei Basis-Samplerate.
    if (! oversampleEnabled)
        return 0;

    int latency = 0;

    if (driveLatencyActive)
        latency += StreamingTwoX::totalLatencySamples;

    if (clipActive)
        latency += StreamingTwoX::totalLatencySamples;

    return latency;
}

//==============================================================================
bool KickSynthesisEngine::renderOfflineBounce (const juce::File& target,
                                               ParameterSmoothing& smoothing,
                                               double secondsToRender,
                                               float bpm)
{
    constexpr int blockSize = 512;
    const int totalSamples = juce::jmax (1, (int) std::floor (secondsToRender * sampleRate))
                             & ~(blockSize - 1);

    if (totalSamples <= 0 || sampleRate <= 0.0)
        return false;

    juce::AudioBuffer<float> bounce (2, totalSamples);
    bounce.clear();
    juce::AudioBuffer<float> block (2, blockSize);

    const double samplesPerBeat = 60.0 / juce::jlimit (20.0f, 300.0f, bpm) * sampleRate;

    if (samplesPerBeat <= 0.0)
        return false;

    double nextTriggerSample = 0.0;

    for (int blockStart = 0; blockStart < totalSamples; blockStart += blockSize)
    {
        block.clear();
        smoothing.update();

        while (nextTriggerSample < (double) (blockStart + blockSize)
               && nextTriggerSample < (double) totalSamples)
        {
            const int offset = juce::jmax (0, (int) std::lround (nextTriggerSample) - blockStart);
            enqueueTrigger (1.0f, 0.0f, offset);
            nextTriggerSample += samplesPerBeat;
        }

        render (block, blockSize, smoothing);

        for (int ch = 0; ch < 2; ++ch)
            bounce.copyFrom (ch, blockStart, block, ch, 0, blockSize);
    }

    // WAV (32 bit, IEEE float) schreiben; Fehler bei Stream/Writer/Write -> false.
    juce::WavAudioFormat format;
    std::unique_ptr<juce::OutputStream> outputStream = std::make_unique<juce::FileOutputStream> (target);

    if (auto* fileStream = dynamic_cast<juce::FileOutputStream*> (outputStream.get());
        fileStream == nullptr || ! fileStream->openedOk())
        return false;

    const auto options = juce::AudioFormatWriterOptions {}
        .withSampleRate (sampleRate)
        .withNumChannels (2)
        .withBitsPerSample (32)
        .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);

    std::unique_ptr<juce::AudioFormatWriter> writer (
        format.createWriterFor (outputStream, options));

    if (writer == nullptr)
        return false;

    return writer->writeFromAudioSampleBuffer (bounce, 0, totalSamples);
}
