#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <cstdint>
#include <juce_audio_processors/juce_audio_processors.h>
#include "Parameters/ParameterSmoothing.h"
#include "Parameters/ParameterIDs.h"
#include "DSP/KickSynthesisEngine.h"

//==============================================================================
/** Step-Sequencer mit sample-genauer Clock im Audio-Thread (§7).

    Taktgeber (SND-136/137): intern über sequencerBpm oder host-synchron über
    AudioPlayHead::PositionInfo (PPQ/BPM). Im Host-Modus wird die Phase aus der
    PPQ-Position relativ zum letzten Taktanfang abgeleitet (Phase-Reset auf der
    Host-Bar) und der Start/Stop des Hosts übernommen.

    Swing (SND-138): sequencerSwing 0..0.75, Anzeige 50..75 %.
    r = 0.5 + swing/3; das Grid bleibt gerade, ungerade Steps werden um
    (r-0.5)*2 Steps verzoegert (bei 0.75 ein halber Step). Damit wirkt Swing
    auch im Host-Sync.

    Pro Step (SND-140..143): Gate, Velocity, Pitch, Ratchet (1..4), Flam
    (0..50 ms), Probability (0..1, deterministisch) und Length (0.25..4 Steps).
    Pattern (SND-145/146): 1..64 Steps, Pattern A-D mit Kette 1..8.

    Die 48 bestehenden Step-Parameter (stepVel_i, stepGate_i, stepPitch_i)
    bleiben unverändert; die neuen Step-Parameter liegen auf denselben
    Namensschema-IDs und werden über patternStride verschoben.
 */
class StepSequencer
{
public:
    //==========================================================================
    // Neue Parameter-IDs (Definition/Range/Default übernimmt der
    // ParameterLayout-Batch; hier stehen nur die vom Sequencer gelesenen IDs).
    static constexpr auto* paramSyncMode    = "sequencerSyncMode";   // Choice: Internal/Host
    static constexpr auto* paramGridMode    = "sequencerGrid";       // Choice: 1/16, 1/8T, 1/16T
    static constexpr auto* paramPattern     = "sequencerPattern";    // Choice: A/B/C/D
    static constexpr auto* paramChainLength = "sequencerChain";      // 0..8 (0 = aus)
    static constexpr auto* paramChainSlot   = "sequencerChainSlot_"; // 0..7, Choice A-D
    static constexpr auto* paramClockOut    = "sequencerClockOut";   // Bool

    static constexpr auto* stepPrefixRatchet     = "stepRatchet_";     // 1..4
    static constexpr auto* stepPrefixFlam        = "stepFlam_";        // 0..50 ms
    static constexpr auto* stepPrefixProbability = "stepProbability_"; // 0..1
    static constexpr auto* stepPrefixLength      = "stepLength_";      // 0.25..4 Steps

    //==========================================================================
    static constexpr int maxSteps       = 16;  // SND-145 (M14: 64 -> 16, GUI zeigt 16 Steps)
    static constexpr int numPatterns    = 4;   // SND-146 (A-D)
    static constexpr int maxChainLength = 8;   // SND-146
    /** Parameter-Offset zwischen zwei Patterns (Pattern B = Schritt 64 usw.). */
    static constexpr int patternStride  = maxSteps;

    enum class SyncMode { internal = 0, host = 1 };                        // SND-136
    enum class GridMode { sixteenth = 0, eighthTriplet = 1, sixteenthTriplet = 2 }; // SND-144

    /** Alle musikalischen Werte eines Steps (unsmoothed gelesen, SND-147). */
    struct StepSettings
    {
        bool  gate { false };
        float velocity { 1.0f };
        float pitchSemitones { 0.0f };
        int   ratchet { 1 };          // SND-140: 1..4 Trigger pro Step
        float flamMs { 0.0f };        // SND-141: 0..50 ms, zweiter Trigger
        float probability { 1.0f };   // SND-142: 0..1
        float lengthSteps { 1.0f };   // SND-143: 0.25..4 Grid-Steps
    };

    //==========================================================================
    StepSequencer (juce::AudioProcessorValueTreeState& apvtsToUse,
                   ParameterSmoothing& smoothingToUse,
                   KickSynthesisEngine& engineToUse)
        : apvts (apvtsToUse), smoothing (smoothingToUse), engine (engineToUse) {}

    void prepare (double sampleRate, int samplesPerBlock);
    void reset();

    /** Prozess-Block ohne Host-Information (interner Taktgeber). */
    void processBlock (int numSamples);

    /** Prozess-Block mit Host-Position (nur im SyncMode::host ausgewertet).
        hostPosition darf nullptr sein (Standalone/kein Playhead). */
    void processBlock (int numSamples,
                       const juce::AudioPlayHead::PositionInfo* hostPosition);

    //==========================================================================
    // Message-Thread-API
    void setPlaying (bool playing);
    bool isPlaying() const { return playing.load (std::memory_order_acquire); }

    void setBpm (float bpm);
    void setSteps (int steps);
    void setSwing (float swing);
    void setSyncMode (SyncMode mode);
    void setGridMode (GridMode mode);
    void setPattern (int pattern);
    void setPatternChain (const std::array<int, maxChainLength>& patterns, int length);

    /** SND-148: Klick auf einen Step triggert die Note sofort (Message-Thread).
        Gate/Probability werden ignoriert, damit die Vorschau immer hörbar ist. */
    void previewStep (int stepIndex);

