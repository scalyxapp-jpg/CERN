#include "ParameterPatchView.h"

#include "Parameters/ParameterIDs.h"
#include "GUI/Theme/CERNLookAndFeel.h"
#include "GUI/PowerSwitch.h"
#include "GUI/ParamKnob.h"

#include <array>
#include <cmath>
#include <map>
#include <set>
#include <vector>

namespace
{
    constexpr int rowHeight  = 30;
    constexpr int headerHeight = 34;
    constexpr int groupGap = 5;
    constexpr int inspectorHeight = 72;

    /** Rohen (denormalisierten) APVTS-Wert lesen; Fallback fuer kaputte States. */
    float readRawParameter (juce::AudioProcessorValueTreeState& apvts,
                            const char* id,
                            float fallback)
    {
        if (const auto* raw = apvts.getRawParameterValue (id))
            return raw->load();

        return fallback;
    }

    /** Plain-Wert in die APVTS schreiben (host-sichtbar, Preset-persistent). */
    void writeRawParameter (juce::AudioProcessorValueTreeState& apvts,
                            const char* id,
                            float plainValue)
    {
        if (auto* parameter = apvts.getParameter (id))
            parameter->setValueNotifyingHost (
                juce::jlimit (0.0f, 1.0f, parameter->convertTo0to1 (plainValue)));
    }

    struct ParameterGroup
    {
        const char* name;
        std::vector<juce::String> ids;
    };

    std::vector<ParameterGroup> makeGroups()
    {
        namespace P = ParameterIDs;

        std::vector<ParameterGroup> groups;

        groups.push_back ({ "MEMBRANE", { P::membraneTension, P::membraneRadius, P::membraneMass, P::membraneDamping,
                                          P::membraneModes, P::fundamentalHz, P::membraneNonlinearity, P::membraneStiffness } });
        groups.push_back ({ "TUBE", { P::tubeResonanceFreq, P::tubeResonanceQ, P::tubeResonanceLevel, P::tubeCoupling, P::hollowMetallic } });
        groups.push_back ({ "PITCH ENVELOPE", { P::pitchEnvAttack, P::pitchEnvDecay, P::pitchEnvDepth, P::pitchEnvCurve } });
        groups.push_back ({ "CLICK", { P::clickLevel, P::clickDecay, P::clickTone, P::clickDistortion, P::clickBeaterMaterial } });
        groups.push_back ({ "BODY", { P::bodyLevel, P::bodyAttack, P::bodyDecay, P::bodyTuning } });
        groups.push_back ({ "SUB", { P::subLevel, P::subDecay, P::subDetune, P::subShape, P::subRatio } });
        groups.push_back ({ "DRIVE", { P::driveAmount, P::driveMix, P::driveCharacter, P::drivePreHP } });
        groups.push_back ({ "EQUALIZER", { P::eqSubFreq, P::eqSubGain, P::eqPunchFreq, P::eqPunchGain, P::eqAirFreq, P::eqAirGain, P::eqRumbleHP } });
        groups.push_back ({ "COMPRESSOR", { P::compThreshold, P::compRatio, P::compAttack, P::compRelease, P::compKnee, P::compMakeup } });
        groups.push_back ({ "OUTPUT", { P::outputGain, P::outputPan, P::outputWidth, P::outputMonoBelow, P::outputClip, P::outputLimiter } });
        groups.push_back ({ "ROOM", { P::roomLevel, P::roomDecay, P::roomRumble } });
        groups.push_back ({ "REVERB", { P::reverbMaterial, P::reverbPreDelay, P::reverbDecay, P::reverbSize,
                                         P::reverbDamping, P::reverbMix, P::reverbLowCut, P::reverbHighCut,
                                         P::reverbFreeze, P::reverbGate } });
        // MATERIAL ROOM (MATERIAL_ROOM_SPEC.md §4): materialRoomEnabled bleibt in
        // MODULE ENABLE, damit es nur einen Enable-Eintrag gibt.
        groups.push_back ({ "MATERIAL ROOM", { P::materialRoomMaterial, P::materialRoomSize, P::materialRoomMix,
                                               P::materialRoomLayer, P::materialRoomWidth } });
        groups.push_back ({ "SUB GRUMBLE", { P::grumbleLevel, P::grumbleFreq, P::grumbleRate, P::grumbleDepth,
                                             P::grumblePanA, P::grumblePanB, P::grumbleWave,
                                             P::grumblePanWave, P::grumbleDecay } });
        // FEATURES (SND-175..181): gleiche IDs wie die Front-Sektion.
        // compMix/compAutoMakeup gehoeren nur hierher (nicht in COMPRESSOR).
        groups.push_back ({ "FEATURES", { P::transientAttack, P::transientSustain,
                                          P::midiChannel, P::midiNoteLow, P::midiNoteHigh, P::midiChokeMs,
                                          P::velToClick, P::humanize, P::oversampleEnabled, P::fxWidth,
                                          P::compMix, P::compAutoMakeup,
                                          P::envFollowAttack, P::envFollowRelease } });
        groups.push_back ({ "SIDECHAIN", { P::duckDepth, P::duckRelease } });
        // Modul-Ein/Aus sind echte APVTS-Parameter (Bool) und damit gültige
        // CV-Ziele/Quellen. higgsEnabled sitzt bereits in der HIGGS-FIELD-Gruppe.
        groups.push_back ({ "MODULE ENABLE", { P::membraneEnabled, P::tubeEnabled, P::pitchEnvEnabled,
                                               P::clickEnabled, P::bodyEnabled, P::subEnabled,
                                               P::driveEnabled, P::eqEnabled, P::compEnabled,
                                               P::outputEnabled, P::roomEnabled, P::reverbEnabled,
                                               P::materialRoomEnabled } });
        groups.push_back ({ "GLOBAL", { P::rootNote, P::velocitySensitivity, P::keyRoot, P::scale,
                                        P::glideMs, P::velToPitch, P::tubeTrack } });
        groups.push_back ({ "SEQUENCER", { P::sequencerBpm, P::sequencerPlaying, P::sequencerSteps,
                                           P::sequencerSwing, P::sequencerSyncMode, P::sequencerGrid,
                                           P::sequencerPattern, P::sequencerChain,
                                           P::sequencerClockOut } });
        // Performance-/CV-Quellen (SND-156/157/162): LFO und Makros sind APVTS-Parameter.
        {
            std::vector<juce::String> macroIds { P::lfoRate, P::lfoDepth };
            for (int i = 1; i <= 5; ++i)
                macroIds.push_back (P::macro (i));
            groups.push_back ({ "MACROS / LFO", std::move (macroIds) });
        }

        std::vector<juce::String> stepIds;
        for (int i = 0; i < 16; ++i)
        {
            stepIds.push_back (P::stepVel (i));
            stepIds.push_back (P::stepGate (i));
            stepIds.push_back (P::stepPitch (i));
        }
        groups.push_back ({ "SEQUENCER STEPS", std::move (stepIds) });

        groups.push_back ({ "HIGGS FIELD", { P::higgsEnabled, P::higgsCursorPhi, P::higgsCursorTheta,
                                            P::higgsFieldStrength, P::higgsVev, P::higgsSelfCoupling,
                                            P::higgsInertia, P::higgsYukawaBoost, P::higgsBosonHz,
                                            P::higgsBosonQ, P::higgsSymmetry, P::higgsDecay, P::higgsMix,
                                            P::higgsRate, P::higgsDepth } });
        groups.push_back ({ "TRIGGER", { P::triggerButton } });

        return groups;
    }

