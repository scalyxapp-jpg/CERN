#pragma once

#include <array>
#include <atomic>
#include <cmath>
#include <vector>
#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_dsp/juce_dsp.h>

#include "MembraneModel.h"
#include "PitchEnvelope.h"
#include "ClickGenerator.h"
#include "TubeResonator.h"
#include "SoftKneeCompressor.h"
#include "TransientShaper.h"
#include "OversamplingHelper.h"
#include "MaterialReverb.h"
#include "MaterialRoom.h"
#include "MixerStage.h"
#include "HiggsFieldEngine.h"
#include "PatchGraph.h"
#include "ParameterPatchGraph.h"
#include "Parameters/ParameterSmoothing.h"
#include "Parameters/ParameterIDs.h"

//==============================================================================
/** Stereo-Sammelpuffer für die Jack-Buchsen (Mono-Buchsen: l == r). */
struct StereoMix
{
    float l = 0.0f;
    float r = 0.0f;
};

//==============================================================================
/** CERN Synthese-Engine.

    Signal-Flow (mono-intern, Stereo-Ausgang) gemäß TECHNICAL_WIRING_FULL.md §3:
    Pitch Envelope -> Membrane -> Tube/Hollow -> Sub -> Click -> Drive -> EQ ->
    Compressor -> Output (Gain -> Mono-Below/Width -> SoftClip -> Limiter ->
    -1-dB-Kalibrierung -> Pan) + parallele Room / Higgs / Reverb FX.

    Alle Parameter werden einmal pro Block über smoothing.getById(id) gelesen.
    OutputGain und Pan werden sample-genau gerampet; die Pan-Gains werden als
    Block-Rampe interpoliert (kein cos/sin pro Sample).

    Trigger: Sample-genaue Events (Sequencer, MIDI) laufen über eine kleine
    FIFO, die ausschließlich im Audio-Thread beschrieben wird. Trigger aus dem
    Message-Thread (GUI-Button, triggerNote()) laufen über die atomare
    pendingTrigger-Fallback-Strecke und werden bei Offset 0 einsortiert.
*/
class KickSynthesisEngine
{
public:
    explicit KickSynthesisEngine (juce::AudioProcessorValueTreeState& apvts);

    void prepare (double sampleRate, int samplesPerBlock);
    void reset();

    /** Einen Kick sofort triggern (Velocity 0..1, Pitch-Offset in Halbtönen). */
    void trigger (float velocity = 1.0f, float pitchSemitones = 0.0f);

    /** Audio-Thread: Trigger sample-genau in den laufenden Block einspeisen.
        sampleOffset ist relativ zum Blockanfang (wird geklemmt). */
    void enqueueTrigger (float velocity, float pitchSemitones, int sampleOffset);

    /** Haupt-Render: verarbeitet den Buffer (mono-intern, Stereo-Ausgang). */
    void render (juce::AudioBuffer<float>& buffer, int numSamples,
                 ParameterSmoothing& smoothing);

    bool isActive() const { return isActiveFlag.load (std::memory_order_acquire); }
    float getCompressorGainReductionDb() const { return compressor.getGainReductionDb(); }

    /** Limiter-Gain-Reduction des letzten Blocks in dB (0 = keine Regelung). */
    float getLimiterGainReductionDb() const { return limiterGainReductionDb; }

    /** Limiter-GR-Maximum seit prepare()/reset() in dB (SND-006). */
    float getLimiterGainReductionMaxDb() const { return limiterGainReductionMaxDb; }

    /** Ausgangspegel des Drive-Moduls im letzten Block in dBFS (Metering, SND-020). */
    float getDriveOutputLevelDb() const { return driveOutputLevelDb; }

    /** Maximale Soft-Clip-Dämpfung des letzten Blocks in dB (0 = keine Regelung).
        Misst die größte Betragsdämpfung innerhalb der Clip-Stufe (SND-006/020). */
    float getClipGainReductionDb() const { return clipGainReductionDb; }

    /** Clip-GR-Maximum seit prepare()/reset() in dB (SND-006). */
    float getClipGainReductionMaxDb() const { return clipGainReductionMaxDb; }

