#include "ParameterPatchGraph.h"
#include "Parameters/ParameterIDs.h"
#include "Parameters/ParameterLayout.h"

#include <algorithm>
#include <array>
#include <cmath>

namespace
{
    /** Reale Parameter-Indizes müssen in die Snapshot-Kapazität passen.
        getAllIDs() ist nicht constexpr, daher prüft der static_assert die
        Layout-Konstante; die Laufzeitprüfung sitzt im Konstruktor. */
    static_assert (numLayoutParameters <= ParameterPatchGraph::maxParameterCount,
                   "Parameteranzahl überschreitet ParameterPatchGraph::maxParameterCount");

    static_assert (ParameterPatchGraph::Snapshot {}.connections.size()
                       == (size_t) ParameterPatchGraph::maxConnectionCount,
                   "Snapshot-Kapazität und Verbindungs-Limit müssen identisch sein");

    static_assert (ParameterPatchGraph::virtualSourceBase >= ParameterPatchGraph::maxParameterCount,
                   "Virtuelle CV-Quellen müssen oberhalb aller Parameter-Indizes liegen");

    static_assert (ParameterPatchGraph::virtualSourceBase + ParameterPatchGraph::virtualSourceCount
                       == ParameterPatchGraph::sourceCapacity,
                   "Virtual-Base + Anzahl muss die Enable-Kapazität ergeben");

    /** Gültige CV-Quelle: echter Parameter-Index oder virtuelle Quelle. */
    inline bool isValidSource (int index, int parameterCount)
    {
        return index >= 0
               && (index < parameterCount || ParameterPatchGraph::isVirtualSource (index));
    }

    /** Numerische Indizes aus v2/v3-Dateien (virtualBase war 248) auf den
        reservierten Virtual-Bereich abbilden. */
    inline int mapLegacyVirtualIndex (int index)
    {
        constexpr int legacyBase = ParameterPatchGraph::legacyVirtualSourceBase;

        if (index >= legacyBase && index < legacyBase + ParameterPatchGraph::virtualSourceCount)
            return index - legacyBase + ParameterPatchGraph::virtualSourceBase;

        return index;
    }

    juce::String virtualSourceId (int index)
    {
        if (! ParameterPatchGraph::isVirtualSource (index))
            return {};

        switch (ParameterPatchGraph::virtualSourceAt (index))
        {
            case ParameterPatchGraph::VirtualSource::lfo:              return "virtual.lfo";
            case ParameterPatchGraph::VirtualSource::envelopeFollower: return "virtual.envFollow";
            case ParameterPatchGraph::VirtualSource::macro1:           return "virtual.macro1";
            case ParameterPatchGraph::VirtualSource::macro2:           return "virtual.macro2";
            case ParameterPatchGraph::VirtualSource::macro3:           return "virtual.macro3";
            case ParameterPatchGraph::VirtualSource::macro4:           return "virtual.macro4";
            case ParameterPatchGraph::VirtualSource::macro5:           return "virtual.macro5";
        }

        return {};
    }

    std::optional<int> indexOfVirtualId (const juce::String& id)
    {
        for (int i = 0; i < ParameterPatchGraph::virtualSourceCount; ++i)
        {
            const int index = ParameterPatchGraph::virtualSourceIndex (
                static_cast<ParameterPatchGraph::VirtualSource> (i));

            if (virtualSourceId (index) == id)
                return index;
        }

        return std::nullopt;
    }
}

ParameterPatchGraph::ParameterPatchGraph()
{
    sourceEnabled.fill (true);
    targetEnabled.fill (true);

    for (auto& macro : macroValues)
        macro.store (0.0f, std::memory_order_relaxed);

    // getAllIDs() einmal auf dem Message-Thread initialisieren, damit
    // fillSnapshot() im Audio-Thread garantiert nicht allokiert.
    juce::ignoreUnused (parameterCount());

    jassert (parameterCount() <= maxParameterCount);
    jassert (parameterCount() <= (int) sourceEnabled.size());
}

void ParameterPatchGraph::clearAll()
{
    juce::SpinLock::ScopedLockType sl (stateLock);
    connections.clear();
    sourceEnabled.fill (true);
    targetEnabled.fill (true);
}

