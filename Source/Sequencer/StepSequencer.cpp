#include "StepSequencer.h"

//==============================================================================
void StepSequencer::prepare (double newSampleRate, int newSamplesPerBlock)
{
    sampleRate = newSampleRate;
    samplesPerBlock = newSamplesPerBlock;

    // ---- Smoothing-Indizes einmalig cachen (keine Allokation im Audio-Thread) ----
    indexBpm     = smoothing.indexOf (ParameterIDs::sequencerBpm);
    indexSwing   = smoothing.indexOf (ParameterIDs::sequencerSwing);
    indexSteps   = smoothing.indexOf (ParameterIDs::sequencerSteps);
    indexPlaying = smoothing.indexOf (ParameterIDs::sequencerPlaying);

    for (int pattern = 0; pattern < numPatterns; ++pattern)
    {
        for (int step = 0; step < patternStride; ++step)
        {
            const int flat = pattern * patternStride + step;
            const auto index = (size_t) flat;

            indexGate[index]        = smoothing.indexOf (ParameterIDs::stepGate (flat).toRawUTF8());
            indexVelocity[index]    = smoothing.indexOf (ParameterIDs::stepVel (flat).toRawUTF8());
            indexPitch[index]       = smoothing.indexOf (ParameterIDs::stepPitch (flat).toRawUTF8());
            indexRatchet[index]     = smoothing.indexOf ((juce::String (stepPrefixRatchet)
                                                          + juce::String (flat)).toRawUTF8());
            indexFlam[index]        = smoothing.indexOf ((juce::String (stepPrefixFlam)
                                                          + juce::String (flat)).toRawUTF8());
            indexProbability[index] = smoothing.indexOf ((juce::String (stepPrefixProbability)
                                                          + juce::String (flat)).toRawUTF8());
            indexLength[index]      = smoothing.indexOf ((juce::String (stepPrefixLength)
                                                          + juce::String (flat)).toRawUTF8());
        }
    }

    rawSyncMode    = apvts.getRawParameterValue (paramSyncMode);
    rawGridMode    = apvts.getRawParameterValue (paramGridMode);
    rawPattern     = apvts.getRawParameterValue (paramPattern);
    rawChainLength = apvts.getRawParameterValue (paramChainLength);
    rawClockOut    = apvts.getRawParameterValue (paramClockOut);

    for (int i = 0; i < maxChainLength; ++i)
    {
        rawChainSlots[(size_t) i] =
            apvts.getRawParameterValue (juce::String (paramChainSlot) + juce::String (i));
        chainEntries[(size_t) i].store (i % numPatterns, std::memory_order_relaxed);
    }

    updateTiming();
    reset();
}

void StepSequencer::reset()
{
    currentStep.store (0, std::memory_order_release);
    playing.store (false, std::memory_order_release);
    hostTransportRunning.store (false, std::memory_order_release);
    sampleCounter = 0.0;
    scheduledCount = 0;
    chainPosition = 0;
    patternCycle = 0;
    lastHostBar = -1;
    activeStepDuration = 0.0;
}

//==============================================================================
float StepSequencer::valueOf (int smoothingIndex, float fallback) const
{
    return smoothingIndex >= 0 ? smoothing.getTarget (smoothingIndex) : fallback;
}

float StepSequencer::rawValue (const std::atomic<float>* parameter, float fallback) const
{
    return parameter != nullptr ? parameter->load (std::memory_order_relaxed) : fallback;
}

bool StepSequencer::writeParameter (const char* id, float plainValue)
{
    if (auto* parameter = apvts.getParameter (id))
    {
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (plainValue));
        return true;
    }
    return false;
}

int StepSequencer::stepsPerQuarter() const
{
    switch (grid)
    {
        case GridMode::eighthTriplet:    return 3;  // 1/8-Triole
        case GridMode::sixteenthTriplet: return 6;  // 1/16-Triole
        case GridMode::sixteenth:
        default:                         return 4;  // 1/16
    }
}

void StepSequencer::updateTiming()
{
    const double secondsPerBeat = 60.0 / (double) juce::jmax (1.0f, bpm);
    samplesPerStep = secondsPerBeat * sampleRate / (double) stepsPerQuarter();
}

double StepSequencer::stepDurationSamples (int stepIndex) const
{
    const double length = (double) readStep (activePattern.load (std::memory_order_relaxed),
                                             stepIndex).lengthSteps;

    return juce::jmax (1.0, samplesPerStep * length);
}

