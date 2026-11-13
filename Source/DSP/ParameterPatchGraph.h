#pragma once

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <optional>
#include <vector>

#include <juce_core/juce_core.h>

//==============================================================================
/** ParameterPatchGraph – parametrierbare CV-/Modulationsverkabelung.
 *
 *  Jeder Parameter-ID-Index aus ParameterIDs::getAllIDs() besitzt sowohl einen
 *  Ausgang (normierter Wert 0..1 als bipolares CV-Signal -1..+1) als auch einen
 *  Eingang. Kabel von ParamOut nach ParamIn werden bipolar summiert;
 *  der effektive Normwert des Ziel-Parameters ist clamp01(base + sum(sourceCV)).
 *
 *  Jedes Kabel hat zusätzlich:
 *   - amount    -1..+1  (Skalierung inkl. Inversion, UI-Knob)
 *   - unipolar  false = -1..+1, true = 0..+1
 *   - slewMs    0..20 ms Ein-Pol-Glättung (0 = aus)
 *   - rate      pro Block oder 1/64-Note Sample&Hold
 *
 *  Virtuelle CV-Quellen (LFO, Envelope-Follower, Makro 1-5) liegen als feste
 *  Indizes oberhalb der Parameter-Indizes und sind reine Quellen.
 *
 *  Die Topologie wird zyklenfrei gehalten; mehrere Kabel an einen Eingang werden
 *  summiert. Zugriff auf die Verbindungstabelle ist über einen Lock geschützt,
 *  die DSP arbeitet mit Snapshots.
 */
class ParameterPatchGraph
{
public:
    /** Aktualisierungsrate eines Kabels (SND-154). */
    enum class CvRate
    {
        perBlock = 0,      ///< CV wird einmal pro Audio-Block neu berechnet.
        sixtyFourthNote    ///< Sample&Hold: CV bleibt für eine 1/64-Note stehen.
    };

    /** Virtuelle CV-Quellen ohne APVTS-Parameter (SND-156/157). */
    enum class VirtualSource
    {
        lfo = 0,
        envelopeFollower,
        macro1,
        macro2,
        macro3,
        macro4,
        macro5
    };

    static constexpr int virtualSourceCount = 7;

    /** Kapazität für reale Parameter-Indizes (Layout: 1229, Reserve bis 4096). */
    static constexpr int maxParameterCount = 4096;

    /** Maximalzahl gleichzeitig gepatchter Kabel (realistisch << Parameterzahl). */
    static constexpr int maxConnectionCount = 512;

    /** Indizes der virtuellen Quellen liegen oberhalb aller Parameter-Indizes,
        damit kein realer Parameter je als virtuelle Quelle gilt. */
    static constexpr int virtualSourceBase = maxParameterCount;

    /** Gesamtkapazität der Source-/Target-Enable-Arrays. */
    static constexpr int sourceCapacity = virtualSourceBase + virtualSourceCount;

    /** Alte Basis der virtuellen Quellen (vor der Kapazitätserweiterung);
        wird nur beim Laden numerischer v2/v3-Kabel ohne ID benötigt. */
    static constexpr int legacyVirtualSourceBase = 248;

    struct Connection
    {
        int sourceParam = -1;              // Index in ParameterIDs::getAllIDs() oder virtuelle Quelle
        int targetParam = -1;              // Index in ParameterIDs::getAllIDs()
        float amount = 0.25f;              // -1..+1 (SND-151); 0.25 = 25 % des Bereichs
        bool unipolar = false;             // SND-153
        float slewMs = 0.0f;               // 0..20 ms, 0 = aus (SND-155)
        CvRate rate = CvRate::perBlock;    // SND-154
    };

    /** Allokationsfreier Audio-Thread-Snapshot (ein Lock, keine Einzel-Locks). */
    struct Snapshot
    {
        std::array<Connection, maxConnectionCount> connections {};
        std::array<bool, sourceCapacity> sourceEnabled {};
        std::array<bool, sourceCapacity> targetEnabled {};
        std::array<int, maxParameterCount> order {};
        int connectionCount = 0;
        int parameterCount = 0;
        bool wrapEnabled = false; ///< SND-161: true = 0..1 wrap statt clamp
    };

    ParameterPatchGraph();

    void clearAll();
    void resetEnables();

    bool canConnect (int sourceParam, int targetParam) const;
    bool addConnection (int sourceParam, int targetParam);

    /** Verbindung mit eigenem Amount (z. B. Preset-Makro-Kabel). */
    bool addConnection (int sourceParam, int targetParam, float amount);
    bool removeConnection (int connectionIndex);

    /** Entfernt eine Verbindung wertbasiert (stabil gegen Index-Verschiebungen). */
    bool removeConnection (int sourceParam, int targetParam);
    void removeConnectionsAt (int parameterIndex);

    bool isSourceEnabled (int parameterIndex) const;
    bool isTargetEnabled (int parameterIndex) const;
    std::array<bool, sourceCapacity> getSourceEnabled() const;
    std::array<bool, sourceCapacity> getTargetEnabled() const;
    void setSourceEnabled (int parameterIndex, bool enabled);
    void setTargetEnabled (int parameterIndex, bool enabled);

    std::vector<Connection> getConnections() const;
    int getConnectionCount() const;
    std::vector<Connection> getConnectionsAt (int parameterIndex) const;

