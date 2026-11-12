#pragma once

#include <juce_gui_basics/juce_gui_basics.h>

//==============================================================================
/** CERN LookAndFeel – Schwarz-Weiss-Rot, prozedurale 3D-Optik.
 *
 *  Farbwelt:
 *    - Background: #08080a (Tiefschwarz)
 *    - Panel:      #111216 (Anthrazit)
 *    - Raised:     #1b1d22 (erhabene Flaechen)
 *    - Text:       #f4f4f6 (Off-White)
 *    - Accent:     #e10600 (Signal-Rot), Highlight #ff3b3b
 *    - Edge:       #3a3d45 (Metallkante)
 *
 *  Alle Regler/Panels werden prozedural gezeichnet (Verlaeufe, Kanten,
 *  Schatten, Glow). Die Metall-/Kappen-Texturen kommen als BinaryData
 *  dazu, werden aber nur als Textur-Layer verwendet – die Form selbst
 *  bleibt vektoriell und damit bei jeder Groesse scharf.
 */
class CERNLookAndFeel : public juce::LookAndFeel_V4
{
public:
    CERNLookAndFeel();

    //==============================================================================
    // Farben
    static const juce::Colour backgroundColour;
    static const juce::Colour panelColour;
    static const juce::Colour panelRaisedColour;
    static const juce::Colour accentColour;
    static const juce::Colour accentBrightColour;
    static const juce::Colour textColour;
    static const juce::Colour knobColour;
    static const juce::Colour knobActiveColour;
    static const juce::Colour edgeColour;

    /** dezentes Signalgreen (CV-Aktiv-LEDs, Patch-Ziel-Ringe) –
        ersetzt das zu saure Colours::lime aus der Palette. */
    static const juce::Colour signalColour;

    //==============================================================================
    // LookAndFeel Overrides
    void drawRotarySlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float rotaryStartAngle, float rotaryEndAngle,
                           juce::Slider& slider) override;

    void drawLinearSlider (juce::Graphics& g, int x, int y, int width, int height,
                           float sliderPos, float minSliderPos, float maxSliderPos,
                           const juce::Slider::SliderStyle style, juce::Slider& slider) override;

    juce::Label* createSliderTextBox (juce::Slider& slider) override;

    void drawToggleButton (juce::Graphics& g, juce::ToggleButton& button,
                           bool shouldDrawButtonAsHighlighted, bool shouldDrawButtonAsDown) override;

    void drawComboBox (juce::Graphics& g, int width, int height, bool isButtonDown,
                       int buttonX, int buttonY, int buttonW, int buttonH,
                       juce::ComboBox& box) override;

    void positionComboBoxText (juce::ComboBox& box, juce::Label& label) override;

    void drawButtonBackground (juce::Graphics& g, juce::Button& button,
                               const juce::Colour& backgroundColour,
                               bool shouldDrawButtonAsHighlighted,
                               bool shouldDrawButtonAsDown) override;

    //==============================================================================
    // Sektion-Panel Hintergrund zeichnen
    static void drawSectionPanel (juce::Graphics& g, int x, int y, int w, int h,
                                  const juce::String& title);

    /** Erhabene 3D-Flaeche (Basis fuer Panels, Displays, Header). */
    static void drawBevel (juce::Graphics& g, juce::Rectangle<float> bounds,
                           float cornerSize, bool raised = true,
                           float textureAlpha = 0.10f);

    /** Versenkte Flaeche (Displays, Fader-Schacht). */
    static void drawWell (juce::Graphics& g, juce::Rectangle<float> bounds,
                          float cornerSize);

    /** Kleine Metallschraube als 3D-Akzent. */
    static void drawScrew (juce::Graphics& g, juce::Point<float> centre, float radius);

    /** Vertikaler Verlauf Schwarz -> Anthrazit fuer grosse Flaechen. */
    static void drawBackdrop (juce::Graphics& g, juce::Rectangle<float> bounds);

    //==============================================================================
    // Fonts
    static juce::Font getSectionTitleFont();
    static juce::Font getNameFont();
    static juce::Font getLabelFont();
    static juce::Font getValueFont();
    static juce::Font getDisplayFont();

    /** Textur-Bilder (BinaryData), lazy geladen; koennen nullptr sein. */
    static const juce::Image& metalTexture();
    static const juce::Image& knobCapTexture();
    static const juce::Image& grainTexture();
    static const juce::Image& faderTrackTexture();

    // Standard-Instanz (fuer Komponenten, die kein eigenes LF haben)
    static CERNLookAndFeel& getDefaultLookAndFeel();

private:
    static void drawRedKnob (juce::Graphics& g, int x, int y, int w, int h,
                             float angle, bool active);

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (CERNLookAndFeel)
};
