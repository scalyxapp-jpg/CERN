#include "PluginProcessor.h"
#include "PluginEditor.h"
#include "DSP/Metering.h"

//==============================================================================
CERNAudioProcessor::CERNAudioProcessor()
#ifndef JucePlugin_PreferredChannelConfigurations
    : juce::AudioProcessor (BusesProperties()
                             .withInput  ("Input",  juce::AudioChannelSet::mono(), true)
                             .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
#endif
    , apvts (*this, &undoManager, "Parameters", createLayout())
    , smoothing (apvts)
    , engine (apvts)
    , sequencer (apvts, smoothing, engine)
    , presetManager (apvts, engine.getPatchGraph(), engine.getParameterPatchGraph())
{
}

CERNAudioProcessor::~CERNAudioProcessor() = default;

//==============================================================================
void CERNAudioProcessor::prepareToPlay (double sampleRate, int samplesPerBlock)
{
    engine.prepare (sampleRate, samplesPerBlock);
    smoothing.prepare (sampleRate);
    smoothing.setBlockSize (samplesPerBlock);
    sequencer.prepare (sampleRate, samplesPerBlock);
    presetManager.prepare (sampleRate);
    loudnessMeter.prepare (sampleRate);
    midiFilter.prepare (sampleRate);

    prepared = true;
    updateTailLength();
}

//==============================================================================
void CERNAudioProcessor::updateTailLength()
{
    const auto* roomParam   = apvts.getRawParameterValue (ParameterIDs::roomDecay);
    const auto* reverbParam = apvts.getRawParameterValue (ParameterIDs::reverbDecay);

    if (roomParam == nullptr || reverbParam == nullptr)
        return;

    // Room-/Reverb-Ausklang abdecken plus Sicherheitsmarge; nie unter der
    // bisherigen Festlegung von 1.5 s (Hosts truncaten sonst den Tail).
    const float decayMs = juce::jmax (roomParam->load(), reverbParam->load());
    const float tail = juce::jlimit (1.5f, 6.0f, decayMs / 1000.0f + 0.5f);

    if (std::abs (tail - tailLengthSeconds) > 1.0e-4f)
    {
        tailLengthSeconds = tail;
        engine.setTailLength (tail);
    }
}

void CERNAudioProcessor::releaseResources()
{
    if (! prepared)
        return;

    sequencer.reset();
    engine.reset();
    loudnessMeter.reset();
    midiFilter.reset();
    lastTriggerButtonState = false;
    midiClockSamplesUntilTick = 0.0;
    midiClockWasRunning = false;
    prepared = false;
}

//==============================================================================
void CERNAudioProcessor::renderMidiClock (juce::MidiBuffer& midiMessages, int numSamples)
{
    if (! sequencer.isMidiClockOutEnabled())
    {
        midiClockSamplesUntilTick = 0.0;
        // Bewusst false: wird Clock-Out im laufenden Betrieb aktiviert, soll
        // der Empfaenger ein frisches Start + Clock bekommen.
        midiClockWasRunning = false;
        return;
    }

    const bool running = sequencer.isTransportRunning();

    if (running && ! midiClockWasRunning)
    {
        midiMessages.addEvent (juce::MidiMessage::midiStart(), 0);
        midiClockSamplesUntilTick = 0.0; // erster Tick bei Sample 0
    }
    else if (! running && midiClockWasRunning)
    {
        midiMessages.addEvent (juce::MidiMessage::midiStop(), 0);
    }

    midiClockWasRunning = running;

    if (! running)
    {
        midiClockSamplesUntilTick = 0.0;
        return;
    }

    const double bpmValue = (double) juce::jlimit (20.0f, 2000.0f, sequencer.getBpm());
    const double samplesPerTick = 60.0 / juce::jmax (1.0, bpmValue) / 24.0 * getSampleRate();

    if (samplesPerTick <= 0.0)
        return;

    double tick = midiClockSamplesUntilTick;

    while (tick < (double) numSamples)
    {
        const int offset = juce::jlimit (0, juce::jmax (0, numSamples - 1), (int) tick);
        midiMessages.addEvent (juce::MidiMessage::midiClock(), offset);
        tick += samplesPerTick;
    }

    midiClockSamplesUntilTick = tick - (double) numSamples;
}

bool CERNAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    // Mono-In / Stereo-Out (und Mono-Out) unterstützen
    const auto& output = layouts.getMainOutputChannelSet();
    return output == juce::AudioChannelSet::stereo()
        || output == juce::AudioChannelSet::mono();
}

void CERNAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midiMessages)
{
    juce::ScopedNoDenormals noDenormals;
    const int numSamples = buffer.getNumSamples();

    // ---- SND-176: MIDI-Filter (Kanal / Notenbereich / Choke) ----
    // Channel: 0 = Omni, 1..16 = fester Kanal; Note-On außerhalb [Low, High]
    // wird verworfen (Note-Offs passieren immer). Choke (midiChokeMs, 0 = aus):
    // der Filter erzeugt nach midiChokeMs ms nach dem Note-On automatisch ein
    // Note-Off für dieselbe Note auf demselben Kanal. Das ist funktional
    // äquivalent zum geforderten All-Notes-Off vor dem Note-On: der Kick ist
    // mono und one-shot, Noten klingen maximal chokeMs, ein Retrigger wirkt
    // sofort. Die erzeugten Note-Offs verlassen das Plugin nicht
    // (Eingangs-MIDI wird konsumiert, kein Thru) und werden vom Synth ignoriert.
    {
        const auto readIntParam = [this] (const char* id, int fallback)
        {
            if (const auto* raw = apvts.getRawParameterValue (id))
                return (int) std::lround (raw->load());

            return fallback;
        };

        midiFilter.setChannel (readIntParam (ParameterIDs::midiChannel, 0));
        midiFilter.setNoteRange (readIntParam (ParameterIDs::midiNoteLow, 0),
                                 readIntParam (ParameterIDs::midiNoteHigh, 127));

        if (const auto* choke = apvts.getRawParameterValue (ParameterIDs::midiChokeMs))
            midiFilter.setChokeMs (choke->load());

        midiFilter.process (midiMessages, numSamples);
    }

    // ---- MIDI Note-On als Trigger (§10.3) ----
    // SND-128/129: rootNote ist die Note von fundamentalHz (Default 30 = F#1).
    // Der Offset ist relativ und auf +-12 Halbtöne begrenzt.
    const float rootNote = [this]
    {
        if (const auto* rootParam = apvts.getRawParameterValue (ParameterIDs::rootNote))
            return rootParam->load();
        return 30.0f;
    }();

    for (const auto& metadata : midiMessages)
    {
        const auto msg = metadata.getMessage();

        if (msg.isNoteOn())
        {
            const float velocity = msg.getVelocity() / 127.0f;
            const float pitchSemitones = juce::jlimit (-12.0f, 12.0f,
                                                       (float) msg.getNoteNumber() - rootNote);

            // Sample-genau: Position des MIDI-Events im Block (nicht Blockanfang).
            engine.enqueueTrigger (velocity, pitchSemitones, metadata.samplePosition);
        }
    }

    // Eingangs-MIDI wird konsumiert (kein Thru): das Plugin erzeugt jetzt
    // selbst MIDI (Clock/Start/Stop, SND-150) und darf die Noten nicht doppelt
    // an den Host zurueckgeben.
    midiMessages.clear();

    // ---- Trigger-Parameter (Rising Edge) für Automatisierung/Patch-Ansicht ----
    if (auto* tb = apvts.getRawParameterValue (ParameterIDs::triggerButton))
    {
        const bool on = tb->load (std::memory_order_relaxed) > 0.5f;
        if (on && ! lastTriggerButtonState)
            engine.setPendingTrigger (1.0f, 0.0f);
        lastTriggerButtonState = on;
    }
    else
    {
        // Parameter fehlt: Flankenzustand zuruecksetzen, damit ein spaeter
        // wieder verfuegbarer Parameter nicht mit veraltetem Zustand klemmt.
        lastTriggerButtonState = false;
    }

    // ---- Parameter einmal pro Block einlesen (§4) ----
    smoothing.update();

    // Tail-Laenge folgt den Decay-Parametern (auch bei Automatisierung).
    updateTailLength();

    // ---- Parameter-Patch/CV vor Sequencer & Engine anwenden ----
    engine.prepareParameterModulation (smoothing, numSamples);

    // ---- Step-Sequencer: enqueued Trigger mit Sample-Offset ----
    // Die Engine konsumiert die Events in render() an ihrem Offset, daher
    // müssen Sequencer und Render denselben Block (numSamples) sehen.
    // SND-136/137: Host-Position (BPM/PPQ/Transport) wird mitgegeben; der
    // Sequencer wertet sie nur im SyncMode "Host" aus.
    if (auto* hostPlayHead = getPlayHead())
    {
        if (const auto position = hostPlayHead->getPosition())
            sequencer.processBlock (numSamples, &*position);
        else
            sequencer.processBlock (numSamples, nullptr);
    }
    else
    {
        sequencer.processBlock (numSamples);
    }

    // ---- MIDI-Clock-Ausgang (SND-150) ----
    renderMidiClock (midiMessages, numSamples);

    // ---- Engine rendern ----
    engine.render (buffer, numSamples, smoothing);

    // ---- Display-Buffers aktualisieren (Audio -> GUI, thread-safe via atomics) ----
    // Beide Kanaele messen: bei Hard-Pan liefert der jeweils lautere Kanal den Wert.
    float rmsLin  = 0.0f;
    float peakLin = 0.0f;
    for (int channel = 0; channel < buffer.getNumChannels(); ++channel)
    {
        rmsLin  = juce::jmax (rmsLin,  Metering::rms  (buffer, channel));
        peakLin = juce::jmax (peakLin, Metering::peak (buffer, channel));
    }
    displayBuffers.rmsDb.store (Metering::toDb (rmsLin), std::memory_order_release);
    displayBuffers.peakDb.store (Metering::toDb (peakLin), std::memory_order_release);

    // ---- SND-170: Kurzzeit-LUFS (400 ms) + GR-History ----
    loudnessMeter.processBlock (buffer);
    displayBuffers.shortTermLufs.store (loudnessMeter.getShortTermLufs(), std::memory_order_release);

    const float grDb = engine.getCompressorGainReductionDb();
    displayBuffers.grDb.store (grDb, std::memory_order_release);

    {
        constexpr int historySize = DisplayBuffers::grHistorySize;
        const int historyPos = displayBuffers.grHistoryWritePos.load (std::memory_order_relaxed);
        displayBuffers.grHistory[(size_t) (historyPos & (historySize - 1))]
            .store (grDb, std::memory_order_relaxed);
        displayBuffers.grHistoryWritePos.store (historyPos + 1, std::memory_order_release);
    }

    // Trigger-LED für GUI (wenn Engine aktiv)
    displayBuffers.triggerLit.store (engine.isActive(), std::memory_order_release);

    // ---- Waveform-Ringpuffer (dezimiert, fuer GUI-Display) ----
    if (buffer.getNumChannels() > 0 && numSamples > 0)
    {
        const float* leftData  = buffer.getReadPointer (0);
        const float* rightData = buffer.getNumChannels() > 1 ? buffer.getReadPointer (1) : leftData;
        constexpr int ringSize = DisplayBuffers::waveformSize;
        constexpr int mask = ringSize - 1;

        int writePos = displayBuffers.waveformWritePos.load (std::memory_order_relaxed);

        for (int i = 0; i < numSamples; i += DisplayBuffers::waveformDecimation)
        {
            // Mid-Signal: repraesentativ fuer beide Kanaele (kein Kanal-0-Metering).
            const float sample = 0.5f * (leftData[i] + rightData[i]);
            displayBuffers.waveform[(size_t) (writePos & mask)]
                .store (sample, std::memory_order_relaxed);
            ++writePos;
        }

        displayBuffers.waveformWritePos.store (writePos, std::memory_order_release);
    }
}

