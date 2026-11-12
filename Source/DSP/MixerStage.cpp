#include "MixerStage.h"

#include <algorithm>

//==============================================================================
int MixerStage::channelForModule (int moduleIndex) noexcept
{
    // Reihenfolge = ModuleIndex (PatchGraph.h): Membrane, Tube, Sub, Click,
    // Drive, Eq, Compressor, Room, Higgs, Reverb, Output, MaterialRoom.
    // Body hat im Graph keinen eigenen Jack (haengt am Membrane-Bus) und
    // bekommt deshalb keinen eigenen Kanal - es laeuft ueber den
    // Membrane-Kanal (siehe Engine).
    switch (moduleIndex)
    {
        case 0:  return (int) Channel::Membrane;   // Membrane
        case 1:  return (int) Channel::Tube;       // Tube
        case 2:  return (int) Channel::Sub;        // Sub
        case 3:  return (int) Channel::Click;      // Click
        case 4:  return (int) Channel::Drive;      // Drive
        case 5:  return (int) Channel::Eq;         // Eq
        case 6:  return (int) Channel::Compressor; // Compressor
        case 7:  return (int) Channel::Room;       // Room
        case 8:  return (int) Channel::Higgs;      // Higgs
        case 9:  return (int) Channel::Reverb;     // Reverb
        case 10: return (int) Channel::Output;     // OutputModule
        case 11: return (int) Channel::MaterialRoom;
        default: return -1;
    }
    // Hinweis: Body hat im Graph keinen eigenen Jack und laeuft ueber den
    // Membrane-Kanal (siehe Engine: processBody -> Channel::Body).
}

const char* MixerStage::channelName (int channel) noexcept
{
    switch (channel)
    {
        case (int) Channel::Membrane:    return "MEMBRANE";
        case (int) Channel::Body:        return "BODY";
        case (int) Channel::Tube:        return "TUBE";
        case (int) Channel::Sub:         return "SUB";
        case (int) Channel::Click:       return "CLICK";
        case (int) Channel::Drive:       return "DRIVE";
        case (int) Channel::Eq:          return "EQ";
        case (int) Channel::Compressor:  return "COMP";
        case (int) Channel::Room:        return "ROOM";
        case (int) Channel::Higgs:       return "HIGGS";
        case (int) Channel::Reverb:      return "REVERB";
        case (int) Channel::MaterialRoom:return "MAT ROOM";
        case (int) Channel::Output:      return "OUTPUT";
        default:                         return "CH";
    }
}

int MixerStage::sidechainSource (int channel) noexcept
{
    if (channel < 0 || channel >= numChannels)
        return -1;

    return sidechainSourceTable[channel];
}

//==============================================================================
void MixerStage::prepare (double newSampleRate)
{
    sampleRate = (float) juce::jmax (1.0, newSampleRate);

    // Attack ~1 ms, Release ~90 ms (Peak-Follower).
    attackCoeff  = std::exp (-1.0f / (0.001f * (float) sampleRate));
    releaseCoeff = std::exp (-1.0f / (0.090f * (float) sampleRate));

    // Meter-Ballistik: ~20 dB/s Fallback (pro Block naeherungsweise konstant).
    const float blockSeconds = 512.0f / (float) sampleRate;
    peakDecayPerBlock = 20.0f * blockSeconds;

    reset();
}

void MixerStage::reset()
{
    for (auto& ch : channels)
    {
        ch.stateL.reset();
        ch.stateR.reset();
        ch.envelope = 0.0f;
        ch.peakDb = -120.0f;
    }
}