    /** Sub-Oszillator-Verhältnis zur Grundfrequenz (0.5 = Sub-Oktave, 1.0 = Unison). */
    void setSubRatio (float ratio) { subRatio = (ratio < 0.75f) ? 0.5f : 1.0f; }
    float getSubRatio() const { return subRatio; }

    /** Tube-Resonanz trackt die Grundfrequenz (SND-068). */
    void setTubeTracking (bool shouldTrack) { tubeTracking = shouldTrack; }

    /** Velocity -> Pitch-Anteil (0..1, SND-030); 0 = aus. */
    void setVelocityToPitch (float amount) { velocityToPitch = juce::jlimit (0.0f, 1.0f, amount); }

    // ---- Integration Points für die Parameter/Layout-Agentur ----
    // Die DSP-Seite ist implementiert; sobald die IDs in ParameterIDs.h /
    // ParameterLayout.cpp existieren, genügt ein Aufruf pro Block (oder beim
    // Laden des States) mit dem gelesenen Parameterwert.

    /** SND-073: EQ-Sub-Band trackt die Grundfrequenz (ID "eqSubTrack", Default an). */
    void setEqSubTracking (bool shouldTrack) { eqSubTrack = shouldTrack; }
    bool isEqSubTracking() const { return eqSubTrack; }

    /** SND-093: Room-Tap-Timing (ID "roomSync": 1/16, 1/8, dotted 1/8, frei). */
    enum class RoomSync : int { sixteenth = 0, eighth = 1, eighthDotted = 2, free = 3 };
    void setRoomSync (RoomSync mode) { roomSync = mode; }
    RoomSync getRoomSync() const { return roomSync; }

    // Trigger-Atomics (Message-Thread schreibt, Audio-Thread liest bei Offset 0)
    void setPendingTrigger (float velocity, float pitchSemitones)
    {
        pendingVelocity.store (velocity, std::memory_order_relaxed);
        pendingPitchSemitones.store (pitchSemitones, std::memory_order_relaxed);
        pendingTrigger.store (true, std::memory_order_release);
    }
    bool consumePendingTrigger (float& velocity, float& pitchSemitones)
    {
        if (! pendingTrigger.exchange (false, std::memory_order_acq_rel))
            return false;
        velocity   = pendingVelocity.load (std::memory_order_relaxed);
        pitchSemitones = pendingPitchSemitones.load (std::memory_order_relaxed);
        return true;
    }

    /** Aktuellen Audio-Patch abfragen / ändern (EURORACK-artiges Re-Patching). */
    /** Mixer-Stufe (Kanaele, EQ, Sidechain) fuer die GUI. */
    MixerStage& getMixer() noexcept { return mixer; }
    const MixerStage& getMixer() const noexcept { return mixer; }

    PatchGraph& getPatchGraph() noexcept { return patchGraph; }
    const PatchGraph& getPatchGraph() const noexcept { return patchGraph; }

    /** Aktuellen Parameter-Patch (CV-Verkabelung) abfragen / ändern. */
    ParameterPatchGraph& getParameterPatchGraph() noexcept { return parameterPatchGraph; }
    const ParameterPatchGraph& getParameterPatchGraph() const noexcept { return parameterPatchGraph; }

    /** Berechnet effektiv verkabelte Parameterwerte für den aktuellen Block.

        Berücksichtigt Kabel-Amount/Polarität/Rate/Slew, virtuelle Quellen
        (LFO, Envelope-Follower, Makros) und das optionale 0..1-Wrap.
        Wird automatisch in render() aufgerufen, wenn nicht bereits vorbereitet. */
    void prepareParameterModulation (ParameterSmoothing& smoothing, int samplesPerBlock);

    void setTailLength (float seconds) { tailLengthSeconds = seconds; }
    float getTailLengthSeconds() const { return tailLengthSeconds; }