//==============================================================================
void CERNAudioProcessor::copyGainReductionHistory (float* dest, int numValues) const
{
    if (dest == nullptr || numValues <= 0)
        return;

    constexpr int historySize = DisplayBuffers::grHistorySize;
    constexpr int historyMask = historySize - 1;

    const int writePos = displayBuffers.grHistoryWritePos.load (std::memory_order_acquire);
    const int available = juce::jmin (numValues, juce::jmin (writePos, historySize));

    std::fill (dest, dest + numValues, 0.0f);

    // Aeltester -> neuester Wert; die ersten Slots bleiben vor dem Start leer (0 dB).
    for (int i = 0; i < available; ++i)
    {
        const int slot = (writePos - available + i) & historyMask;
        dest[numValues - available + i] = displayBuffers.grHistory[(size_t) slot]
                                              .load (std::memory_order_relaxed);
    }
}

//==============================================================================
juce::AudioProcessorEditor* CERNAudioProcessor::createEditor()
{
    return new CERNAudioProcessorEditor (*this);
}

bool CERNAudioProcessor::hasEditor() const
{
    return true;
}

//==============================================================================
void CERNAudioProcessor::getStateInformation (juce::MemoryBlock& destData)
{
    auto state = apvts.copyState();
    std::unique_ptr<juce::XmlElement> xml (state.createXml());

    if (xml == nullptr)
        return;

    xml->deleteAllChildElementsWithTagName ("Patch");
    xml->deleteAllChildElementsWithTagName ("ParameterPatch");

    if (auto patchXml = engine.getPatchGraph().createStateXml())
        xml->addChildElement (patchXml.release());

    if (auto parameterPatchXml = engine.getParameterPatchGraph().createStateXml())
        xml->addChildElement (parameterPatchXml.release());

    copyXmlToBinary (*xml, destData);
}

