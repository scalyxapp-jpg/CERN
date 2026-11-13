/**
    C.E.R.N – Lab-Renderer (Offline-Auswertung).

    Rendert die Factory-Presets als WAV (A/B hörbar) und misst dabei die
    Kennzahlen, die für Gain-Staging, PA und Mono-Kompatibilität wichtig sind:

        ./CERNLab <zielOrdner> [layeringAmountProzent]

    Pro Preset entstehen zwei Dateien:
        <i>_<name>.wav        – Original (Layering aus)
        <i>_<name>_layer.wav  – mit Layering (Amount laut Argument, Default 100 %)

    Ausgabe auf stdout: LUFS (stereo), LUFS (Mono-Summe), Peak, Crest-Faktor,
    Differenz der beiden Varianten. Werte dienen der Beurteilung von
    "voller statt lauter" und der Mono-Kompatibilität auf PA.

    Kein GUI, kein Audio-Device – reiner Offline-Render.
*/
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_audio_processors/juce_audio_processors.h>

#include "DSP/KickSynthesisEngine.h"
#include "DSP/LoudnessMeter.h"
#include "Parameters/ParameterIDs.h"
#include "Parameters/ParameterLayout.h"
#include "Parameters/ParameterSmoothing.h"
#include "Presets/PresetManager.h"

#include <cmath>
#include <cstdio>
#include <memory>

//==============================================================================
namespace
{
    /** Minimaler AudioProcessor-Placeholder (wie in den Tests). */
    class LabProcessor : public juce::AudioProcessor
    {
    public:
        LabProcessor()
            : AudioProcessor (BusesProperties().withOutput ("Out",
                                juce::AudioChannelSet::stereo(), true)) {}

        const juce::String getName() const override { return "CernLab"; }
        void prepareToPlay (double, int) override {}
        void releaseResources() override {}
        bool isBusesLayoutSupported (const BusesLayout&) const override { return true; }
        void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override {}
        juce::AudioProcessorEditor* createEditor() override { return nullptr; }
        bool hasEditor() const override { return false; }
        bool acceptsMidi() const override { return false; }
        bool producesMidi() const override { return false; }
        double getTailLengthSeconds() const override { return 0.0; }
        int getNumPrograms() override { return 1; }
        int getCurrentProgram() override { return 0; }
        void setCurrentProgram (int) override {}
        const juce::String getProgramName (int) override { return {}; }
        void changeProgramName (int, const juce::String&) override {}
        void getStateInformation (juce::MemoryBlock&) override {}
        void setStateInformation (const void*, int) override {}
    };

    struct Metrics
    {
        float lufsStereo { -100.0f };
        float lufsMono   { -100.0f };
        float peak       { 0.0f };
        float rms        { 0.0f };
        float crestDb    { 0.0f };
        bool  valid      { false };
    };

    /** Rendert einen Preset offline und schreibt die WAV. */
    bool renderPreset (int presetIndex,
                       bool layering,
                       float amountPct,
                       const juce::File& target,
                       double seconds,
                       float bpm,
                       Metrics& out)
    {
        LabProcessor dummy;
        juce::AudioProcessorValueTreeState apvts (dummy, nullptr, "Params", createLayout());
        ParameterSmoothing smoothing (apvts);
        KickSynthesisEngine engine (apvts);
        PresetManager presets (apvts, engine.getPatchGraph(), engine.getParameterPatchGraph());

        if (! presets.loadFactory (presetIndex))
            return false;

        if (auto* parameter = apvts.getParameter (ParameterIDs::layeringEnabled))
            parameter->setValueNotifyingHost (layering ? 1.0f : 0.0f);

        if (auto* parameter = apvts.getParameter (ParameterIDs::layeringAmount))
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (amountPct));

        constexpr double sampleRate = 48000.0;
        constexpr int blockSize = 512;

        smoothing.prepare (sampleRate);
        smoothing.setBlockSize (blockSize);
        engine.prepare (sampleRate, blockSize);
        engine.setTailLength (2.0f);

        for (int block = 0; block < 20; ++block)
            smoothing.update();

        const int totalSamples = juce::jmax (1, (int) std::floor (seconds * sampleRate))
                                 & ~(blockSize - 1);

        juce::AudioBuffer<float> bounce (2, totalSamples);
        bounce.clear();
        juce::AudioBuffer<float> block (2, blockSize);

        const double samplesPerBeat = 60.0 / juce::jlimit (20.0f, 300.0f, bpm) * sampleRate;
        double nextTriggerSample = 0.0;