    /** SND-183: Offline-Bounce des aktuellen Klangs auf eine WAV-Datei.

        Rendert `secondsToRender` (auf volle 512-Sample-Blöcke abgerundet) als
        Stereo-WAV (32 bit float, native Samplerate) mit einem Viertelschlag
        bei `bpm` als Trigger-Raster. Nutzt den aktuellen APVTS-Parameter-
        Pegel des Engine-States; der Block nimmt denselben Routing-Pfad wie
        der Echtzeitbetrieb. Nach dem Bounce befindet sich die Engine im
        „abgespielten“ Zustand (Hüllkurven/Queue) – für den Live-Pfad reset()
        aufrufen. Keine GUI nötig, allokiert im Zielpfad (kein Audio-Thread).
        Rückgabe: true bei Erfolg (Datei geschrieben und nicht leer). */
    bool renderOfflineBounce (const juce::File& target,
                              ParameterSmoothing& smoothing,
                              double secondsToRender,
                              float bpm);

private:
    // ---- DSP-Module ----
    MembraneModel membrane;
    PitchEnvelope pitchEnv;
    ClickGenerator click;
    TubeResonator tube;
    SoftKneeCompressor compressor;
    MaterialReverb reverb;
    MaterialRoom materialRoom;
    HiggsFieldEngine higgs;

    /** Mehrkanal-Mixer (letzte Stufe vor dem Output): pro Klang-Modul ein
        Kanal mit Lautstaerke, 4-Band-EQ, Mute und automatischer Sidechain. */
    MixerStage mixer;

    // ---- Modulare Verkabelung (Eurorack-artig) ----
    PatchGraph patchGraph;
    ParameterPatchGraph parameterPatchGraph;
    ParameterPatchGraph::Snapshot parameterSnapshot;
    PatchGraph::Snapshot patchSnapshot; ///< Audio-Block-Snapshot (ein Lock, keine Allokation)
    std::array<float, ParameterPatchGraph::maxParameterCount> modulatedParameterValues {};
    bool parameterModulationPrepared { false };

    /** Gecachte RangedAudioParameter* (kein dynamic_cast/getParameter pro Block). */
    static constexpr int maxCachedParameters = ParameterPatchGraph::maxParameterCount;
    std::array<juce::RangedAudioParameter*, maxCachedParameters> cachedParameters {};
    void cacheParameterPointers();

    static float monoAverage (const StereoMix& mix) noexcept { return (mix.l + mix.r) * 0.5f; }

    juce::AudioProcessorValueTreeState& parameters;

    double sampleRate { 48000.0 };

    // ---- Trigger-Queue (nur Audio-Thread beschreibt sie) ----
    struct TriggerEvent
    {
        float velocity { 1.0f };
        float pitchSemitones { 0.0f };
        int sampleOffset { 0 };
    };

    static constexpr int triggerQueueSize = 16;
    std::array<TriggerEvent, triggerQueueSize> triggerQueue {};
    int triggerCount { 0 };

    void pushTriggerEvent (float velocity, float pitchSemitones, int sampleOffset);

    /** Konsumiert fällige Trigger-Events; true, wenn mindestens einer feuerte. */
    bool consumeTriggerEvents (int sample, float velSensitivity);

    // ---- Sub ----
    float subPhase { 0.0f };
    float subEnv { 0.0f };
    float subOmega { 0.0f };       ///< Block-Konstante (auf glideRatio skaliert)
    float subOmegaBase { 0.0f };   ///< Grundwert bei Pitch-Faktor 1.0
    float subDt { 0.0f };          ///< normierter Phasenschritt für PolyBLEP
    float subDtBase { 0.0f };
    float subDecayCoeff { 0.0f };  ///< Block-Konstante
    float subTriState { 0.0f };    ///< integrierter Square für bandbegrenztes Dreieck
    float subRatio { 1.0f };       ///< 0.5 = Oktave, 1.0 = Unison (SND-049)
    float triggerPitchSemitones { 0.0f };
    float velocityToPitch { 0.0f }; ///< 0..1, expliziter Velocity->Pitch-Anteil (SND-030)