    juce::String parameterDisplayText (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
    {
        // SND-131: fundamentalHz/stepPitch als Note + Cents anzeigen.
        if (TuningDisplay::isPitchedParameter (id))
            return TuningDisplay::pitchedValueText (apvts, id);

        if (auto* param = apvts.getParameter (id))
        {
            // getText() erwartet den normalisierten Wert; es liefert Choice-Namen
            // ("Wood", "Metal", …) und "On"/"Off" statt der rohen Zahlen 0..6.
            const auto text = param->getText (param->getValue(), 24);

            if (text.isNotEmpty())
                return text;

            // Fallback: roher (denormalisierter) Wert, falls ein Parameter keinen
            // eigenen Text-Formatter hat.
            if (auto* value = apvts.getRawParameterValue (id))
                return juce::String (value->load(), 2);
        }

        return "-";
    }

    juce::Colour parameterCableColour (const juce::AudioProcessorValueTreeState& apvts,
                                       const ParameterPatchGraph& graph,
                                       int sourceParam, int targetParam,
                                       bool selected)
    {
        if (selected)
            return juce::Colours::white;

        if (! graph.isSourceEnabled (sourceParam) || ! graph.isTargetEnabled (targetParam))
            return juce::Colour (0xff5a5a5a);

        if (ParameterPatchGraph::isVirtualSource (sourceParam))
            return juce::Colour (0xff22b8cf);

        const auto& ids = ParameterIDs::getAllIDs();

        if (sourceParam < 0 || sourceParam >= (int) ids.size())
            return CERNLookAndFeel::accentColour;

        // Gruppen einmalig aufbauen (nicht pro Kabel/Paint neu allokieren).
        static const std::vector<ParameterGroup> groups = makeGroups();

        for (const auto& group : groups)
            for (const auto& id : group.ids)
                if (id == ids[(size_t) sourceParam])
                {
                    const juce::String name = group.name;

                    if (name == "MEMBRANE") return juce::Colour (0xfff05a5a);
                    if (name == "TUBE") return juce::Colour (0xffff922b);
                    if (name == "PITCH ENVELOPE") return juce::Colour (0xfff06292);
                    if (name == "CLICK") return juce::Colour (0xffd6336c);
                    if (name == "BODY") return juce::Colour (0xffb197fc);
                    if (name == "SUB") return juce::Colour (0xfff6d258);
                    if (name == "DRIVE") return juce::Colour (0xffff6b35);
                    if (name == "EQUALIZER") return juce::Colour (0xfff59f00);
                    if (name == "COMPRESSOR") return juce::Colour (0xffc92a2a);
                    if (name == "OUTPUT") return juce::Colour (0xfff2f2f2);
                    if (name == "ROOM") return juce::Colour (0xff20c997);
                    if (name == "REVERB") return juce::Colour (0xff4dabf7);
                    if (name == "MATERIAL ROOM") return juce::Colour (0xff0ca678);
                    if (name == "SUB GRUMBLE") return juce::Colour (0xff0ca678);
                    if (name == "FEATURES") return juce::Colour (0xffe599f7);
                    if (name == "SIDECHAIN") return juce::Colour (0xffa5d8ff);
                    if (name == "MODULE ENABLE") return juce::Colour (0xffc9b18a);
                    if (name == "GLOBAL") return juce::Colour (0xff82c91e);
                    if (name == "SEQUENCER") return juce::Colour (0xff3bc9db);
                    if (name == "SEQUENCER STEPS") return juce::Colour (0xff69db7c);
                    if (name == "HIGGS FIELD") return juce::Colour (0xff9775fa);
                    if (name == "TRIGGER") return juce::Colour (0xffadb5bd);

                    return CERNLookAndFeel::accentColour;
                }

        return CERNLookAndFeel::accentColour;
    }

    juce::String parameterDisplayName (juce::AudioProcessorValueTreeState& apvts, const juce::String& id)
    {
        if (auto* param = apvts.getParameter (id))
            return param->getName (48);

        return id;
    }
}

//==============================================================================
class ParameterPatchView::Canvas : public juce::Component
{
public:
    explicit Canvas (ParameterPatchView& ownerToUse,
                     juce::AudioProcessorValueTreeState& apvtsToUse,
                     ParameterPatchGraph& graphToUse)
        : owner (ownerToUse), parameters (apvtsToUse), graph (graphToUse)
    {
        setOpaque (true);
        rebuildRows();
        createSwitches();
    }

