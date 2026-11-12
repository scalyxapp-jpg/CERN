#pragma once

#include <array>
#include <memory>
#include <vector>

#include <juce_core/juce_core.h>

//==============================================================================
/** PatchGraph – modulare Signal-Verkabelung (Eurorack-/Hardware-artig).
 *
 *  Jedes Modul besitzt benannte Ein-/Ausgangsbuchsen (Jacks). Eine Verbindung
 *  kopiert das Ausgangssignal einer Buchse auf die Eingangsbuchse; mehrere
 *  Kabel an einen Eingang werden summiert (das ist der Eurorack-Standard).
 *  Die Reihenfolge der Module wird topologisch aus den Verbindungen berechnet;
 *  Zyklen werden beim Verkabeln abgelehnt (Feedback ist ohne Laufzeit-Zustand
 *  nicht sicher möglich).
 *
 *  Alle Lese- und Schreibzugriffe auf die Verbindungstabelle sind über einen
 *  internen Lock geschützt; die Engine arbeitet pro Audio-Block mit Snapshots.
 *
 *  Die Default-Verschaltung (applyDefaultPatch) entspricht dem ursprünglichen
 *  Signal-Flow: Membrane -> Tube -> Drive -> EQ -> Compressor, mit Click/Sub
 *  als zusätzliche Quellen in Drive und Room/Higgs/Reverb parallel zum Ausgang.
 */

enum class Jack : int
{
    // Ausgänge
    MembraneOut = 0,
    TubeOut,
    SubOut,
    ClickOut,
    DriveOut,
    EqOut,
    CompOut,
    RoomOut,
    HiggsOut,
    ReverbOut,
    OutputOut,

    // Eingänge
    TubeIn,
    DriveIn,
    EqIn,
    CompIn,
    RoomIn,
    HiggsIn,
    ReverbIn,
    OutputIn,

    // Material Room (Material Room + Sub Grumble, MATERIAL_ROOM_SPEC.md §4)
    MaterialRoomOut,       // OUT, stereo
    MaterialRoomKickIn,    // IN,  stereo
    MaterialRoomRumbleIn,  // IN,  stereo

    JackCount
};

enum class ModuleIndex : int
{
    Membrane = 0,
    Tube,
    Sub,
    Click,
    Drive,
    Eq,
    Compressor,
    Room,
    Higgs,
    Reverb,
    OutputModule,
    MaterialRoom,
    ModuleCount
};

//==============================================================================
struct JackDef
{
    const char* displayName; ///< kurze, beschriftete Buchse (z. B. "OUT")
    ModuleIndex module;      ///< zugehöriges Modul
    bool isInput;            ///< Eingang oder Ausgang
    bool isStereo;           ///< Stereo-Paar (true) oder Mono (false)
};

inline constexpr std::array<JackDef, (int) Jack::JackCount> jackDefs {{
    /* MembraneOut */ JackDef { "OUT",  ModuleIndex::Membrane,     false, false },
    /* TubeOut     */ JackDef { "OUT",  ModuleIndex::Tube,         false, false },
    /* SubOut      */ JackDef { "OUT",  ModuleIndex::Sub,          false, false },
    /* ClickOut    */ JackDef { "OUT",  ModuleIndex::Click,        false, false },
    /* DriveOut    */ JackDef { "OUT",  ModuleIndex::Drive,        false, false },
    /* EqOut       */ JackDef { "OUT",  ModuleIndex::Eq,           false, false },
    /* CompOut     */ JackDef { "OUT",  ModuleIndex::Compressor,   false, false },
    /* RoomOut     */ JackDef { "OUT",  ModuleIndex::Room,         false, false },
    /* HiggsOut    */ JackDef { "OUT",  ModuleIndex::Higgs,        false, true  },
    /* ReverbOut   */ JackDef { "OUT",  ModuleIndex::Reverb,       false, true  },
    /* OutputOut   */ JackDef { "OUT",  ModuleIndex::OutputModule, false, true  },

    /* TubeIn      */ JackDef { "IN",   ModuleIndex::Tube,         true,  false },
    /* DriveIn     */ JackDef { "IN",   ModuleIndex::Drive,        true,  false },
    /* EqIn        */ JackDef { "IN",   ModuleIndex::Eq,           true,  false },
    /* CompIn      */ JackDef { "IN",   ModuleIndex::Compressor,   true,  false },
    /* RoomIn      */ JackDef { "IN",   ModuleIndex::Room,         true,  false },
    /* HiggsIn     */ JackDef { "IN",   ModuleIndex::Higgs,        true,  true  },
    /* ReverbIn    */ JackDef { "IN",   ModuleIndex::Reverb,       true,  true  },
    /* OutputIn    */ JackDef { "IN",   ModuleIndex::OutputModule, true,  true  },

    /* MaterialRoomOut      */ JackDef { "OUT",  ModuleIndex::MaterialRoom, false, true },
    /* MaterialRoomKickIn   */ JackDef { "IN",   ModuleIndex::MaterialRoom, true,  true },
    /* MaterialRoomRumbleIn */ JackDef { "IN",   ModuleIndex::MaterialRoom, true,  true }
}};

//==============================================================================
struct ModuleDef
{
    const char* id;          ///< interner Modul-Id (Debug/Tests)
    const char* displayName; ///< graviertes Label (obere Platte)
    const char* paramNames;  ///< Parameter-Namen, die zum Modul gehören (kleine Zeile)
    int row;                 ///< Panel-Reihe (0..2)
    int column;              ///< Panel-Spalte (0..3)
};