    //==========================================================================
    int getCurrentStep() const { return currentStep.load (std::memory_order_acquire); }
    int getCurrentPattern() const { return activePattern.load (std::memory_order_acquire); }
    int getSteps() const { return steps; }
    float getBpm() const { return bpm; }
    float getSwing() const { return swing; }
    GridMode getGridMode() const { return grid; }
    bool isHostTransportRunning() const { return hostTransportRunning.load (std::memory_order_acquire); }
    /** Clock-/Start-/Stop-Quelle für SND-150. */
    bool isTransportRunning() const { return playing.load (std::memory_order_acquire); }
    /** SND-150: MIDI-Clock-Versand aktiv? (sequencerClockOut, Default aus). */
    bool isMidiClockOutEnabled() const;

    /** Diagnose: Anzahl der in die Engine eingereihten Trigger. */
    int getEnqueuedTriggerCount() const { return enqueuedTriggerCount.load (std::memory_order_relaxed); }

    //==========================================================================
    // Swing-Anzeige: 0 -> 50 %, 0.75 -> 75 %.
    static float swingToDisplayPercent (float swingValue)
    {
        return 100.0f * (0.5f + juce::jlimit (0.0f, 0.75f, swingValue) / 3.0f);
    }

    static float displayPercentToSwing (float displayPercent)
    {
        return juce::jlimit (0.0f, 0.75f, (displayPercent / 100.0f - 0.5f) * 3.0f);
    }

private:
    //==========================================================================
    struct ScheduledEvent
    {
        int   samplesUntil { 0 };   ///< relativ zum Beginn des aktuellen Blocks
        float velocity { 1.0f };
        float pitchSemitones { 0.0f };
    };

    void updateTiming();
    int  stepsPerQuarter() const;
    double stepDurationSamples (int stepIndex) const;
    double swingDelaySamples (int stepIndex) const;

    float valueOf (int smoothingIndex, float fallback) const;
    float smoothed (int index) const { return smoothing.get (index); }
    float rawValue (const std::atomic<float>* parameter, float fallback) const;
    bool  writeParameter (const char* id, float plainValue);
    StepSettings readStep (int pattern, int stepIndex) const;

    void triggerStep (int stepIndex, int sampleOffset, int numSamples);
    void scheduleTrigger (float velocity, float pitchSemitones,
                          int sampleOffset, int numSamples);
    void flushScheduledTriggers (int numSamples);
    bool passesProbability (int stepIndex, float probability) const;
    void renderStepClock (int numSamples);
    void advancePattern();
    int  chainPatternAt (int index) const;
    int  effectiveChainLength() const;
    /** Übernimmt PPQ/Phase aus der Host-Position (SND-136/137). */
    void applyHostPhase (const juce::AudioPlayHead::PositionInfo& position);
    void stopTransport();

    juce::AudioProcessorValueTreeState& apvts;
    ParameterSmoothing& smoothing;
    KickSynthesisEngine& engine;

    double sampleRate { 48000.0 };
    int samplesPerBlock { 512 };

    std::atomic<bool> playing { false };
    std::atomic<bool> hostTransportRunning { false };
    std::atomic<int>  currentStep { 0 };
    std::atomic<int>  activePattern { 0 };

    float bpm { 128.0f };        ///< effektives Tempo (Host oder intern)
    float internalBpm { 128.0f };///< sequencerBpm-Parameter
    int   steps { 16 };
    float swing { 0.0f };
    GridMode grid { GridMode::sixteenth };
    SyncMode syncMode { SyncMode::internal };

    double sampleCounter { 0.0 };      ///< Samples seit Beginn des aktuellen Steps
    double samplesPerStep { 0.0 };     ///< ein Grid-Step (1/16, 1/8T oder 1/16T)
    double activeStepDuration { 0.0 }; ///< inkl. Swing und stepLength
    int64_t lastHostBar { -1 };
    uint32_t patternCycle { 0 };       ///< deterministischer Seed (SND-142)

    // ---- Pattern-Kette (SND-146), Message-Thread -> Audio-Thread ----
    std::array<std::atomic<int>, maxChainLength> chainEntries {};
    std::atomic<int> chainLength { 0 };
    int chainPosition { 0 };

    // ---- Über Blockgrenzen laufende Sub-Trigger (Ratchet/Flam) ----
    static constexpr int maxScheduledEvents = 64;
    std::array<ScheduledEvent, maxScheduledEvents> scheduled {};
    int scheduledCount { 0 };
    std::atomic<int> enqueuedTriggerCount { 0 };

    // ---- Gecachte Parameter-Zugriffe (allokationsfrei im Audio-Thread) ----
    int indexBpm { -1 };
    int indexSwing { -1 };
    int indexSteps { -1 };
    int indexPlaying { -1 };

    static constexpr int numStepSlots = numPatterns * patternStride;
    std::array<int, numStepSlots> indexGate {};
    std::array<int, numStepSlots> indexVelocity {};
    std::array<int, numStepSlots> indexPitch {};
    std::array<int, numStepSlots> indexRatchet {};
    std::array<int, numStepSlots> indexFlam {};
    std::array<int, numStepSlots> indexProbability {};
    std::array<int, numStepSlots> indexLength {};

    const std::atomic<float>* rawSyncMode { nullptr };
    const std::atomic<float>* rawGridMode { nullptr };
    const std::atomic<float>* rawPattern { nullptr };
    const std::atomic<float>* rawChainLength { nullptr };
    const std::atomic<float>* rawClockOut { nullptr };
    std::array<const std::atomic<float>*, maxChainLength> rawChainSlots {};

    // Fallbacks, wenn die neuen Parameter noch nicht im Layout existieren.
    std::atomic<int> syncModeFallback { (int) SyncMode::internal };
    std::atomic<int> gridModeFallback { (int) GridMode::sixteenth };
    std::atomic<int> patternFallback { 0 };
};