    // ---- Tuning / Glide (SND-128/129/132/134/135) ----
    float baseFundamentalHz { 60.0f };  ///< f0 des Blocks, auf Key/Scale gesnappt
    int   baseNoteNumber { 36 };        ///< MIDI-Note von baseFundamentalHz
    float glideRatio { 1.0f };          ///< aktueller Pitch-Faktor (2^(st/12))
    float glideTargetRatio { 1.0f };
    float glideCoeff { 1.0f };          ///< multiplikativer Schritt pro Sample
    int   glideSamplesLeft { 0 };
    float trackGlideRatio { 1.0f };     ///< 30-%-Tracking für Click/Tube (SND-134)
    float trackGlideTargetRatio { 1.0f };
    float trackGlideCoeff { 1.0f };
    bool  hasTriggerPitch { false };
    int   blockKeyRoot { 0 };           ///< 0..11 (SND-132)
    int   blockScale { 0 };             ///< 0 = Chromatic, 1 = Minor (SND-132)
    float blockGlideMs { 0.0f };        ///< 0..200 ms (SND-135)

    void startPitchGlide (float targetSemitones);

    // ---- Body-Resonator (eigener abklingender Sinus, nicht die Membran) ----
    float bodyPhase { 0.0f };
    float bodyEnv { 0.0f };         ///< Amplitude des Body-Oszillators
    float bodyAttackCoeff { 0.0f }; ///< Block-Konstante (Attack)
    float bodyDecayCoeff { 0.0f };  ///< Block-Konstante
    float bodyOmega { 0.0f };       ///< Block-Konstante (auf glideRatio skaliert)
    float bodyOmegaBase { 0.0f };   ///< Grundwert bei Pitch-Faktor 1.0
    float bodyPeak { 0.0f };        ///< Zielamplitude des laufenden Triggers
    bool  bodyInAttack { false };

    // ---- Room / late rumble (4-Tap-Netzwerk, SND-091..100) ----
    std::vector<float> roomDelayL, roomDelayR; ///< in prepare() dimensioniert (keine Allokation im Render)
    int roomBufferSize { 0 };
    int roomPos { 0 };
    std::array<int, 4> roomTapSamplesL { { 1, 1, 1, 1 } };
    std::array<int, 4> roomTapSamplesR { { 1, 1, 1, 1 } };
    std::array<float, 4> roomTapGain { { 1.0f, 0.80f, 0.65f, 0.52f } }; ///< per-Tap-Gain (SND-091)
    float roomSendGain { 0.0f };   ///< sqrt(roomLevel) (SND-092/097)
    float roomFeedback { 0.0f };   ///< Block-Konstante (aus roomDecay + Tap-Abstand)
    float roomDcCoeff { 0.9995f }; ///< 2-Hz-DC-Blocker (SND-099)
    bool  roomToneLowpass { true };///< Send-Tone: LP unter 0.5, HP darüber (SND-096)
    float roomToneCoeff { 0.999f };
    float roomToneLpL { 0.0f }, roomToneLpR { 0.0f };
    float roomToneHpXPrevL { 0.0f }, roomToneHpYPrevL { 0.0f };
    float roomToneHpXPrevR { 0.0f }, roomToneHpYPrevR { 0.0f };
    float roomFbXPrevL { 0.0f }, roomFbYPrevL { 0.0f }; ///< DC-Blocker im Feedback L
    float roomFbXPrevR { 0.0f }, roomFbYPrevR { 0.0f }; ///< DC-Blocker im Feedback R
    RoomSync roomSync { RoomSync::free };

    // ---- Drive (Band-Split: Low clean, High gesättigt; DC nur im Wet-Zweig) ----
    float driveOutDcXPrev { 0.0f };
    float driveOutDcYPrev { 0.0f };
    float driveLowLpState { 0.0f };   ///< Crossover-LP (150 Hz, komplementär)
    float driveHpLpState { 0.0f };    ///< optionaler Pre-HP im gesättigten Zweig
    float driveHpCoeff { 0.0f };      ///< Block-Konstante (aus drivePreHP)
    float driveSplitCoeff { 0.0f };   ///< Block-Konstante (150 Hz Crossover)
    float driveDcCoeff { 0.0f };      ///< Block-Konstante (~10 Hz DC-Blocker)
    bool  drivePreHpActive { false };
    float driveCachedAmount { -1.0f };
    int   driveCachedCharacter { -1 };
    float driveNormCache { 1.0f };    ///< 1/tanh(k) je Character (SND-011)
    float drivePeak { 0.0f };
    float driveOutputLevelDb { -100.0f };