double StepSequencer::swingDelaySamples (int stepIndex) const
{
    // SND-138: Swing verzoegert die ungeraden Steps gegenueber dem geraden
    // Grid. r = 0.5 + swing/3 (0..0.75 -> 0.5..0.75); die Verzoegerung ist
    // (r - 0.5) * 2 Steps, d.h. bei swing = 0.75 genau ein halber Step.
    // Dadurch funktioniert Swing auch bei Host-Sync (gerades PPQ-Grid).
    if (stepIndex % 2 == 0)
        return 0.0;

    const double swingRatio = 0.5 + (double) swing / 3.0;
    return juce::jmax (0.0, (swingRatio - 0.5) * 2.0 * samplesPerStep);
}

//==============================================================================
StepSequencer::StepSettings StepSequencer::readStep (int pattern, int stepIndex) const
{
    const int flat = juce::jlimit (0, numPatterns - 1, pattern) * patternStride
                   + juce::jlimit (0, patternStride - 1, stepIndex);
    const auto index = (size_t) flat;

    StepSettings settings;
    settings.gate        = valueOf (indexGate[index], 0.0f) > 0.5f;
    settings.velocity    = juce::jlimit (0.0f, 1.0f, valueOf (indexVelocity[index], 1.0f));
    settings.pitchSemitones = valueOf (indexPitch[index], 0.0f);
    settings.ratchet     = juce::jlimit (1, 4, (int) std::lround (valueOf (indexRatchet[index], 1.0f)));
    settings.flamMs      = juce::jlimit (0.0f, 50.0f, valueOf (indexFlam[index], 0.0f));
    settings.probability = juce::jlimit (0.0f, 1.0f, valueOf (indexProbability[index], 1.0f));
    settings.lengthSteps = juce::jlimit (0.25f, 4.0f, valueOf (indexLength[index], 1.0f));
    return settings;
}

int StepSequencer::chainPatternAt (int index) const
{
    const int slot = juce::jlimit (0, maxChainLength - 1, index);
    const auto* parameter = rawChainSlots[(size_t) slot];

    if (parameter != nullptr)
        return juce::jlimit (0, numPatterns - 1, (int) std::lround (parameter->load()));

    return juce::jlimit (0, numPatterns - 1,
                         chainEntries[(size_t) slot].load (std::memory_order_relaxed));
}

int StepSequencer::effectiveChainLength() const
{
    if (rawChainLength != nullptr)
        return juce::jlimit (0, maxChainLength, (int) std::lround (rawChainLength->load()));

    return juce::jlimit (0, maxChainLength, chainLength.load (std::memory_order_relaxed));
}

void StepSequencer::advancePattern()
{
    const int length = effectiveChainLength();
    if (length <= 0)
        return;

    chainPosition = (chainPosition + 1) % length;
    activePattern.store (chainPatternAt (chainPosition), std::memory_order_release);
}

bool StepSequencer::isMidiClockOutEnabled() const
{
    return rawValue (rawClockOut, 0.0f) > 0.5f;
}

//==============================================================================
void StepSequencer::setBpm (float bpmVal)
{
    const float clamped = juce::jlimit (40.0f, 240.0f, bpmVal);

    if (! writeParameter (ParameterIDs::sequencerBpm, clamped))
        internalBpm = clamped;

    bpm = clamped;
    updateTiming();
}

void StepSequencer::setSteps (int stepsVal)
{
    const int clamped = juce::jlimit (1, maxSteps, stepsVal);

    if (! writeParameter (ParameterIDs::sequencerSteps, (float) clamped))
    {
        steps = clamped;

        if (currentStep.load (std::memory_order_acquire) >= steps)
            currentStep.store (0, std::memory_order_release);
    }
}

void StepSequencer::setSwing (float swingVal)
{
    // SND-138: 0..0.75 (Anzeige 50..75 %), nicht mehr -1..+1.
    const float clamped = juce::jlimit (0.0f, 0.75f, swingVal);

    if (! writeParameter (ParameterIDs::sequencerSwing, clamped))
        swing = clamped;
}

void StepSequencer::setSyncMode (SyncMode mode)
{
    const int value = (int) mode;

    if (! writeParameter (paramSyncMode, (float) value))
        syncModeFallback.store (value, std::memory_order_relaxed);
}

void StepSequencer::setGridMode (GridMode mode)
{
    const int value = (int) mode;

    if (! writeParameter (paramGridMode, (float) value))
        gridModeFallback.store (value, std::memory_order_relaxed);
}