    void paint (juce::Graphics& g) override
    {
        g.fillAll (juce::Colour (0xff0a0b0d));

        // Gebürstete Metall-Fläche und dezente Vignette
        for (int y = 0; y < getHeight(); y += 3)
        {
            g.setColour (juce::Colours::white.withAlpha (0.015f + 0.01f * std::sin ((float) y * 0.7f)));
            g.drawHorizontalLine (y, 0.0f, (float) getWidth());
        }

        g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.25f));
        g.drawRect (getLocalBounds(), 2);

        // Gruppenheader
        g.setFont (CERNLookAndFeel::getNameFont().withHeight (14.0f).boldened());
        for (const auto& header : headers)
        {
            g.setColour (juce::Colour (0xff111216));
            g.fillRect (0, header.y, getWidth(), headerHeight - 1);
            g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.35f));
            g.fillRect (0, header.y + headerHeight - 3, getWidth(), 1);
            g.setColour (CERNLookAndFeel::textColour);
            g.drawText (header.name, 16, header.y, getWidth() - 32, headerHeight - 2,
                        juce::Justification::centredLeft);
        }

        // Parameter-Zeilen (nur sichtbare zeichnen - bei 1229 Zeilen wichtig).
        const auto clip = g.getClipBounds();

        for (const auto& row : rows)
        {
            if (row.y + rowHeight < clip.getY() || row.y > clip.getBottom())
                continue;

            const bool zebra = (row.index % 2) == 0;
            g.setColour (zebra ? juce::Colour (0xff101116) : juce::Colour (0xff0d0e11));
            g.fillRect (0, row.y, getWidth(), rowHeight - 1);

            // Modul-Tag
            const auto tag = juce::String (row.module).substring (0, 18);
            g.setColour (CERNLookAndFeel::accentColour.withAlpha (0.65f));
            g.setFont (juce::FontOptions (9.0f).withStyle ("Bold"));
            g.drawText (tag, 12, row.y, 72, rowHeight, juce::Justification::centredLeft);

            // Parameter-Name
            const auto displayName = row.virtualSource ? row.displayName
                                                       : parameterDisplayName (parameters, row.id);
            g.setColour (CERNLookAndFeel::textColour.withAlpha (0.9f));
            g.setFont (juce::FontOptions (11.0f));
            g.drawText (displayName, 88, row.y, getWidth() - 330, rowHeight,
                        juce::Justification::centredLeft);

            // Live-Wert (bei virtuellen Quellen der aktuelle CV-Pegel)
            const auto valueText = row.virtualSource
                                       ? juce::String (graph.getVirtualSourceNormalised (row.index) * 100.0f, 0) + "%"
                                       : parameterDisplayText (parameters, row.id);
            g.setColour (CERNLookAndFeel::textColour.withAlpha (0.55f));
            g.setFont (juce::FontOptions (10.0f));
            g.drawText (valueText, getWidth() - 365, row.y, 58, rowHeight,
                        juce::Justification::centredRight);

            // Schalterbeschriftung
            g.setColour (CERNLookAndFeel::textColour.withAlpha (0.4f));
            g.setFont (juce::FontOptions (8.5f));
            g.drawText ("SRC", getWidth() - 296, row.y + 1, 44, 12, juce::Justification::centred);
            if (! row.virtualSource)
                g.drawText ("DST", getWidth() - 242, row.y + 1, 44, 12, juce::Justification::centred);

            // Buchsenbeschriftung
            g.setColour (CERNLookAndFeel::textColour.withAlpha (0.45f));
            g.setFont (juce::FontOptions (8.5f));
            g.drawText ("OUT", getWidth() - 136, row.y + 1, 32, 12, juce::Justification::centred);
            if (! row.virtualSource)
                g.drawText ("IN",  getWidth() - 66,  row.y + 1, 32, 12, juce::Justification::centred);
        }

        // Kabel + Kabel-LED (SND-158): LED-Pegel = aktueller CV des Kabels.
        const auto cables = graph.getConnections();
        std::array<float, ParameterPatchGraph::sourceCapacity> incomingCv {};

        for (int i = 0; i < static_cast<int> (cables.size()); ++i)
        {
            const auto& cable = cables[(size_t) i];
            const float cv = cableCv (cable);

            if (cable.targetParam >= 0 && cable.targetParam < (int) incomingCv.size())
                incomingCv[(size_t) cable.targetParam] += cv;

            const bool selected = (i == owner.getSelectedCable());
            drawCable (g, jackOut (cable.sourceParam), jackIn (cable.targetParam),
                       parameterCableColour (parameters, graph, cable.sourceParam, cable.targetParam, selected),
                       selected, false);

            drawCableLed (g, jackOut (cable.sourceParam), jackIn (cable.targetParam), cv, selected);
        }

        if (pending)
            drawPendingCable (g);

        // Jacks obenauf, inkl. Modulationsring (SND-160).
        for (const auto& row : rows)
        {
            if (row.y + rowHeight < clip.getY() - 16 || row.y > clip.getBottom() + 16)
                continue;

            const bool hover = (hoverParam == row.index && ! pending);
            const bool activePending = pending && pendingParam == row.index;
            const bool validTarget = pending
                && hoverTargetParam == row.index
                && canConnectTo (row.index, pendingIsOutput);

            drawJack (g, jackOut (row.index), true,  hover && pendingIsOutput, activePending || validTarget);
            drawCvRing (g, jackOut (row.index), sourceNormalisedOf (row));

            if (! row.virtualSource)
            {
                drawJack (g, jackIn (row.index), false, hover && ! pendingIsOutput, activePending || validTarget);

                if (row.index < (int) incomingCv.size() && incomingCv[(size_t) row.index] != 0.0f)
                    drawCvRing (g, jackIn (row.index),
                                juce::jlimit (-1.0f, 1.0f, incomingCv[(size_t) row.index]));
            }
        }
    }

    void resized() override
    {
        if (! rebuildingRows)
        {
            if (rows.empty())
                rebuildRows();
            if (sourceSwitches.empty() && ! rows.empty())
                createSwitches();
            layoutSwitches();
        }
    }

    void mouseDown (const juce::MouseEvent& e) override
    {
        mouseDownPos = e.position;
        dragMoved = false;

        if (e.mods.isPopupMenu())
        {
            const auto jack = findJackAt (e.position);

            if (jack.first >= 0)
            {
                graph.removeConnectionsAt (jack.first);
                pending = false;
                owner.setSelectedCable (-1);
                repaint();
                return;
            }

            const int cable = findCableAt (e.position);
            if (cable >= 0)
            {
                // Wertbasiert entfernen (Quelle/Ziel) statt über den Index,
                // der sich seit dem letzten Paint verschoben haben kann.
                const auto cables = graph.getConnections();
                if (cable < (int) cables.size())
                    graph.removeConnection (cables[(size_t) cable].sourceParam,
                                            cables[(size_t) cable].targetParam);
                owner.setSelectedCable (-1);
                repaint();
                return;
            }
        }

        const auto jack = findJackAt (e.position);

        if (pending)
        {
            if (jack.first >= 0 && tryConnect (jack))
            {
                pending = false;
                repaint();
                return;
            }

            pending = false;
            repaint();
            return;
        }

        if (jack.first >= 0)
        {
            pending = true;
            pendingParam = jack.first;
            pendingIsOutput = jack.second;
            mousePos = e.position;
            owner.setSelectedCable (-1);
            repaint();
            return;
        }

        owner.setSelectedCable (findCableAt (e.position));
        repaint();
    }

    void mouseDrag (const juce::MouseEvent& e) override
    {
        mousePos = e.position;
        if (e.position.getDistanceFrom (mouseDownPos) > 4.0f)
            dragMoved = true;
        if (pending)
            repaint();
    }

    void mouseMove (const juce::MouseEvent& e) override
    {
        mousePos = e.position;
        const auto jack = findJackAt (e.position);
        const bool changed = jack.first != hoverTargetParam;
        hoverTargetParam = jack.first;
        if (changed || pending)
            repaint();
    }

    void mouseUp (const juce::MouseEvent& e) override
    {
        if (! pending || ! dragMoved)
            return;

        if (tryConnect (findJackAt (e.position)))
        {
            pending = false;
            repaint();
            return;
        }

        pending = false;
        repaint();
    }

    void mouseExit (const juce::MouseEvent&) override
    {
        hoverTargetParam = -1;
        repaint();
    }