void ParameterPatchGraph::resetEnables()
{
    juce::SpinLock::ScopedLockType sl (stateLock);
    sourceEnabled.fill (true);
    targetEnabled.fill (true);
}

std::vector<ParameterPatchGraph::Connection> ParameterPatchGraph::getConnections() const
{
    juce::SpinLock::ScopedLockType sl (stateLock);
    return connections;
}

int ParameterPatchGraph::getConnectionCount() const
{
    juce::SpinLock::ScopedLockType sl (stateLock);
    return static_cast<int> (connections.size());
}

std::vector<ParameterPatchGraph::Connection> ParameterPatchGraph::getConnectionsAt (int parameterIndex) const
{
    juce::SpinLock::ScopedLockType sl (stateLock);

    std::vector<Connection> result;
    for (const auto& c : connections)
        if (c.sourceParam == parameterIndex || c.targetParam == parameterIndex)
            result.push_back (c);
    return result;
}

void ParameterPatchGraph::fillSnapshot (Snapshot& snapshot) const
{
    juce::SpinLock::ScopedLockType sl (stateLock);

    snapshot.parameterCount = juce::jmin (parameterCount(), (int) snapshot.order.size());
    snapshot.connectionCount = juce::jmin ((int) connections.size(),
                                           (int) snapshot.connections.size());

    for (int i = 0; i < snapshot.connectionCount; ++i)
        snapshot.connections[(size_t) i] = connections[(size_t) i];

    for (int i = 0; i < (int) snapshot.sourceEnabled.size(); ++i)
    {
        snapshot.sourceEnabled[(size_t) i] = sourceEnabled[(size_t) i];
        snapshot.targetEnabled[(size_t) i] = targetEnabled[(size_t) i];
    }

    snapshot.wrapEnabled = wrapEnabled;
    snapshot.order = computeOrderUnlocked();
}

bool ParameterPatchGraph::isSourceEnabled (int parameterIndex) const
{
    juce::SpinLock::ScopedLockType sl (stateLock);
    const int n = parameterCount();

    if (parameterIndex < 0 || parameterIndex >= (int) sourceEnabled.size())
        return false;

    if (! isVirtualSource (parameterIndex) && parameterIndex >= n)
        return false;

    return sourceEnabled[(size_t) parameterIndex];
}

bool ParameterPatchGraph::isTargetEnabled (int parameterIndex) const
{
    juce::SpinLock::ScopedLockType sl (stateLock);
    const int n = parameterCount();
    return parameterIndex >= 0 && parameterIndex < n
           && parameterIndex < (int) targetEnabled.size()
           && targetEnabled[(size_t) parameterIndex];
}

std::array<bool, ParameterPatchGraph::sourceCapacity> ParameterPatchGraph::getSourceEnabled() const
{
    juce::SpinLock::ScopedLockType sl (stateLock);
    return sourceEnabled;
}

std::array<bool, ParameterPatchGraph::sourceCapacity> ParameterPatchGraph::getTargetEnabled() const
{
    juce::SpinLock::ScopedLockType sl (stateLock);
    return targetEnabled;
}

void ParameterPatchGraph::setSourceEnabled (int parameterIndex, bool enabled)
{
    juce::SpinLock::ScopedLockType sl (stateLock);
    const int n = parameterCount();

    if (isVirtualSource (parameterIndex)
        || (parameterIndex >= 0 && parameterIndex < n && parameterIndex < (int) sourceEnabled.size()))
        sourceEnabled[(size_t) parameterIndex] = enabled;
}

void ParameterPatchGraph::setTargetEnabled (int parameterIndex, bool enabled)
{
    juce::SpinLock::ScopedLockType sl (stateLock);
    const int n = parameterCount();
    if (parameterIndex >= 0 && parameterIndex < n)
        targetEnabled[(size_t) parameterIndex] = enabled;
}

int ParameterPatchGraph::parameterCount()
{
    return static_cast<int> (ParameterIDs::getAllIDs().size());
}

std::optional<int> ParameterPatchGraph::indexOfId (const juce::String& parameterId)
{
    if (parameterId.isEmpty())
        return std::nullopt;

    const auto& ids = ParameterIDs::getAllIDs();

    for (size_t i = 0; i < ids.size(); ++i)
        if (ids[i] == parameterId)
            return static_cast<int> (i);

    return std::nullopt;
}