    // ---- Tube ----
    bool tubeTracking { true };

    // ---- EQ (Sub 12 dB/oct, Punch als Biquad-Peak, Air; parallel + Makeup) ----
    float eqSubState1   { 0.0f };
    float eqSubState2   { 0.0f };
    float eqHighShelfState { 0.0f };
    float eqHighpassState { 0.0f };
    float eqRumbleCoeff { 0.99f };
    float eqSubCoeff { 0.99f };
    float eqAirCoeff { 0.5f };
    float eqPunchB0 { 0.0f }, eqPunchB1 { 0.0f }, eqPunchB2 { 0.0f };
    float eqPunchA1 { 0.0f }, eqPunchA2 { 0.0f };
    float eqPunchX1 { 0.0f }, eqPunchX2 { 0.0f };
    float eqPunchY1 { 0.0f }, eqPunchY2 { 0.0f };
    float eqSubGainLin { 0.0f };
    float eqPunchGainLin { 0.0f };
    float eqAirGainLin { 0.0f };
    float eqMakeupNorm { 1.0f }; ///< 1/(1+Boost-Summe), Clamp (SND-079)
    bool  eqSubTrack { true };   ///< Sub-Band trackt f0 (SND-073)

    // ---- Compressor-Integration (SND-088/090) ----
    float compressorMix { 1.0f };         ///< "compMix"
    bool  compressorAutoMakeup { true };  ///< "compAutoMakeup"

    // ---- Output ----
    // Per-sample ramp für OutputGain/Pan (P0-4)
    float outGainCurrent { 0.0f };
    float outPanLCurrent { 1.0f };
    float outPanRCurrent { 1.0f };
    bool outputRampInitialized { false };

    // Block-Konstanten des Output-Stages
    float monoBelowCoeffValue { 1.0f };
    bool  monoBelowActive { false };
    float outputWidthValue { 1.0f };
    float clipDrive { 1.0f };   ///< 1 + 4*clipAmt
    float clipNorm { 1.0f };    ///< clipCeiling / tanh(clipDrive)
    float clipCeiling { 1.0f }; ///< Ceiling der Clip-Stufe (SND-010 Invariante)
    bool  clipActive { false };
    float limiterGainReductionDb { 0.0f };

    // Limiter: Instant-Attack + Release, Peak-Hold, kein Per-Sample-Brickwall.
    // (Kein Lookahead -> keine Latenz; der Pegel wird über peakEnv garantiert
    //  <= ceiling gehalten, weil needed <= ceiling/|x| ist.)
    float limiterGain { 1.0f };
    float limiterPeakEnv { 0.0f };
    float limiterCeiling { 1.0f };
    float limiterAttackCoeff { 0.0f };
    float limiterReleaseCoeff { 0.0f };
    float limiterPeakDecay { 0.0f };
    float limiterMinGain { 1.0f }; ///< Block-Minimum für GR-Metering (SND-005/020)
    float limiterGainReductionMaxDb { 0.0f }; ///< Maximum über alle Blöcke seit reset

    // Mono-Below (komplementäres First-Order-Paar)
    float monoBelowState { 0.0f };

    // ---- Oversampling (SND-179): Drive-Sättigung + Soft-Clip, 2x ----
    StreamingTwoX driveUp, driveDown;     ///< Drive-Pfad (Mono)
    StreamingTwoX clipUp[2], clipDown[2]; ///< Clip-Pfad L/R (Stereo)
    bool oversampleEnabled { false };     ///< "oversampleEnabled" (Bool)
    std::array<float, 32> driveLowDelay {}; ///< Low-Zweig-Verzögerung = 2x-Latenz (15 Samples)
    int driveLowDelayPos { 0 };

    // ---- Transient-Shaper (SND-175): Bus vor Drive/Comp ----
    TransientShaper transientShaper;