public:
    struct Row
    {
        juce::String module;
        juce::String id;
        int index = -1;
        int y = 0;
        juce::String displayName;   ///< nur für virtuelle Quellen gesetzt
        bool virtualSource = false; ///< LFO/ENV/Makros haben kein IN-Jack
    };

    struct Header
    {
        juce::String name;
        int y = 0;
    };

    void rebuildRows()
    {
        if (rebuildingRows)
            return;

        rebuildingRows = true;
        static const auto groups = makeGroups();
        headers.clear();
        rows.clear();

        const auto ids = ParameterIDs::getAllIDs();
        std::map<juce::String, int> idToIndex;
        for (int i = 0; i < (int) ids.size(); ++i)
            idToIndex[ids[(size_t) i]] = i;

        int y = 8;

        // Virtuelle CV-Quellen (SND-156/157) stehen oben, damit sie ohne
        // Scrollen erreichbar sind; reine Quellen ohne IN-Buchse.
        headers.push_back ({ "VIRTUAL CV", y });
        y += headerHeight + groupGap;

        for (int v = 0; v < ParameterPatchGraph::virtualSourceCount; ++v)
        {
            const int index = ParameterPatchGraph::virtualSourceIndex (
                static_cast<ParameterPatchGraph::VirtualSource> (v));

            Row row;
            row.module = "CV";
            row.index = index;
            row.y = y;
            row.displayName = ParameterPatchGraph::virtualSourceName (index);
            row.virtualSource = true;
            rows.push_back (row);
            y += rowHeight;
        }

        y += groupGap;

        for (const auto& group : groups)
        {
            const int headerY = y;
            int groupY = y + headerHeight + groupGap;
            bool addedAny = false;

            for (const auto& id : group.ids)
            {
                auto it = idToIndex.find (id);
                if (it == idToIndex.end())
                    continue;

                if (! addedAny)
                {
                    headers.push_back ({ group.name, headerY });
                    addedAny = true;
                }

                rows.push_back ({ group.name, id, it->second, groupY });
                groupY += rowHeight;
            }

            if (addedAny)
                y = groupY + groupGap;
        }

        // Kein Parameter darf fehlen: alle IDs, die in keiner der festen
        // Gruppen stehen, landen in einer Auffanggruppe (1229-Parameter-Layout).
        std::set<juce::String> grouped;

        for (const auto& group : groups)
            for (const auto& id : group.ids)
                grouped.insert (id);

        std::vector<juce::String> ungrouped;

        for (const auto& id : ids)
            if (grouped.find (id) == grouped.end())
                ungrouped.push_back (id);

        if (! ungrouped.empty())
        {
            headers.push_back ({ "OTHER PARAMETERS", y });
            y += headerHeight + groupGap;

            for (const auto& id : ungrouped)
            {
                const auto it = idToIndex.find (id);
                if (it == idToIndex.end())
                    continue;

                Row row;
                row.module = "OTHER";
                row.id = id;
                row.index = it->second;
                row.y = y;
                rows.push_back (std::move (row));
                y += rowHeight;
            }

            y += groupGap;
        }

        contentHeight = y + 10;
        if (getHeight() != contentHeight)
            setSize (getWidth(), contentHeight);

        rebuildingRows = false;
    }

    void createSwitches()
    {
        sourceSwitches.clear();
        targetSwitches.clear();

        for (const auto& row : rows)
        {
            auto sourceSwitch = std::make_unique<PowerSwitch> ("SRC");
            sourceSwitch->setClickingTogglesState (true);
            sourceSwitch->setToggleState (graph.isSourceEnabled (row.index), juce::dontSendNotification);
            sourceSwitch->setTooltip ("CV source on/off");
            sourceSwitch->onClick = [this, rowIndex = row.index, switchPtr = sourceSwitch.get()]
            {
                graph.setSourceEnabled (rowIndex, switchPtr->getToggleState());
                repaint();
            };
            addAndMakeVisible (*sourceSwitch);
            sourceSwitches.push_back (std::move (sourceSwitch));

            // Virtuelle Quellen haben keinen Eingang.
            if (row.virtualSource)
            {
                targetSwitches.push_back (nullptr);
                continue;
            }

            auto targetSwitch = std::make_unique<PowerSwitch> ("DST");
            targetSwitch->setClickingTogglesState (true);
            targetSwitch->setToggleState (graph.isTargetEnabled (row.index), juce::dontSendNotification);
            targetSwitch->setTooltip ("CV destination on/off");
            targetSwitch->onClick = [this, rowIndex = row.index, switchPtr = targetSwitch.get()]
            {
                graph.setTargetEnabled (rowIndex, switchPtr->getToggleState());
                repaint();
            };
            addAndMakeVisible (*targetSwitch);
            targetSwitches.push_back (std::move (targetSwitch));
        }
    }

    void layoutSwitches()
    {
        if (sourceSwitches.size() != rows.size() || targetSwitches.size() != rows.size())
            return;

        for (size_t i = 0; i < rows.size(); ++i)
        {
            const float y = (float) rows[i].y + 4.0f;
            if (sourceSwitches[i] != nullptr)
                sourceSwitches[i]->setBounds (getWidth() - 292, (int) y, 44, 22);
            if (targetSwitches[i] != nullptr)
                targetSwitches[i]->setBounds (getWidth() - 242, (int) y, 44, 22);
        }
    }

    void syncSwitches()
    {
        if (sourceSwitches.size() != rows.size() || targetSwitches.size() != rows.size())
            return;

        for (size_t i = 0; i < rows.size(); ++i)
        {
            if (sourceSwitches[i] != nullptr)
                sourceSwitches[i]->setToggleState (graph.isSourceEnabled (rows[i].index),
                                                   juce::dontSendNotification);
            if (targetSwitches[i] != nullptr)
                targetSwitches[i]->setToggleState (graph.isTargetEnabled (rows[i].index),
                                                   juce::dontSendNotification);
        }
    }