void StepSequencer::setPattern (int pattern)
{
    const int value = juce::jlimit (0, numPatterns - 1, pattern);

    if (! writeParameter (paramPattern, (float) value))
        patternFallback.store (value, std::memory_order_relaxed);
}

void StepSequencer::setPatternChain (const std::array<int, maxChainLength>& patterns, int length)
{
    for (int i = 0; i < maxChainLength; ++i)
    {
        const int value = juce::jlimit (0, numPatterns - 1, patterns[(size_t) i]);

        if (! writeParameter ((juce::String (paramChainSlot) + juce::String (i)).toRawUTF8(),
                              (float) value))
            chainEntries[(size_t) i].store (value, std::memory_order_relaxed);
    }

    const int chain = juce::jlimit (0, maxChainLength, length);

    if (! writeParameter (paramChainLength, (float) chain))
        this->chainLength.store (chain, std::memory_order_relaxed);
}

void StepSequencer::setPlaying (bool playingVal)
{
    // Der APVTS-Parameter ist die einzige Quelle der Wahrheit. Ein direkter
    // Write auf das Atomic würde im nächsten processBlock() sofort wieder vom
    // Parameter überschrieben (bisheriges Verhalten: setPlaying war wirkungslos).
    if (auto* parameter = apvts.getParameter (ParameterIDs::sequencerPlaying))
        parameter->setValueNotifyingHost (playingVal ? 1.0f : 0.0f);

    playing.store (playingVal, std::memory_order_release);
}

void StepSequencer::previewStep (int stepIndex)
{
    if (! juce::isPositiveAndBelow (stepIndex, maxSteps))
        return;

    // SND-148: Vorschau ignoriert Gate/Probability, damit ein Klick auf einen
    // ausgeschalteten Step trotzdem hörbar ist. Velocity 0 -> voll.
    const StepSettings settings = readStep (activePattern.load (std::memory_order_acquire),
                                            stepIndex);
    const float velocity = settings.velocity > 0.0f ? settings.velocity : 1.0f;

    engine.setPendingTrigger (velocity, settings.pitchSemitones);
}

//==============================================================================
void StepSequencer::processBlock (int numSamples)
{
    processBlock (numSamples, nullptr);
}

void StepSequencer::processBlock (int numSamples,
                                  const juce::AudioPlayHead::PositionInfo* hostPosition)
{
    if (numSamples <= 0)
        return;

    // ---- Parameter einmal pro Block lesen (§4/§7) ----
    // SND-133: Bool-Parameter roh lesen (15-ms-Glättung ist semantisch
    // ungültig); stepGate läuft bereits über valueOf (getTarget).
    internalBpm = juce::jlimit (40.0f, 240.0f, smoothed (indexBpm));
    const float swingValue   = juce::jlimit (0.0f, 0.75f, smoothed (indexSwing));
    const int   stepsValue   = juce::jlimit (1, maxSteps, (int) std::lround (smoothed (indexSteps)));
    const bool  playingValue = valueOf (indexPlaying, 0.0f) > 0.5f;

    const auto requestedSync = (SyncMode) juce::jlimit (0, 1, (int) std::lround (
        rawValue (rawSyncMode, (float) syncModeFallback.load (std::memory_order_relaxed))));
    const auto requestedGrid = (GridMode) juce::jlimit (0, 2, (int) std::lround (
        rawValue (rawGridMode, (float) gridModeFallback.load (std::memory_order_relaxed))));
    const int patternParam = juce::jlimit (0, numPatterns - 1, (int) std::lround (
        rawValue (rawPattern, (float) patternFallback.load (std::memory_order_relaxed))));

    const bool gridChanged  = requestedGrid != grid;
    const bool stepsChanged = stepsValue != steps;
    const bool swingChanged = std::abs (swingValue - swing) > 1.0e-6f;
    const bool bpmChanged   = std::abs (internalBpm - bpm) > 1.0e-4f;

    grid = requestedGrid;
    swing = swingValue;
    syncMode = requestedSync;

    if (stepsChanged)
    {
        steps = stepsValue;

        if (currentStep.load (std::memory_order_acquire) >= steps)
            currentStep.store (0, std::memory_order_release);
    }

    // ---- Pattern/Kette (SND-146): Kette hat Vorrang vor dem Pattern-Parameter ----
    const int chainLengthValue = effectiveChainLength();

    if (chainLengthValue > 0)
    {
        if (chainPosition >= chainLengthValue)
            chainPosition = 0;

        activePattern.store (chainPatternAt (chainPosition), std::memory_order_release);
    }
    else
    {
        activePattern.store (patternParam, std::memory_order_release);
    }

    // ---- Transport ----
    const bool hostMode = (syncMode == SyncMode::host)
                       && (hostPosition != nullptr)
                       && hostPosition->getPpqPosition().hasValue();

    if (hostMode)
    {
        if (! hostPosition->getIsPlaying())
        {
            stopTransport();
            return;
        }

        const bool transportStarted = ! hostTransportRunning.load (std::memory_order_relaxed);
        hostTransportRunning.store (true, std::memory_order_release);

        if (const auto hostBpm = hostPosition->getBpm(); hostBpm.hasValue() && *hostBpm > 1.0)
            bpm = (float) juce::jlimit (20.0, 2000.0, *hostBpm); // Host-Tempo (Sanity-Clamp)
        else
            bpm = internalBpm;

        updateTiming();

        if (transportStarted)
        {
            chainPosition = 0;
            patternCycle = 0;
            lastHostBar = -1;
            sampleCounter = 0.0;
        }

        applyHostPhase (*hostPosition);
        playing.store (true, std::memory_order_release);
    }
    else
    {
        hostTransportRunning.store (false, std::memory_order_release);

        if (! playingValue)
        {
            stopTransport();
            return;
        }

        const bool transportStarted = ! playing.load (std::memory_order_relaxed);
        bpm = internalBpm;

        if (transportStarted || gridChanged || stepsChanged || swingChanged
            || bpmChanged || activeStepDuration <= 0.0)
        {
            updateTiming();

            if (transportStarted)
            {
                currentStep.store (0, std::memory_order_release);
                chainPosition = 0;
                patternCycle = 0;
                lastHostBar = -1;
                sampleCounter = 0.0;
            }

            activeStepDuration = stepDurationSamples (currentStep.load (std::memory_order_relaxed));
        }

        // Beim Start sofort Step 0 triggern (nicht erst nach einer vollen Step-Dauer).
        if (transportStarted)
            sampleCounter = activeStepDuration;

        playing.store (true, std::memory_order_release);
    }

    // ---- Fällige Sub-Trigger (Ratchet/Flam) aus dem Vorgängerblock ----
    flushScheduledTriggers (numSamples);

    // ---- Steps innerhalb des Blocks sample-genau abarbeiten ----
    renderStepClock (numSamples);
}

