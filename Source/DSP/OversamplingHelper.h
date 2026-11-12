#pragma once

#include <array>
#include <cmath>
#include <memory>
#include <utility>
#include <juce_dsp/juce_dsp.h>

//==============================================================================
/** 2x-Oversampling-Wrapper für nichtlineare Stufen (SND-179).

    Kapselt juce::dsp::Oversampling<float> mit Faktor 2 (Polyphase-IIR,
    maximale Qualität), damit Drive und Clip ihre Kennlinien bei 2x
    Samplerate berechnen können (weniger Aliasing).

    Benutzung (Funktor arbeitet auf dem os-Signal, gleiche Kanäle):

        oversamplingHelper.prepare (sampleRate, maxBlockSize, 2);

        oversamplingHelper.process (buffer, [&] (juce::dsp::AudioBlock<float>& block)
        {
            // block.getNumSamples() == 2 * buffer.getNumSamples()
            // block.getSampleRate()  == 2 * sampleRate
            for (size_t ch = 0; ch < block.getNumChannels(); ++ch)
            {
                auto* data = block.getChannelPointer (ch);
                for (size_t i = 0; i < block.getNumSamples(); ++i)
                    data[i] = std::tanh (data[i] * drive);
            }
        });

    Der Funktor wird mit einem AudioBlock<float> aufgerufen und muss in-place
    arbeiten. Ist Oversampling deaktiviert (oder der Block größer als in
    prepare() angekündigt), wird der Funktor ohne Oversampling auf dem
    Basis-Signal ausgeführt - Drive/Clip bleibt also immer aktiv.

    Latenz: durch processSamplesUp/Down entsteht eine Verzögerung von
    getLatencyInSamples() Samples. initProcessing() allokiert in prepare();
    process() selbst allokiert nicht. Header-only.
*/
class OversamplingHelper
{
public:
    OversamplingHelper() = default;

    /** maxBlockSizeIn = maximale Blockgröße in Samples bei Basis-Samplerate. */
    void prepare (double newSampleRate, int maxBlockSizeIn, int numChannelsIn = 2)
    {
        sampleRate = juce::jmax (1.0, newSampleRate);
        maxBlockSize = juce::jmax (1, maxBlockSizeIn);
        numChannels = juce::jlimit (1, 8, numChannelsIn);

        oversampler = std::make_unique<juce::dsp::Oversampling<float>> (
            (size_t) this->numChannels,
            1, // 2^1 = 2x
            juce::dsp::Oversampling<float>::filterHalfBandPolyphaseIIR,
            true,  // maximale Qualität
            true); // ganzzahlige Latenz

        oversampler->initProcessing ((size_t) this->maxBlockSize);
        oversampler->reset();
    }

    void reset()
    {
        if (oversampler != nullptr)
            oversampler->reset();
    }

    void setEnabled (bool shouldBeEnabled) { enabled = shouldBeEnabled; }
    bool isEnabled() const { return enabled; }

    /** Latenz in Samples bei Basis-Samplerate (0, wenn nicht vorbereitet). */
    int getLatencyInSamples() const
    {
        if (oversampler == nullptr)
            return 0;

        return (int) std::lround ((double) oversampler->getLatencyInSamples());
    }

    /** Faktor 2, wenn vorbereitet und aktiv, sonst 1. */
    int getActiveFactor() const { return (enabled && oversampler != nullptr) ? 2 : 1; }

    /** Samplerate, mit der der Funktor rechnen muss. */
    double getProcessingSampleRate() const
    {
        return sampleRate * (double) getActiveFactor();
    }

    /** Verarbeitet buffer in-place: processOversampled (AudioBlock<float>&). */
    template <typename ProcessFn>
    void process (juce::AudioBuffer<float>& buffer, ProcessFn&& processOversampled)
    {
        const int numSamples = buffer.getNumSamples();
        const int bufferChannels = buffer.getNumChannels();

        if (numSamples <= 0 || bufferChannels <= 0)
            return;

        juce::dsp::AudioBlock<float> block (buffer);

        const bool canOversample = enabled
                                && oversampler != nullptr
                                && numSamples <= maxBlockSize
                                && bufferChannels <= numChannels;

        if (! canOversample)
        {
            processOversampled (block); // Basis-Rate-Fallback
            return;
        }

        auto oversampledBlock = oversampler->processSamplesUp (block);
        processOversampled (oversampledBlock);
        oversampler->processSamplesDown (block);
    }

private:
    double sampleRate { 48000.0 };
    int maxBlockSize { 0 };
    int numChannels { 2 };
    bool enabled { true };
    std::unique_ptr<juce::dsp::Oversampling<float>> oversampler;
};