private:
    juce::Point<float> jackOut (int index) const { return { (float) getWidth() - 156.0f, jackY (index) }; }
    juce::Point<float> jackIn  (int index) const { return { (float) getWidth() - 80.0f,  jackY (index) }; }

    float jackY (int index) const
    {
        for (const auto& row : rows)
            if (row.index == index)
                return (float) row.y + (float) rowHeight * 0.58f;
        return 0.0f;
    }

    /** Aktueller CV eines Kabels inkl. amount und Polarität (-1..+1). */
    float cableCv (const ParameterPatchGraph::Connection& cable) const
    {
        const bool virtualSource = ParameterPatchGraph::isVirtualSource (cable.sourceParam);
        const float normalised = virtualSource
                                     ? graph.getVirtualSourceNormalised (cable.sourceParam)
                                     : owner.sourceNormalised (cable.sourceParam);

        // LFO ist bipolar, Envelope-Follower und Makros sind unipolar.
        const bool inherentlyUnipolar = virtualSource
            && ParameterPatchGraph::virtualSourceAt (cable.sourceParam)
                   != ParameterPatchGraph::VirtualSource::lfo;

        const float cv = (cable.unipolar || inherentlyUnipolar)
                             ? normalised
                             : 2.0f * normalised - 1.0f;

        return juce::jlimit (-1.0f, 1.0f, cv * cable.amount);
    }

    float sourceNormalisedOf (const Row& row) const
    {
        if (row.virtualSource)
            return graph.getVirtualSourceNormalised (row.index);

        return owner.sourceNormalised (row.index);
    }

    /** Modulationsring um eine Buchse (SND-160). */
    void drawCvRing (juce::Graphics& g, juce::Point<float> centre, float cv) const
    {
        const float magnitude = juce::jlimit (0.0f, 1.0f, std::abs (cv));

        if (magnitude < 0.02f)
            return;

        const float radius = 13.0f;
        juce::Path arc;
        arc.addCentredArc (centre.x, centre.y, radius, radius, 0.0f,
                           -juce::MathConstants<float>::halfPi,
                           -juce::MathConstants<float>::halfPi
                               + magnitude * juce::MathConstants<float>::twoPi,
                           true);

        g.setColour ((cv >= 0.0f ? juce::Colours::lime : juce::Colours::orange)
                         .withAlpha (0.25f + 0.6f * magnitude));
        g.strokePath (arc, juce::PathStrokeType (2.0f));
    }

    /** Kabel-LED in der Kabelmitte (SND-158). */
    void drawCableLed (juce::Graphics& g, juce::Point<float> from, juce::Point<float> to,
                       float cv, bool selected) const
    {
        const float sag = 34.0f + std::abs (to.x - from.x) * 0.08f + std::abs (to.y - from.y) * 0.02f;
        const juce::Point<float> led { (from.x + to.x) * 0.5f, (from.y + to.y) * 0.5f + sag * 0.75f };
        const float magnitude = juce::jlimit (0.0f, 1.0f, std::abs (cv));

        g.setColour (juce::Colours::black.withAlpha (0.6f));
        g.fillEllipse (led.x - 5.0f, led.y - 5.0f, 10.0f, 10.0f);

        g.setColour ((cv >= 0.0f ? juce::Colours::limegreen : juce::Colours::orange)
                         .withAlpha (selected ? 1.0f : 0.35f + 0.65f * magnitude));
        g.fillEllipse (led.x - 3.0f, led.y - 3.0f, 6.0f, 6.0f);
    }

    std::pair<int, bool> findJackAt (juce::Point<float> pos) const
    {
        for (const auto& row : rows)
        {
            if (jackOut (row.index).getDistanceFrom (pos) <= 10.0f)
                return { row.index, true };

            if (! row.virtualSource && jackIn (row.index).getDistanceFrom (pos) <= 10.0f)
                return { row.index, false };
        }
        return { -1, false };
    }

    bool canConnectTo (int paramIndex, bool pendingOutputSide) const
    {
        if (paramIndex < 0 || ! pending)
            return false;

        const int source = pendingIsOutput ? pendingParam : paramIndex;
        const int target = pendingIsOutput ? paramIndex : pendingParam;
        return graph.canConnect (source, target);
    }

    bool tryConnect (std::pair<int, bool> jack)
    {
        if (jack.first < 0)
            return false;

        const int source = pendingIsOutput ? pendingParam : jack.first;
        const int target = pendingIsOutput ? jack.first : pendingParam;

        if (source == target)
            return false;

        if (pendingIsOutput == jack.second)
            return false; // beide Seiten gleiche Richtung

        return graph.addConnection (source, target);
    }

    int findCableAt (juce::Point<float> pos, float threshold = 7.0f) const
    {
        const auto cables = graph.getConnections();

        for (int i = (int) cables.size() - 1; i >= 0; --i)
        {
            const auto p1 = jackOut (cables[(size_t) i].sourceParam);
            const auto p2 = jackIn  (cables[(size_t) i].targetParam);

            const float sag = 34.0f + std::abs (p2.x - p1.x) * 0.08f + std::abs (p2.y - p1.y) * 0.02f;
            const juce::Point<float> c1 { p1.x, p1.y + sag };
            const juce::Point<float> c2 { p2.x, p2.y + sag };

            juce::Point<float> prev = p1;
            for (int t = 1; t <= 32; ++t)
            {
                const float u = (float) t / 32.0f;
                const float iu = 1.0f - u;
                const juce::Point<float> pt {
                    iu*iu*iu*p1.x + 3*iu*iu*u*c1.x + 3*iu*u*u*c2.x + u*u*u*p2.x,
                    iu*iu*iu*p1.y + 3*iu*iu*u*c1.y + 3*iu*u*u*c2.y + u*u*u*p2.y
                };

                const auto delta = pt - prev;
                const float len2 = delta.getDotProduct (delta);
                const float tproj = len2 > 0.0f ? juce::jlimit (0.0f, 1.0f, (pos - prev).getDotProduct (delta) / len2) : 0.0f;
                const auto closest = prev + delta * tproj;

                if (pos.getDistanceFrom (closest) <= threshold)
                    return i;

                prev = pt;
            }
        }

        return -1;
    }

    void drawCable (juce::Graphics& g, juce::Point<float> from, juce::Point<float> to,
                    juce::Colour colour, bool selected, bool pendingCable) const
    {
        const float sag = 34.0f + std::abs (to.x - from.x) * 0.08f + std::abs (to.y - from.y) * 0.02f;
        juce::Path path;
        path.startNewSubPath (from);
        path.cubicTo (from.x, from.y + sag, to.x, to.y + sag, to.x, to.y);

        juce::Path shadow = path;
        shadow.applyTransform (juce::AffineTransform::translation (2.0f, 4.0f));
        g.setColour (juce::Colours::black.withAlpha (selected ? 0.5f : 0.3f));
        g.strokePath (shadow, juce::PathStrokeType (selected ? 8.0f : 6.5f));

        g.setColour (colour.withAlpha (pendingCable ? 0.85f : 0.95f));
        g.strokePath (path, juce::PathStrokeType (selected ? 6.5f : 5.0f));

        juce::Path highlight = path;
        highlight.applyTransform (juce::AffineTransform::translation (-0.5f, -1.5f));
        g.setColour (juce::Colours::white.withAlpha (selected ? 0.45f : 0.25f));
        g.strokePath (highlight, juce::PathStrokeType (2.0f));

        auto plug = [&] (juce::Point<float> p)
        {
            g.setColour (juce::Colour (0xff33373e));
            g.fillEllipse (p.x - 5.0f, p.y - 5.0f, 10.0f, 10.0f);
            g.setColour (colour.brighter (0.3f));
            g.fillEllipse (p.x - 2.2f, p.y - 2.2f, 4.4f, 4.4f);
        };
        plug (from);
        plug (to);
    }

    void drawPendingCable (juce::Graphics& g)
    {
        const auto from = pendingIsOutput ? jackOut (pendingParam) : mousePos;
        const auto to   = pendingIsOutput ? mousePos : jackIn (pendingParam);
        drawCable (g, from, to, CERNLookAndFeel::accentColour.brighter (0.35f), false, true);
    }

    void drawJack (juce::Graphics& g, juce::Point<float> centre, bool isOutput,
                   bool highlight, bool active) const
    {
        const float r = 8.5f;

        juce::ColourGradient nut (juce::Colour (0xff8a817b), centre.x - r, centre.y - r,
                                  juce::Colour (0xff39342f), centre.x + r, centre.y + r, true);
        g.setGradientFill (nut);
        g.fillEllipse (centre.x - r, centre.y - r, r * 2.0f, r * 2.0f);

        g.setColour (juce::Colours::black);
        g.fillEllipse (centre.x - r * 0.42f, centre.y - r * 0.42f, r * 0.84f, r * 0.84f);

        if (active || highlight)
            g.setColour (highlight ? juce::Colours::lime.withAlpha (0.9f)
                                   : CERNLookAndFeel::accentColour.brighter (0.4f));
        else
            g.setColour (juce::Colours::white.withAlpha (0.15f));

        g.drawEllipse (centre.x - r - 1.5f, centre.y - r - 1.5f, (r + 1.5f) * 2.0f, (r + 1.5f) * 2.0f,
                       active || highlight ? 2.0f : 1.0f);
    }

    ParameterPatchView& owner;
    juce::AudioProcessorValueTreeState& parameters;
    ParameterPatchGraph& graph;

    std::vector<std::unique_ptr<PowerSwitch>> sourceSwitches;
    std::vector<std::unique_ptr<PowerSwitch>> targetSwitches;
    bool rebuildingRows = false;

    juce::Point<float> mousePos;
    juce::Point<float> mouseDownPos;
    bool pending = false;
    int pendingParam = -1;
    bool pendingIsOutput = true;
    int hoverTargetParam = -1;
    int hoverParam = -1;
    bool dragMoved = false;

    std::vector<Row> rows;
    std::vector<Header> headers;
    int contentHeight = 1000;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (Canvas)
};

