#pragma once

#include <array>
#include <atomic>
#include <cmath>

#include <juce_audio_processors/juce_audio_processors.h>

#include "Parameters/ParameterIDs.h"
#include "Parameters/ParameterSmoothing.h"

//==============================================================================
/** Mehrkanal-Mixer als letzte Stufe vor dem Output-Modul.

    Jedes klang-erzeugende Modul der Engine ist auf genau einen Mixer-Kanal
    gemappt (Reihenfolge = MixerStage::Channel). Pro Kanal:

      - GAIN   (0..2, 1 = neutral): Lautstärke/Intensität des Moduls im Mix
      - LOW    (20..250 Hz Shelf), MIDLOW (250..1000 Hz Peak),
        MID    (1..4 kHz Peak), HIGH (4..16 kHz Shelf): je -24..+24 dB
      - MUTE: Kanal komplett stumm

    Automatische Sidechain-Beziehungen (fest verdrahtet, Tiefe über
    `mixerSidechainDepth` (0..1) global skalierbar):

      - COMP  duckt ROOM / HIGGS / REVERB / MATERIAL ROOM
        (Kick-Bus -> FX-Nebel: klassisch, haelt den Kick trocken).
        Der Comp-Kanal ist der letzte Kanal VOR den FX und enthaelt bei
        bypassetem Kompressor das trockene Kick-Signal - das Ducking folgt
        bewusst dem Kick, nicht der Kompressor-Gain-Reduktion.

    Kein CLICK -> SUB mehr: der Beater-Transient hat den Sub-Grundton
    moduliert ("Wobbel") und dem Attack Tiefe genommen.

    Das Ducking ist proportional (weiches Kennlinien-Normalisieren):
    `norm = env / (env + sidechainHalfDepthLevel)` - ein lauterer Kick
    duckt tiefer, ein leiser Kick nur wenig. Damit ist die Tiefe nicht
    mehr binaer (env/0.35 geklemmt) und nicht preset-abhaengig.

    Die Kanäle werden im Graph-Routing an der Quelle getrennt: Membran und
    Body werden getrennt gemischt (je eigener Kanal), danach wie bisher
    gemeinsam ueber den Membrane-Jack gefuehrt.
*/
class MixerStage
{
public:
    /** Ein Kanal pro klang-erzeugendem Modul. */
    enum class Channel
    {
        Membrane = 0,
        Body,
        Tube,
        Sub,
        Click,
        Drive,
        Eq,
        Compressor,
        Room,
        Higgs,
        Reverb,
        MaterialRoom,
        Output,
        Count
    };

    static constexpr int numChannels = (int) Channel::Count;

    /** Modulindex (ModuleIndex) -> Kanalindex. -1 = kein Kanal. */
    static int channelForModule (int moduleIndex) noexcept;

    /** Anzeigename des Kanals (GUI). */
    static const char* channelName (int channel) noexcept;

    //==============================================================================
    /** JUCE_DECLARE_NON_COPYABLE loescht den Kopier-Konstruktor und unterdrueckt
        damit den impliziten Default-Konstruktor - deshalb hier explizit. */
    MixerStage() = default;

    void prepare (double sampleRate);
    void reset();

    /** Parameter dieses Blocks uebernehmen (Message-Thread-Werte, geglaettet). */
    void updateParameters (ParameterSmoothing& smoothing);

    /** Mono-Sample eines Kanals verarbeiten. */
    float processMono (int channel, float x);

    /** Stereo-Sample eines Kanals verarbeiten. */
    void processStereo (int channel, float& l, float& r);

    /** Peak des Kanals in dBFS mit Fallback-Ballistik (fuer die GUI-Meter). */
    float getPeakDb (int channel) const noexcept
    {
        return channels[(size_t) juce::jlimit (0, numChannels - 1, channel)]
            .peakDb.load (std::memory_order_relaxed);
    }

    /** Aktuelle Sidechain-Daempfung eines Kanals in dB (0 = kein Ducking). */
    float getDuckDb (int channel) const noexcept
    {
        return channels[(size_t) juce::jlimit (0, numChannels - 1, channel)]
            .duckDb.load (std::memory_order_relaxed);
    }

    /** Sidechain-Quelle eines Kanals (fuer die GUI-Anzeige), -1 = keine. */
    static int sidechainSource (int channel) noexcept;

private:
    //==============================================================================
    /** RBJ-Biquad (Direktform II, transponiert). */
    struct Biquad
    {
        double b0 { 1.0 }, b1 { 0.0 }, b2 { 0.0 }, a1 { 0.0 }, a2 { 0.0 };
        float z1 { 0.0f }, z2 { 0.0f };

        void reset() noexcept { z1 = 0.0f; z2 = 0.0f; }

        float process (float x) noexcept
        {
            const float y = (float) (b0 * x + z1);
            z1 = (float) (b1 * x - a1 * y + z2);
            z2 = (float) (b2 * x - a2 * y);
            return y;
        }
    };

