#include "PatchGraph.h"

#include <algorithm>

namespace
{
    /** SND-066: Click optional zusätzlich in den EQ patchen (statt nur DriveIn).

        Der Werks-Patch bleibt bei 17 Kabeln (Drive-Split unverändert), weil
        Patch-/Preset-Roundtrips und die Factory-Presets auf dieser Anzahl
        aufbauen. Zum Aktivieren auf true setzen: dann kommt ein 18. Kabel
        ClickOut -> EqIn hinzu (Drive bleibt Band-Split, SND-014). */
    constexpr bool defaultPatchRoutesClickIntoEq = false;

    constexpr std::array<PatchGraph::Connection, 17> defaultConnections
    {{
        /* 0 */ { Jack::MembraneOut, Jack::TubeIn },
        /* 1 */ { Jack::TubeOut,     Jack::DriveIn },
        /* 2 */ { Jack::MembraneOut, Jack::DriveIn }, // Membran parallel über Tube
        /* 3 */ { Jack::SubOut,      Jack::DriveIn },
        /* 4 */ { Jack::ClickOut,    Jack::DriveIn },
        /* 5 */ { Jack::DriveOut,    Jack::EqIn },
        /* 6 */ { Jack::EqOut,       Jack::CompIn },
        /* 7 */ { Jack::CompOut,     Jack::RoomIn },
        /* 8 */ { Jack::CompOut,     Jack::HiggsIn },
        /* 9 */ { Jack::CompOut,     Jack::ReverbIn },
        /* 10*/ { Jack::CompOut,     Jack::OutputIn },
        /* 11*/ { Jack::RoomOut,     Jack::OutputIn },
        /* 12*/ { Jack::HiggsOut,    Jack::OutputIn },
        /* 13*/ { Jack::ReverbOut,   Jack::OutputIn },
        /* 14*/ { Jack::CompOut,          Jack::MaterialRoomKickIn },   // Material Room: Kick-Layer
        /* 15*/ { Jack::RoomOut,          Jack::MaterialRoomRumbleIn }, // Material Room: Rumble-Layer
        /* 16*/ { Jack::MaterialRoomOut,  Jack::OutputIn }              // Material Room: Wet additiv
    }};
}

//==============================================================================
void PatchGraph::applyDefaultPatch()
{
    juce::SpinLock::ScopedLockType sl (stateLock);
    connections.assign (defaultConnections.begin(), defaultConnections.end());

    // SND-066: optionales Kabel ClickOut -> EqIn (siehe oben).
    if (defaultPatchRoutesClickIntoEq)
        connections.push_back ({ Jack::ClickOut, Jack::EqIn });
}

void PatchGraph::clearAll()
{
    juce::SpinLock::ScopedLockType sl (stateLock);
    connections.clear();
}

std::vector<PatchGraph::Connection> PatchGraph::getConnections() const
{
    juce::SpinLock::ScopedLockType sl (stateLock);
    return connections;
}

void PatchGraph::fillSnapshot (Snapshot& out) const
{
    juce::SpinLock::ScopedLockType sl (stateLock);

    out.connectionCount = juce::jmin ((int) connections.size(), Snapshot::maxConnections);

    for (int i = 0; i < out.connectionCount; ++i)
        out.connections[(size_t) i] = connections[(size_t) i];

    out.order = computeOrderUnlocked();
}

int PatchGraph::getConnectionCount() const
{
    juce::SpinLock::ScopedLockType sl (stateLock);
    return static_cast<int> (connections.size());
}

std::vector<PatchGraph::Connection> PatchGraph::getConnectionsAt (Jack jack) const
{
    juce::SpinLock::ScopedLockType sl (stateLock);

    std::vector<Connection> result;

    for (const auto& c : connections)
        if (c.from == jack || c.to == jack)
            result.push_back (c);

    return result;
}

bool PatchGraph::hasConnection (Jack from, Jack to) const noexcept
{
    for (const auto& c : connections)
        if (c.from == from && c.to == to)
            return true;

    return false;
}

bool PatchGraph::canConnectUnlocked (Jack from, Jack to) const
{
    const int fi = static_cast<int> (from);
    const int ti = static_cast<int> (to);

    if (fi < 0 || fi >= static_cast<int> (Jack::JackCount)) return false;
    if (ti < 0 || ti >= static_cast<int> (Jack::JackCount)) return false;

    if (! isInput (to))   return false;   // Ziel muss Eingangsbuchse sein
    if (isInput (from))  return false;    // Quelle muss Ausgangsbuchse sein
    if (from == to)      return false;

    // Zyklus-Vermeidung: ein Zyklus entsteht genau dann, wenn es vom Modul
    // der Ziel-Buchse schon einen Pfad zurück zum Modul der Quell-Buchse gibt.
    return ! reachesModule (moduleOf (to), moduleOf (from));
}

bool PatchGraph::canConnect (Jack from, Jack to) const
{
    juce::SpinLock::ScopedLockType sl (stateLock);
    return canConnectUnlocked (from, to);
}

bool PatchGraph::addConnection (Jack from, Jack to)
{
    juce::SpinLock::ScopedLockType sl (stateLock);

    if (hasConnection (from, to))
        return false;

    if (! canConnectUnlocked (from, to))
        return false;

    if (connections.size() >= (size_t) Snapshot::maxConnections)
        return false;

    connections.push_back ({ from, to });
    return true;
}