void CERNAudioProcessor::setStateInformation (const void* data, int sizeInBytes)
{
    std::unique_ptr<juce::XmlElement> xmlState (getXmlFromBinary (data, sizeInBytes));

    if (xmlState.get() == nullptr)
        return;

    // Unbekannte Parameter/Child-Elemente bleiben tolerant erhalten; fehlender
    // <Patch>-Knoten entspricht dem Werks-Patch, fehlender <ParameterPatch> dem leeren CV-Patch.
    auto patchStateXml = std::make_unique<juce::XmlElement> (*xmlState);
    xmlState->deleteAllChildElementsWithTagName ("Patch");
    xmlState->deleteAllChildElementsWithTagName ("ParameterPatch");

    apvts.replaceState (juce::ValueTree::fromXml (*xmlState));

    // SND-168: replaceState() leert die Undo-History des APVTS, weil der
    // UndoManager an den APVTS gebunden ist (JUCE-Verhalten). Damit gilt das
    // auch fuer Preset-Loads ueber PresetManager::applyXml: Ctrl+Z kann nicht
    // ueber einen State-/Preset-Load hinweg alte Werte zurueckschreiben.

    engine.getPatchGraph().restoreStateXml (*patchStateXml);
    engine.getParameterPatchGraph().restoreStateXml (*patchStateXml);
}

//==============================================================================
void CERNAudioProcessor::triggerNote (float velocity, float pitchSemitones)
{
    engine.setPendingTrigger (velocity, pitchSemitones);
}