//==============================================================================
ParameterPatchView::ParameterPatchView (juce::AudioProcessorValueTreeState& apvts,
                                        ParameterPatchGraph& graph)
    : parameters (apvts), parameterPatch (graph)
{
    setOpaque (true);

    cableLabel.setFont (juce::FontOptions (11.0f));
    cableLabel.setColour (juce::Label::textColourId, CERNLookAndFeel::textColour);
    cableLabel.setText ("No cable selected - click a cable to edit it", juce::dontSendNotification);
    addAndMakeVisible (cableLabel);

    amountSlider.setRange (-1.0, 1.0, 0.01);
    amountSlider.setValue (1.0, juce::dontSendNotification);
    amountSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
    amountSlider.setDoubleClickReturnValue (true, 1.0);
    amountSlider.setTextValueSuffix (" amt");
    amountSlider.setTooltip ("Cable amount -1..+1 (negative inverts the CV)");
    amountSlider.onValueChange = [this] { writeInspectorToCable(); };
    addAndMakeVisible (amountSlider);

    slewSlider.setRange (0.0, 20.0, 0.5);
    slewSlider.setValue (0.0, juce::dontSendNotification);
    slewSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 48, 20);
    slewSlider.setDoubleClickReturnValue (true, 0.0);
    slewSlider.setTextValueSuffix (" ms");
    slewSlider.setTooltip ("Cable slew 0..20 ms (0 = off)");
    slewSlider.onValueChange = [this] { writeInspectorToCable(); };
    addAndMakeVisible (slewSlider);

    lfoRateSlider.setRange (0.05, 20.0, 0.01);
    lfoRateSlider.setSkewFactorFromMidPoint (1.0);
    lfoRateSlider.setValue (readRawParameter (parameters, ParameterIDs::lfoRate,
                                             parameterPatch.getLfoRateHz()),
                            juce::dontSendNotification);
    lfoRateSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 56, 20);
    lfoRateSlider.setDoubleClickReturnValue (true, 0.5);
    lfoRateSlider.setTextValueSuffix (" Hz");
    lfoRateSlider.setTooltip ("LFO rate for the virtual LFO source (parameter: lfoRate)");
    lfoRateSlider.onValueChange = [this]
    {
        // Fuehrende Quelle ist die APVTS (SND-154): automatisierbar, im Preset
        // und im Host-State gespeichert. Die Engine spiegelt den Wert pro Block
        // in den Parameter-Patch.
        writeRawParameter (parameters, ParameterIDs::lfoRate,
                           (float) lfoRateSlider.getValue());
    };
    addAndMakeVisible (lfoRateSlider);

    lfoDepthSlider.setRange (0.0, 1.0, 0.01);
    lfoDepthSlider.setValue (readRawParameter (parameters, ParameterIDs::lfoDepth,
                                               parameterPatch.getLfoDepth()),
                             juce::dontSendNotification);
    lfoDepthSlider.setTextBoxStyle (juce::Slider::TextBoxRight, false, 74, 20);
    lfoDepthSlider.setDoubleClickReturnValue (true, 0.5);
    lfoDepthSlider.setTextValueSuffix (" depth");
    lfoDepthSlider.setTooltip ("LFO depth for the virtual LFO source (parameter: lfoDepth)");
    lfoDepthSlider.onValueChange = [this]
    {
        writeRawParameter (parameters, ParameterIDs::lfoDepth,
                           (float) lfoDepthSlider.getValue());
    };
    addAndMakeVisible (lfoDepthSlider);

    unipolarButton.setTooltip ("CV polarity: unipolar 0..+1 instead of bipolar -1..+1");
    unipolarButton.onClick = [this] { writeInspectorToCable(); };
    addAndMakeVisible (unipolarButton);

    noteRateButton.setTooltip ("Update the CV only once per 1/64 note (sample & hold)");
    noteRateButton.onClick = [this] { writeInspectorToCable(); };
    addAndMakeVisible (noteRateButton);

    wrapButton.setTooltip ("Wrap the modulated value into 0..1 instead of clamping");
    wrapButton.setToggleState (parameterPatch.isWrapEnabled(), juce::dontSendNotification);
    wrapButton.onClick = [this]
    {
        parameterPatch.setWrapEnabled (wrapButton.getToggleState());
    };
    addAndMakeVisible (wrapButton);

    removeCableButton.setTooltip ("Remove the selected cable");
    removeCableButton.onClick = [this]
    {
        const auto cables = parameterPatch.getConnections();

        if (juce::isPositiveAndBelow (selectedCable, (int) cables.size()))
            parameterPatch.removeConnection (cables[(size_t) selectedCable].sourceParam,
                                             cables[(size_t) selectedCable].targetParam);

        setSelectedCable (-1);
    };
    addAndMakeVisible (removeCableButton);

    canvas = std::make_unique<Canvas> (*this, parameters, parameterPatch);
    addChildComponent (*canvas);

    viewport.setViewedComponent (canvas.get(), false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);

    refreshInspector();
    startTimerHz (10);
}