bool ParameterPatchGraph::hasConnection (int sourceParam, int targetParam) const noexcept
{
    for (const auto& c : connections)
        if (c.sourceParam == sourceParam && c.targetParam == targetParam)
            return true;
    return false;
}

bool ParameterPatchGraph::reachesParameter (int fromParam, int targetParam) const
{
    juce::SpinLock::ScopedLockType sl (stateLock);
    return reachesParameterUnlocked (fromParam, targetParam);
}

bool ParameterPatchGraph::reachesParameterUnlocked (int fromParam, int targetParam) const
{
    const int n = parameterCount();

    if (targetParam < 0 || targetParam >= n)
        return false;

    if (fromParam < 0 || (fromParam >= n && ! isVirtualSource (fromParam)))
        return false;

    std::vector<int> stack { fromParam };
    std::vector<bool> visited ((size_t) n, false);

    while (! stack.empty())
    {
        const int current = stack.back();
        stack.pop_back();

        if (current == targetParam)
            return true;

        if (current < 0)
            continue;

        // Virtuelle Quellen sind reine Quellen: nur Kanten folgen, nicht besuchen.
        if (current >= n)
        {
            if (isVirtualSource (current))
                for (const auto& c : connections)
                    if (c.sourceParam == current)
                        stack.push_back (c.targetParam);

            continue;
        }

        if (visited[(size_t) current])
            continue;

        visited[(size_t) current] = true;

        for (const auto& c : connections)
            if (c.sourceParam == current)
                stack.push_back (c.targetParam);
    }

    return false;
}

bool ParameterPatchGraph::canConnect (int sourceParam, int targetParam) const
{
    juce::SpinLock::ScopedLockType sl (stateLock);
    const int n = parameterCount();

    if (! isVirtualSource (sourceParam) && (sourceParam < 0 || sourceParam >= n)) return false;
    if (targetParam < 0 || targetParam >= n) return false;
    if (! sourceEnabled[(size_t) sourceParam] || ! targetEnabled[(size_t) targetParam])
        return false;

    return canConnectTopologyUnlocked (sourceParam, targetParam);
}

bool ParameterPatchGraph::canConnectTopologyUnlocked (int sourceParam, int targetParam) const
{
    const int n = parameterCount();

    if (! isVirtualSource (sourceParam) && (sourceParam < 0 || sourceParam >= n)) return false;
    if (targetParam < 0 || targetParam >= n) return false;
    if (sourceParam == targetParam) return false;
    return ! reachesParameterUnlocked (targetParam, sourceParam);
}

bool ParameterPatchGraph::addConnection (int sourceParam, int targetParam)
{
    juce::SpinLock::ScopedLockType sl (stateLock);

    if (hasConnection (sourceParam, targetParam))
        return false;

    if (! canConnectTopologyUnlocked (sourceParam, targetParam))
        return false;

    if (! sourceEnabled[(size_t) sourceParam] || ! targetEnabled[(size_t) targetParam])
        return false;

    if (connections.size() >= (size_t) maxConnectionCount)
        return false;

    connections.push_back ({ sourceParam, targetParam });
    return true;
}

bool ParameterPatchGraph::removeConnection (int connectionIndex)
{
    juce::SpinLock::ScopedLockType sl (stateLock);

    if (connectionIndex < 0 || connectionIndex >= static_cast<int> (connections.size()))
        return false;

    connections.erase (connections.begin() + connectionIndex);
    return true;
}

bool ParameterPatchGraph::removeConnection (int sourceParam, int targetParam)
{
    juce::SpinLock::ScopedLockType sl (stateLock);

    const auto it = std::find_if (connections.begin(), connections.end(),
        [sourceParam, targetParam] (const Connection& c)
        {
            return c.sourceParam == sourceParam && c.targetParam == targetParam;
        });

    if (it == connections.end())
        return false;

    connections.erase (it);
    return true;
}

void ParameterPatchGraph::removeConnectionsAt (int parameterIndex)
{
    juce::SpinLock::ScopedLockType sl (stateLock);

    connections.erase (std::remove_if (connections.begin(), connections.end(),
        [parameterIndex] (const Connection& c)
        {
            return c.sourceParam == parameterIndex || c.targetParam == parameterIndex;
        }),
        connections.end());
}

