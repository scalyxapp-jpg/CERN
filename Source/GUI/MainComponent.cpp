#include "MainComponent.h"
#include "GUI/Theme/CERNLookAndFeel.h"
#include "GUI/AudioDeviceDialog.h"

#include <cmath>
#include <map>
#include <memory>

//==============================================================================
namespace
{
    constexpr int headerHeight = 66;
    constexpr int macroHeight  = 76;
    constexpr int footerHeight = 216;
    constexpr int outerMargin  = 8;
    constexpr int displayGap   = 6;
}

//==============================================================================
MainComponent::MainComponent (CERNAudioProcessor& processorRef,
                              juce::AudioDeviceManager* audioDeviceManagerToShow)
    : processor (processorRef), audioDeviceManager (audioDeviceManagerToShow)
{
    static CERNLookAndFeel customLookAndFeel;
    setLookAndFeel (&customLookAndFeel);
    setOpaque (true);
    setWantsKeyboardFocus (true); // Shortcuts (SND-173)

    fftBuffer.resize ((size_t) fftSize * 2, 0.0f);
    spectrumBars.assign (64, 0.0f);
    spectrumBarFrequencies.assign (64, 0.0f);

    header = std::make_unique<HeaderBar> (processor, audioDeviceManager);
    header->onPatchToggle = [this]
    {
        setViewMode (viewMode == ViewMode::patchbay ? ViewMode::front : ViewMode::patchbay);
    };
    header->onMixerToggle = [this]
    {
        setViewMode (viewMode == ViewMode::mixer ? ViewMode::front : ViewMode::mixer);
    };
    addAndMakeVisible (*header);

    macroPanel = std::make_unique<MacroPanel> (processor);
    addAndMakeVisible (*macroPanel);

    footer = std::make_unique<FooterSequencer> (processor);
    addAndMakeVisible (*footer);

    addAndMakeVisible (sectionViewport);
    sectionViewport.setViewedComponent (&sectionContainer, false);
    sectionViewport.setScrollBarsShown (true, false);

    // ---- Sektionen (§13); EQ/Comp/Output getrennt (SND-163), Material Room (MR),
    //      FEATURES-Gruppe (SND-175..181) und (falls vorhanden) Sidechain ----
    addSection ("Membrane", {
        ParameterIDs::membraneTension,      ParameterIDs::membraneRadius,
        ParameterIDs::membraneMass,         ParameterIDs::membraneDamping,
        ParameterIDs::membraneModes,        ParameterIDs::fundamentalHz,
        ParameterIDs::membraneNonlinearity, ParameterIDs::membraneStiffness
    }, { ParameterIDs::membraneEnabled });
    addSection ("Tube", {
        ParameterIDs::tubeResonanceFreq,    ParameterIDs::tubeResonanceQ,
        ParameterIDs::tubeResonanceLevel,   ParameterIDs::tubeCoupling,
        ParameterIDs::hollowMetallic
    }, { ParameterIDs::tubeEnabled });
    addSection ("Pitch Envelope", {
        ParameterIDs::pitchEnvAttack,       ParameterIDs::pitchEnvDecay,
        ParameterIDs::pitchEnvDepth,        ParameterIDs::pitchEnvCurve
    }, { ParameterIDs::pitchEnvEnabled });
    addSection ("Click", {
        ParameterIDs::clickLevel,           ParameterIDs::clickDecay,
        ParameterIDs::clickTone,            ParameterIDs::clickDistortion,
        ParameterIDs::clickBeaterMaterial
    }, { ParameterIDs::clickEnabled });
    addSection ("Body", {
        ParameterIDs::bodyLevel,            ParameterIDs::bodyAttack,
        ParameterIDs::bodyDecay,            ParameterIDs::bodyTuning
    }, { ParameterIDs::bodyEnabled });
    addSection ("Sub", {
        ParameterIDs::subLevel,             ParameterIDs::subDecay,
        ParameterIDs::subDetune,            ParameterIDs::subShape,
        ParameterIDs::subRatio
    }, { ParameterIDs::subEnabled });
    addSection ("Drive", {
        ParameterIDs::driveAmount,          ParameterIDs::driveMix,
        ParameterIDs::driveCharacter,       ParameterIDs::drivePreHP
    }, { ParameterIDs::driveEnabled });
    addSection ("EQ", {
        ParameterIDs::eqSubFreq,            ParameterIDs::eqSubGain,
        ParameterIDs::eqPunchFreq,          ParameterIDs::eqPunchGain,
        ParameterIDs::eqAirFreq,            ParameterIDs::eqAirGain,
        ParameterIDs::eqRumbleHP
    }, { ParameterIDs::eqEnabled });
    // M13: compMix/compAutoMakeup gehoeren thematisch zum Kompressor.
    addSection ("Comp", {
        ParameterIDs::compThreshold,        ParameterIDs::compRatio,
        ParameterIDs::compAttack,           ParameterIDs::compRelease,
        ParameterIDs::compKnee,             ParameterIDs::compMakeup,
        ParameterIDs::compMix,              ParameterIDs::compAutoMakeup
    }, { ParameterIDs::compEnabled });
    addSection ("Output", {
        ParameterIDs::outputGain,           ParameterIDs::outputPan,
        ParameterIDs::outputWidth,          ParameterIDs::outputMonoBelow,
        ParameterIDs::outputClip,           ParameterIDs::outputLimiter
    }, { ParameterIDs::outputEnabled });
    addSection ("Room", {
        ParameterIDs::roomLevel,            ParameterIDs::roomDecay,
        ParameterIDs::roomRumble
    }, { ParameterIDs::roomEnabled });
    addSection ("Reverb", {
        ParameterIDs::reverbMaterial,       ParameterIDs::reverbPreDelay,
        ParameterIDs::reverbDecay,          ParameterIDs::reverbSize,
        ParameterIDs::reverbDamping,        ParameterIDs::reverbMix,
        ParameterIDs::reverbLowCut,         ParameterIDs::reverbHighCut,
        ParameterIDs::reverbFreeze,         ParameterIDs::reverbGate
    }, { ParameterIDs::reverbEnabled });
    addSection ("Material Room", {
        ParameterIDs::materialRoomMaterial, ParameterIDs::materialRoomSize,
        ParameterIDs::materialRoomMix,      ParameterIDs::materialRoomLayer,
        ParameterIDs::materialRoomWidth,    ParameterIDs::grumbleLevel,
        ParameterIDs::grumbleFreq,          ParameterIDs::grumbleRate,
        ParameterIDs::grumbleDepth,         ParameterIDs::grumblePanA,
        ParameterIDs::grumblePanB,          ParameterIDs::grumbleWave,
        ParameterIDs::grumblePanWave,       ParameterIDs::grumbleDecay
    }, { ParameterIDs::materialRoomEnabled });
    addSection ("Higgs Field", {
        ParameterIDs::higgsCursorPhi,       ParameterIDs::higgsCursorTheta,
        ParameterIDs::higgsFieldStrength,
        ParameterIDs::higgsVev,             ParameterIDs::higgsSelfCoupling,
        ParameterIDs::higgsInertia,         ParameterIDs::higgsYukawaBoost,
        ParameterIDs::higgsBosonHz,         ParameterIDs::higgsBosonQ,
        ParameterIDs::higgsSymmetry,        ParameterIDs::higgsDecay,
        ParameterIDs::higgsMix,             ParameterIDs::higgsRate,
        ParameterIDs::higgsDepth
    }, { ParameterIDs::higgsEnabled });
    // ---- FEATURES (SND-175..181): neue Front-Sektion, 4 Spalten ----
    addSection ("FEATURES", {
        ParameterIDs::transientAttack,      ParameterIDs::transientSustain,
        ParameterIDs::midiChannel,          ParameterIDs::midiNoteLow,
        ParameterIDs::midiNoteHigh,         ParameterIDs::midiChokeMs,
        ParameterIDs::velToClick,           ParameterIDs::humanize,
        ParameterIDs::oversampleEnabled,    ParameterIDs::fxWidth,
        ParameterIDs::envFollowAttack,      ParameterIDs::envFollowRelease,
        ParameterIDs::layeringEnabled,      ParameterIDs::layeringAmount
    });

    addSection ("SIDECHAIN", { ParameterIDs::duckDepth, ParameterIDs::duckRelease });

    // SND-150/136/144/146/145: Sequencer-Konfiguration (Sync, Grid, Pattern,
    // Chain, MIDI-Clock-Out). Die Step-Ebene sitzt im Footer.
    addSection ("Sequencer", {
        ParameterIDs::sequencerSyncMode,    ParameterIDs::sequencerGrid,
        ParameterIDs::sequencerPattern,     ParameterIDs::sequencerChain,
        ParameterIDs::sequencerClockOut
    });

    addSection ("Global", {
        ParameterIDs::rootNote,             ParameterIDs::velocitySensitivity,
        ParameterIDs::keyRoot,              ParameterIDs::scale,
        ParameterIDs::glideMs,              ParameterIDs::velToPitch,
        ParameterIDs::tubeTrack
    });

    // ---- Displays ----
    addAndMakeVisible (waveformDisplay);
    waveformDisplay.attachLayeringControl (processor.getAPVTS(),
                                           ParameterIDs::layeringEnabled,
                                           ParameterIDs::layeringAmount);

    // Preset-Vorschau (Review §7.7): 64-Punkt-Wellenform je Preset im Hintergrund.
    presetThumbnails = std::make_unique<PresetThumbnailCache>();
    presetThumbnails->startRendering();

    if (header != nullptr)
        header->setPresetThumbnailCache (presetThumbnails.get());
    addAndMakeVisible (spectrumAnalyzer);
    addAndMakeVisible (envelopeEditor);

    meterDisplay = std::make_unique<MeterDisplay>();
    addAndMakeVisible (*meterDisplay);

    // SND-184/185/183: Statusmeldungen aus dem Header in den Meter-Strip.
    header->onStatus = [this] (const juce::String& message)
    {
        if (meterDisplay != nullptr)
            meterDisplay->showStatus (message);
    };

    higgsField = std::make_unique<HiggsField3D> (processor.getAPVTS());
    addAndMakeVisible (*higgsField);

    // Patch-/Rückseiten-Ansicht (wird bei Bedarf eingeblendet)
    patchView = std::make_unique<PatchView> (processor.getPatchGraph(),
                                              processor.getParameterPatchGraph(),
                                              processor.getAPVTS());
    addChildComponent (*patchView);

    // Mixer-Tab: ein Kanalstreifen je klang-erzeugendem Modul.
    mixerView = std::make_unique<MixerView> (processor.getAPVTS(),
                                              processor.getEngine().getMixer());
    addChildComponent (*mixerView);

    startTimerHz (30);
}

MainComponent::~MainComponent()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MainComponent::addSection (const juce::String& title,
                                const std::vector<juce::String>& paramIds,
                                const std::vector<juce::String>& enableIds)
{
    auto panel = std::make_unique<SectionPanel> (processor.getAPVTS(), title, paramIds, enableIds);
    sectionContainer.addAndMakeVisible (*panel);
    sections.push_back (std::move (panel));
}

void MainComponent::paint (juce::Graphics& g)
{
    CERNLookAndFeel::drawBackdrop (g, getLocalBounds().toFloat());
}

void MainComponent::setPatchMode (bool on)
{
    setViewMode (on ? ViewMode::patchbay : ViewMode::front);
}

void MainComponent::setViewMode (ViewMode mode)
{
    viewMode = mode;
    patchMode = (mode == ViewMode::patchbay);

    const bool front = (mode == ViewMode::front);

    if (header != nullptr)
    {
        header->setPatchActive (patchMode);
        header->setMixerActive (mode == ViewMode::mixer);
    }

    sectionViewport.setVisible (front);
    waveformDisplay.setVisible (front);
    spectrumAnalyzer.setVisible (front);
    envelopeEditor.setVisible (front);
    if (meterDisplay) meterDisplay->setVisible (front);
    if (higgsField) higgsField->setVisible (front);
    if (footer) footer->setVisible (front);
    if (macroPanel) macroPanel->setVisible (front);
    if (patchView) patchView->setVisible (patchMode);
    if (mixerView) mixerView->setVisible (mode == ViewMode::mixer);

    resized();
    repaint();
}