//==============================================================================
void MixerStage::updateParameters (ParameterSmoothing& smoothing)
{
    // Indizes der Mixer-Parameter einmalig ermitteln (String-Lookups nur beim
    // ersten Block; danach nur noch Indexzugriffe im Audio-Thread).
    if (! paramIndicesResolved)
    {
        const auto& table = ParameterIDs::mixerParamIdTable();

        for (size_t i = 0; i < table.size(); ++i)
            paramIndices[i] = smoothing.indexOf (table[i].c_str());

        paramIndicesResolved = true;
    }

    sidechainDepth = juce::jlimit (0.0f, 1.0f, smoothing.getById (ParameterIDs::mixerSidechainDepth));

    for (int c = 0; c < numChannels; ++c)
    {
        auto& state = channels[(size_t) c];

        const int gainIdx = paramIndices[(size_t) (c * 6 + 0)];
        const int lowIdx  = paramIndices[(size_t) (c * 6 + 1)];
        const int midLoIdx= paramIndices[(size_t) (c * 6 + 2)];
        const int midIdx  = paramIndices[(size_t) (c * 6 + 3)];
        const int highIdx = paramIndices[(size_t) (c * 6 + 4)];
        const int muteIdx = paramIndices[(size_t) (c * 6 + 5)];

        state.gain = gainIdx >= 0 ? juce::jlimit (0.0f, 2.0f, smoothing.get (gainIdx)) : 1.0f;
        state.mute = muteIdx >= 0 ? smoothing.getUnsmoothed (muteIdx) > 0.5f : false;

        const float lowDb    = lowIdx    >= 0 ? smoothing.get (lowIdx)    : 0.0f;
        const float midLowDb = midLoIdx  >= 0 ? smoothing.get (midLoIdx)  : 0.0f;
        const float midDb    = midIdx    >= 0 ? smoothing.get (midIdx)    : 0.0f;
        const float highDb   = highIdx   >= 0 ? smoothing.get (highIdx)   : 0.0f;

        updateBandCoefficients (state.coeffs, sampleRate, lowDb, midLowDb, midDb, highDb);
    }
}

//==============================================================================
void MixerStage::updateBandCoefficients (EqCoeffs& coeffs, double sr,
                                          float lowDb, float midLowDb, float midDb, float highDb) const
{
    const auto setPeak = [&sr] (BandCoeffs& c, double freq, double q, double gainDb)
    {
        const double A     = std::pow (10.0, gainDb / 40.0);
        const double w0    = 2.0 * juce::MathConstants<double>::pi * freq / sr;
        const double alpha = std::sin (w0) / (2.0 * q);
        const double cw    = std::cos (w0);

        // RBJ Peaking-EQ: a2 = 1 - alpha/A (NICHT 1/A) - sonst entsteht
        // ein steiler Low-Cut statt eines neutralen 0-dB-Filters.
        const double a0 = 1.0 + alpha / A;

        c.b0 = (1.0 + alpha * A) / a0;
        c.b1 = -2.0 * cw / a0;
        c.b2 = (1.0 - alpha * A) / a0;
        c.a1 = -2.0 * cw / a0;
        c.a2 = (1.0 - alpha / A) / a0;
    };

    // 0: Low-Shelf 120 Hz, 1: Peak 400 Hz, 2: Peak 1.6 kHz, 3: High-Shelf 6 kHz.
    {
        auto& c = coeffs.bands[0];
        const double lowA = std::pow (10.0, (double) lowDb / 40.0);
        const double w0    = 2.0 * juce::MathConstants<double>::pi * 120.0 / sr;
        const double alpha = std::sin (w0) * 0.5 * std::sqrt (2.0);
        const double cw    = std::cos (w0);
        const double sqA   = std::sqrt (lowA);

        const double a0 = (lowA + 1.0) + (lowA - 1.0) * cw + 2.0 * sqA * alpha;

        c.b0 = lowA * ((lowA + 1.0) - (lowA - 1.0) * cw + 2.0 * sqA * alpha) / a0;
        c.b1 = 2.0 * lowA * ((lowA - 1.0) - (lowA + 1.0) * cw) / a0;
        c.b2 = lowA * ((lowA + 1.0) - (lowA - 1.0) * cw - 2.0 * sqA * alpha) / a0;
        c.a1 = -2.0 * ((lowA - 1.0) + (lowA + 1.0) * cw) / a0;
        c.a2 = ((lowA + 1.0) + (lowA - 1.0) * cw - 2.0 * sqA * alpha) / a0;
    }

    setPeak (coeffs.bands[1], 400.0, 0.9, (double) midLowDb);
    setPeak (coeffs.bands[2], 1600.0, 0.9, (double) midDb);

    {
        auto& c = coeffs.bands[3];
        const double highA = std::pow (10.0, (double) highDb / 40.0);
        const double w0    = 2.0 * juce::MathConstants<double>::pi * 6000.0 / sr;
        const double alpha = std::sin (w0) * 0.5 * std::sqrt (2.0);
        const double cw    = std::cos (w0);
        const double sqA   = std::sqrt (highA);

        const double a0 = (highA + 1.0) - (highA - 1.0) * cw + 2.0 * sqA * alpha;

        c.b0 = highA * ((highA + 1.0) + (highA - 1.0) * cw + 2.0 * sqA * alpha) / a0;
        c.b1 = -2.0 * highA * ((highA - 1.0) + (highA + 1.0) * cw) / a0;
        c.b2 = highA * ((highA + 1.0) + (highA - 1.0) * cw - 2.0 * sqA * alpha) / a0;
        c.a1 = 2.0 * ((highA - 1.0) - (highA + 1.0) * cw) / a0;
        c.a2 = ((highA + 1.0) - (highA - 1.0) * cw - 2.0 * sqA * alpha) / a0;
    }
}

