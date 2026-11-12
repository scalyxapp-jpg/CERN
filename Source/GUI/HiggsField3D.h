#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/** HiggsField3D – interaktive 3D-Projektion des Higgs-Feldes (§9).

    Alle 13 Parameter sind im Display selbst sichtbar (linke Fläche = Mexican-Hat-
    Wireframe, rechte Spalte = numerische Anzeige) und aktualisieren sich bei
    jeder Parameteränderung.

    Sichtbare Parameter-Zuordnungen:
      PHI, THETA  -> Cursor-Position (Azimut + radiale Höhe auf der Fläche)
      STRENGTH    -> Höhe der inneren Kuppel (z-Skalierung)
      MASSE       -> Radius des Potentialminimums (gestrichelter Ring; VEV)
      KOPPLUNG    -> Wandsteilheit (nichtlineare Ringverteilung; λ-Self)
      INERTIA     -> Cursor-Glow (Chaos-Trägheit)
      PRÄSENZ     -> Anzahl/Intensität der Feld-Partikel (Yukawa)
      RESONANZ    -> Resonanz-Ring (optional dargestellt bei Q>1; Boson Hz)
      RESO-Q      -> Linienstärke des Resonanz-Rings (Boson Q)
      SYMMETRY    -> Verkippung der Oberfläche (Mischung zweier Pole)
      DECAY       -> Rest-Intensität der Geister-Ringe ausserhalb der Schale
      MIX         -> gefülltes Wet/Dry-Volumen am Boden (Accent-Alpha)
      ENABLED     -> Dimmen des gesamten Displays (OFF)

    Der Punkt kann mit der Maus gezogen werden. Beim Ziehen werden ALLE
    (float-)Parameter aus der Cursorposition abgeleitet und live in die APVTS
    geschrieben; dadurch reagieren Feldgrafik, Higgs-Potenzial und alle
    Higgs-Knobs im Panel sofort. Ist das Higgs-Feld deaktiviert, bleibt die
    Grafik sichtbar (gedimmt), die Cursor-Interaktion ist jedoch abgeschaltet.
 */
class HiggsField3D : public juce::Component,
                     public juce::SettableTooltipClient
{
public:
    /** Abgeleitete Parameter aus einer normalisierten Cursorposition (x, y in [0, 1]). */
    struct CursorDerived
    {
        float phi;          // 0..1
        float theta;        // 0..360 deg
        float fieldStrength;// 0..1
        float vev;          // 0.2..0.95
        float selfCoupling; // 0..1
        float inertia;      // 0..1
        float yukawaBoost;  // 0..1
        float bosonHz;      // 100..140 Hz (SND-119: auf 40..400 anheben)
        float bosonQ;       // 1..30 (SND-126: auf 1..40 anheben)
        float symmetry;     // 0..1
        float decay;        // 0.5..1
        float mix;          // 0..1
    };

    /** Pure Funktion: leitet alle Parameter aus der Cursorposition ab (nur für Tests und Drag). */
    static CursorDerived deriveFromCursor (float normX, float normY);

    HiggsField3D (juce::AudioProcessorValueTreeState& apvtsToUse);
    ~HiggsField3D() override = default;

    void paint (juce::Graphics& g) override;

    void mouseDown (const juce::MouseEvent& e) override;
    void mouseDrag (const juce::MouseEvent& e) override;
    void mouseMove (const juce::MouseEvent& e) override;
    void mouseExit (const juce::MouseEvent& e) override;

    /** Alle Parameter setzen (aus der APVTS, z. B. vom GUI-Timer). */
    void setParams (float phi, float theta, float fieldStrength, float vev,
                    float selfCoupling, float inertia, float yukawaBoost,
                    float bosonHz, float bosonQ, float symmetry, float decay,
                    float mix, bool enabled);

private:
    juce::Rectangle<float> getBowlArea() const;
    juce::Point<float> projectPoint (float radiusFraction, float azimuthRad) const;
    float potentialHeight (float radiusFraction) const;

    void drawBowl (juce::Graphics& g);
    void drawCursor (juce::Graphics& g);
    void drawParameterColumn (juce::Graphics& g);

    void updateCursorFromMouse (const juce::MouseEvent& e);
    void writeDerivedToApvts (const CursorDerived& derived);

    juce::AudioProcessorValueTreeState& apvts;

    // Aktuelle (angezeigte) Parameter
    float phi           = 0.5f;
    float theta         = 180.0f;
    float fieldStrength = 0.5f;
    float vev           = 0.7f;
    float selfCoupling  = 0.5f;
    float inertia       = 0.5f;
    float yukawaBoost   = 0.3f;
    float bosonHz       = 98.0f;
    float bosonQ        = 8.0f;
    float symmetry      = 0.5f;
    float decay         = 0.5f;
    float mix           = 0.10f;
    bool  enabled       = false;

    bool dragging      = false;
    bool hovering      = false;

    // Cache der zuletzt geschriebenen Parameter, damit Drag-Events nur
    // tatsächlich geänderte Werte an die APVTS melden (kein Automation-Spam).
    CursorDerived lastWritten {};
    bool hasWrittenDerived = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (HiggsField3D)
};
