#pragma once

#include <algorithm>
#include <cmath>

//==============================================================================
/** Automatisches Layering: zwei vollstaendig abgeleitete Zusatz-Layer.

    Wenn "Layering enabled" aktiv ist, legt diese Stufe zwei weitere Ebenen
    unter/ueber die Basis-Wave. Alle Werte (Frequenz, Laenge, Attack, Decay,
    Pegel, Dichte, Bandlage) werden aus den bereits gesetzten Modul-Parametern
    berechnet – es gibt keine eigenen Regler.

    Layer 1 "DEPTH":   Sinus-Stuetzung auf dem Grundton (bzw. eine Oktave
                       darunter, wenn f0 hoch genug ist) mit langem Decay.
                       Traegt Tiefe, Rumble und Subbass bis in den Tail.
    Layer 2 "PRESENCE": bandbegrenztes Rausch-/Ton-Gemisch (Dichte-abhaengig)
                       mit sehr kurzem Decay und wenigen ms Delay. Traegt
                       Definition und Durchsetzung im Attack/Click.

    Beide Layer folgen exakt derselben Modulation wie die Basis: die
    Momentanfrequenz wird pro Sample von aussen vorgegeben (Pitch-Envelope,
    Glide, Velocity->Pitch), die Velocity skaliert beide Huellen.

    Pegelneutralitaet: die Gewichte werden equal-power normiert
    (1/sqrt(1 + k1^2 + k2^2)) und derselbe Faktor auf Membrane+Body
    angewendet. Damit bleibt die RMS-Energie konstant, das Layering macht
    den Kick breiter, nicht lauter.
*/
class LayeringStage
{
public:
    /** Vom Engine pro Block aus den bestehenden Parametern gefuellt. */
    struct Params
    {
        bool  enabled          { false };
        float fundamentalHz    { 46.25f };
        float subLevel         { 0.58f };
        float subDecayMs       { 520.0f };
        float bodyAttackMs     { 4.0f };
        float bodyDecayMs      { 300.0f };
        float clickLevel       { 0.45f };
        float clickDecayMs     { 35.0f };
        float clickTone        { 0.5f };
        float clickDistortion  { 0.2f };
        float driveAmount      { 0.3f };
    };

    /** Abgeleitete Werte fuer die GUI-Anzeige (Waveform-Legende). */
    struct Info
    {
        bool  enabled     { false };
        float lowHz       { 0.0f };
        float lowAttackMs { 0.0f };
        float lowDecayMs  { 0.0f };
        float lowGainDb   { -120.0f };
        float highHz      { 0.0f };
        float highAttackMs{ 0.0f };
        float highDecayMs { 0.0f };
        float highGainDb  { -120.0f };
        float highDelayMs { 0.0f };
        float density     { 0.0f };
        float baseTrimDb  { 0.0f };
    };

    void prepare (double newSampleRate);
    void reset();

    /** Einmal pro Block: leitet alle Layer-Werte aus den Modul-Parametern ab. */
    void setParams (const Params& newParams);

    /** Bei jedem Trigger: Layer-Huellen, Phasen und Delay neu starten. */
    void trigger (float velocity);

    /** Pro Sample: liefert die beiden Layer-Beitraege (0, wenn aus).
        instantaneousFundamentalHz ist die bereits modulierte Momentanfrequenz
        (Pitch-Envelope * Glide), damit alle Layer identisch moduliert sind. */
    void process (float instantaneousFundamentalHz, float& lowOut, float& highOut);

    /** Gain, der auf den Basis-Bus (Membrane+Body) angewendet werden muss,
        damit das Layering energieneutral bleibt (1.0, wenn aus). */
    float getBaseTrim() const noexcept { return baseTrim; }

    bool isEnabled() const noexcept { return params.enabled; }
    const Info& getInfo() const noexcept { return info; }

private:
    void updateDerived();

    static float clamp01 (float v) noexcept { return std::min (1.0f, std::max (0.0f, v)); }

    double sampleRate { 48000.0 };

    Params params;
    Info info;

    // Equal-power-Mix
    float baseTrim   { 1.0f };
    float lowWeight  { 0.0f };
    float highWeight { 0.0f };

    // ---- Layer 1: DEPTH ----
    float lowPhase       { 0.0f };
    float lowEnv         { 0.0f };
    float lowPeak        { 0.0f };
    bool  lowInAttack    { false };
    float lowAttackCoeff { 0.0f };
    float lowDecayCoeff  { 0.0f };

    // ---- Layer 2: PRESENCE ----
    float highPhase       { 0.0f };
    float highEnv         { 0.0f };
    float highPeak        { 0.0f };
    bool  highInAttack    { false };
    float highAttackCoeff { 0.0f };
    float highDecayCoeff  { 0.0f };
    int   highDelaySamples { 0 };
    int   highDelayCounter { 0 };

    // Ein-Pol-Bandpass (HP + LP) fuer das Rausch-Gemisch von Layer 2.
    float hpState { 0.0f };
    float lpState { 0.0f };

    // Deterministisches Rauschen (pro Trigger neu gesetzt).
    unsigned int noiseState { 0x1234567u };

    // Abgeleitete Klangform
    float lowRatio { 1.0f };   ///< 0.5 = Oktave unter f0, 1.0 = Unison
    float harmonic { 0.12f };  ///< kleiner 2. Oberton fuer TV-Hoerbarkeit
    float toneMix  { 0.40f };  ///< 0 = nur Rauschen, 1 = nur Ton (Dichte)
};