bool MainComponent::keyPressed (const juce::KeyPress& key)
{
    // Tastatur nie aus einem Textfeld stehlen (BPM-Eingabe, Preset-Namen).
    if (auto* focused = juce::Component::getCurrentlyFocusedComponent())
        if (dynamic_cast<juce::TextEditor*> (focused) != nullptr)
            return false;

    // SND-168: Undo/Redo (Ctrl+Z, Ctrl+Shift+Z, Ctrl+Y).
    const auto modifiers = key.getModifiers();

    if (modifiers.isCommandDown())
    {
        const auto typed =
            juce::CharacterFunctions::toLowerCase ((juce::juce_wchar) key.getKeyCode());

        if (typed == 'z')
        {
            if (modifiers.isShiftDown())
                performRedo();
            else
                performUndo();

            return true;
        }

        if (typed == 'y')
        {
            performRedo();
            return true;
        }
    }

    if (key == juce::KeyPress::spaceKey)
    {
        if (footer) footer->togglePlayback();
        return true;
    }

    // Ableton reserviert Space fuer den Transport und leitet es NICHT an
    // Plugins weiter. 'P' schaltet die Sequencer-Wiedergabe ueberall um.
    const juce::juce_wchar playCharacter = key.getTextCharacter();

    if (playCharacter == 'p' || playCharacter == 'P')
    {
        if (footer) footer->togglePlayback();
        return true;
    }

    const juce::juce_wchar character = key.getTextCharacter();

    if (character == 't' || character == 'T')
    {
        if (header) header->trigger();
        return true;
    }

    if (key == juce::KeyPress::leftKey)
    {
        if (header) header->selectRelativePreset (-1);
        return true;
    }

    if (key == juce::KeyPress::rightKey)
    {
        if (header) header->selectRelativePreset (1);
        return true;
    }

    return false;
}

void MainComponent::performUndo()
{
    auto& undoManager = processor.getUndoManager();

    if (! undoManager.canUndo())
    {
        if (meterDisplay) meterDisplay->showStatus ("NOTHING TO UNDO");
        return;
    }

    undoManager.undo();
    if (meterDisplay) meterDisplay->showStatus ("UNDO");
}

void MainComponent::performRedo()
{
    auto& undoManager = processor.getUndoManager();

    if (! undoManager.canRedo())
    {
        if (meterDisplay) meterDisplay->showStatus ("NOTHING TO REDO");
        return;
    }

    undoManager.redo();
    if (meterDisplay) meterDisplay->showStatus ("REDO");
}

void MainComponent::resized()
{
    const int w = getWidth();
    const int h = getHeight();

    // Bei schmalen Fenstern (oder sichtbarem MORPH bei mittlerer Breite)
    // bekommt die Kopfzeile zwei Reihen (siehe HeaderBar::resized), damit
    // nichts abgeschnitten wird.
    const int headerH = HeaderBar::useTwoRows (w, header != nullptr && header->isMorphVisible())
                          ? 88 : headerHeight;

    header->setBounds (0, 0, w, headerH);

    if (patchMode || viewMode == ViewMode::mixer)
    {
        if (footer) footer->setBounds (0, h, w, 0);
        if (macroPanel) macroPanel->setBounds (0, headerH, w, 0);
        if (patchView) patchView->setBounds (0, headerH, w, juce::jmax (0, h - headerH));
        if (mixerView) mixerView->setBounds (0, headerH, w, juce::jmax (0, h - headerH));
        return;
    }

    macroPanel->setBounds (0, headerH, w, macroHeight);
    footer->setBounds (0, h - footerHeight, w, footerHeight);

    const int contentY = headerH + macroHeight;
    const int contentH = juce::jmax (0, h - contentY - footerHeight);

    // ---- Linke Spalte: Sektionen (scrollbar) ----
    const int sectionsWidth = juce::jmax (420, (int) ((float) w * 0.56f));
    sectionViewport.setBounds (0, contentY, sectionsWidth, contentH);

    const int scrollbarWidth = sectionViewport.getScrollBarThickness();
    const int canvasWidth = sectionsWidth - scrollbarWidth;

    int y = outerMargin;
    for (auto& section : sections)
    {
        const int sectionWidth = canvasWidth - 2 * outerMargin;
        const int panelHeight = section->getPreferredHeight (sectionWidth);
        section->setBounds (outerMargin, y, sectionWidth, panelHeight);
        y += panelHeight + outerMargin;
    }
    sectionContainer.setSize (canvasWidth, y);

    // ---- Rechte Spalte: Waveform/Spectrum oben, Envelope + Higgs unten ----
    const int displaysX = sectionsWidth;
    const int displaysW = juce::jmax (0, w - sectionsWidth);

    const int halfW  = displaysW / 2;
    const int halfH  = contentH / 2;
    const int bottomY = contentY + halfH;
    const int bottomH = contentH - halfH;
    const int envelopeW = (int) (displaysW * 0.34f);

    // Meter-Strip (SND-170) sitzt unten in der rechten Spalte, ueber dem Footer.
    const int meterHeight = juce::jlimit (0, bottomH, 64);
    const int meterY      = bottomY + bottomH - meterHeight;
    const int bottomDisplayH = juce::jmax (0, bottomH - meterHeight - displayGap);

    // Unter ~96 px bleibt fuer Pitch-Envelope + Higgs-Legende kein lesbarer
    // Raum (900x650-Befund: 65 px, Legende auf 3 Zeilen beschnitten). Dann
    // entfaellt die untere Zeile, und Waveform/Spectrum strecken sich bis
    // zum Meter-Strip.
    const bool compactBottom = bottomDisplayH < 96;
    envelopeEditor.setVisible (! compactBottom);
    higgsField->setVisible (! compactBottom);

    const int topDisplayH = compactBottom ? juce::jmax (24, meterY - contentY - 2 * displayGap)
                                          : halfH - displayGap;

    waveformDisplay.setBounds (displaysX + displayGap, contentY + displayGap,
                               halfW - displayGap, topDisplayH);
    spectrumAnalyzer.setBounds (displaysX + halfW, contentY + displayGap,
                                displaysW - halfW - displayGap, topDisplayH);
    if (! compactBottom)
    {
        envelopeEditor.setBounds (displaysX + displayGap, bottomY,
                                  envelopeW - displayGap, bottomDisplayH);
        higgsField->setBounds (displaysX + envelopeW, bottomY,
                               displaysW - envelopeW - displayGap, bottomDisplayH);
    }

    if (meterDisplay)
        meterDisplay->setBounds (displaysX + displayGap, meterY,
                                 juce::jmax (0, displaysW - displayGap), meterHeight);
}

float MainComponent::rawValue (const juce::String& parameterId)
{
    if (auto* value = processor.getAPVTS().getRawParameterValue (parameterId))
        return value->load (std::memory_order_relaxed);

    return 0.0f;
}

void MainComponent::updateDisplays()
{
    // ---- Waveform: Ringpuffer-Snapshot (aeltester -> neuester Wert) ----
    const auto& buffers = processor.getDisplayBuffers();
    constexpr int ringSize = CERNAudioProcessor::DisplayBuffers::waveformSize;
    constexpr int mask = ringSize - 1;

    std::vector<float> snapshot ((size_t) ringSize, 0.0f);
    const int writePos = buffers.waveformWritePos.load (std::memory_order_acquire);

    for (int i = 0; i < ringSize; ++i)
        snapshot[(size_t) i] =
            buffers.waveform[(size_t) ((writePos + i) & mask)].load (std::memory_order_relaxed);

    waveformDisplay.setSamples (snapshot);

    // ---- Layering: separate Layer-Taps + errechnete Werte (nur Anzeige) ----
    {
        std::vector<float> lowSnapshot ((size_t) ringSize, 0.0f);
        std::vector<float> highSnapshot ((size_t) ringSize, 0.0f);

        for (int i = 0; i < ringSize; ++i)
        {
            const int index = (writePos + i) & mask;
            lowSnapshot[(size_t) i] = buffers.layerLow[(size_t) index].load (std::memory_order_relaxed);
            highSnapshot[(size_t) i] = buffers.layerHigh[(size_t) index].load (std::memory_order_relaxed);
        }

        waveformDisplay.setLayerSamples (lowSnapshot, highSnapshot);
        waveformDisplay.setLayerInfo (processor.getEngine().getLayering().getInfo());

        // ---- Modul-Overlay: Membrane/Body/Sub/Click als eigene Kurven ----
        std::vector<float> membraneSnapshot ((size_t) ringSize, 0.0f);
        std::vector<float> bodySnapshot ((size_t) ringSize, 0.0f);
        std::vector<float> subSnapshot ((size_t) ringSize, 0.0f);
        std::vector<float> clickSnapshot ((size_t) ringSize, 0.0f);

        for (int i = 0; i < ringSize; ++i)
        {
            const int index = (writePos + i) & mask;
            membraneSnapshot[(size_t) i] = buffers.moduleMembrane[(size_t) index].load (std::memory_order_relaxed);
            bodySnapshot[(size_t) i] = buffers.moduleBody[(size_t) index].load (std::memory_order_relaxed);
            subSnapshot[(size_t) i] = buffers.moduleSub[(size_t) index].load (std::memory_order_relaxed);
            clickSnapshot[(size_t) i] = buffers.moduleClick[(size_t) index].load (std::memory_order_relaxed);
        }

        waveformDisplay.setModuleSamples (membraneSnapshot, bodySnapshot,
                                          subSnapshot, clickSnapshot);
    }

    // ---- Spectrum: FFT auf dem Waveform-Snapshot ----
    std::fill (fftBuffer.begin(), fftBuffer.end(), 0.0f);
    std::copy (snapshot.begin(), snapshot.end(), fftBuffer.begin());

    fftWindow.multiplyWithWindowingTable (fftBuffer.data(), (size_t) fftSize);
    fft.performFrequencyOnlyForwardTransform (fftBuffer.data());

    const double sampleRate = processor.getSampleRate() > 0.0
                                  ? processor.getSampleRate() : 48000.0;
    const double snapshotRate =
        sampleRate / (double) CERNAudioProcessor::DisplayBuffers::waveformDecimation;

    // Frequenzachse: 20–120 Hz LINEAR (der Sub-Band ist bei einem Kick die
    // entscheidende Information), darueber logarithmisch bis 20 kHz.
    const int numBars = (int) spectrumBars.size();
    const int linearBars = juce::jmax (8, (int) std::round ((double) numBars * 0.40));
    const double subTop = 120.0;

    for (int bar = 0; bar < numBars; ++bar)
    {
        double f0 = 0.0, f1 = 0.0;

        if (bar < linearBars)
        {
            f0 = 20.0 + (subTop - 20.0) * (double) bar / (double) linearBars;
            f1 = 20.0 + (subTop - 20.0) * (double) (bar + 1) / (double) linearBars;
        }
        else
        {
            const double logBars = (double) (numBars - linearBars);
            const double u0 = (double) (bar - linearBars) / logBars;
            const double u1 = (double) (bar + 1 - linearBars) / logBars;
            f0 = subTop * std::pow (20000.0 / subTop, u0);
            f1 = subTop * std::pow (20000.0 / subTop, u1);
        }

        const int bin0 = juce::jlimit (1, fftSize / 2 - 1,
                                       (int) (f0 / snapshotRate * (double) fftSize));
        const int bin1 = juce::jlimit (bin0 + 1, fftSize / 2,
                                       (int) (f1 / snapshotRate * (double) fftSize));

        float peak = 0.0f;
        for (int bin = bin0; bin < bin1; ++bin)
            peak = juce::jmax (peak, fftBuffer[(size_t) bin]);

        spectrumBars[(size_t) bar] = peak * 2.0f / (float) fftSize;
        spectrumBarFrequencies[(size_t) bar] = 0.5f * (float) (f0 + f1);
    }

    spectrumAnalyzer.setSpectrum (spectrumBars);
    spectrumAnalyzer.setBarFrequencies (spectrumBarFrequencies);

    // ---- SND-170: Kurzzeit-LUFS + GR-History (30-Hz-Timer, keine Audio-Thread-Locks) ----
    if (meterDisplay != nullptr)
    {
        meterDisplay->setShortTermLufs (processor.getShortTermLufs());

        processor.copyGainReductionHistory (grHistoryScratch.data(),
                                            (int) grHistoryScratch.size());
        meterDisplay->setGainReductionHistory (grHistoryScratch.data(),
                                               (int) grHistoryScratch.size());

        // M12: Ausgabe-Meter (Limiter-GR, Clip-GR, True-Peak).
        meterDisplay->setOutputMeters (processor.getEngine().getLimiterGainReductionDb(),
                                       processor.getEngine().getClipGainReductionDb(),
                                       processor.getEngine().getTruePeakDb());
    }

    // ---- Pitch-Envelope (echter exponentieller Verlauf inkl. Curve, SND-171) ----
    envelopeEditor.setParameters (rawValue (ParameterIDs::pitchEnvAttack),
                                  rawValue (ParameterIDs::pitchEnvDecay),
                                  rawValue (ParameterIDs::pitchEnvDepth),
                                  rawValue (ParameterIDs::pitchEnvCurve));

    // ---- Higgs Field (alle 13 Parameter sichtbar) ----
    higgsField->setParams (rawValue (ParameterIDs::higgsCursorPhi),
                           rawValue (ParameterIDs::higgsCursorTheta),
                           rawValue (ParameterIDs::higgsFieldStrength),
                           rawValue (ParameterIDs::higgsVev),
                           rawValue (ParameterIDs::higgsSelfCoupling),
                           rawValue (ParameterIDs::higgsInertia),
                           rawValue (ParameterIDs::higgsYukawaBoost),
                           rawValue (ParameterIDs::higgsBosonHz),
                           rawValue (ParameterIDs::higgsBosonQ),
                           rawValue (ParameterIDs::higgsSymmetry),
                           rawValue (ParameterIDs::higgsDecay),
                           rawValue (ParameterIDs::higgsMix),
                           rawValue (ParameterIDs::higgsEnabled) > 0.5f);
}

