#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <vector>
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_core/juce_core.h>

//==============================================================================
/** Kurzzeit-LUFS nach ITU-R BS.1770 (K-Gewichtung, 400-ms-Fenster).

    Header-only, damit die Abnahmetests (SND-006) die Messung ohne Plugin-Instanz
    verwenden koennen. Die Koeffizienten folgen der Referenzimplementierung
    (High-Shelf + High-Pass, gueltig fuer beliebige Sampleraten). Der Audio-Pfad
    allokiert nichts: der Fenster-Ringpuffer wird in prepare() angelegt.
 */
class LoudnessMeter
{
public:
    static constexpr double shortTermWindowSeconds = 0.4;
    static constexpr float  silenceLufs = -100.0f;

    /** Meldet die Samplerate an und legt das 400-ms-Fenster an (Message-Thread). */
    void prepare (double newSampleRate)
    {
        sampleRate = newSampleRate > 0.0 ? newSampleRate : 48000.0;

        shelf.setHighShelf (sampleRate);
        highPass.setHighPass (sampleRate);

        windowLength = juce::jmax (1, juce::roundToInt (shortTermWindowSeconds * sampleRate));
        window.assign ((size_t) windowLength, 0.0f);

        reset();
    }

    void reset() noexcept
    {
        shelf.reset();
        highPass.reset();
        std::fill (window.begin(), window.end(), 0.0f);
        writePos = 0;
        windowSum = 0.0;
        shortTermLufs.store (silenceLufs, std::memory_order_relaxed);
    }

    /** Audio-Thread: K-gewichtet den Block und aktualisiert den Kurzzeitwert. */
    void processBlock (const juce::AudioBuffer<float>& buffer) noexcept
    {
        const int numSamples  = buffer.getNumSamples();
        const int numChannels = juce::jmin (2, buffer.getNumChannels());

        if (numSamples <= 0 || numChannels <= 0 || window.empty())
            return;

        const float* left  = buffer.getReadPointer (0);
        const float* right = numChannels > 1 ? buffer.getReadPointer (1) : left;

        for (int i = 0; i < numSamples; ++i)
        {
            float sumOfSquares = 0.0f;

            for (int channel = 0; channel < numChannels; ++channel)
            {
                const float x = (channel == 0 ? left[i] : right[i]);
                const float weighted = highPass.process (shelf.process (x, channel), channel);
                sumOfSquares += weighted * weighted;
            }

            // Gleitende Summe: aeltester Wert verlaesst das Fenster, neuer kommt hinzu.
            windowSum += (double) sumOfSquares - (double) window[(size_t) writePos];
            window[(size_t) writePos] = sumOfSquares;

            if (++writePos >= windowLength)
                writePos = 0;
        }

        shortTermLufs.store (lufsFromMeanSquare (windowSum / (double) windowLength),
                             std::memory_order_relaxed);
    }

    /** Letzter Kurzzeitwert in LUFS (thread-safe, -inf wird auf -100 geklemmt). */
    float getShortTermLufs() const noexcept
    {
        return shortTermLufs.load (std::memory_order_relaxed);
    }

    //==============================================================================
    // SND-006: Offline-Messung (kein Plugin-State, keine Allokation im Audio-Pfad).

    /** LUFS der letzten `seconds` Sekunden des Buffers (beide Kanaele, K-gewichtet). */
    static float measureLufsLastSeconds (const juce::AudioBuffer<float>& buffer,
                                         double sampleRateToUse,
                                         double seconds)
    {
        const int totalSamples = buffer.getNumSamples();
        const int numChannels  = juce::jmin (2, buffer.getNumChannels());

        if (totalSamples <= 0 || numChannels <= 0 || sampleRateToUse <= 0.0 || seconds <= 0.0)
            return silenceLufs;

        const int windowSamples = juce::jlimit (1, totalSamples,
                                                juce::roundToInt (seconds * sampleRateToUse));

        Biquad offlineShelf;
        Biquad offlineHighPass;
        offlineShelf.setHighShelf (sampleRateToUse);
        offlineHighPass.setHighPass (sampleRateToUse);

        double sumOfSquares = 0.0;

        for (int i = totalSamples - windowSamples; i < totalSamples; ++i)
        {
            for (int channel = 0; channel < numChannels; ++channel)
            {
                const float weighted = offlineHighPass.process (
                    offlineShelf.process (buffer.getReadPointer (channel)[i], channel), channel);
                sumOfSquares += (double) weighted * (double) weighted;
            }
        }

        return lufsFromMeanSquare (sumOfSquares / (double) windowSamples);
    }