//==============================================================================
bool ParameterPatchGraph::updateConnection (int sourceParam, int targetParam,
                                            const std::function<void (Connection&)>& update)
{
    juce::SpinLock::ScopedLockType sl (stateLock);

    for (auto& connection : connections)
        if (connection.sourceParam == sourceParam && connection.targetParam == targetParam)
        {
            update (connection);
            return true;
        }

    return false;
}

bool ParameterPatchGraph::setConnectionAmount (int sourceParam, int targetParam, float amount)
{
    const float clamped = juce::jlimit (-1.0f, 1.0f, amount);

    return updateConnection (sourceParam, targetParam,
                             [clamped] (Connection& c) { c.amount = clamped; });
}

bool ParameterPatchGraph::setConnectionUnipolar (int sourceParam, int targetParam, bool unipolar)
{
    return updateConnection (sourceParam, targetParam,
                             [unipolar] (Connection& c) { c.unipolar = unipolar; });
}

bool ParameterPatchGraph::setConnectionSlewMs (int sourceParam, int targetParam, float slewMs)
{
    const float clamped = juce::jlimit (0.0f, 20.0f, slewMs);

    return updateConnection (sourceParam, targetParam,
                             [clamped] (Connection& c) { c.slewMs = clamped; });
}

bool ParameterPatchGraph::setConnectionRate (int sourceParam, int targetParam, CvRate rate)
{
    return updateConnection (sourceParam, targetParam,
                             [rate] (Connection& c) { c.rate = rate; });
}

void ParameterPatchGraph::setWrapEnabled (bool wrap)
{
    juce::SpinLock::ScopedLockType sl (stateLock);
    wrapEnabled = wrap;
}

bool ParameterPatchGraph::isWrapEnabled() const
{
    juce::SpinLock::ScopedLockType sl (stateLock);
    return wrapEnabled;
}

//==============================================================================
int ParameterPatchGraph::virtualSourceIndex (VirtualSource source) noexcept
{
    return virtualSourceBase + static_cast<int> (source);
}

bool ParameterPatchGraph::isVirtualSource (int index) noexcept
{
    return index >= virtualSourceBase && index < virtualSourceBase + virtualSourceCount;
}

ParameterPatchGraph::VirtualSource ParameterPatchGraph::virtualSourceAt (int index) noexcept
{
    return static_cast<VirtualSource> (juce::jlimit (0, virtualSourceCount - 1,
                                                     index - virtualSourceBase));
}

juce::String ParameterPatchGraph::virtualSourceName (int index)
{
    if (! isVirtualSource (index))
        return {};

    switch (virtualSourceAt (index))
    {
        case VirtualSource::lfo:              return "LFO";
        case VirtualSource::envelopeFollower: return "ENV FOLLOW";
        case VirtualSource::macro1:           return "MACRO 1 (TUNE)";
        case VirtualSource::macro2:           return "MACRO 2 (PUNCH)";
        case VirtualSource::macro3:           return "MACRO 3 (BOOM)";
        case VirtualSource::macro4:           return "MACRO 4 (DIRT)";
        case VirtualSource::macro5:           return "MACRO 5 (SPACE)";
    }

    return {};
}

void ParameterPatchGraph::setLfoRateHz (float rateHz)
{
    lfoRateHz.store (juce::jlimit (0.05f, 20.0f, rateHz), std::memory_order_relaxed);
}

float ParameterPatchGraph::getLfoRateHz() const
{
    return lfoRateHz.load (std::memory_order_relaxed);
}

void ParameterPatchGraph::setLfoDepth (float depth)
{
    lfoDepth.store (juce::jlimit (0.0f, 1.0f, depth), std::memory_order_relaxed);
}

float ParameterPatchGraph::getLfoDepth() const
{
    return lfoDepth.load (std::memory_order_relaxed);
}

void ParameterPatchGraph::setMacroValue (int macroIndex, float value)
{
    if (! juce::isPositiveAndBelow (macroIndex, (int) macroValues.size()))
        return;

    macroValues[(size_t) macroIndex].store (juce::jlimit (0.0f, 1.0f, value),
                                            std::memory_order_relaxed);
}