void MainComponent::timerCallback()
{
    updateDisplays();

    if (meterDisplay != nullptr)
        meterDisplay->tickStatus (juce::Time::getMillisecondCounterHiRes());

    if (header != nullptr)
        header->refreshPresetPreview();

    // Wert-Anzeigen nur selten neu formatieren (2 Hz statt 30 Hz)
    static int refreshCounter = 0;
    if (++refreshCounter >= 15)
    {
        refreshCounter = 0;

        for (auto& section : sections)
            section->refresh();

        // Kabel-LED auf der Front (SND-158): Patch- + Parameter-Kabel
        if (header != nullptr)
            header->setCableCount (processor.getPatchGraph().getConnectionCount()
                                   + processor.getParameterPatchGraph().getConnectionCount());
    }
}

//==============================================================================
// MeterDisplay (SND-170)
MainComponent::MeterDisplay::MeterDisplay()
{
    setTooltip ("Short-term loudness (LUFS, 400 ms) and compressor gain-reduction history "
                "(0 dB top, 24 dB reduction bottom). Undo: Ctrl+Z, Redo: Ctrl+Shift+Z or Ctrl+Y.");
}

void MainComponent::MeterDisplay::setShortTermLufs (float lufs)
{
    const float sanitised = std::isfinite (lufs) ? juce::jlimit (-100.0f, 0.0f, lufs)
                                                 : -100.0f;

    if (std::abs (sanitised - currentLufs) < 0.05f)
        return;

    currentLufs = sanitised;
    lufsText = sanitised <= -99.9f ? juce::String ("-inf")
                                   : juce::String (sanitised, 1) + " dB";
    repaint();
}

void MainComponent::MeterDisplay::setGainReductionHistory (const float* values, int numValues)
{
    if (values == nullptr || numValues <= 0)
        return;

    const int count = juce::jmin (numValues, maxHistory);
    bool changed = (count != numHistory);

    for (int i = 0; i < count; ++i)
    {
        const float value = std::isfinite (values[i]) ? values[i] : 0.0f;

        if (! changed && std::abs (value - history[(size_t) i]) > 0.01f)
            changed = true;

        history[(size_t) i] = value;
    }

    numHistory = count;

    if (! changed)
        return;

    currentGr = history[(size_t) (count - 1)];
    grText = juce::String (currentGr, 1) + " dB";

    rebuildGraphPath();
    repaint();
}

void MainComponent::MeterDisplay::showStatus (const juce::String& message)
{
    statusText = message;
    statusUntilMs = juce::Time::getMillisecondCounterHiRes() + 1600.0;
    repaint();
}

void MainComponent::MeterDisplay::tickStatus (double nowMs)
{
    if (statusText.isEmpty() || nowMs < statusUntilMs)
        return;

    statusText.clear();
    repaint();
}

void MainComponent::MeterDisplay::resized()
{
    auto area = getLocalBounds().toFloat().reduced (10.0f, 4.0f);
    area.removeFromTop (16.0f);

    graphArea = area;

    rebuildGraphPath();
}

void MainComponent::MeterDisplay::rebuildGraphPath()
{
    graphPath.clear();
    graphFill.clear();

    if (numHistory < 2 || graphArea.getWidth() < 2.0f || graphArea.getHeight() < 2.0f)
        return;

    const float span = maxGrDb - minGrDb;

    for (int i = 0; i < numHistory; ++i)
    {
        const float x = graphArea.getX()
                      + graphArea.getWidth() * (float) i / (float) (numHistory - 1);
        const float db = juce::jlimit (minGrDb, maxGrDb, history[(size_t) i]);
        const float y = graphArea.getY() + graphArea.getHeight() * (db - minGrDb) / span;

        if (i == 0)
            graphPath.startNewSubPath (x, y);
        else
            graphPath.lineTo (x, y);
    }

    graphFill = graphPath;
    graphFill.lineTo (graphArea.getRight(), graphArea.getBottom());
    graphFill.lineTo (graphArea.getX(), graphArea.getBottom());
    graphFill.closeSubPath();
}

void MainComponent::MeterDisplay::setOutputMeters (float limiterGrDb, float clipGrDb,
                                                   float truePeakDb)
{
    const juce::String newFull =
          "TPK " + juce::String (truePeakDb, 1) + " dBTP  |  LIM "
        + juce::String (limiterGrDb, 1) + " dB  |  CLIP "
        + juce::String (clipGrDb, 1) + " dB";

    const juce::String newShort =
          "TPK " + juce::String (truePeakDb, 1) + "  LIM "
        + juce::String (limiterGrDb, 1) + "  CLIP " + juce::String (clipGrDb, 1);

    if (newFull != outputText)
    {
        outputText = newFull;
        outputTextShort = newShort;
        repaint();
    }
}

void MainComponent::MeterDisplay::paint (juce::Graphics& g)
{
    // Alle Texte/Pfade sind vorbereitet -> paint() allokiert nichts.
    const juce::Rectangle<float> bounds (0.0f, 0.0f,
                                         (float) getWidth(), (float) getHeight());

    // Meter als versenktes Display mit Metallrahmen.
    CERNLookAndFeel::drawBevel (g, bounds, 5.0f, true, 0.10f);
    CERNLookAndFeel::drawWell (g, bounds.reduced (3.0f), 3.5f);

    // ---- Textzeile: LUFS | GR | Status bzw. Shortcut-Hinweis ----
    auto textRow = bounds.reduced (10.0f, 4.0f).removeFromTop (16.0f);

    const juce::Font valueFont (CERNLookAndFeel::getValueFont());
    g.setFont (valueFont);

    // Breite Felder aus gemessenen Textbreiten: verhindert, dass "-inf" und
    // "GR" bei schmalen Fenstern zusammenstossen (700x500-Befund).
    const float lufsLabelW = juce::GlyphArrangement::getStringWidth (valueFont, "LUFS ST");
    const float lufsValueW = juce::GlyphArrangement::getStringWidth (valueFont, lufsText);
    const float grLabelW   = juce::GlyphArrangement::getStringWidth (valueFont, "GR");
    const float grValueW   = juce::GlyphArrangement::getStringWidth (valueFont, grText);

    auto drawSeq = [this, &g, &textRow] (const juce::String& text, float w,
                                         const juce::Colour& colour)
    {
        g.setColour (colour);
        g.drawText (text, textRow.removeFromLeft (w), juce::Justification::centredLeft, false);
    };

    const float gap = 8.0f;
    drawSeq ("LUFS ST", lufsLabelW, CERNLookAndFeel::textColour.withAlpha (0.6f));
    textRow.removeFromLeft (gap);
    drawSeq (lufsText, lufsValueW, CERNLookAndFeel::accentBrightColour);
    textRow.removeFromLeft (gap * 2.0f);
    drawSeq ("GR", grLabelW, CERNLookAndFeel::textColour.withAlpha (0.6f));
    textRow.removeFromLeft (gap);
    drawSeq (grText, grValueW, CERNLookAndFeel::textColour);

    // M12: True-Peak / Limiter-GR / Clip-GR in der ersten Zeile (wenn Platz ist).
    {
        const float gap = 8.0f;
        const juce::Font smallFont (CERNLookAndFeel::getLabelFont());
        const float outW = juce::GlyphArrangement::getStringWidth (smallFont, outputTextShort);

        if (textRow.getWidth() > outW + 130.0f)
        {
            textRow.removeFromLeft (gap * 2.0f);
            g.setFont (smallFont);
            g.setColour (CERNLookAndFeel::textColour.withAlpha (0.55f));
            g.drawText (outputTextShort, textRow.removeFromLeft (outW),
                        juce::Justification::centredLeft, false);
            g.setFont (valueFont);
        }
    }

    const bool hasStatus = statusText.isNotEmpty();
    const bool narrowMeter = textRow.getWidth() < 190.0f;
    const juce::String* rightText = hasStatus ? &statusText
                                              : (narrowMeter ? &hintTextShort : &hintText);

    // Bei wenig Platz auf die kurze Variante wechseln, statt rechts abzuschneiden.
    if (! hasStatus && rightText == &hintText && textRow.getWidth() > 0.0f)
    {
        if (juce::GlyphArrangement::getStringWidth (valueFont, hintText) > textRow.getWidth())
            rightText = &hintTextShort;
    }

    // rechtsbuerendig, aber nur wenn mindestens 110 px fuer den Hinweis uebrig sind
    if (textRow.getWidth() >= 110.0f && rightText != nullptr)
    {
        g.setColour (hasStatus ? CERNLookAndFeel::accentBrightColour
                               : CERNLookAndFeel::textColour.withAlpha (0.55f));
        g.drawText (*rightText, textRow, juce::Justification::centredRight, false);
    }

    // ---- GR-Graph: 0 / 6 / 12 / 18 / 24 dB Reduktion ----
    if (graphArea.getWidth() < 2.0f || graphArea.getHeight() < 2.0f)
        return;

    g.setColour (CERNLookAndFeel::textColour.withAlpha (0.08f));

    for (int i = 1; i <= 4; ++i)
    {
        const float y = graphArea.getY() + graphArea.getHeight() * (float) i / 4.0f;
        g.drawHorizontalLine ((int) y, graphArea.getX(), graphArea.getRight());
    }

    if (numHistory < 2)
    {
        g.setColour (CERNLookAndFeel::textColour.withAlpha (0.35f));
        g.drawText ("NO HISTORY", graphArea, juce::Justification::centred, false);
        return;
    }

    g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.16f));
    g.fillPath (graphFill);

    g.setColour (CERNLookAndFeel::accentColour);
    g.strokePath (graphPath, juce::PathStrokeType (1.4f));
}