inline constexpr std::array<ModuleDef, (int) ModuleIndex::ModuleCount> moduleDefs {{
    /* Membrane   */ ModuleDef { "membrane",  "MEMBRANE",   "TENSION RADIUS MASS DAMPING MODES NL STIFF F0", 0, 0 },
    /* Tube       */ ModuleDef { "tube",      "TUBE",       "FREQ Q LEVEL COUPLING METALLIC",                0, 1 },
    /* Sub        */ ModuleDef { "sub",       "SUB",        "LEVEL DECAY DETUNE SHAPE",                      0, 2 },
    /* Click      */ ModuleDef { "click",     "CLICK",      "LEVEL DECAY TONE DIST BEATER",                  0, 3 },
    /* Drive      */ ModuleDef { "drive",     "DRIVE",      "AMOUNT MIX CHARACTER PRE-HP",                   1, 0 },
    /* Eq         */ ModuleDef { "eq",        "EQUALIZER",  "SUB GAIN PUNCH GAIN AIR GAIN RUMBLE",             1, 1 },
    /* Compressor */ ModuleDef { "comp",      "COMPRESSOR", "THRESH RATIO ATTACK REL KNEE MAKEUP",            1, 2 },
    /* Room       */ ModuleDef { "room",      "ROOM",       "LEVEL DECAY RUMBLE",                             1, 3 },
    /* Higgs      */ ModuleDef { "higgs",     "HIGGS FIELD","PHI THETA STR VEV LAM YUK BOS SYM DECAY MIX",    2, 0 },
    /* Reverb     */ ModuleDef { "reverb",    "REVERB",     "MAT PRED DAMP SIZE MIX LOWC UTC",                2, 1 },
    /* OutputModule */ ModuleDef { "output",  "OUTPUT",     "GAIN PAN WIDTH MONO CLIP LIMIT",                 2, 2 },
    /* MaterialRoom */ ModuleDef { "materialroom", "MATERIAL ROOM", "MAT SIZE MIX LAYER WIDTH GRUMB P-RATE P-DEPTH WAVE", 2, 3 }
}};

static_assert (jackDefs.size() == (int) Jack::JackCount);
static_assert (moduleDefs.size() == (int) ModuleIndex::ModuleCount);

//==============================================================================
class PatchGraph
{
public:
    struct Connection
    {
        Jack from;   ///< immer eine Ausgangsbuchse
        Jack to;     ///< immer eine Eingangsbuchse
    };

    /** Allokationsfreier Audio-Thread-Snapshot (ein Lock, Verbindungen + Reihenfolge). */
    struct Snapshot
    {
        static constexpr int maxConnections = 64;

        std::array<Connection, maxConnections> connections {};
        std::array<int, (int) ModuleIndex::ModuleCount> order {};
        int connectionCount = 0;
    };

    PatchGraph() { applyDefaultPatch(); }

    /** Werks-Patch (ursprüngliche interne Verschaltung). */
    void applyDefaultPatch();

    /** Entfernt alle Verbindungen. */
    void clearAll();

    /** Prüft, ob eine Verbindung möglich ist.
        Ungültig bei falscher Richtung, gleicher Buchse oder Zyklus. */
    bool canConnect (Jack from, Jack to) const;

    /** Fügt eine Verbindung hinzu (nach canConnect() true).
        Lehnt ab, wenn die Snapshot-Kapazität erreicht ist. */
    bool addConnection (Jack from, Jack to);

    /** Entfernt die Verbindung an Index (nach getConnections()).
        Nur für Aufrufer, die den Index unmittelbar zuvor gelesen haben. */
    bool removeConnection (int connectionIndex);

    /** Entfernt eine Verbindung wertbasiert (stabil gegen Index-Verschiebungen). */
    bool removeConnection (Jack from, Jack to);

    /** Entfernt alle Verbindungen, die die Buchse `jack` berühren. */
    void removeConnectionsAt (Jack jack);

    /** Kopiert Verbindungen und topologische Reihenfolge unter EINEM Lock. */
    void fillSnapshot (Snapshot& out) const;

    /** Snapshot der aktuellen Verbindungen. */
    std::vector<Connection> getConnections() const;

    /** Anzahl Verbindungen. */
    int getConnectionCount() const;

    /** Verbindungen, die eine bestimmte Buchse berühren. */
    std::vector<Connection> getConnectionsAt (Jack jack) const;

    /** Serialisiert den Patch als <Patch>-Element (z. B. für Preset-/State-XML). */
    std::unique_ptr<juce::XmlElement> createStateXml() const;

    /** Lädt den Patch aus einem <Patch>-Element. Fehlt es, wird der Werks-Patch gesetzt;
        ungültige/doppelte Kabel werden tolerant ignoriert. */
    void restoreStateXml (const juce::XmlElement& stateXml);

    /** Topologische Modul-Reihenfolge (12 Module, garantiert vollständig). */
    std::array<int, (int) ModuleIndex::ModuleCount> getProcessingOrder() const;

private:
    std::vector<Connection> connections;
    mutable juce::SpinLock stateLock;

    bool hasConnection (Jack from, Jack to) const noexcept;
    bool canConnectUnlocked (Jack from, Jack to) const;
    bool reachesModule (ModuleIndex fromModule, ModuleIndex targetModule) const;
    std::array<int, (int) ModuleIndex::ModuleCount> computeOrderUnlocked() const;

    static ModuleIndex moduleOf (Jack jack) noexcept { return jackDefs[(int) jack].module; }
    static bool isInput (Jack jack) noexcept { return jackDefs[(int) jack].isInput; }
};