    // ---- Internes Sidechain/Ducking (SND-174) ----
    // Duck-Quelle: das getriggerte Kick/Sub-Signal   (Membrane+Body+Sub-Sample
    // dieses Samples, |x|-Peak-Nachfolger, ~2 ms Attack,
    // Release = "duckRelease"). Die Duck-Gain-Stufe sitzt am Eingang
    // der Output-Stufe (post Summen-Knoten, pre-Gain/Clip/Limiter) und
    // duckt damit den kompletten Mix inkl. FX-Tails. duckDepth = 0 -> Gain exakt 1.
    float duckDepthDb { 0.0f };        ///< "duckDepth" (0..24 dB)
    float duckAttackCoeff { 0.0f };    ///< ~2 ms Attack
    float duckReleaseCoeff { 0.0f };   ///< Block-Konstante aus "duckRelease" (20..500 ms)
    float duckEnv { 0.0f };            ///< Follower-State (0..1, geklemmt)
    float membraneBusSample { 0.0f };  ///< Duck-Quelle: |Membrane+Body| des laufenden Samples
    float subBusSample { 0.0f };       ///< Duck-Quelle: |Sub| des laufenden Samples

    // ---- Humanize (SND-178) ----
    uint32_t humanizeCounter { 0 };        ///< Pitch/Level-Jitter (alle Trigger)
    uint32_t humanizeTimingCounter { 0 };  ///< Timing-Jitter (enqueueTrigger)
    float humanizeAmount { 0.0f };         ///< "humanize" (0..1)

    // ---- Velocity -> Click (SND-177) ----
    float velocityToClick { 0.0f };        ///< "velToClick" (0..1)

    // ---- Envelope-Follower (SND-156): Attack/Release aus der APVTS ----
    float envFollowAttackCoeff { 0.0f };
    float envFollowReleaseCoeff { 0.0f };
    float envFollowerValue { 0.0f };       ///< geglätteter Pegel (0..1)

    // ---- FX-Wet-Breite (SND-181): nur Room/Reverb/MaterialRoom ----
    float fxWidthValue { 1.0f };

    // ---- Clip-GR Metering (SND-006/020) ----
    float clipGainReductionDb { 0.0f }; ///< Block-Maximum der Clip-Absenkung
    float clipMinGain { 1.0f };         ///< Block-Minimum von |y|/|x|
    float clipGainReductionMaxDb { 0.0f }; ///< Maximum über alle Blöcke seit reset (SND-006)

    // ---- Enable-Flanken (Reset beim Wiedereinschalten, Audio-Thread) ----
    struct ModuleEnableState
    {
        bool membrane { true }, tube { true }, pitchEnv { true }, click { true },
             body { true }, sub { true }, drive { true }, eq { true },
             comp { true }, room { true }, reverb { true }, higgs { true },
             materialRoom { true };
    };
    ModuleEnableState previousEnableState {};
    void applyEnableTransitions (const ModuleEnableState& current);

    // ---- Trigger-Atomics (Message-Thread -> Audio-Thread, Offset 0) ----
    std::atomic<bool>  pendingTrigger { false };
    std::atomic<float> pendingVelocity { 1.0f };
    std::atomic<float> pendingPitchSemitones { 0.0f };

    std::atomic<bool> isActiveFlag { false };
    int tailCounter { 0 };

    float tailLengthSeconds { 1.0f };

    // ---- Hilfsfunktionen ----
    float applyDcBlocker (float x, float& xPrev, float& yPrev, float coeff = 0.995f);
    void updateEQ (float rumbleHP, float subFreq, float subGain,
                   float punchFreq, float punchGain, float airFreq, float airGain,
                   float fundamentalHz);
    float processEQSample (float x);
    void updateSubOscillator (float tunedFundamental, float decayMs, float detuneCents);
    void updateBodyOscillator (float tunedFundamental, float tuning, float decayMs, float attackMs);
    float processSub (float subLevel, int subShape);
    float processBody (float bodyLevel, float pitchRatio);
    float processDrive (float x, float amount, float mix, int character);
    void updateRoom (float level, float decayMs, float rumbleTone, float bpm);
    StereoMix processRoomRumble (const StereoMix& in);
    void processOutputStage (float& l, float& r, float gainDb, float panL, float panR);
};