float ParameterPatchGraph::getMacroValue (int macroIndex) const
{
    if (! juce::isPositiveAndBelow (macroIndex, (int) macroValues.size()))
        return 0.0f;

    return macroValues[(size_t) macroIndex].load (std::memory_order_relaxed);
}

void ParameterPatchGraph::setEnvelopeFollowerLevel (float level)
{
    envelopeFollowerLevel.store (juce::jlimit (0.0f, 1.0f, level), std::memory_order_relaxed);
}

float ParameterPatchGraph::getVirtualSourceNormalised (int index) const noexcept
{
    if (! isVirtualSource (index))
        return 0.5f;

    switch (virtualSourceAt (index))
    {
        case VirtualSource::lfo:
        {
            const float phase = lfoPhase.load (std::memory_order_relaxed);
            const float depth = lfoDepth.load (std::memory_order_relaxed);
            return juce::jlimit (0.0f, 1.0f, 0.5f + 0.5f * depth * std::sin (phase));
        }

        case VirtualSource::envelopeFollower:
            return envelopeFollowerLevel.load (std::memory_order_relaxed);

        case VirtualSource::macro1:
        case VirtualSource::macro2:
        case VirtualSource::macro3:
        case VirtualSource::macro4:
        case VirtualSource::macro5:
            return macroValues[(size_t) (static_cast<int> (virtualSourceAt (index))
                                         - static_cast<int> (VirtualSource::macro1))]
                .load (std::memory_order_relaxed);
    }

    return 0.5f;
}

void ParameterPatchGraph::advanceVirtualSources (float sampleRate, int numSamples) noexcept
{
    if (sampleRate <= 0.0f || numSamples <= 0)
        return;

    const float phaseDelta = juce::MathConstants<float>::twoPi
                             * lfoRateHz.load (std::memory_order_relaxed)
                             * (float) numSamples / sampleRate;

    float phase = lfoPhase.load (std::memory_order_relaxed) + phaseDelta;

    if (phase >= juce::MathConstants<float>::twoPi)
        phase -= juce::MathConstants<float>::twoPi;

    lfoPhase.store (phase, std::memory_order_relaxed);
}

float ParameterPatchGraph::wrapNormalised (float value) noexcept
{
    return value - std::floor (value);
}

void ParameterPatchGraph::prepareCableCvImpl (const Snapshot& snapshot,
                                              const float* effectiveNormalised,
                                              int effectiveCount,
                                              int parameterCount,
                                              float sampleRate,
                                              int samplesPerBlock,
                                              float bpm,
                                              float* cableCv,
                                              int cableCount) const
{
    // Kein Lock: der Aufruf kommt aus dem Audio-Thread und arbeitet ausschließlich
    // auf dem privaten Snapshot, den Atomics der virtuellen Quellen und den
    // Slew-/Hold-Arrays, die nur der Audio-Thread berührt.
    const int capacity = juce::jmin ((int) snapshot.connections.size(),
                                     juce::jmin ((int) slewState.size(), cableCount));
    const int count = juce::jlimit (0, capacity, snapshot.connectionCount);

    if (effectiveNormalised == nullptr || cableCv == nullptr)
        return;

    // 1/64-Note-Haltezeit in Samples; BPM <= 0 -> Blockrate.
    const float secondsPerHold = (bpm > 1.0f && sampleRate > 0.0f)
                                     ? (60.0f / bpm) / 16.0f
                                     : 0.0f;
    const int holdSamples = (int) std::round (secondsPerHold * sampleRate);

    for (int i = 0; i < count; ++i)
    {
        const auto& connection = snapshot.connections[(size_t) i];
        const int source = connection.sourceParam;

        if (! isValidSource (source, parameterCount))
        {
            cableCv[(size_t) i] = 0.0f;
            continue;
        }

        float cv = 0.0f;

        if (isVirtualSource (source))
        {
            const float normalised = getVirtualSourceNormalised (source);

            // LFO ist von Natur aus bipolar, Envelope-Follower/Makros unipolar.
            cv = (virtualSourceAt (source) == VirtualSource::lfo)
                     ? 2.0f * normalised - 1.0f
                     : normalised;
        }
        else
        {
            // Der Aufrufer darf einen kleineren Puffer als die Parameterzahl
            // übergeben (Engine: 256) -> niemals außerhalb lesen.
            const float normalised = (source < effectiveCount)
                                         ? juce::jlimit (0.0f, 1.0f,
                                                         effectiveNormalised[(size_t) source])
                                         : 0.5f;
            cv = connection.unipolar ? normalised : 2.0f * normalised - 1.0f;
        }

        cv *= juce::jlimit (-1.0f, 1.0f, connection.amount);

        // Slew (Ein-Pol, blockweise) – Zustand pro Kabelindex.
        if (connection.slewMs > 0.001f && sampleRate > 0.0f && samplesPerBlock > 0)
        {
            const float tauSamples = connection.slewMs * 0.001f * sampleRate;
            const float coeff = std::exp (-(float) samplesPerBlock / juce::jmax (1.0f, tauSamples));
            slewState[(size_t) i] = cv + (slewState[(size_t) i] - cv) * coeff;
            cv = slewState[(size_t) i];
        }
        else
        {
            slewState[(size_t) i] = cv;
        }

        // 1/64-Note Sample&Hold.
        if (connection.rate == CvRate::sixtyFourthNote && holdSamples > 0)
        {
            if (holdSamplesLeft[(size_t) i] > 0)
            {
                cableCv[(size_t) i] = heldCv[(size_t) i];
                holdSamplesLeft[(size_t) i] -= samplesPerBlock;
                continue;
            }

            heldCv[(size_t) i] = cv;
            holdSamplesLeft[(size_t) i] = holdSamples;
        }

        cableCv[(size_t) i] = cv;
    }

    for (int i = count; i < cableCount; ++i)
        cableCv[(size_t) i] = 0.0f;

    for (int i = count; i < capacity; ++i)
    {
        slewState[(size_t) i] = 0.0f;
        heldCv[(size_t) i] = 0.0f;
        holdSamplesLeft[(size_t) i] = 0;
    }
}

