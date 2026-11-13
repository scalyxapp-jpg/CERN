#include "PresetThumbnails.h"
#include "GUI/Theme/CERNLookAndFeel.h"

//==============================================================================
PresetThumbnailCache::PresetThumbnailCache()
    : Thread ("PresetThumbnails")
{
}

PresetThumbnailCache::~PresetThumbnailCache()
{
    stopThread (3000);
}

void PresetThumbnailCache::startRendering()
{
    if (started)
        return;

    started = true;

    const int numPresets = [&]()
    {
        LabProcessor dummy;
        juce::AudioProcessorValueTreeState apvts (dummy, nullptr, "Params", createLayout());
        KickSynthesisEngine engine (apvts);
        PresetManager presets (apvts, engine.getPatchGraph(), engine.getParameterPatchGraph());
        return presets.getFactoryPresetNames().size();
    }();

    const size_t n = (size_t) numPresets + 1;

    {
        const juce::SpinLock::ScopedLockType sl (dataLock);
        curves.assign (n, {});
        ready.assign (n, (char) 0);
    }

    startThread (juce::Thread::Priority::low);
}

bool PresetThumbnailCache::isReady (int thumbnailIndex) const
{
    const juce::SpinLock::ScopedLockType sl (dataLock);

    if (thumbnailIndex < 0 || thumbnailIndex >= (int) ready.size())
        return false;

    return ready[(size_t) thumbnailIndex] != 0;
}

std::vector<float> PresetThumbnailCache::getCurve (int thumbnailIndex) const
{
    const juce::SpinLock::ScopedLockType sl (dataLock);

    if (thumbnailIndex < 0 || thumbnailIndex >= (int) ready.size())
        return {};

    if (ready[(size_t) thumbnailIndex] == 0)
        return {};

    return curves[(size_t) thumbnailIndex];
}

void PresetThumbnailCache::run()
{
    LabProcessor dummy;
    juce::AudioProcessorValueTreeState apvts (dummy, nullptr, "Params", createLayout());
    ParameterSmoothing smoothing (apvts);
    KickSynthesisEngine engine (apvts);
    PresetManager presets (apvts, engine.getPatchGraph(), engine.getParameterPatchGraph());

    constexpr double sampleRate = 48000.0;
    constexpr int blockSize = 512;
    constexpr int totalSamples = 28672; // 56 Bloecke = 0,597 s, Vielfaches von 512

    smoothing.prepare (sampleRate);
    smoothing.setBlockSize (blockSize);
    engine.prepare (sampleRate, blockSize);
    engine.setTailLength (2.0f);

    for (int index = 0; index <= presets.getFactoryPresetNames().size(); ++index)
    {
        if (threadShouldExit())
            return;

        const bool wantsInit = (index == initThumbnailIndex);

        if (wantsInit)
        {
            // Init-Stand rendern, damit auch "NO PRESET" eine Vorschau bekommt.
            if (auto* p = apvts.getParameter (ParameterIDs::layeringEnabled))
                p->setValueNotifyingHost (0.0f);
        }
        else if (! presets.loadFactory (index - 1))
        {
            continue;
        }

        for (int block = 0; block < 30; ++block)
            smoothing.update();

        juce::AudioBuffer<float> buffer (2, totalSamples);
        buffer.clear();
        juce::AudioBuffer<float> block (2, blockSize);
        double nextTriggerSample = 0.0;

        for (int blockStart = 0; blockStart < totalSamples; blockStart += blockSize)
        {
            if (threadShouldExit())
                return;

            block.clear();
            smoothing.update();

            while (nextTriggerSample < (double) (blockStart + blockSize)
                   && nextTriggerSample < (double) totalSamples)
            {
                const int offset = juce::jmax (0, (int) std::lround (nextTriggerSample) - blockStart);
                engine.enqueueTrigger (1.0f, 0.0f, offset);
                nextTriggerSample += 24000.0; // 1/4 bei 120 BPM
            }

            engine.render (block, blockSize, smoothing);
            buffer.copyFrom (0, blockStart, block, 0, 0, blockSize);
            buffer.copyFrom (1, blockStart, block, 1, 0, blockSize);
        }

        // 64 Punkte auf den Preset-Peak normieren.
        std::vector<float> curve ((size_t) curvePoints, 0.0f);
        float maxAbs = 1.0e-6f;

        for (int sample = 0; sample < totalSamples; ++sample)
            maxAbs = juce::jmax (maxAbs, std::abs (buffer.getSample (0, sample)),
                                 std::abs (buffer.getSample (1, sample)));

        const int step = totalSamples / curvePoints;

        for (int point = 0; point < curvePoints; ++point)
        {
            float peak = 0.0f;

            for (int sample = point * step; sample < (point + 1) * step; ++sample)
                peak = juce::jmax (peak, std::abs (buffer.getSample (0, sample)),
                                   std::abs (buffer.getSample (1, sample)));

            curve[(size_t) point] = peak / maxAbs;
        }

        {
            const juce::SpinLock::ScopedLockType sl (dataLock);
            curves[(size_t) index] = curve;
            ready[(size_t) index] = (char) 1;
        }
    }
}

//==============================================================================
PresetPreview::PresetPreview()
{
    setTitle ("Preset preview");
    setDescription ("Waveform of the currently selected factory preset");
    setTooltip ("Waveform of the selected preset (rendered offline in the background)");
}

void PresetPreview::setCurve (const std::vector<float>& newCurve)
{
    if (newCurve == curve)
        return;

    curve = newCurve;
    repaint();
}

void PresetPreview::paint (juce::Graphics& g)
{
    const juce::Rectangle<float> bounds (0.0f, 0.0f, (float) getWidth(), (float) getHeight());

    CERNLookAndFeel::drawWell (g, bounds, 3.0f);

    auto area = bounds.reduced (3.0f, 3.0f);

    if (curve.size() < 2)
    {
        g.setColour (CERNLookAndFeel::textColour.withAlpha (0.35f));
        g.setFont (CERNLookAndFeel::getLabelFont().withHeight (7.0f));
        g.drawText ("PRESET", area, juce::Justification::centred, false);
        return;
    }

    const float centreY = area.getCentreY();
    const float amplitude = area.getHeight() * 0.46f;

    juce::Path path;
    path.startNewSubPath (area.getX(), centreY);

    for (size_t i = 0; i < curve.size(); ++i)
    {
        const float x = area.getX() + area.getWidth() * (float) i / (float) (curve.size() - 1);
        path.lineTo (x, centreY - curve[i] * amplitude);
    }

    g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.9f));
    g.strokePath (path, juce::PathStrokeType (1.3f,
                                              juce::PathStrokeType::curved,
                                              juce::PathStrokeType::rounded));
}