        for (int blockStart = 0; blockStart < totalSamples; blockStart += blockSize)
        {
            block.clear();
            smoothing.update();

            while (nextTriggerSample < (double) (blockStart + blockSize)
                   && nextTriggerSample < (double) totalSamples)
            {
                const int offset = juce::jmax (0, (int) std::lround (nextTriggerSample) - blockStart);
                engine.enqueueTrigger (1.0f, 0.0f, offset);
                nextTriggerSample += samplesPerBeat;
            }

            engine.render (block, blockSize, smoothing);

            for (int ch = 0; ch < 2; ++ch)
                bounce.copyFrom (ch, blockStart, block, ch, 0, blockSize);
        }

        // ---- Kennzahlen ----
        juce::AudioBuffer<float> mono (1, totalSamples);
        for (int i = 0; i < totalSamples; ++i)
            mono.setSample (0, i, 0.5f * (bounce.getSample (0, i) + bounce.getSample (1, i)));

        out.lufsStereo = LoudnessMeter::measureLufs (bounce, sampleRate);
        out.lufsMono   = LoudnessMeter::measureLufs (mono, sampleRate);
        out.peak = bounce.getMagnitude (0, totalSamples);
        out.rms  = bounce.getRMSLevel (0, 0, totalSamples);
        out.crestDb = (out.rms > 1.0e-9f && out.peak > 1.0e-9f)
                        ? juce::Decibels::gainToDecibels (out.peak / out.rms) : 0.0f;
        out.valid = std::isfinite (out.lufsStereo) && std::isfinite (out.lufsMono);

        // ---- WAV schreiben (32 bit float) ----
        juce::WavAudioFormat format;
        std::unique_ptr<juce::OutputStream> stream = std::make_unique<juce::FileOutputStream> (target);

        if (auto* fileStream = dynamic_cast<juce::FileOutputStream*> (stream.get());
            fileStream == nullptr || ! fileStream->openedOk())
            return false;

        const auto options = juce::AudioFormatWriterOptions {}
            .withSampleRate (sampleRate)
            .withNumChannels (2)
            .withBitsPerSample (32)
            .withSampleFormat (juce::AudioFormatWriterOptions::SampleFormat::floatingPoint);

        std::unique_ptr<juce::AudioFormatWriter> writer (format.createWriterFor (stream, options));

        if (writer == nullptr)
            return false;

        writer->writeFromAudioSampleBuffer (bounce, 0, totalSamples);
        return true;
    }
}

//==============================================================================
int main (int argc, char* argv[])
{
    const juce::File outDir (argc > 1 ? juce::String (argv[1]) : juce::String ("/tmp/cern-lab"));
    const float amountPct = (argc > 2 ? juce::String (argv[2]).getFloatValue() : 100.0f);
    // 8 s = 4 Takte bei 120 BPM (identisch zur Messung im Loudness-Spread-Test).
    const double seconds = (argc > 3 ? juce::String (argv[3]).getDoubleValue() : 8.0);

    outDir.createDirectory();

    std::printf ("C.E.R.N Lab-Render -> %s\n", outDir.getFullPathName().toRawUTF8());
    std::printf ("Layering-Amount: %.0f %%, Dauer: %.1f s\n\n", amountPct, seconds);
    std::printf ("%-3s %-24s %8s %8s %7s %7s %8s %8s\n",
                 "#", "Preset", "LUFS", "LUFSmono", "Peak", "Crest", "dLUFS", "dMono");

    LabProcessor dummy;
    juce::AudioProcessorValueTreeState names (dummy, nullptr, "Params", createLayout());
    KickSynthesisEngine nameEngine (names);
    PresetManager nameSource (names, nameEngine.getPatchGraph(), nameEngine.getParameterPatchGraph());

    const juce::StringArray presetNames = nameSource.getFactoryPresetNames();

    for (int preset = 0; preset < presetNames.size(); ++preset)
    {
        const juce::String name = presetNames[preset];
        const juce::String safeName = name.replaceCharacters (" \t", "_");

        const juce::File dryFile   = outDir.getChildFile (juce::String (preset) + "_" + safeName + ".wav");
        const juce::File layerFile = outDir.getChildFile (juce::String (preset) + "_" + safeName + "_layer.wav");

        Metrics dry, layer;
        const bool dryOk = renderPreset (preset, false, amountPct, dryFile, seconds, 120.0f, dry);
        const bool layerOk = renderPreset (preset, true, amountPct, layerFile, seconds, 120.0f, layer);

        if (! dryOk || ! layerOk)
        {
            std::printf ("%-3d %-24s  RENDER FEHLGESCHLAGEN\n", preset, name.toRawUTF8());
            continue;
        }

        std::printf ("%-3d %-24s %8.2f %8.2f %7.3f %7.1f %+8.2f %+8.2f\n",
                     preset, name.toRawUTF8(),
                     dry.lufsStereo, dry.lufsMono, dry.peak, dry.crestDb,
                     layer.lufsStereo - dry.lufsStereo,
                     layer.lufsMono - dry.lufsMono);
    }

    std::printf ("\nFertig.\n");
    return 0;
}