bool PatchGraph::removeConnection (Jack from, Jack to)
{
    juce::SpinLock::ScopedLockType sl (stateLock);

    const auto it = std::find_if (connections.begin(), connections.end(),
        [from, to] (const Connection& c) { return c.from == from && c.to == to; });

    if (it == connections.end())
        return false;

    connections.erase (it);
    return true;
}

bool PatchGraph::removeConnection (int connectionIndex)
{
    juce::SpinLock::ScopedLockType sl (stateLock);

    if (connectionIndex < 0 || connectionIndex >= static_cast<int> (connections.size()))
        return false;

    connections.erase (connections.begin() + connectionIndex);
    return true;
}

void PatchGraph::removeConnectionsAt (Jack jack)
{
    juce::SpinLock::ScopedLockType sl (stateLock);

    connections.erase (std::remove_if (connections.begin(), connections.end(),
        [jack] (const Connection& c) { return c.from == jack || c.to == jack; }),
        connections.end());
}

std::unique_ptr<juce::XmlElement> PatchGraph::createStateXml() const
{
    juce::SpinLock::ScopedLockType sl (stateLock);

    auto root = std::make_unique<juce::XmlElement> ("Patch");
    root->setAttribute ("version", 1);

    for (const auto& connection : connections)
    {
        auto* cable = new juce::XmlElement ("Cable");
        cable->setAttribute ("from", static_cast<int> (connection.from));
        cable->setAttribute ("to",   static_cast<int> (connection.to));
        root->addChildElement (cable);
    }

    return root;
}

void PatchGraph::restoreStateXml (const juce::XmlElement& stateXml)
{
    juce::SpinLock::ScopedLockType sl (stateLock);

    const juce::XmlElement* patch = nullptr;

    if (stateXml.hasTagName ("Patch"))
    {
        patch = &stateXml;
    }
    else
    {
        // Alte Builds konnten durch einen Fehler mehrere <Patch>-Knoten anhängen;
        // der jeweils zuletzt geschriebene ist der aktuelle Stand.
        for (const auto* child : stateXml.getChildIterator())
            if (child->hasTagName ("Patch"))
                patch = child;
    }

    if (patch == nullptr)
    {
        connections.assign (defaultConnections.begin(), defaultConnections.end());
        return;
    }

    connections.clear();

    auto isJackIndex = [] (int raw) noexcept
    {
        return raw >= 0 && raw < static_cast<int> (Jack::JackCount);
    };

    for (const auto* cable : patch->getChildIterator())
    {
        if (! cable->hasTagName ("Cable"))
            continue;

        const int fromRaw = cable->getIntAttribute ("from", -1);
        const int toRaw   = cable->getIntAttribute ("to",   -1);

        if (! isJackIndex (fromRaw) || ! isJackIndex (toRaw))
            continue;

        const Jack from = static_cast<Jack> (fromRaw);
        const Jack to   = static_cast<Jack> (toRaw);

        if (hasConnection (from, to) || ! canConnectUnlocked (from, to))
            continue;

        if (connections.size() >= (size_t) Snapshot::maxConnections)
            break;

        connections.push_back ({ from, to });
    }
}

bool PatchGraph::reachesModule (ModuleIndex fromModule, ModuleIndex targetModule) const
{
    // DFS entlang der Verbindungen (aus Ausgängen heraus), meldet Erreichbarkeit.
    std::vector<ModuleIndex> stack { fromModule };
    bool visited[(int) ModuleIndex::ModuleCount] = {};

    while (! stack.empty())
    {
        const ModuleIndex current = stack.back();
        stack.pop_back();

        if (current == targetModule)
            return true;

        const int ci = static_cast<int> (current);

        if (visited[ci])
            continue;

        visited[ci] = true;

        for (const auto& c : connections)
            if (moduleOf (c.from) == current)
                stack.push_back (moduleOf (c.to));
    }

    return false;
}

std::array<int, (int) ModuleIndex::ModuleCount> PatchGraph::computeOrderUnlocked() const
{
    constexpr int N = static_cast<int> (ModuleIndex::ModuleCount);

    std::array<int, N> inDegree {};
    std::array<bool, N> placed {};
    std::array<int, N> order {};

    for (const auto& connection : connections)
        ++inDegree[(size_t) moduleOf (connection.to)];

    int pos = 0;

    while (pos < N)
    {
        int chosen = -1;

        for (int module = 0; module < N; ++module)
            if (! placed[(size_t) module] && inDegree[(size_t) module] == 0)
            {
                chosen = module;
                break;
            }

        // Defensiv: bei einem (durch die UI nicht erzeugbaren) Zyklus trotzdem
        // das nächste noch nicht platzierte Modul nehmen, statt -1 zu indizieren.
        if (chosen < 0)
            for (int module = 0; module < N; ++module)
                if (! placed[(size_t) module])
                {
                    chosen = module;
                    break;
                }

        if (chosen < 0)
            break;

        order[(size_t) pos++] = chosen;
        placed[(size_t) chosen] = true;

        for (const auto& connection : connections)
            if (moduleOf (connection.from) == static_cast<ModuleIndex> (chosen))
                --inDegree[(size_t) moduleOf (connection.to)];
    }

    // Garantie: jedes Modul genau einmal, auch wenn oben vorzeitig abgebrochen wurde.
    for (int module = 0; module < N && pos < N; ++module)
        if (! placed[(size_t) module])
            order[(size_t) pos++] = module;

    return order;
}

std::array<int, (int) ModuleIndex::ModuleCount> PatchGraph::getProcessingOrder() const
{
    juce::SpinLock::ScopedLockType sl (stateLock);
    return computeOrderUnlocked();
}
