#pragma once

#include <cmath>
#include <juce_audio_basics/juce_audio_basics.h>

//==============================================================================
/** MIDI-Filter mit Kanal-/Notenbereich und Choke (SND-176).

    Filtert einen eingehenden MidiBuffer:
      - Kanal: 0 = Omni, 1..16 = nur dieser Kanal
      - Notenbereich: Note-Ons außerhalb [lowNote, highNote] werden verworfen
      - Choke: nach noteOn + chokeMs wird automatisch ein Note-Off erzeugt
        (0 ms = Choke aus)

    Note-Offs werden immer durchgelassen (auch außerhalb des Notenbereichs),
    damit ein Notenbereichswechsel keine hängenden Noten erzeugt. System-
    Nachrichten (Status >= 0xF0) passieren ungefiltert.

    process() arbeitet in-place auf dem MidiBuffer und allokiert nach
    prepare() nicht mehr (Arbeitspuffer + festes Choke-Array). Chokes, die
    über das Blockende hinausgehen, werden im Folgeblock ausgegeben; der
    Sample-Zähler läuft dafür kontinuierlich weiter. Header-only.
*/
class MidiFilter
{
public:
    static constexpr int maxPendingChokes = 32;

    MidiFilter() = default;

    void prepare (double newSampleRate)
    {
        sampleRate = juce::jmax (1.0, newSampleRate);
        working.ensureSize (8192);
        updateChokeSamples();
        reset();
    }

    void reset()
    {
        working.clear();
        numPendingChokes = 0;
        sampleCounter = 0;
    }

    /** 0 = Omni, 1..16 = fester Kanal. */
    void setChannel (int newChannel)
    {
        channelFilter = juce::jlimit (0, 16, newChannel);
    }

    int getChannel() const { return channelFilter; }

    /** Notenbereich (aufsteigend, MIDI 0..127). */
    void setNoteRange (int low, int high)
    {
        const int lo = juce::jlimit (0, 127, juce::jmin (low, high));
        const int hi = juce::jlimit (0, 127, juce::jmax (low, high));
        lowNote = lo;
        highNote = hi;
    }

    int getNoteRangeLow() const { return lowNote; }
    int getNoteRangeHigh() const { return highNote; }

    /** Choke-Zeit in ms nach Note-On (0 = aus). */
    void setChokeMs (float ms)
    {
        chokeMs = juce::jlimit (0.0f, 5000.0f, std::isfinite (ms) ? ms : 0.0f);
        updateChokeSamples();
    }

    float getChokeMs() const { return chokeMs; }
    int getNumPendingChokes() const { return numPendingChokes; }

    /** True, wenn die Nachricht den Kanal-/Notenbereich passiert. */
    bool accepts (const juce::MidiMessage& msg) const
    {
        if (msg.getRawDataSize() <= 0)
            return false;

        const auto status = (juce::uint8) msg.getRawData()[0];

        if ((status & 0xF0) == 0xF0) // System Common / Realtime / Meta
            return true;

        if (channelFilter != 0 && msg.getChannel() != channelFilter)
            return false;

        if (msg.isNoteOn (false)) // Velocity 0 zählt als Note-Off
        {
            const int note = msg.getNoteNumber();
            if (note < lowNote || note > highNote)
                return false;
        }

        return true;
    }

    /** Filtert buffer in-place und fügt fällige Choke-Note-Offs ein. */
    void process (juce::MidiBuffer& buffer, int numSamples)
    {
        const auto blockStart = sampleCounter;
        const auto blockEnd = blockStart + (juce::int64) juce::jmax (0, numSamples);

        // Puffergröße sichern (allokiert nur beim Wachsen, nie im Normalfall).
        working.ensureSize ((size_t) (buffer.getNumEvents() + maxPendingChokes + 8));
        working.clear();

        // 1) Fällige Chokes dieses Blocks ausgeben, Rest behalten.
        int kept = 0;

        for (int i = 0; i < numPendingChokes; ++i)
        {
            const auto choke = pending[i];

            if (choke.atSample < blockEnd)
            {
                if (numSamples > 0)
                {
                    const int offset = (int) juce::jlimit<juce::int64> (0,
                                                                       (juce::int64) numSamples - 1,
                                                                       choke.atSample - blockStart);
                    working.addEvent (juce::MidiMessage::noteOff (choke.channel, choke.note), offset);
                }
            }
            else
            {
                pending[kept++] = choke;
            }
        }

        numPendingChokes = kept;

        // 2) Eingehende Nachrichten filtern und Chokes einplanen.
        for (const auto metadata : buffer)
        {
            const auto msg = metadata.getMessage();

            if (! accepts (msg))
                continue;

            working.addEvent (msg, metadata.samplePosition);

            if (msg.isNoteOn (false) && chokeSamples > 0)
                scheduleChoke (blockStart + (juce::int64) metadata.samplePosition + chokeSamples,
                               msg.getNoteNumber(),
                               msg.getChannel());
        }

        buffer.swapWith (working);
        sampleCounter = blockEnd;
    }

private:
    struct PendingChoke
    {
        juce::int64 atSample { 0 };
        int note { 0 };
        int channel { 1 };
    };

    void updateChokeSamples()
    {
        chokeSamples = (chokeMs > 0.0f)
            ? (juce::int64) std::llround ((double) chokeMs * 0.001 * sampleRate)
            : (juce::int64) 0;
    }

    void scheduleChoke (juce::int64 atSample, int note, int channel)
    {
        if (numPendingChokes >= maxPendingChokes)
        {
            // Ältesten Eintrag verwerfen, damit neue Noten nicht blockiert werden.
            for (int i = 1; i < numPendingChokes; ++i)
                pending[i - 1] = pending[i];
            --numPendingChokes;
        }

        pending[numPendingChokes++] = { atSample, note, channel };
    }

    double sampleRate { 48000.0 };
    int channelFilter { 0 };   // 0 = Omni
    int lowNote { 0 };
    int highNote { 127 };
    float chokeMs { 0.0f };
    juce::int64 chokeSamples { 0 };
    juce::int64 sampleCounter { 0 };
    PendingChoke pending[maxPendingChokes] {};
    int numPendingChokes { 0 };
    juce::MidiBuffer working;
};