//==============================================================================
// HeaderBar
MainComponent::HeaderBar::HeaderBar (CERNAudioProcessor& p,
                                     juce::AudioDeviceManager* deviceManager)
    : processor (p), audioDeviceManager (deviceManager)
{
    static CERNLookAndFeel customLookAndFeel;
    setLookAndFeel (&customLookAndFeel);

    addAndMakeVisible (logo);

    presetPreview.setTitle ("Preset preview");
    addAndMakeVisible (presetPreview);

    patchButton.setTooltip ("Patchbay: shows and edits the module and parameter cables. "
                            "The LED counts the active cables.");
    patchButton.setClickingTogglesState (false);
    patchButton.onClick = [this] { if (onPatchToggle) onPatchToggle(); };
    addAndMakeVisible (patchButton);

    mixerButton.setTooltip ("Mixer: one channel per sound module (gain, 4-band EQ, mute) "
                            "plus the automatic sidechain relations.");
    mixerButton.setClickingTogglesState (false);
    mixerButton.onClick = [this] { if (onMixerToggle) onMixerToggle(); };
    addAndMakeVisible (mixerButton);

    cableLed.setTooltip ("Active cables in the patchbay");
    addAndMakeVisible (cableLed);

    triggerButton.setTooltip ("Trigger the kick once (shortcut: T)");
    triggerButton.setClickingTogglesState (false);
    triggerButton.onClick = [this] { triggerClicked(); };
    addAndMakeVisible (triggerButton);

    // AUDIO-Button: nur im Standalone (mit AudioDeviceManager) sichtbar
    if (audioDeviceManager != nullptr)
    {
        audioButton.setTooltip ("Select audio device, sample rate and buffer size");
        audioButton.onClick = [this] { CERNAudioSettings::showDialog (*audioDeviceManager); };
        addAndMakeVisible (audioButton);
    }

    deleteButton.setTooltip ("Delete the selected user preset");
    saveButton.setTooltip ("Save the current sound under a name and folder");
    loadButton.setTooltip ("Load a preset file from disk");
    deleteButton.onClick = [this] { deletePreset(); };
    saveButton.onClick = [this] { savePresetAs(); };
    loadButton.onClick = [this] { loadPresetFromDisk(); };
    addAndMakeVisible (saveButton);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (deleteButton);

    abButton.setTooltip ("A/B: zwei vollständige States (Sound, Audio-Patch und CV-Kabel)");
    copyButton.setTooltip ("Den aktiven A/B-Zustand in den anderen Slot kopieren");
    abButton.onClick = [this] { toggleAB(); };
    copyButton.onClick = [this] { copyAB(); };
    addAndMakeVisible (abButton);
    addAndMakeVisible (copyButton);

    rndButton.setTooltip ("Zufallssound: konservative, tuning-sichere Bereiche. "
                          "Pitch-Parameter (Fundamental, Root, Scale, Step-Pitch, Glide, Pitch-Envelope) bleiben unverändert");
    rndButton.setClickingTogglesState (false);
    rndButton.onClick = [this] { randomizeSound(); };
    addAndMakeVisible (rndButton);

    renderButton.setTooltip ("Ein Takt wird offline als WAV nach ~/Documents/C.E.R.N-Bounce.wav gerendert");
    renderButton.setClickingTogglesState (false);
    renderButton.onClick = [this] { renderBounce(); };
    addAndMakeVisible (renderButton);

    morphLabel.setText ("MORPH", juce::dontSendNotification);
    morphLabel.setJustificationType (juce::Justification::centredRight);
    morphLabel.setFont (CERNLookAndFeel::getNameFont());
    morphLabel.setColour (juce::Label::textColourId,
                          CERNLookAndFeel::textColour.withAlpha (0.7f));
    morphLabel.setInterceptsMouseClicks (false, false);
    addChildComponent (morphLabel); // nur sichtbar, wenn beide A/B-Slots belegt sind

    morphSlider.setRange (0.0, 100.0, 1.0);
    morphSlider.setValue (0.0, juce::dontSendNotification);
    morphSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 44, 22);
    morphSlider.setTextValueSuffix (" %");
    morphSlider.setDoubleClickReturnValue (true, 0.0);
    morphSlider.setTooltip ("Preset-Morph A \u2194 B (0\u2013100 %): Floats werden interpoliert, "
                            "Schalter/Choices kippen bei 50 % zum näheren Zustand. "
                            "Der Regler schreibt live in die APVTS");
    morphSlider.onValueChange = [this] { applyMorph ((float) morphSlider.getValue()); };
    addChildComponent (morphSlider); // nur sichtbar, wenn beide A/B-Slots belegt sind

    presetSelector.setTextWhenNothingSelected ("NO PRESET");
    presetSelector.onChange = [this] { presetChanged(); };
    addAndMakeVisible (presetSelector);

    // Prominente Browser-Tasten in der Kopfzeilenmitte: <  ‹Preset›  >
    presetPrevButton.setTitle ("Vorheriges Preset");
    presetPrevButton.setDescription ("Vorheriges Preset auswaehlen");
    presetPrevButton.setTooltip ("Vorheriges Preset (auch: Cursor-Links)");
    presetPrevButton.onClick = [this] { selectRelativePreset (-1); };
    addAndMakeVisible (presetPrevButton);

    presetNextButton.setTitle ("Naechstes Preset");
    presetNextButton.setDescription ("Naechstes Preset auswaehlen");
    presetNextButton.setTooltip ("Naechstes Preset (auch: Cursor-Rechts)");
    presetNextButton.onClick = [this] { selectRelativePreset (1); };
    addAndMakeVisible (presetNextButton);

    rebuildPresetList();
    updateABDisplay();

    // Modified-Marker (SND-169): jede Parameter-Aenderung markiert das Preset.
    // Der Trigger ist ein Momentary-Parameter und zaehlt nicht als Aenderung.
    for (const auto& parameterId : ParameterIDs::getAllIDs())
    {
        if (parameterId == ParameterIDs::triggerButton)
            continue;

        processor.getAPVTS().addParameterListener (parameterId, this);
    }

    listenersAttached = true;
}

MainComponent::HeaderBar::~HeaderBar()
{
    if (listenersAttached)
    {
        for (const auto& parameterId : ParameterIDs::getAllIDs())
        {
            if (parameterId == ParameterIDs::triggerButton)
                continue;

            processor.getAPVTS().removeParameterListener (parameterId, this);
        }
    }

    setLookAndFeel (nullptr);
}

void MainComponent::HeaderBar::setPatchActive (bool active)
{
    patchButton.setButtonText (active ? "FRONT" : "PATCHBAY");
}

void MainComponent::HeaderBar::setMixerActive (bool active)
{
    mixerButton.setButtonText (active ? "FRONT" : "MIXER");
}

void MainComponent::HeaderBar::setCableCount (int count)
{
    cableLed.setCableCount (count);
}

void MainComponent::HeaderBar::trigger()
{
    triggerClicked();
}

void MainComponent::HeaderBar::selectRelativePreset (int delta)
{
    const int count = presetEntries.size();

    if (count <= 0 || delta == 0)
        return;

    int currentIndex = 0;

    for (int i = 0; i < count; ++i)
    {
        if (presetEntryIds[i] == currentPresetId)
        {
            currentIndex = i;
            break;
        }
    }

    const int nextIndex = ((currentIndex + delta) % count + count) % count;
    loadPresetEntry (presetEntries.getReference (nextIndex));
}

void MainComponent::HeaderBar::rebuildPresetList()
{
    presetEntries = processor.getPresetManager().getPresetEntries();
    presetEntryIds.clearQuick();
    displayedPresetId = 0;

    presetSelector.clear (juce::dontSendNotification);

    juce::String currentCategory;
    int nextId = 1;

    for (const auto& entry : presetEntries)
    {
        // Ordner/Kategorien als Ueberschriften (SND-165).
        if (entry.category != currentCategory)
        {
            currentCategory = entry.category;
            presetSelector.addSectionHeading (currentCategory);
        }

        presetSelector.addItem (entry.name, nextId);
        presetEntryIds.add (nextId);
        ++nextId;
    }

    // Nach dem Neuaufbau das aktuelle Preset wieder markieren (SND-172).
    currentPresetId = hasCurrentPreset ? findPresetItemId (currentPreset) : 0;

    if (currentPresetId > 0)
        currentPresetName = currentPreset.name;

    applyPresetDisplayText();
}

int MainComponent::HeaderBar::findPresetItemId (const PresetManager::Entry& entry) const
{
    for (int i = 0; i < presetEntries.size(); ++i)
    {
        const auto& candidate = presetEntries.getReference (i);

        if (candidate.kind != entry.kind || candidate.factoryIndex != entry.factoryIndex)
            continue;

        if (entry.kind == PresetManager::Entry::Kind::user && candidate.file != entry.file)
            continue;

        return presetEntryIds[i];
    }

    return 0;
}

void MainComponent::HeaderBar::setCurrentEntry (const PresetManager::Entry& entry)
{
    currentPreset = entry;
    hasCurrentPreset = true;
    currentPresetName = entry.name;
    presetModified = false;
    currentPresetId = findPresetItemId (entry);
    applyPresetDisplayText();
    refreshPresetPreview();
}

void MainComponent::HeaderBar::refreshPresetPreview()
{
    int index = PresetThumbnailCache::initThumbnailIndex;

    if (thumbnailCache != nullptr && hasCurrentPreset)
    {
        switch (currentPreset.kind)
        {
            case PresetManager::Entry::Kind::init:    index = PresetThumbnailCache::initThumbnailIndex; break;
            case PresetManager::Entry::Kind::factory: index = currentPreset.factoryIndex + 1;          break;
            case PresetManager::Entry::Kind::user:    index = -1;                                     break;
        }
    }

    thumbnailIndex = index;

    if (index < 0 || thumbnailCache == nullptr)
        presetPreview.setCurve ({});
    else
        presetPreview.setCurve (thumbnailCache->getCurve (index));
}

void MainComponent::HeaderBar::loadPresetEntry (const PresetManager::Entry& entry)
{
    auto& presets = processor.getPresetManager();

    // Parameter-Rueckschreiben des Preset-Loads nicht als User-Aenderung werten.
    suppressModifiedTracking = true;

    bool loaded = false;

    switch (entry.kind)
    {
        case PresetManager::Entry::Kind::init:    loaded = presets.loadInit(); break;
        case PresetManager::Entry::Kind::factory: loaded = presets.loadFactory (entry.factoryIndex); break;
        case PresetManager::Entry::Kind::user:    loaded = presets.loadUser (entry.file); break;
    }

    suppressModifiedTracking = false;

    if (! loaded)
    {
        // Auswahl auf den tatsaechlich klingenden State zuruecksetzen.
        applyPresetDisplayText();
        return;
    }

    setCurrentEntry (entry);
}

void MainComponent::HeaderBar::applyPresetDisplayText()
{
    const juce::String suffix = presetModified ? " *" : "";

    // Alte Markierung entfernen, wenn die angezeigte Auswahl wechselt.
    if (displayedPresetId != 0 && displayedPresetId != currentPresetId)
    {
        const int index = displayedPresetId - 1;

        if (index >= 0 && index < presetEntries.size())
            presetSelector.changeItemText (displayedPresetId,
                                           presetEntries.getReference (index).name);
    }

    if (currentPresetId > 0)
    {
        // Item-Text aktualisieren, damit die Auswahl (currentId) erhalten bleibt.
        presetSelector.changeItemText (currentPresetId,
                                       currentPresetName + suffix);
        presetSelector.setSelectedId (currentPresetId, juce::dontSendNotification);
        displayedPresetId = currentPresetId;
    }
    else
    {
        const juce::String base = currentPresetName.isNotEmpty() ? currentPresetName
                                                                 : juce::String ("NO PRESET");
        presetSelector.setText (base + suffix, juce::dontSendNotification);
        displayedPresetId = 0;
    }
}

void MainComponent::HeaderBar::parameterChanged (const juce::String&, float)
{
    if (suppressModifiedTracking || presetModified)
        return;

    presetModified = true;
    applyPresetDisplayText();
}