//==============================================================================
void StepSequencer::applyHostPhase (const juce::AudioPlayHead::PositionInfo& position)
{
    const double ppq = position.getPpqPosition().orFallback (0.0);

    double beatsPerBar = 4.0;

    if (const auto signature = position.getTimeSignature();
        signature.hasValue() && signature->numerator > 0 && signature->denominator > 0)
    {
        beatsPerBar = juce::jmax (1.0, (double) signature->numerator * 4.0
                                        / (double) signature->denominator);
    }

    // Phase relativ zum letzten Taktanfang: damit startet jeder Host-Takt
    // wieder auf Step 0 (SND-137, Phase-Reset auf der Host-Bar).
    const double barStart = position.getPpqPositionOfLastBarStart().orFallback (0.0);
    double rel = std::fmod (ppq - barStart, beatsPerBar);

    if (rel < 0.0)
        rel += beatsPerBar;

    const double stepPosition = rel * (double) stepsPerQuarter();
    const int flooredStep = (int) std::floor (stepPosition);
    int hostStep = flooredStep % juce::jmax (1, steps);

    if (hostStep < 0)
        hostStep += steps;

    const double fraction = stepPosition - (double) flooredStep;

    currentStep.store (hostStep, std::memory_order_release);
    activeStepDuration = stepDurationSamples (hostStep);

    // Pattern-Kette: im Host-Modus pro Host-Bar weiterschalten.
    const auto barIndex = (int64_t) std::floor (ppq / beatsPerBar);

    if (barIndex != lastHostBar)
    {
        if (lastHostBar >= 0)
        {
            advancePattern();
            ++patternCycle;
        }

        lastHostBar = barIndex;
    }

    // Bereits verstrichene Zeit im aktuellen Step (Host ist die Zeitbasis).
    sampleCounter = (fraction < 1.0e-9) ? activeStepDuration : fraction * activeStepDuration;
}

void StepSequencer::stopTransport()
{
    playing.store (false, std::memory_order_release);
    hostTransportRunning.store (false, std::memory_order_release);
    currentStep.store (0, std::memory_order_release);
    sampleCounter = 0.0;
    scheduledCount = 0;
    chainPosition = 0;
    patternCycle = 0;
    lastHostBar = -1;
}