//==============================================================================
/** Streaming-2x-Halfband für blockweise nichtlineare Stufen (SND-179, eigener Pfad).

    Mono-In-Place-Pfad mit kaiser-ähnlicher (Hamming) 31-Tap-Halfband-FIR,
    Cutoff bei pi/2 (halbe Bandbreite), alle geraden Taps ausser der Mitte
    verschwinden dadurch automatisch. Die Koeffizienten werden in prepare()
    normalisiert (DC-Gain = 1); der Upsampler arbeitet auf dem zero-stuffed
    Signal und hebt mit Faktor 2 nach, damit das Ergebnis pegelgleich ist.

    Latenz: (numTaps - 1) / 2 Samples pro Richtung bei 2x-Rate, also exakt
    (numTaps - 1) / 2 = 15 Samples bei Basis-Rate insgesamt. L/R-identisch.

    Geplante Eigenausbau-Latenz des Drive-Pfads: der Low-Zweig (150 Hz,
    clean) muss um dieselben 15 Samples verzögert werden, sonst kämmt die
    Band-Split-Summierung (siehe KickSynthesisEngine).

    Aliasing: Stopband des Hamming-Fensters ≈ -53 dB; Alias-Images landen
    typischerweise > 40 dB unter dem Nutzsignal (statt ~-3 dB wie bei naiver
    Halbschritt-Auswertung). Header-only, keine Allokationen.
*/
class StreamingTwoX
{
public:
    static constexpr int numTaps = 31;
    static constexpr int totalLatencySamples = (numTaps - 1) / 2; ///< bei Basis-Rate

    StreamingTwoX() = default;

    /** Koeffizienten entwerfen (unabhängig von der Samplerate). */
    void prepare()
    {
        const int centre = numTaps / 2;
        double sum = 0.0;

        for (int k = 0; k < numTaps; ++k)
        {
            const int offset = k - centre;
            const double fc = 0.5; // normiert auf pi/2 (Nyquist = 1)
            double h;

            if (offset == 0)
                h = fc;
            else
                h = std::sin (juce::MathConstants<double>::pi * fc * (double) offset)
                  / (juce::MathConstants<double>::pi * (double) offset);

            // Hamming-Fenster zur Stopband-Aufweitung
            const double w = 0.54 - 0.46 * std::cos (2.0 * juce::MathConstants<double>::pi
                                                     * (double) k / (double) (numTaps - 1));
            coeffs[(size_t) k] = (float) (h * w);
            sum += (double) coeffs[(size_t) k];
        }

        for (auto& c : coeffs)
            c = (float) ((double) c / sum); // DC-Gain = 1

        reset();
    }

    void reset()
    {
        upHist.fill (0.0f);
        downHist.fill (0.0f);
        upPos = 0;
        downPos = 0;
    }

    /** Ein Basis-Sample -> zwei 2x-Samples (zero-stuff + Halfband, DC x2). */
    std::pair<float,float> upsample (float x)
    {
        upHist[(size_t) upPos] = x;
        upPos = (upPos + 1) % numTaps;
        const float y0 = dotBank (upHist, upPos) * 2.0f;

        upHist[(size_t) upPos] = 0.0f; // Zero-Stuff
        upPos = (upPos + 1) % numTaps;
        const float y1 = dotBank (upHist, upPos) * 2.0f;

        return { y0, y1 };
    }

    /** Zwei 2x-Samples -> ein Basis-Sample (Halfband + Decimate). */
    float downsample (float y0, float y1)
    {
        downHist[(size_t) downPos] = y0;
        downPos = (downPos + 1) % numTaps;
        (void) dotBank (downHist, downPos); // gerader Takt: verworfen

        downHist[(size_t) downPos] = y1;
        downPos = (downPos + 1) % numTaps;
        return dotBank (downHist, downPos);
    }

private:
    float dotBank (const std::array<float, numTaps>& hist, int writePos) const
    {
        float acc = 0.0f;

        for (int k = 0; k < numTaps; ++k)
        {
            // Ältester Eintrag steht genau hinter writePos.
            int idx = writePos - 1 - k;
            while (idx < 0)
                idx += numTaps;
            acc += coeffs[(size_t) k] * hist[(size_t) idx];
        }

        return acc;
    }

    std::array<float, numTaps> coeffs {};
    std::array<float, numTaps> upHist {};
    std::array<float, numTaps> downHist {};
    int upPos { 0 };
    int downPos { 0 };
};