void MainComponent::HeaderBar::paint (juce::Graphics& g)
{
    const auto bounds = getLocalBounds().toFloat();

    // Tiefe: Verlauf + Metall-Textur, unten eine harte Schattenkante.
    CERNLookAndFeel::drawBackdrop (g, bounds);

    g.setColour (juce::Colours::white.withAlpha (0.05f));
    g.fillRect (0.0f, 0.0f, bounds.getWidth(), 1.0f);

    g.setColour (juce::Colours::black.withAlpha (0.65f));
    g.fillRect (0.0f, bounds.getBottom() - 5.0f, bounds.getWidth(), 5.0f);

    // Roter Signalstreifen mit Glow: klarster Akzent der Kopfzeile.
    g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.16f));
    g.fillRect (0.0f, bounds.getBottom() - 9.0f, bounds.getWidth(), 6.0f);
    g.setColour (CERNLookAndFeel::accentColour);
    g.fillRect (0.0f, bounds.getBottom() - 3.0f, bounds.getWidth(), 2.0f);

    // Feine Trennlinie hinter dem Logo (Hardware-Panel-Optik).
    if (logo.getWidth() > 0)
    {
        const float x = (float) logo.getRight() + 8.0f;
        g.setColour (CERNLookAndFeel::edgeColour.withAlpha (0.5f));
        g.fillRect (x, 12.0f, 1.0f, bounds.getHeight() - 30.0f);
    }

    CERNLookAndFeel::drawScrew (g, { 8.0f, bounds.getBottom() - 18.0f }, 3.0f);
    CERNLookAndFeel::drawScrew (g, { bounds.getRight() - 10.0f, bounds.getBottom() - 18.0f }, 3.0f);
}

void MainComponent::HeaderBar::resized()
{
    const int h = getHeight();
    const int margin = 12;
    const bool morphVisible = morphSlider.isVisible();
    const bool narrow = useTwoRows (getWidth(), morphVisible);
    const int gap = 6;

    // Prominenter Preset-Browser in der Mitte: < ‹Preset› > + Wellenform-Vorschau
    auto placeBrowser = [&] (int rowYArg, int rowHArg, int leftBound, int rightBound)
    {
        const int freeW = juce::jmax (0, rightBound - leftBound);

        int arrowW = 34, comboW = 116, previewW = 46;

        if (freeW < 250) { arrowW = 30; comboW = 92; previewW = 0; }
        else if (freeW < 300) { arrowW = 32; comboW = 104; previewW = 40; }

        const int previewTotal = previewW > 0 ? previewW + gap : 0;
        const int total = arrowW + gap + comboW + gap + arrowW + previewTotal;
        const int x = leftBound + juce::jmax (0, (freeW - total) / 2);

        presetPrevButton.setBounds (x, rowYArg, arrowW, rowHArg);
        presetSelector.setBounds (x + arrowW + gap, rowYArg, comboW, rowHArg);
        presetNextButton.setBounds (x + arrowW + gap + comboW + gap, rowYArg, arrowW, rowHArg);

        if (previewW > 0)
        {
            presetPreview.setVisible (true);
            presetPreview.setBounds (x + total - previewW, rowYArg, previewW, rowHArg);
        }
        else
        {
            presetPreview.setVisible (false);
        }
    };

    if (narrow)
    {
        // Zwei Reihen: bei 700 px Breite bleibt jedes Control vollstaendig
        // sichtbar, statt Text abzuschneiden.
        const int rowH = 30;
        const int row1Y = 6;
        const int row2Y = h - rowH - 12;
        const int logoH = 36;

        logo.setBounds (margin, 4, LogoComponent::preferredWidthForHeight (logoH), logoH);

        // Reihe 1: Logo links | Preset-Browser mitte | Datei-Aktionen rechts
        int x = getWidth() - margin - 38;
        deleteButton.setBounds (x, row1Y, 38, rowH);

        x -= 44 + gap;
        loadButton.setBounds (x, row1Y, 44, rowH);

        x -= 44 + gap;
        saveButton.setBounds (x, row1Y, 44, rowH);

        placeBrowser (row1Y, rowH, logo.getRight() + 8, x - gap);

        // Reihe 2: Signalweg links, Performance rechts.
        int leftX = margin;
        patchButton.setBounds (leftX, row2Y, 88, rowH);
        leftX += 88 + gap;

        mixerButton.setBounds (leftX, row2Y, 76, rowH);
        leftX += 76 + gap;

        cableLed.setBounds (leftX, row2Y, 50, rowH);
        leftX += 50 + gap;

        triggerButton.setBounds (leftX, row2Y, 84, rowH);
        leftX += 84 + gap;

        if (audioDeviceManager != nullptr)
        {
            audioButton.setBounds (leftX, row2Y, 76, rowH);
            leftX += 76 + gap;
        }

        int rx = getWidth() - margin - 62;
        renderButton.setBounds (rx, row2Y, 62, rowH);

        rx -= 44 + gap;
        rndButton.setBounds (rx, row2Y, 44, rowH);

        rx -= 40 + gap;
        copyButton.setBounds (rx, row2Y, 40, rowH);

        rx -= 32 + gap;
        abButton.setBounds (rx, row2Y, 32, rowH);

        if (morphVisible)
        {
            rx -= 82 + gap;
            morphSlider.setBounds (rx, row2Y + 2, 82, rowH - 4);

            // 40 px: "MORPH" war bei 32 px mit Ellipse abgeschnitten.
            rx -= 40 + gap;
            morphLabel.setBounds (rx, row2Y, 40, rowH);
        }
        else
        {
            morphSlider.setBounds (0, 0, 0, 0);
            morphLabel.setBounds (0, 0, 0, 0);
        }

        return;
    }

    const int rowH = 32;
    const int rowY = (h - rowH) / 2;

    const int logoH = juce::jmin (h - 18, 44);
    logo.setBounds (margin, (h - logoH) / 2, LogoComponent::preferredWidthForHeight (logoH), logoH);

    int leftX = margin + LogoComponent::preferredWidthForHeight (logoH) + 16;
    patchButton.setBounds (leftX, rowY, 96, rowH);
    leftX += 96 + 6;

    mixerButton.setBounds (leftX, rowY, 84, rowH);
    leftX += 84 + 6;

    cableLed.setBounds (leftX, rowY, 54, rowH);
    leftX += 54 + 6;

    triggerButton.setBounds (leftX, rowY, 96, rowH);
    leftX += 96 + 6;

    if (audioDeviceManager != nullptr)
    {
        audioButton.setBounds (leftX, rowY, 84, rowH);
        leftX += 84 + 6;
    }

    // Rechte Seite: Preset-Anzeige + A/B + Save-As/Load/Delete (SND-164/165/167)
    // + RND (SND-184), RENDER (SND-183), MORPH (SND-185, nur bei zwei Slots).
    // Bei sichtbarem Morph weicht die Preset-Beschriftung dem Regler.
    // Combo 112 px: genug Platz fuer "NO PRESET *" und Dropdown-Pfeil ohne Clipping.
    const int buttonW = 52;
    const int smallW  = 42;
    const int iconW   = 46;
    const int renderW = 64;
    const int morphW  = 84;
    const int morphLabelW = 34;

    // Rechte Seite: Datei-Aktionen + Render/Randomize/A-B/Morph
    int x = getWidth() - margin - buttonW;
    loadButton.setBounds (x, rowY, buttonW, rowH);

    x -= buttonW + gap;
    loadButton.setBounds (x, rowY, buttonW, rowH);

    x -= buttonW + gap;
    saveButton.setBounds (x, rowY, buttonW, rowH);

    x -= smallW + gap;
    deleteButton.setBounds (x, rowY, smallW, rowH);

    x -= smallW + gap;
    copyButton.setBounds (x, rowY, smallW, rowH);

    x -= smallW + gap;
    abButton.setBounds (x, rowY, smallW, rowH);

    x -= iconW + gap;
    rndButton.setBounds (x, rowY, iconW, rowH);

    x -= renderW + gap;
    renderButton.setBounds (x, rowY, renderW, rowH);

    if (morphVisible)
    {
        x -= morphW + gap;
        morphSlider.setBounds (x, rowY + 2, morphW, rowH - 4);

        x -= morphLabelW + gap;
        morphLabel.setBounds (x, rowY, morphLabelW, rowH);
    }
    else
    {
        morphSlider.setBounds (0, 0, 0, 0);
        morphLabel.setBounds (0, 0, 0, 0);
    }

    // x ist jetzt die linke Kante des rechten Clusters -> Browser in die Mitte.
    placeBrowser (rowY, rowH, leftX, x - gap);
}

void MainComponent::HeaderBar::triggerClicked()
{
    // Einzige Trigger-Quelle: der Parameter. processBlock erkennt die steigende
    // Flanke und ruft genau einmal setPendingTrigger auf. Kein direkter
    // triggerNote()-Aufruf mehr -> kein doppelter Trigger (Flam).
    if (auto* triggerParam = processor.getAPVTS().getParameter (ParameterIDs::triggerButton))
    {
        // Laeuft bereits ein Puls (Wert noch 1)? Dann nichts tun: ein zweiter
        // Puls wuerde vor dem Reset keine neue Flanke erzeugen und nur den
        // laufenden Puls verlaengern.
        if (triggerParam->getValue() >= 0.5f)
            return;

        triggerParam->setValueNotifyingHost (1.0f);

        juce::Component::SafePointer<HeaderBar> safeThis (this);
        juce::Timer::callAfterDelay (40, [safeThis]
        {
            if (safeThis == nullptr)
                return;

            if (auto* param = safeThis->processor.getAPVTS().getParameter (ParameterIDs::triggerButton))
                param->setValueNotifyingHost (0.0f);
        });
    }
}

void MainComponent::HeaderBar::presetChanged()
{
    const int index = presetEntryIds.indexOf (presetSelector.getSelectedId());

    if (index >= 0 && index < presetEntries.size())
        loadPresetEntry (presetEntries.getReference (index));
}

void MainComponent::HeaderBar::savePresetAs()
{
    const juce::String suggestedName =
        juce::File::createLegalFileName (currentPresetName.isNotEmpty() ? currentPresetName
                                                                       : juce::String ("My Kick"));

    // Zielordner anlegen, damit der Dialog direkt im User-Preset-Ordner startet.
    PresetManager::getUserPresetDirectory().createDirectory();

    fileChooser = std::make_unique<juce::FileChooser> (
        "Save Preset As",
        PresetManager::getUserPresetDirectory().getChildFile (suggestedName + ".xml"),
        "*.xml");

    juce::Component::SafePointer<HeaderBar> safeThis (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::saveMode
                                  | juce::FileBrowserComponent::canSelectFiles
                                  | juce::FileBrowserComponent::warnAboutOverwriting,
                              [safeThis] (const juce::FileChooser& chooser)
                              {
                                  if (safeThis == nullptr)
                                      return;

                                  const auto file = chooser.getResult();

                                  if (file.getFullPathName().isEmpty())
                                      return;

                                  safeThis->savePresetFile (file.withFileExtension (".xml"));
                              });
}

void MainComponent::HeaderBar::savePresetFile (const juce::File& file)
{
    // Nie still ueberschreiben: vorhandene Datei erst bestaetigen lassen.
    if (file.existsAsFile())
    {
        juce::Component::SafePointer<HeaderBar> safeThis (this);

        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::QuestionIcon)
                .withTitle ("Overwrite preset?")
                .withMessage ("\"" + file.getFileName() + "\" already exists.\nOverwrite it?")
                .withButton ("Overwrite", 1)
                .withButton ("Cancel", 0),
            [safeThis, file] (int result)
            {
                if (result == 1 && safeThis != nullptr)
                    safeThis->writePreset (file);
            });

        return;
    }

    writePreset (file);
}

void MainComponent::HeaderBar::writePreset (const juce::File& file)
{
    if (! processor.getPresetManager().saveUser (file))
    {
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::WarningIcon)
                .withTitle ("Save failed")
                .withMessage ("Could not write:\n" + file.getFullPathName())
                .withButton ("OK", 1),
            nullptr);

        return;
    }

    PresetManager::Entry entry;
    entry.kind = PresetManager::Entry::Kind::user;
    entry.name = file.getFileNameWithoutExtension();
    entry.category = "USER";
    entry.file = file;

    rebuildPresetList();     // neue Datei sofort im Browser (SND-165)
    setCurrentEntry (entry); // Combo zeigt das gespeicherte Preset (SND-172)
}