void StepSequencer::renderStepClock (int numSamples)
{
    for (int i = 0; i < numSamples; ++i)
    {
        sampleCounter += 1.0;

        if (sampleCounter < activeStepDuration)
            continue;

        sampleCounter -= activeStepDuration;

        const int step = currentStep.load (std::memory_order_relaxed);

        // Swing verschiebt nur den Trigger des ungeraden Steps, nicht das Grid.
        triggerStep (step, i + (int) std::llround (swingDelaySamples (step)), numSamples);

        const int next = (step + 1) % juce::jmax (1, steps);

        if (next == 0)
        {
            ++patternCycle;
            advancePattern();
        }

        currentStep.store (next, std::memory_order_release);
        activeStepDuration = stepDurationSamples (next);
    }
}

//==============================================================================
bool StepSequencer::passesProbability (int stepIndex, float probability) const
{
    if (probability >= 1.0f)
        return true;

    if (probability <= 0.0f)
        return false;

    // SND-142: deterministischer Seed aus Pattern/Step/Pattern-Zyklus. Kein
    // Zustand, kein RNG -> identisches Ergebnis bei jedem Lauf.
    uint32_t hash = (uint32_t) activePattern.load (std::memory_order_relaxed) * 0x9E3779B9u;
    hash ^= (uint32_t) stepIndex * 0x85EBCA6Bu;
    hash ^= patternCycle * 0xC2B2AE35u;
    hash ^= hash >> 16;
    hash *= 0x7FEB352Du;
    hash ^= hash >> 15;
    hash *= 0x846CA68Bu;
    hash ^= hash >> 16;

    const float random = (float) (hash & 0x00FFFFFFu) / 16777216.0f;
    return random < probability;
}

void StepSequencer::triggerStep (int stepIndex, int sampleOffset, int numSamples)
{
    if (! juce::isPositiveAndBelow (stepIndex, steps))
        return;

    const StepSettings settings = readStep (activePattern.load (std::memory_order_relaxed),
                                            stepIndex);

    if (! settings.gate || settings.velocity <= 0.0f)
        return;

    if (! passesProbability (stepIndex, settings.probability))
        return;

    // SND-140: Ratchet teilt den Step in n gleichmäßige Trigger.
    const int ratchet = juce::jlimit (1, 4, settings.ratchet);
    const double spacing = activeStepDuration / (double) ratchet;

    for (int k = 0; k < ratchet; ++k)
    {
        const int offset = sampleOffset + (int) std::llround (spacing * (double) k);
        scheduleTrigger (settings.velocity, settings.pitchSemitones, offset, numSamples);
    }

    // SND-141: Flam als zweiter Trigger kurz nach dem Haupttrigger.
    if (settings.flamMs > 0.0f)
    {
        const int flamSamples = (int) std::lround ((double) settings.flamMs * 0.001 * sampleRate);

        if (flamSamples > 0)
            scheduleTrigger (settings.velocity, settings.pitchSemitones,
                             sampleOffset + flamSamples, numSamples);
    }
}

void StepSequencer::scheduleTrigger (float velocity, float pitchSemitones,
                                     int sampleOffset, int numSamples)
{
    const int offset = juce::jmax (0, sampleOffset);

    if (offset < numSamples)
    {
        engine.enqueueTrigger (velocity, pitchSemitones, offset);
        enqueuedTriggerCount.fetch_add (1, std::memory_order_relaxed);
        return;
    }

    // Trigger liegt hinter dem Blockende (Ratchet/Flam): relativ zum naechsten
    // Block vormerken, damit die Timing-Auflösung erhalten bleibt.
    if (scheduledCount >= maxScheduledEvents)
        return;

    scheduled[(size_t) scheduledCount++] = { offset - numSamples, velocity, pitchSemitones };
}

void StepSequencer::flushScheduledTriggers (int numSamples)
{
    for (int i = 0; i < scheduledCount; )
    {
        if (scheduled[(size_t) i].samplesUntil < numSamples)
        {
            const ScheduledEvent event = scheduled[(size_t) i];
            engine.enqueueTrigger (event.velocity, event.pitchSemitones, event.samplesUntil);
            enqueuedTriggerCount.fetch_add (1, std::memory_order_relaxed);

            scheduled[(size_t) i] = scheduled[(size_t) --scheduledCount];
        }
        else
        {
            scheduled[(size_t) i].samplesUntil -= numSamples;
            ++i;
        }
    }
}