std::unique_ptr<juce::XmlElement> ParameterPatchGraph::createStateXml() const
{
    juce::SpinLock::ScopedLockType sl (stateLock);

    auto root = std::make_unique<juce::XmlElement> ("ParameterPatch");

    // v3 ist das Basisformat; v4 wird nur geschrieben, wenn ein Kabel die
    // erweiterten Felder (amount/unipolar/slew/rate) oder Wrap tatsächlich nutzt.
    bool needsV4 = wrapEnabled;

    for (const auto& connection : connections)
        if (connection.amount != 1.0f || connection.unipolar
            || connection.slewMs > 0.0f || connection.rate != CvRate::perBlock)
            needsV4 = true;

    root->setAttribute ("version", needsV4 ? 4 : 3);

    const auto& ids = ParameterIDs::getAllIDs();

    for (const auto& connection : connections)
    {
        auto* cable = new juce::XmlElement ("Cable");
        // Legacy-Indizes (v2) bleiben erhalten; die IDs machen den Patch robust
        // gegen Umsortierungen von getAllIDs().
        cable->setAttribute ("source", connection.sourceParam);
        cable->setAttribute ("target", connection.targetParam);

        if (connection.sourceParam >= 0 && connection.sourceParam < (int) ids.size())
            cable->setAttribute ("sourceId", ids[(size_t) connection.sourceParam]);
        else if (isVirtualSource (connection.sourceParam))
            cable->setAttribute ("sourceId", virtualSourceId (connection.sourceParam));

        if (connection.targetParam >= 0 && connection.targetParam < (int) ids.size())
            cable->setAttribute ("targetId", ids[(size_t) connection.targetParam]);

        // v4-Felder; v3-Reader ignorieren sie und bleiben bitgenau kompatibel.
        cable->setAttribute ("amount", (double) connection.amount);
        cable->setAttribute ("unipolar", connection.unipolar ? 1 : 0);
        cable->setAttribute ("slewMs", (double) connection.slewMs);
        cable->setAttribute ("rate", (int) connection.rate);

        root->addChildElement (cable);
    }

    for (int i = 0; i < parameterCount(); ++i)
    {
        if (! sourceEnabled[(size_t) i])
        {
            auto* node = new juce::XmlElement ("SourceEnable");
            node->setAttribute ("node", i);
            if (i < (int) ids.size())
                node->setAttribute ("id", ids[(size_t) i]);
            node->setAttribute ("enabled", 0);
            root->addChildElement (node);
        }
        if (! targetEnabled[(size_t) i])
        {
            auto* node = new juce::XmlElement ("TargetEnable");
            node->setAttribute ("node", i);
            if (i < (int) ids.size())
                node->setAttribute ("id", ids[(size_t) i]);
            node->setAttribute ("enabled", 0);
            root->addChildElement (node);
        }
    }

    // Virtuelle CV-Quellen (SND-156/157)
    {
        auto* lfo = new juce::XmlElement ("VirtualSource");
        lfo->setAttribute ("id", "virtual.lfo");
        lfo->setAttribute ("rateHz", (double) getLfoRateHz());
        lfo->setAttribute ("depth", (double) getLfoDepth());
        root->addChildElement (lfo);

        for (int m = 0; m < (int) macroValues.size(); ++m)
        {
            auto* macro = new juce::XmlElement ("VirtualSource");
            macro->setAttribute ("id", "virtual.macro" + juce::String (m + 1));
            macro->setAttribute ("value", (double) getMacroValue (m));
            root->addChildElement (macro);
        }
    }

    if (wrapEnabled)
    {
        auto* wrap = new juce::XmlElement ("CvWrap");
        wrap->setAttribute ("enabled", 1);
        root->addChildElement (wrap);
    }

    return root;
}