ParameterPatchView::~ParameterPatchView()
{
    stopTimer();
}

void ParameterPatchView::resized()
{
    auto r = getLocalBounds();

    // Kabel-Inspektor (SND-151/153/154/155/161)
    auto inspector = r.removeFromTop (inspectorHeight);
    auto topRow = inspector.removeFromTop (inspectorHeight / 2).reduced (8, 4);
    auto bottomRow = inspector.reduced (8, 4);

    cableLabel.setBounds (topRow.removeFromLeft (juce::jmax (150, topRow.getWidth() / 4)));
    topRow.removeFromLeft (8);

    // Breiten gewaehlt, damit die Schaltertexte (Switch + Label) nie mit
    // Ellipse abgeschnitten werden: UNIPOLAR, 1/64 HOLD, CV WRAP.
    unipolarButton.setBounds (topRow.removeFromLeft (110));
    topRow.removeFromLeft (6);
    noteRateButton.setBounds (topRow.removeFromLeft (118));
    topRow.removeFromLeft (6);
    wrapButton.setBounds (topRow.removeFromLeft (108));
    removeCableButton.setBounds (topRow.removeFromRight (110));

    amountSlider.setBounds (bottomRow.removeFromLeft (juce::jmax (150, bottomRow.getWidth() / 3)));
    bottomRow.removeFromLeft (6);
    slewSlider.setBounds (bottomRow.removeFromLeft (juce::jmax (130, bottomRow.getWidth() / 3)));
    bottomRow.removeFromLeft (6);
    lfoRateSlider.setBounds (bottomRow.removeFromLeft (juce::jmax (130, bottomRow.getWidth() / 2)));
    bottomRow.removeFromLeft (6);
    lfoDepthSlider.setBounds (bottomRow);

    viewport.setBounds (r);

    if (canvas != nullptr)
    {
        const int width = juce::jmax (r.getWidth() - viewport.getScrollBarThickness(), 600);
        canvas->setSize (width, juce::jmax (canvas->getHeight(), r.getHeight()));
    }
}