//==============================================================================
float MixerStage::sidechainGain (int channel) const noexcept
{
    if (sidechainDepth <= 0.0f)
        return 1.0f;

    const int source = sidechainSourceTable[channel];

    if (source < 0)
        return 1.0f;

    const float env = channels[(size_t) source].envelope;

    if (env <= 0.0f)
        return 1.0f;

    const float depthDb = sidechainDepthDb[channel] * sidechainDepth;
    const float norm = juce::jlimit (0.0f, 1.0f, env / sidechainReferenceLevel);

    return juce::Decibels::decibelsToGain (-depthDb * norm);
}

//==============================================================================
float MixerStage::processMono (int channel, float x)
{
    if (channel < 0 || channel >= numChannels)
        return x;

    auto& state = channels[(size_t) channel];

    if (state.mute)
    {
        feedEnvelope (channel, 0.0f);
        decayPeak (state);
        return 0.0f;
    }

    float y = state.stateL.process (state.coeffs, x);
    y *= state.gain * sidechainGain (channel);

    // Huelle erst NACH Fader/Mute bilden, damit ein gemuteter/stummer
    // Quellkanal nicht weiter duckt.
    feedEnvelope (channel, y);

    const float peak = std::abs (y);
    state.peakDb = juce::jmax (peakDbFromLinear (peak), state.peakDb - peakDecayPerBlock);

    return y;
}

void MixerStage::feedEnvelope (int channel, float value) noexcept
{
    auto& state = channels[(size_t) channel];
    const float a = std::abs (value);
    state.envelope = a > state.envelope
        ? attackCoeff * state.envelope + (1.0f - attackCoeff) * a
        : releaseCoeff * state.envelope + (1.0f - releaseCoeff) * a;
}

void MixerStage::decayPeak (ChannelState& state) noexcept
{
    state.peakDb = juce::jmax (-120.0f, state.peakDb - peakDecayPerBlock);
}

float MixerStage::peakDbFromLinear (float linear) noexcept
{
    return 20.0f * std::log10 (juce::jmax (1.0e-6f, linear));
}

float MixerStage::getDuckDb (int channel) const noexcept
{
    const float g = sidechainGain (juce::jlimit (0, numChannels - 1, channel));
    return 20.0f * std::log10 (juce::jmax (1.0e-6f, g));
}

void MixerStage::processStereo (int channel, float& l, float& r)
{
    if (channel < 0 || channel >= numChannels)
        return;

    auto& state = channels[(size_t) channel];

    if (state.mute)
    {
        feedEnvelope (channel, 0.0f);
        decayPeak (state);
        l = 0.0f;
        r = 0.0f;
        return;
    }

    l = state.stateL.process (state.coeffs, l);
    r = state.stateR.process (state.coeffs, r);

    const float g = state.gain * sidechainGain (channel);
    l *= g;
    r *= g;

    feedEnvelope (channel, juce::jmax (std::abs (l), std::abs (r)));

    const float peak = juce::jmax (std::abs (l), std::abs (r));
    state.peakDb = juce::jmax (peakDbFromLinear (peak), state.peakDb - peakDecayPerBlock);
}