    // --- Kabel-Eigenschaften (SND-151/153/154/155) ---
    bool setConnectionAmount (int sourceParam, int targetParam, float amount);
    bool setConnectionUnipolar (int sourceParam, int targetParam, bool unipolar);
    bool setConnectionSlewMs (int sourceParam, int targetParam, float slewMs);
    bool setConnectionRate (int sourceParam, int targetParam, CvRate rate);

    /** SND-161: globales Wrap-Flag für das Ziel-Clamping. */
    void setWrapEnabled (bool wrap);
    bool isWrapEnabled() const;

    // --- Virtuelle CV-Quellen (SND-156/157) ---
    static int virtualSourceIndex (VirtualSource source) noexcept;
    static bool isVirtualSource (int index) noexcept;
    static VirtualSource virtualSourceAt (int index) noexcept;
    static juce::String virtualSourceName (int index);

    void setLfoRateHz (float rateHz);
    float getLfoRateHz() const;
    void setLfoDepth (float depth);
    float getLfoDepth() const;

    /** Makro 1..5 (Index 0..4), Wert 0..1. */
    void setMacroValue (int macroIndex, float value);
    float getMacroValue (int macroIndex) const;

    /** Von der Engine pro Block geschrieben (0..1). */
    void setEnvelopeFollowerLevel (float level);

    /** Liefert die aktuelle virtuelle Quelle als Normwert 0..1 (Audio-Thread-sicher). */
    float getVirtualSourceNormalised (int index) const noexcept;

    /** Audio-Thread: schaltet die LFO-Phase für den nächsten Block weiter. */
    void advanceVirtualSources (float sampleRate, int numSamples) noexcept;

    /** Audio-Thread: wendet amount, Polarität, Rate und Slew auf alle Kabel an.
        Allokationsfrei, ohne Lock (arbeitet nur auf dem privaten Snapshot).
        cableCv[i] gehört zu snapshot.connections[i]. Die Puffergrößen werden
        zur Laufzeit aus den übergebenen Arrays bestimmt (Engine übergibt
        weiterhin 256er-Puffer, Tests dürfen größere Puffer nutzen). */
    template <typename EffectiveValues, typename CableCvValues>
    void prepareCableCv (const Snapshot& snapshot,
                         const EffectiveValues& effectiveNormalised,
                         int parameterCount,
                         float sampleRate,
                         int samplesPerBlock,
                         float bpm,
                         CableCvValues& cableCv) const
    {
        prepareCableCvImpl (snapshot,
                            effectiveNormalised.data(),
                            static_cast<int> (effectiveNormalised.size()),
                            parameterCount,
                            sampleRate,
                            samplesPerBlock,
                            bpm,
                            cableCv.data(),
                            static_cast<int> (cableCv.size()));
    }

    /** SND-161: 0..1-Wrap für einen CV-Summenwert. */
    static float wrapNormalised (float value) noexcept;

    /** Kopiert Verbindungen, Enable-Flags und Reihenfolge unter EINEM Lock. */
    void fillSnapshot (Snapshot& snapshot) const;

    std::unique_ptr<juce::XmlElement> createStateXml() const;
    void restoreStateXml (const juce::XmlElement& stateXml);

    std::array<int, maxParameterCount> getProcessingOrder() const;

private:
    std::vector<Connection> connections;
    std::array<bool, sourceCapacity> sourceEnabled {};
    std::array<bool, sourceCapacity> targetEnabled {};
    bool wrapEnabled = false;

    // Virtuelle Quellen: atomar, da Audio-Thread liest und Message-Thread schreibt.
    // Fuer lfoRateHz/lfoDepth und macroValues ist die APVTS die fuehrende Quelle
    // (Parameter "lfoRate", "lfoDepth", "macro1".."macro5"); die Engine spiegelt
    // sie pro Block hierher. Die XML-Werte dienen nur noch der Migration alter
    // Zustaende und werden danach von den APVTS-Werten ueberschrieben.
    std::atomic<float> lfoRateHz { 0.5f };
    std::atomic<float> lfoDepth { 0.5f };
    std::atomic<float> lfoPhase { 0.0f };
    std::atomic<float> envelopeFollowerLevel { 0.0f };
    std::array<std::atomic<float>, 5> macroValues {};

    // Slew-/Hold-Zustand pro Kabelindex (nur Audio-Thread, mutable für const-API).
    mutable std::array<float, maxConnectionCount> slewState {};
    mutable std::array<float, maxConnectionCount> heldCv {};
    mutable std::array<int, maxConnectionCount> holdSamplesLeft {};

    mutable juce::SpinLock stateLock;

    static int parameterCount();

    /** Parameter-ID -> Index in getAllIDs(); std::nullopt, wenn unbekannt (nur Message-Thread). */
    static std::optional<int> indexOfId (const juce::String& parameterId);

    bool reachesParameter (int fromParam, int targetParam) const;
    bool hasConnection (int sourceParam, int targetParam) const noexcept;
    bool reachesParameterUnlocked (int fromParam, int targetParam) const;
    bool canConnectTopologyUnlocked (int sourceParam, int targetParam) const;
    std::array<int, maxParameterCount> computeOrderUnlocked() const;

    /** Implementierung von prepareCableCv(); Puffergrenzen sind explizit. */
    void prepareCableCvImpl (const Snapshot& snapshot,
                             const float* effectiveNormalised,
                             int effectiveCount,
                             int parameterCount,
                             float sampleRate,
                             int samplesPerBlock,
                             float bpm,
                             float* cableCv,
                             int cableCount) const;

    bool updateConnection (int sourceParam, int targetParam,
                           const std::function<void (Connection&)>& update);
};