    /** LUFS des gesamten Buffers (fuer die Preset-Streuung in SND-006). */
    static float measureLufs (const juce::AudioBuffer<float>& buffer, double sampleRateToUse)
    {
        if (sampleRateToUse <= 0.0)
            return silenceLufs;

        return measureLufsLastSeconds (buffer, sampleRateToUse,
                                       (double) buffer.getNumSamples() / sampleRateToUse);
    }

private:
    //==============================================================================
    /** Biquad in transponierter Direct-Form-II, ein Zustand pro Kanal (max. 2). */
    struct Biquad
    {
        void setHighShelf (double sr) noexcept
        {
            constexpr double shelfGainDb = 3.999843853973347;
            constexpr double shelfQ      = 0.7071752369554196;
            constexpr double shelfFreq   = 1681.974450955533;

            const double k  = std::tan (juce::MathConstants<double>::pi * shelfFreq / sr);
            const double vh = std::pow (10.0, shelfGainDb / 20.0);
            const double vb = std::pow (vh, 0.4996667741545416);
            const double a0 = 1.0 + k / shelfQ + k * k;

            b0 = (float) ((vh + vb * k / shelfQ + k * k) / a0);
            b1 = (float) (2.0 * (k * k - vh) / a0);
            b2 = (float) ((vh - vb * k / shelfQ + k * k) / a0);
            a1 = (float) (2.0 * (k * k - 1.0) / a0);
            a2 = (float) ((1.0 - k / shelfQ + k * k) / a0);
        }

        void setHighPass (double sr) noexcept
        {
            constexpr double hpFreq = 38.13547087602444;
            constexpr double hpQ    = 0.5003270373238773;

            const double k  = std::tan (juce::MathConstants<double>::pi * hpFreq / sr);
            const double a0 = 1.0 + k / hpQ + k * k;

            b0 = 1.0f;
            b1 = -2.0f;
            b2 = 1.0f;
            a1 = (float) (2.0 * (k * k - 1.0) / a0);
            a2 = (float) ((1.0 - k / hpQ + k * k) / a0);
        }

        void reset() noexcept
        {
            z1[0] = z1[1] = 0.0f;
            z2[0] = z2[1] = 0.0f;
        }

        float process (float x, int channel) noexcept
        {
            const int c = juce::jlimit (0, 1, channel);
            const float y = b0 * x + z1[c];
            z1[c] = b1 * x - a1 * y + z2[c];
            z2[c] = b2 * x - a2 * y;
            return y;
        }

        float b0 { 1.0f }, b1 { 0.0f }, b2 { 0.0f };
        float a1 { 0.0f }, a2 { 0.0f };
        float z1[2] { 0.0f, 0.0f };
        float z2[2] { 0.0f, 0.0f };
    };

    /** BS.1770-Offset -0.691 dB; Floors verhindern -inf bei Stille. */
    static float lufsFromMeanSquare (double meanSquare) noexcept
    {
        return (float) (-0.691 + 10.0 * std::log10 (juce::jmax (meanSquare, 1.0e-10)));
    }

    Biquad shelf;
    Biquad highPass;

    std::vector<float> window;          // Summe der Quadrate pro Sample (beide Kanaele)
    int windowLength { 0 };
    int writePos { 0 };
    double windowSum { 0.0 };
    double sampleRate { 48000.0 };

    std::atomic<float> shortTermLufs { silenceLufs };
};