void MainComponent::HeaderBar::deletePreset()
{
    if (! hasCurrentPreset || currentPreset.kind != PresetManager::Entry::Kind::user)
    {
        juce::AlertWindow::showAsync (
            juce::MessageBoxOptions()
                .withIconType (juce::MessageBoxIconType::InfoIcon)
                .withTitle ("Delete Preset")
                .withMessage ("Only user presets can be deleted. Select a preset from the "
                              "\"USER\" folders first.")
                .withButton ("OK", 1),
            nullptr);

        return;
    }

    const auto file = currentPreset.file;
    const auto name = currentPresetName;
    juce::Component::SafePointer<HeaderBar> safeThis (this);

    juce::AlertWindow::showAsync (
        juce::MessageBoxOptions()
            .withIconType (juce::MessageBoxIconType::QuestionIcon)
            .withTitle ("Delete preset?")
            .withMessage ("Delete \"" + name + "\"?\n" + file.getFullPathName())
            .withButton ("Delete", 1)
            .withButton ("Cancel", 0),
        [safeThis, file] (int result)
        {
            if (result != 1 || safeThis == nullptr)
                return;

            if (! safeThis->processor.getPresetManager().deleteUserPreset (file))
                return;

            // Der Klang bleibt geladen; nur die Zuordnung zur Datei entfaellt.
            safeThis->hasCurrentPreset = false;
            safeThis->currentPreset = {};
            safeThis->currentPresetName.clear();
            safeThis->presetModified = false;
            safeThis->rebuildPresetList();
        });
}

void MainComponent::HeaderBar::loadPresetFromDisk()
{
    PresetManager::getUserPresetDirectory().createDirectory();

    fileChooser = std::make_unique<juce::FileChooser> (
        "Load Preset", PresetManager::getUserPresetDirectory(), "*.xml");

    juce::Component::SafePointer<HeaderBar> safeThis (this);

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode
                                  | juce::FileBrowserComponent::canSelectFiles,
                              [safeThis] (const juce::FileChooser& chooser)
                              {
                                  if (safeThis == nullptr)
                                      return;

                                  const auto file = chooser.getResult();

                                  if (file.existsAsFile())
                                      safeThis->loadUserPreset (file);
                              });
}

void MainComponent::HeaderBar::loadUserPreset (const juce::File& file)
{
    suppressModifiedTracking = true;
    const bool loaded = processor.getPresetManager().loadUser (file);
    suppressModifiedTracking = false;

    if (! loaded)
        return;

    PresetManager::Entry entry;
    entry.kind = PresetManager::Entry::Kind::user;
    entry.name = file.getFileNameWithoutExtension();
    entry.category = "USER";
    entry.file = file;

    setCurrentEntry (entry);
}

void MainComponent::HeaderBar::toggleAB()
{
    auto& presets = processor.getPresetManager();

    abStateXml[(size_t) activeABSlot] = presets.getCurrentStateXml();
    abModified[(size_t) activeABSlot] = presetModified;

    const int target = 1 - activeABSlot;

    // Leerer Ziel-Slot startet mit dem aktuellen State (kein Sprung beim Umschalten).
    if (abStateXml[(size_t) target].isEmpty())
    {
        abStateXml[(size_t) target] = abStateXml[(size_t) activeABSlot];
        abModified[(size_t) target] = abModified[(size_t) activeABSlot];
    }

    suppressModifiedTracking = true;
    const bool loaded = presets.setCurrentStateXml (abStateXml[(size_t) target]);
    suppressModifiedTracking = false;

    if (! loaded)
        return;

    activeABSlot = target;
    presetModified = abModified[(size_t) activeABSlot];
    updateABDisplay();
    applyPresetDisplayText();
}

void MainComponent::HeaderBar::copyAB()
{
    const int target = 1 - activeABSlot;

    abStateXml[(size_t) target] = processor.getPresetManager().getCurrentStateXml();
    abModified[(size_t) target] = presetModified;
    updateABDisplay();
}

void MainComponent::HeaderBar::updateABDisplay()
{
    abButton.setButtonText (activeABSlot == 0 ? "A" : "B");
    copyButton.setButtonText (activeABSlot == 0 ? "A>B" : "B>A");

    // SND-185: Bei Slot-Wechsel steht der Morph auf dem Endpunkt (0 = A, 100 = B).
    if (bothABSlotsStored())
        morphSlider.setValue ((double) activeABSlot * 100.0, juce::dontSendNotification);

    updateMorphVisibility();
}

bool MainComponent::HeaderBar::bothABSlotsStored() const
{
    return abStateXml[0].isNotEmpty() && abStateXml[1].isNotEmpty();
}

void MainComponent::HeaderBar::updateMorphVisibility()
{
    const bool visible = bothABSlotsStored();
    morphSlider.setVisible (visible);
    morphLabel.setVisible (visible);

    // Platzbedarf aendert sich -> Neuaufbau des Header-Layouts. Der
    // MainComponent-Aufbau muss ebenfalls laufen: die Kopfzeile kann von
    // einer auf zwei Reihen wechseln (useTwoRows).
    resized();

    if (auto* owner = getParentComponent())
        owner->resized();
}

namespace
{
/** Ein float-/int-Parameter samt Plausibilitaetsbereich (denormalisiert). */
struct RandomTarget
{
    const char* id;
    float minValue;
    float maxValue;
};

// SND-184: konservative Bereiche; Pitch-Parameter sind bewusst NICHT gelistet.
constexpr RandomTarget randomTargets[] =
{
    { ParameterIDs::membraneTension,      0.35f, 0.85f },
    { ParameterIDs::membraneRadius,       0.10f, 0.35f },
    { ParameterIDs::membraneMass,         0.4f,  1.5f  },
    { ParameterIDs::membraneDamping,      0.15f, 0.60f },
    { ParameterIDs::membraneModes,        4.0f,  12.0f },
    { ParameterIDs::membraneNonlinearity, 0.0f,  0.5f  },
    { ParameterIDs::membraneStiffness,    0.0f,  0.6f  },
    { ParameterIDs::tubeResonanceFreq,    60.0f, 180.0f },
    { ParameterIDs::tubeResonanceQ,       2.0f,  12.0f },
    { ParameterIDs::tubeResonanceLevel,   0.15f, 0.70f },
    { ParameterIDs::tubeCoupling,         0.10f, 0.60f },
    { ParameterIDs::hollowMetallic,       0.0f,  0.60f },
    { ParameterIDs::clickLevel,           0.35f, 1.20f },
    { ParameterIDs::clickDecay,           2.0f,  8.0f  },
    { ParameterIDs::clickTone,            0.35f, 0.75f },
    { ParameterIDs::clickDistortion,      0.0f,  0.45f },
    { ParameterIDs::bodyLevel,            0.25f, 0.80f },
    { ParameterIDs::bodyAttack,           1.0f,  5.0f  },
    { ParameterIDs::bodyDecay,            180.0f, 900.0f },
    { ParameterIDs::bodyTuning,           0.8f,  1.3f  },
    { ParameterIDs::subLevel,             0.25f, 0.85f },
    { ParameterIDs::subDecay,             250.0f, 1500.0f },
    { ParameterIDs::subDetune,            -12.0f, 12.0f },
    { ParameterIDs::driveAmount,          0.15f, 0.65f },
    { ParameterIDs::driveMix,             0.35f, 0.80f },
    { ParameterIDs::drivePreHP,           0.15f, 0.45f },
    { ParameterIDs::eqSubFreq,            40.0f, 80.0f },
    { ParameterIDs::eqSubGain,            -3.0f, 4.0f  },
    { ParameterIDs::eqPunchFreq,          1500.0f, 4000.0f },
    { ParameterIDs::eqPunchGain,          -2.0f, 4.0f  },
    { ParameterIDs::eqAirFreq,            7000.0f, 12000.0f },
    { ParameterIDs::eqAirGain,            -2.0f, 4.0f  },
    { ParameterIDs::eqRumbleHP,           20.0f, 35.0f },
    { ParameterIDs::compRatio,            2.0f,  5.0f  },
    { ParameterIDs::outputWidth,          0.35f, 1.0f  },
    { ParameterIDs::roomLevel,            0.05f, 0.30f },
    { ParameterIDs::roomDecay,            150.0f, 450.0f },
    { ParameterIDs::roomRumble,           0.0f,  0.5f  },
    { ParameterIDs::reverbPreDelay,       0.0f,  30.0f },
    { ParameterIDs::reverbSize,           0.25f, 0.75f },
    { ParameterIDs::reverbDamping,        0.35f, 0.65f },
    { ParameterIDs::reverbDecay,          500.0f, 2500.0f },
    { ParameterIDs::reverbMix,            0.03f, 0.22f },
    { ParameterIDs::materialRoomSize,     3.0f,  16.0f },
    { ParameterIDs::materialRoomMix,      0.05f, 0.35f },
    { ParameterIDs::materialRoomWidth,    0.3f,  1.0f  },
    { ParameterIDs::grumbleLevel,         0.0f,  0.35f },
    { ParameterIDs::grumbleFreq,          30.0f, 70.0f },
    { ParameterIDs::grumbleRate,          0.3f,  2.0f  },
    { ParameterIDs::grumbleDepth,         0.15f, 0.5f  },
    { ParameterIDs::grumbleDecay,         0.3f,  1.2f  },
};
} // namespace

void MainComponent::HeaderBar::randomizeSound()
{
    auto& apvts = processor.getAPVTS();
    juce::Random random;

    // Bewusst NICHT angefasst (Tuning-Sicherheit): fundamentalHz, rootNote,
    // keyRoot, scale, glideMs, velToPitch, stepPitch_*, pitchEnvDepth/Decay
    // (und pitchEnvGravity etc.). Auch die Trigger-Parameter bleiben aus.
    for (const auto& target : randomTargets)
    {
        if (auto* parameter = apvts.getParameter (target.id))
        {
            const float value = target.minValue
                + random.nextFloat() * (target.maxValue - target.minValue);
            parameter->setValueNotifyingHost (parameter->convertTo0to1 (value));
        }
    }

    if (onStatus != nullptr)
        onStatus ("RANDOMIZED (tuning safe)");
}

void MainComponent::HeaderBar::applyMorph (float morphPercent)
{
    if (! bothABSlotsStored())
        return; // Ohne zwei Slots gibt es nichts zu interpolieren.

    const auto xmlA = juce::parseXML (abStateXml[0]);
    const auto xmlB = juce::parseXML (abStateXml[1]);

    if (xmlA == nullptr || xmlB == nullptr)
        return;

    // Denormalisierte PARAM-Werte je Slot einsammeln (Patch-Knoten werden ignoriert:
    // der Morph interpoliert nur Parameter; A/B bleibt der Patch-Schalter).
    auto collectValues = [] (const juce::XmlElement& root)
    {
        std::map<juce::String, float> values;

        for (auto* child : root.getChildIterator())
            if (child->hasTagName ("PARAM"))
            {
                const auto id = child->getStringAttribute ("id");

                if (id.isNotEmpty())
                    values[id] = (float) child->getDoubleAttribute ("value");
            }

        return values;
    };

    const auto valuesA = collectValues (*xmlA);
    const auto valuesB = collectValues (*xmlB);

    const float t = juce::jlimit (0.0f, 1.0f, morphPercent * 0.01f);
    const bool useB = t >= 0.5f; // Bools/Choices kippen bei 50 % zum naecheren Zustand

    auto& apvts = processor.getAPVTS();

    for (const auto& [id, denormA] : valuesA)
    {
        const auto itB = valuesB.find (id);

        if (itB == valuesB.end())
            continue; // Parameter nur in A vorhanden -> unveraendert lassen

        if (id == ParameterIDs::triggerButton)
            continue; // Momentary, kein Zustand

        auto* parameter = apvts.getParameter (id);

        if (parameter == nullptr)
            continue; // z. B. noch nicht angelegte IDs tolerieren

        const float denormB = itB->second;

        if (dynamic_cast<juce::AudioParameterBool*> (parameter) != nullptr
            || dynamic_cast<juce::AudioParameterChoice*> (parameter) != nullptr)
        {
            parameter->setValueNotifyingHost (
                parameter->convertTo0to1 (useB ? denormB : denormA));
            continue;
        }

        // Float und Int: linear interpolieren (Int-Ranges quantisieren selbst).
        const float denorm = denormA + (denormB - denormA) * t;
        parameter->setValueNotifyingHost (parameter->convertTo0to1 (denorm));
    }
}