void ParameterPatchGraph::restoreStateXml (const juce::XmlElement& stateXml)
{
    juce::SpinLock::ScopedLockType sl (stateLock);

    const juce::XmlElement* patch = nullptr;

    if (stateXml.hasTagName ("ParameterPatch"))
    {
        patch = &stateXml;
    }
    else
    {
        for (const auto* child : stateXml.getChildIterator())
            if (child->hasTagName ("ParameterPatch"))
                patch = child;
    }

    connections.clear();
    sourceEnabled.fill (true);
    targetEnabled.fill (true);
    wrapEnabled = false;

    if (patch == nullptr)
        return;

    const int n = parameterCount();

    // v3: ID-Attribut hat Vorrang; v2-Dateien haben nur den Index.
    auto resolveNodeIndex = [n] (const juce::XmlElement& node) -> int
    {
        const auto id = node.getStringAttribute ("id");

        if (id.isNotEmpty())
        {
            if (const auto virtualIndex = indexOfVirtualId (id))
                return *virtualIndex;

            const auto resolved = indexOfId (id);
            return resolved.has_value() ? *resolved : -1;
        }

        const int index = node.getIntAttribute ("node", -1);
        return (index >= 0 && index < n) ? index : -1;
    };

    for (const auto* node : patch->getChildIterator())
    {
        if (node->hasTagName ("SourceEnable"))
        {
            const int index = resolveNodeIndex (*node);
            if (index >= 0)
                sourceEnabled[(size_t) index] = node->getIntAttribute ("enabled", 1) > 0;
        }
        else if (node->hasTagName ("TargetEnable"))
        {
            const int index = resolveNodeIndex (*node);
            if (index >= 0)
                targetEnabled[(size_t) index] = node->getIntAttribute ("enabled", 1) > 0;
        }
        else if (node->hasTagName ("CvWrap"))
        {
            wrapEnabled = node->getIntAttribute ("enabled", 0) > 0;
        }
        else if (node->hasTagName ("VirtualSource"))
        {
            const auto id = node->getStringAttribute ("id");

            if (id == "virtual.lfo")
            {
                setLfoRateHz ((float) node->getDoubleAttribute ("rateHz", 0.5));
                setLfoDepth ((float) node->getDoubleAttribute ("depth", 0.5));
            }
            else
            {
                for (int m = 0; m < (int) macroValues.size(); ++m)
                    if (id == "virtual.macro" + juce::String (m + 1))
                        setMacroValue (m, (float) node->getDoubleAttribute ("value", 0.0));
            }
        }
    }

    // Unbekannte IDs werden tolerant ignoriert (Kabel wird übersprungen).
    auto resolveCableIndex = [n] (const juce::XmlElement& cable,
                                  const char* idAttribute,
                                  const char* indexAttribute,
                                  bool allowLegacyVirtual) -> int
    {
        const auto id = cable.getStringAttribute (idAttribute);

        if (id.isNotEmpty())
        {
            if (const auto virtualIndex = indexOfVirtualId (id))
                return *virtualIndex;

            const auto resolved = indexOfId (id);
            return resolved.has_value() ? *resolved : -1;
        }

        const int rawIndex = cable.getIntAttribute (indexAttribute, -1);

        // Legacy: virtuelle Quellen lagen bei 248..254 (v2/v3 ohne sourceId).
        const int index = allowLegacyVirtual ? mapLegacyVirtualIndex (rawIndex) : rawIndex;

        if (index >= 0 && index < n)
            return index;

        return ParameterPatchGraph::isVirtualSource (index) ? index : -1;
    };

    for (const auto* cable : patch->getChildIterator())
    {
        if (! cable->hasTagName ("Cable"))
            continue;

        const int source = resolveCableIndex (*cable, "sourceId", "source", true);
        const int target = resolveCableIndex (*cable, "targetId", "target", false);

        if (source < 0 || target < 0)
            continue;

        if (hasConnection (source, target) || ! canConnectTopologyUnlocked (source, target))
            continue;

        if (connections.size() >= (size_t) maxConnectionCount)
            break;

        Connection connection;
        connection.sourceParam = source;
        connection.targetParam = target;

        // v2/v3-Dateien haben diese Attribute nicht -> neutrale Defaults.
        connection.amount = juce::jlimit (-1.0f, 1.0f,
                                          (float) cable->getDoubleAttribute ("amount", 1.0));
        connection.unipolar = cable->getIntAttribute ("unipolar", 0) > 0;
        connection.slewMs = juce::jlimit (0.0f, 20.0f,
                                          (float) cable->getDoubleAttribute ("slewMs", 0.0));
        connection.rate = cable->getIntAttribute ("rate", 0) > 0 ? CvRate::sixtyFourthNote
                                                                 : CvRate::perBlock;

        connections.push_back (connection);
    }
}