void ParameterPatchView::setSelectedCable (int cableIndex)
{
    if (cableIndex == selectedCable)
        return;

    selectedCable = cableIndex;
    refreshInspector();

    if (canvas != nullptr)
        canvas->repaint();
}

float ParameterPatchView::sourceNormalised (int parameterIndex) const
{
    if (ParameterPatchGraph::isVirtualSource (parameterIndex))
        return parameterPatch.getVirtualSourceNormalised (parameterIndex);

    const auto& ids = ParameterIDs::getAllIDs();

    if (parameterIndex < 0 || parameterIndex >= (int) ids.size())
        return 0.0f;

    if (auto* parameter = parameters.getParameter (ids[(size_t) parameterIndex]))
        return parameter->getValue();

    return 0.0f;
}

void ParameterPatchView::refreshInspector()
{
    const auto cables = parameterPatch.getConnections();
    const bool hasCable = juce::isPositiveAndBelow (selectedCable, (int) cables.size());

    updatingInspector = true;

    amountSlider.setEnabled (hasCable);
    slewSlider.setEnabled (hasCable);
    unipolarButton.setEnabled (hasCable);
    noteRateButton.setEnabled (hasCable);
    removeCableButton.setEnabled (hasCable);

    if (hasCable)
    {
        const auto& cable = cables[(size_t) selectedCable];
        const auto& ids = ParameterIDs::getAllIDs();

        auto nameOf = [&ids] (int index)
        {
            if (ParameterPatchGraph::isVirtualSource (index))
                return ParameterPatchGraph::virtualSourceName (index);

            if (index >= 0 && index < (int) ids.size())
                return ids[(size_t) index];

            return juce::String ("-");
        };

        cableLabel.setText (nameOf (cable.sourceParam) + "  ->  " + nameOf (cable.targetParam),
                            juce::dontSendNotification);

        amountSlider.setValue (cable.amount, juce::dontSendNotification);
        slewSlider.setValue (cable.slewMs, juce::dontSendNotification);
        unipolarButton.setToggleState (cable.unipolar, juce::dontSendNotification);
        noteRateButton.setToggleState (cable.rate == ParameterPatchGraph::CvRate::sixtyFourthNote,
                                       juce::dontSendNotification);
    }
    else
    {
        cableLabel.setText ("No cable selected - click a cable to edit it", juce::dontSendNotification);
    }

    wrapButton.setToggleState (parameterPatch.isWrapEnabled(), juce::dontSendNotification);

    if (! lfoRateSlider.isMouseButtonDown())
        lfoRateSlider.setValue (readRawParameter (parameters, ParameterIDs::lfoRate,
                                                  parameterPatch.getLfoRateHz()),
                                juce::dontSendNotification);

    if (! lfoDepthSlider.isMouseButtonDown())
        lfoDepthSlider.setValue (readRawParameter (parameters, ParameterIDs::lfoDepth,
                                                   parameterPatch.getLfoDepth()),
                                 juce::dontSendNotification);

    updatingInspector = false;
}

void ParameterPatchView::writeInspectorToCable()
{
    if (updatingInspector)
        return;

    const auto cables = parameterPatch.getConnections();

    if (! juce::isPositiveAndBelow (selectedCable, (int) cables.size()))
        return;

    const auto& cable = cables[(size_t) selectedCable];

    parameterPatch.setConnectionAmount (cable.sourceParam, cable.targetParam,
                                        (float) amountSlider.getValue());
    parameterPatch.setConnectionSlewMs (cable.sourceParam, cable.targetParam,
                                        (float) slewSlider.getValue());
    parameterPatch.setConnectionUnipolar (cable.sourceParam, cable.targetParam,
                                          unipolarButton.getToggleState());
    parameterPatch.setConnectionRate (cable.sourceParam, cable.targetParam,
                                      noteRateButton.getToggleState()
                                          ? ParameterPatchGraph::CvRate::sixtyFourthNote
                                          : ParameterPatchGraph::CvRate::perBlock);

    if (canvas != nullptr)
        canvas->repaint();
}

void ParameterPatchView::timerCallback()
{
    if (canvas != nullptr)
    {
        canvas->syncSwitches();
        canvas->repaint();
    }

    // Externe Änderungen (Preset-Load, Undo) im Inspektor nachziehen.
    const bool sliderBusy = amountSlider.isMouseButtonDown() || slewSlider.isMouseButtonDown()
                            || amountSlider.hasKeyboardFocus (true) || slewSlider.hasKeyboardFocus (true);

    if (! sliderBusy)
    {
        const auto cables = parameterPatch.getConnections();

        if (! juce::isPositiveAndBelow (selectedCable, (int) cables.size()))
            setSelectedCable (-1);
        else
            refreshInspector();
    }
}