void MainComponent::HeaderBar::renderBounce()
{
    // Offline-Bounce ohne neue Processor-API: ein zweiter CERNAudioProcessor
    // rendert den aktuellen State (State als XML kopiert) genau einen Takt.
    CERNAudioProcessor bounce;

    const auto stateXml = processor.getPresetManager().getCurrentStateXml();

    if (stateXml.isEmpty())
    {
        if (onStatus != nullptr) onStatus ("RENDER FAILED");
        return;
    }

    const double sampleRate = processor.getSampleRate() > 0.0 ? processor.getSampleRate()
                                                              : 48000.0;
    const int blockSize = 512;

    bounce.prepareToPlay (sampleRate, blockSize);

    if (! bounce.getPresetManager().setCurrentStateXml (stateXml))
    {
        if (onStatus != nullptr) onStatus ("RENDER FAILED");
        return;
    }

    const auto& bounceState = bounce.getAPVTS();

    float bpm = 120.0f;

    if (const auto* bpmParam = bounceState.getRawParameterValue (ParameterIDs::sequencerBpm))
        bpm = juce::jlimit (20.0f, 400.0f, bpmParam->load());

    float rootNote = 30.0f;

    if (const auto* rootParam = bounceState.getRawParameterValue (ParameterIDs::rootNote))
        rootNote = rootParam->load();

    constexpr int beatsPerBar = 4;
    const int totalSamples = (int) std::ceil (sampleRate * beatsPerBar * 60.0 / (double) bpm);

    juce::AudioBuffer<float> output (2, totalSamples);
    output.clear();

    juce::AudioBuffer<float> block (2, blockSize);
    int written = 0;

    while (written < totalSamples)
    {
        const int numSamples = juce::jmin (blockSize, totalSamples - written);

        if (numSamples != block.getNumSamples())
            block.setSize (2, numSamples, false, false, true);

        block.clear();

        juce::MidiBuffer midi;

        if (written == 0)
            midi.addEvent (juce::MidiMessage::noteOn (1, (int) std::lround (rootNote),
                                                      (juce::uint8) 127), 0);

        bounce.processBlock (block, midi);

        for (int channel = 0; channel < 2; ++channel)
            output.copyFrom (channel, written, block, channel, 0, numSamples);

        written += numSamples;
    }

    bounce.releaseResources();

    const auto target = juce::File::getSpecialLocation (juce::File::userDocumentsDirectory)
                            .getChildFile ("C.E.R.N-Bounce.wav");

    // createOutputStream() haengt nur an; fuer Ueberschreiben vorher trunkieren.
    if (target.existsAsFile())
        target.deleteFile();

    std::unique_ptr<juce::OutputStream> stream (target.createOutputStream());

    if (stream == nullptr)
    {
        if (onStatus != nullptr) onStatus ("RENDER FAILED");
        return;
    }

    juce::WavAudioFormat wavFormat;

    const auto writerOptions = juce::AudioFormatWriterOptions()
                                   .withSampleRate (sampleRate)
                                   .withNumChannels (2)
                                   .withBitsPerSample (16);

    auto writer = wavFormat.createWriterFor (stream, writerOptions);

    if (writer == nullptr)
    {
        if (onStatus != nullptr) onStatus ("RENDER FAILED");
        return;
    }

    if (! writer->writeFromAudioSampleBuffer (output, 0, output.getNumSamples()))
    {
        if (onStatus != nullptr) onStatus ("RENDER FAILED");
        return;
    }

    if (onStatus != nullptr)
        onStatus ("BOUNCE SAVED: ~/Documents/C.E.R.N-Bounce.wav");
}

//==============================================================================
void MainComponent::HeaderBar::CableLed::setCableCount (int count)
{
    const int clamped = juce::jmax (0, count);

    if (clamped == cableCount)
        return;

    cableCount = clamped;
    labelText = "CV " + juce::String (cableCount);
    repaint();
}

void MainComponent::HeaderBar::CableLed::paint (juce::Graphics& g)
{
    const float diameter = 10.0f;
    const float cx = 6.0f;
    const float cy = (float) getHeight() * 0.5f;

    const bool lit = cableCount > 0;

    g.setColour (CERNLookAndFeel::knobColour);
    g.fillEllipse (cx, cy - diameter * 0.5f, diameter, diameter);

    g.setColour (lit ? CERNLookAndFeel::accentColour
                     : CERNLookAndFeel::knobColour.brighter (0.15f));
    g.fillEllipse (cx + 2.0f, cy - diameter * 0.5f + 2.0f, diameter - 4.0f, diameter - 4.0f);

    g.setColour (CERNLookAndFeel::textColour.withAlpha (lit ? 0.9f : 0.45f));
    g.setFont (CERNLookAndFeel::getValueFont());
    g.drawText (labelText, (int) (cx + diameter + 3.0f), 0,
                juce::jmax (0, getWidth() - (int) (cx + diameter + 3.0f)), getHeight(),
                juce::Justification::centredLeft, false);
}

//==============================================================================
// FooterSequencer
MainComponent::FooterSequencer::FooterSequencer (CERNAudioProcessor& p)
    : processor (p)
{
    static CERNLookAndFeel customLookAndFeel;
    setLookAndFeel (&customLookAndFeel);

    auto& apvts = processor.getAPVTS();

    sectionLabel.setText ("SEQUENCER", juce::dontSendNotification);
    sectionLabel.setFont (CERNLookAndFeel::getSectionTitleFont());
    sectionLabel.setColour (juce::Label::textColourId, CERNLookAndFeel::textColour);
    addAndMakeVisible (sectionLabel);

    playButton.setClickingTogglesState (true);
    addAndMakeVisible (playButton);
    playAttachment = std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
        apvts, ParameterIDs::sequencerPlaying, playButton);

    auto configureLabel = [this] (juce::Label& label, const juce::String& text)
    {
        label.setText (text, juce::dontSendNotification);
        label.setJustificationType (juce::Justification::centredRight);
        label.setFont (CERNLookAndFeel::getNameFont());
        label.setColour (juce::Label::textColourId,
                         CERNLookAndFeel::textColour.withAlpha (0.75f));
        addAndMakeVisible (label);
    };

    configureLabel (bpmLabel, "BPM");
    configureLabel (swingLabel, "SWING");
    configureLabel (stepsLabel, "STEPS");

    // BPM als editierbares Eingabefeld (statt Slider)
    bpmInput.setJustificationType (juce::Justification::centred);
    bpmInput.setFont (CERNLookAndFeel::getNameFont());
    bpmInput.setColour (juce::Label::textColourId, CERNLookAndFeel::textColour);
    bpmInput.setColour (juce::Label::backgroundColourId, CERNLookAndFeel::knobColour.withAlpha (0.3f));
    bpmInput.setColour (juce::Label::outlineColourId, CERNLookAndFeel::accentColour.withAlpha (0.5f));
    bpmInput.setEditable (true, false, false);
    bpmInput.setMinimumHorizontalScale (1.0f);
    bpmInput.onTextChange = [this] {
        const auto text = bpmInput.getText().trim();
        if (text.isNotEmpty())
        {
            const double value = text.getDoubleValue(); // User gibt BPM ein (denormalisiert)
            if (value >= 40.0 && value <= 240.0) // Parameter-Range: 40..240 BPM
            {
                if (auto* param = processor.getAPVTS().getParameter (ParameterIDs::sequencerBpm))
                {
                    // Korrekte Normierung für skewed Range (skew=0.5)
                    const float normalised = param->getNormalisableRange().convertTo0to1 ((float) value);
                    param->setValueNotifyingHost (normalised);
                }
            }
        }
    };
    addAndMakeVisible (bpmInput);

    auto configureSlider = [this] (juce::Slider& slider)
    {
        slider.setSliderStyle (juce::Slider::LinearHorizontal);
        slider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 58, 20);
        addAndMakeVisible (slider);
    };

    configureSlider (swingSlider);
    configureSlider (stepsSlider);

    swingAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParameterIDs::sequencerSwing, swingSlider);
    stepsAttachment = std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
        apvts, ParameterIDs::sequencerSteps, stepsSlider);

    // BPM-Parameter initial in das Input-Feld schreiben
    bpmInput.setText (juce::String (apvts.getRawParameterValue (ParameterIDs::sequencerBpm)->load(), 1), juce::dontSendNotification);

    for (int i = 0; i < 16; ++i)
    {
        auto gate = std::make_unique<juce::ToggleButton> (juce::String (i + 1));
        gate->getProperties().set ("compact", true);
        gate->setClickingTogglesState (true);
        addAndMakeVisible (*gate);

        gateAttachments.push_back (
            std::make_unique<juce::AudioProcessorValueTreeState::ButtonAttachment> (
                apvts, ParameterIDs::stepGate (i), *gate));

        gateButtons.push_back (std::move (gate));

        auto velocity = std::make_unique<juce::Slider> (juce::Slider::LinearBarVertical,
                                                        juce::Slider::NoTextBox);
        addAndMakeVisible (*velocity);

        velocityAttachments.push_back (
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                apvts, ParameterIDs::stepVel (i), *velocity));

        velocitySliders.push_back (std::move (velocity));

        // Dritte Reihe: Step-Pitch (SND-139)
        auto pitch = std::make_unique<juce::Slider> (juce::Slider::LinearBarVertical,
                                                     juce::Slider::NoTextBox);
        pitch->setTooltip ("Step " + juce::String (i + 1) + " pitch (semitones)");
        pitch->setDoubleClickReturnValue (true, 0.0);
        addAndMakeVisible (*pitch);

        pitchAttachments.push_back (
            std::make_unique<juce::AudioProcessorValueTreeState::SliderAttachment> (
                apvts, ParameterIDs::stepPitch (i), *pitch));

        pitchSliders.push_back (std::move (pitch));
    }

    startTimerHz (30);
}

MainComponent::FooterSequencer::~FooterSequencer()
{
    stopTimer();
    setLookAndFeel (nullptr);
}

void MainComponent::FooterSequencer::paint (juce::Graphics& g)
{
    CERNLookAndFeel::drawBevel (g, getLocalBounds().toFloat(), 0.0f, true, 0.07f);
    g.setColour (juce::Colours::white.withAlpha (0.06f));
    g.fillRect (0, 0, getWidth(), 1);
    g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.16f));
    g.fillRect (0, 1, getWidth(), 3);
    g.setColour (CERNLookAndFeel::accentColour);
    g.fillRect (0, 1, juce::jmin (180, getWidth()), 2);

    // Playhead-Spalte hinter den Step-Controls
    const int margin = 12;
    const int cellWidth = (getWidth() - 2 * margin) / 16;

    if (cellWidth > 0)
    {
        const int x = margin + currentStep * cellWidth;
        g.setColour (CERNLookAndFeel::accentColour.withAlpha (playing ? 0.20f : 0.07f));
        g.fillRect (x, 44, cellWidth - 3, getHeight() - 50);
    }

    // Reihenbeschriftung: Gates sind nummeriert (1-16), Velocity/Pitch als schmale Labels.
    g.setColour (CERNLookAndFeel::textColour.withAlpha (0.7f));
    g.setFont (CERNLookAndFeel::getNameFont());
    const int labelW = juce::jmax (0, margin - 1);
    g.drawFittedText ("VEL", 0, 82, labelW, 70,
                      juce::Justification::centred, 1, 0.7f);
    g.drawFittedText ("PITCH", 0, 156, labelW, 52,
                      juce::Justification::centred, 1, 0.7f);
}