    /** Koeffizienten eines EQ-Bandes (Direktform II, transponiert). */
    struct BandCoeffs
    {
        double b0 { 1.0 }, b1 { 0.0 }, b2 { 0.0 }, a1 { 0.0 }, a2 { 0.0 };
    };

    /** 4-Band-Koeffizienten (Low, MidLow, Mid, High) eines Kanals. */
    struct EqCoeffs
    {
        std::array<BandCoeffs, 4> bands {};

        void reset() noexcept
        {
            for (auto& b : bands)
                b = BandCoeffs {};
        }
    };

    /** 4-Band-Zustand (eigener State pro Kanal/Stereo-Seite). */
    struct BandState
    {
        std::array<Biquad, 4> filters {};

        void reset() noexcept
        {
            for (auto& f : filters)
                f.reset();
        }

        float process (const EqCoeffs& c, float x) noexcept
        {
            auto y = x;

            for (size_t i = 0; i < filters.size(); ++i)
            {
                auto& f = filters[i];
                const auto& cf = c.bands[i];
                const float out = (float) (cf.b0 * y + f.z1);
                f.z1 = (float) (cf.b1 * y - cf.a1 * out + f.z2);
                f.z2 = (float) (cf.b2 * y - cf.a2 * out);
                y = out;
            }

            return y;
        }
    };

    struct ChannelState
    {
        float gain { 1.0f };
        bool mute { false };
        EqCoeffs coeffs {};
        BandState stateL {};
        BandState stateR {};

        /** Zuletzt berechnete Bandwerte (Koeffizienten nur bei Aenderung). */
        std::array<float, 4> lastBandDb { 0.0f, 0.0f, 0.0f, 0.0f };

        /** Alle Baender bei 0 dB -> Filter komplett umgehen (bit-transparent). */
        bool eqBypassed { true };

        /** Hüllkurve (Peak-Follower) fuer Sidechain-Quellen. */
        float envelope { 0.0f };

        /** GUI-Meter/Duck-Werte: atomar, damit der Message-Thread sie
            gefahrlos lesen kann (Audio-Thread schreibt). */
        std::atomic<float> peakDb { -120.0f };
        std::atomic<float> duckDb { 0.0f };
    };

    //==============================================================================
    void updateBandCoefficients (EqCoeffs& coeffs, double sampleRate,
                                 float lowDb, float midLowDb, float midDb, float highDb) const;

    /** Aktuelle Sidechain-Daempfung eines Kanals (Lineare Verstaerkung). */
    float sidechainGain (int channel) const noexcept;

    /** Huelle des Kanals nachfuehren (post-fader). */
    void feedEnvelope (int channel, float value) noexcept;

    void decayPeak (ChannelState& state) noexcept;
    static float peakDbFromLinear (float linear) noexcept;

    float sampleRate { 48000.0 };
    double coeffSampleRate { 0.0 };
    float sidechainDepth { 1.0f };

    std::array<ChannelState, numChannels> channels {};

    /** Gedaempfte Quellkanal-Indizes (Sidechain-Quellen). */
    static constexpr int sidechainSourceTable[numChannels] =
    {
        -1,     // Membrane
        -1,     // Body
        -1,     // Tube
        -1,     // Sub  (frueher: Click -> Sub, entfernt: Grundton-Wobbel)
        -1,     // Click
        -1,     // Drive
        -1,     // Eq
        -1,     // Compressor
        (int) Channel::Compressor, // Room   <- Comp
        (int) Channel::Compressor, // Higgs  <- Comp
        (int) Channel::Compressor, // Reverb <- Comp
        (int) Channel::Compressor, // MaterialRoom <- Comp
        -1      // Output
    };

    /** Maximale Sidechain-Tiefe pro Zielkanal in dB. */
    static constexpr float sidechainDepthDb[numChannels] =
    {
        0.0f, 0.0f, 0.0f,
        0.0f,   // Sub (kein Sidechain-Ziel mehr)
        0.0f, 0.0f, 0.0f, 0.0f,
        5.0f,   // Room <- Comp
        4.0f,   // Higgs <- Comp
        5.0f,   // Reverb <- Comp
        4.5f,   // MaterialRoom <- Comp
        0.0f
    };

    /** Pegel, bei dem ein Ducking-Ziel die halbe Tiefe erreicht.
        Kleiner Wert = frueher volle Tiefe; 0.10 haelt die Kennlinie
        proportional, ohne bei leisen Presets zu schwach zu ducken. */
    static constexpr float sidechainHalfDepthLevel = 0.10f;

    /** Envelope-Anpassung: Attack ~1 ms, Release ~90 ms. */
    float attackCoeff { 0.0f };
    float releaseCoeff { 0.0f };

    /** Meter-Fallback in dB pro Sample (~20 dB/s). */
    float peakDecayPerSample { 0.0004f };

    /** Indizes der Mixer-Parameter in der ParameterSmoothing (einmalig ermittelt). */
    std::array<int, numChannels * 6> paramIndices {};
    bool paramIndicesResolved { false };

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MixerStage)
};