std::array<int, ParameterPatchGraph::maxParameterCount> ParameterPatchGraph::computeOrderUnlocked() const
{
    constexpr int N = maxParameterCount;
    const int n = juce::jmin (parameterCount(), N);

    std::array<int, N> order {};
    std::array<int, N> inDegree {};
    std::array<bool, N> placed {};

    for (const auto& connection : connections)
        if (connection.targetParam >= 0 && connection.targetParam < n)
            ++inDegree[(size_t) connection.targetParam];

    // Ready-Stack (Kahn ohne Heap): Startknoten absteigend eingestapelt, damit
    // die erste Welle aufsteigend abgearbeitet wird. Deterministisch und
    // O(n + Kabel) statt O(n^2) - bei 1229 Parametern relevant für den Audio-Thread.
    std::array<int, N> ready {};
    int readyCount = 0;

    for (int i = n - 1; i >= 0; --i)
        if (inDegree[(size_t) i] == 0)
            ready[(size_t) readyCount++] = i;

    int pos = 0;

    while (readyCount > 0 && pos < n)
    {
        const int chosen = ready[(size_t) --readyCount];

        if (chosen < 0 || chosen >= n || placed[(size_t) chosen])
            continue;

        placed[(size_t) chosen] = true;
        order[(size_t) pos++] = chosen;

        for (const auto& connection : connections)
            if (connection.sourceParam == chosen
                && connection.targetParam >= 0 && connection.targetParam < n)
                if (--inDegree[(size_t) connection.targetParam] == 0)
                    if (readyCount < N)
                        ready[(size_t) readyCount++] = connection.targetParam;
    }

    // Zyklen (sollten nicht auftreten) und nicht erreichbare Knoten anhängen.
    for (int i = 0; i < n && pos < n; ++i)
        if (! placed[(size_t) i])
            order[(size_t) pos++] = i;

    // Unbenutzte Indizes auf -1 setzen; Engine iteriert nur bis parameterCount().
    while (pos < N)
        order[(size_t) pos++] = -1;

    return order;
}

std::array<int, ParameterPatchGraph::maxParameterCount> ParameterPatchGraph::getProcessingOrder() const
{
    juce::SpinLock::ScopedLockType sl (stateLock);
    return computeOrderUnlocked();
}