void MainComponent::FooterSequencer::resized()
{
    const int w = getWidth();
    const int margin = 12;

    const int rowY = 10;
    const int rowH = 28;

    sectionLabel.setBounds (margin, 8, 120, rowH);

    playButton.setBounds (margin + 130, rowY, 84, rowH);

    // Bei schmalen Fenstern werden die Regler kuerzer, statt rechts
    // aus dem Fenster zu laufen (Mindestbreite 700).
    const bool narrow = w < 900;
    const int bpmW   = narrow ? 104 : 150;
    const int swingW = narrow ? 84 : 140;
    const int stepsW = narrow ? 66 : 100;

    int x = margin + 226;
    bpmLabel.setBounds (x, rowY, 34, rowH);
    bpmInput.setBounds (x + 38, rowY, bpmW, rowH);

    x += 48 + bpmW;
    swingLabel.setBounds (x, rowY, 44, rowH);
    swingSlider.setBounds (x + 48, rowY, swingW, rowH);

    x += 60 + swingW;
    stepsLabel.setBounds (x, rowY, 44, rowH);
    stepsSlider.setBounds (x + 48, rowY, stepsW, rowH);

    const int cellWidth = (w - 2 * margin) / 16;

    for (int i = 0; i < 16; ++i)
    {
        const int cellX = margin + i * cellWidth;

        if (i < (int) gateButtons.size())
            gateButtons[(size_t) i]->setBounds (cellX, 44, cellWidth - 3, 34);

        if (i < (int) velocitySliders.size())
            velocitySliders[(size_t) i]->setBounds (cellX, 82, cellWidth - 3, 70);

        if (i < (int) pitchSliders.size())
            pitchSliders[(size_t) i]->setBounds (cellX, 156, cellWidth - 3, 52);
    }
}

void MainComponent::FooterSequencer::togglePlayback()
{
    // triggerClick() laeuft ueber die ButtonAttachment -> APVTS bleibt Wahrheit.
    playButton.triggerClick();
}

void MainComponent::FooterSequencer::timerCallback()
{
    auto& sequencer = processor.getSequencer();

    const int step = sequencer.getCurrentStep();
    // GUI-seitig ist die APVTS-Wahrheit massgeblich (funktioniert auch,
    // wenn der Audio-Callback gerade nicht läuft).
    const bool isPlaying = playButton.getToggleState();

    playButton.setButtonText (isPlaying ? "STOP" : "PLAY");

    // BPM-Input mit Parameter synchron halten (für Automation/Preset-Load)
    const float bpmValue = processor.getAPVTS().getRawParameterValue (ParameterIDs::sequencerBpm)->load();
    const juce::String bpmText = juce::String (bpmValue, 1);
    if (bpmInput.getText() != bpmText && ! bpmInput.isBeingEdited())
        bpmInput.setText (bpmText, juce::dontSendNotification);

    if (step != currentStep || isPlaying != playing)
    {
        const int previousStep = currentStep;
        currentStep = step;
        playing = isPlaying;

        // Playhead-Markierung auf den Step-Buttons aktualisieren
        if (juce::isPositiveAndBelow (previousStep, (int) gateButtons.size()))
        {
            gateButtons[(size_t) previousStep]->getProperties().set ("playhead", false);
            gateButtons[(size_t) previousStep]->repaint();
        }

        if (playing && juce::isPositiveAndBelow (currentStep, (int) gateButtons.size()))
        {
            gateButtons[(size_t) currentStep]->getProperties().set ("playhead", true);
            gateButtons[(size_t) currentStep]->repaint();
        }

        repaint();
    }
}

//==============================================================================
// MacroPanel (SND-162)
namespace
{
    struct MacroTarget
    {
        const char* parameterId;
        float fromValue; ///< Parameterwert bei Makro 0 %
        float toValue;   ///< Parameterwert bei Makro 100 %
    };

    struct MacroDefinition
    {
        const char* name;
        const char* description;
        std::array<MacroTarget, 3> targets;
    };

    constexpr int numMacroDefinitions = 5; ///< == MainComponent::MacroPanel::numMacros

    /** Dokumentiertes Makro-Mapping auf bestehende Parameter.
        Die mittlere Stellung (50 %) liegt jeweils nahe an den Werk-Defaults. */
    const std::array<MacroDefinition, numMacroDefinitions>& macroDefinitions()
    {
        static const std::array<MacroDefinition, numMacroDefinitions> definitions
        {{
            { "TUNE", "Tune: fundamental (30-90 Hz), body tuning (0.75-1.25), sub detune (-12..+12 ct)",
              {{ { ParameterIDs::fundamentalHz, 30.0f,  90.0f },
                 { ParameterIDs::bodyTuning,     0.75f,  1.25f },
                 { ParameterIDs::subDetune,    -12.0f,  12.0f } }} },

            { "PUNCH", "Punch: EQ punch gain (-4..+10 dB), click level (0.15-0.9), comp attack (15-1 ms)",
              {{ { ParameterIDs::eqPunchGain,   -4.0f,  10.0f },
                 { ParameterIDs::clickLevel,      0.15f,  0.90f },
                 { ParameterIDs::compAttack,     15.0f,   1.0f } }} },

            { "BOOM", "Boom: EQ sub gain (-2..+8 dB), body level (0.3-1.0), body decay (100-800 ms)",
              {{ { ParameterIDs::eqSubGain,     -2.0f,   8.0f },
                 { ParameterIDs::bodyLevel,       0.30f,  1.00f },
                 { ParameterIDs::bodyDecay,     100.0f, 800.0f } }} },

            { "DIRT", "Dirt: drive amount (0-1), drive mix (0.1-0.9), click distortion (0-1)",
              {{ { ParameterIDs::driveAmount,    0.0f,   1.0f },
                 { ParameterIDs::driveMix,        0.10f,  0.90f },
                 { ParameterIDs::clickDistortion, 0.0f,   1.0f } }} },

            { "SPACE", "Space: room level (0-0.6), reverb mix (0-0.5), reverb size (0.2-0.9)",
              {{ { ParameterIDs::roomLevel,      0.0f,   0.60f },
                 { ParameterIDs::reverbMix,       0.0f,   0.50f },
                 { ParameterIDs::reverbSize,      0.20f,  0.90f } }} }
        }};

        return definitions;
    }
}

MainComponent::MacroPanel::MacroPanel (CERNAudioProcessor& p)
    : processor (p)
{
    static_assert (numMacros == numMacroDefinitions, "macro table size mismatch");

    static CERNLookAndFeel customLookAndFeel;
    setLookAndFeel (&customLookAndFeel);

    setTitle ("Macros");
    setDescription ("Performance macros: Tune, Punch, Boom, Dirt, Space");

    const auto& definitions = macroDefinitions();

    for (int i = 0; i < numMacros; ++i)    {
        auto& macro = macros[(size_t) i];
        const auto& definition = definitions[(size_t) i];

        macro.name = definition.name;
        macro.description = definition.description;

        macro.slider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        macro.slider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        macro.slider.setRotaryParameters (juce::MathConstants<float>::pi * 1.25f,
                                          juce::MathConstants<float>::pi * 2.75f,
                                          true);
        macro.slider.setRange (0.0, 1.0, 0.01);
        macro.slider.setValue (0.5, juce::dontSendNotification); // erst beim Anfassen anwenden
        macro.slider.setMouseDragSensitivity (160);
        macro.slider.setTitle (macro.name);
        macro.slider.setDescription (macro.description);
        macro.slider.setTooltip (macro.description);
        macro.slider.onValueChange = [this, i]
        {
            applyMacro (i, (float) macros[(size_t) i].slider.getValue());
        };
        addAndMakeVisible (macro.slider);

        macro.nameLabel.setText (macro.name, juce::dontSendNotification);
        macro.nameLabel.setJustificationType (juce::Justification::centredLeft);
        macro.nameLabel.setFont (CERNLookAndFeel::getSectionTitleFont());
        macro.nameLabel.setColour (juce::Label::textColourId, CERNLookAndFeel::textColour);
        addAndMakeVisible (macro.nameLabel);

        macro.valueLabel.setText ("50 %", juce::dontSendNotification);
        macro.valueLabel.setJustificationType (juce::Justification::centredLeft);
        macro.valueLabel.setFont (CERNLookAndFeel::getValueFont());
        // Helles Signal-Rot: #e10600 liegt auf dem dunklen Panel nur bei ~3.8:1.
        macro.valueLabel.setColour (juce::Label::textColourId,
                                    CERNLookAndFeel::accentBrightColour);
        addAndMakeVisible (macro.valueLabel);
    }
}

MainComponent::MacroPanel::~MacroPanel()
{
    setLookAndFeel (nullptr);
}

void MainComponent::MacroPanel::updateValueLabel (int index, float value)
{
    macros[(size_t) index].valueLabel.setText (
        juce::String (juce::roundToInt (value * 100.0f)) + " %",
        juce::dontSendNotification);
}

void MainComponent::MacroPanel::applyMacro (int index, float value)
{
    const auto& definition = macroDefinitions()[(size_t) index];
    auto& apvts = processor.getAPVTS();

    for (const auto& target : definition.targets)
    {
        auto* parameter = apvts.getParameter (target.parameterId);

        if (parameter == nullptr)
            continue;

        const float realValue = target.fromValue + value * (target.toValue - target.fromValue);
        const float normalised = parameter->getNormalisableRange().convertTo0to1 (realValue);

        parameter->beginChangeGesture();
        parameter->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, normalised));
        parameter->endChangeGesture();
    }

    // Die Makro-Position wird zusätzlich in die APVTS geschrieben: "macro1..5"
    // ist die automatisierbare/persistente Quelle für Kabel von MACRO n im
    // Parameter-Patch (SND-157). Der Wert 0..1 wird 1:1 gespiegelt; die
    // Parameter-Abbildung oben bleibt davon unabhängig.
    if (auto* macroSource = apvts.getParameter (ParameterIDs::macro (index + 1)))
    {
        macroSource->beginChangeGesture();
        macroSource->setValueNotifyingHost (juce::jlimit (0.0f, 1.0f, value));
        macroSource->endChangeGesture();
    }

    updateValueLabel (index, value);
}

void MainComponent::MacroPanel::paint (juce::Graphics& g)
{
    // Titel separat (vorberechneter String -> keine Allokation im paint()).
    CERNLookAndFeel::drawSectionPanel (g, 0, 0, getWidth(), getHeight(), {});

    g.setColour (CERNLookAndFeel::accentColour);
    g.fillEllipse (12.0f, 11.0f, 6.0f, 6.0f);
    g.setColour (CERNLookAndFeel::accentBrightColour.withAlpha (0.35f));
    g.drawEllipse (10.5f, 9.5f, 9.0f, 9.0f, 1.0f);

    g.setColour (CERNLookAndFeel::textColour);
    g.setFont (CERNLookAndFeel::getSectionTitleFont());
    g.drawText (upperTitle, 26, 6, juce::jmax (0, getWidth() - 38), 20,
                juce::Justification::left, true);
}

void MainComponent::MacroPanel::resized()
{
    auto area = getLocalBounds().reduced (10, 8);
    area.removeFromTop (20); // Titelzeile von drawSectionPanel

    const int cellWidth = juce::jmax (1, area.getWidth() / numMacros);

    for (int i = 0; i < numMacros; ++i)
    {
        auto cell = area.withX (area.getX() + i * cellWidth).withWidth (cellWidth)
                        .reduced (4, 0);

        auto& macro = macros[(size_t) i];

        const int knobSide = juce::jmin (cell.getHeight(), 54);
        macro.slider.setBounds (cell.removeFromLeft (knobSide)
                                    .withSizeKeepingCentre (knobSide, knobSide));

        auto textArea = cell.reduced (6, 0);
        macro.nameLabel.setBounds (textArea.removeFromTop (textArea.getHeight() / 2));
        macro.valueLabel.setBounds (textArea);
    }
}
