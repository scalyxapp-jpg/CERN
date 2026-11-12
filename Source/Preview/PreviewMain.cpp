/** Headless-Vorschau: rendert die echte C.E.R.N-GUI in eine PNG-Datei.

    Hintergrund: der Standalone-Start unter Xvfb segfaultet in dieser
    Umgebung (kein Audio-Device / GTK). Dieses kleine Werkzeug baut die
    identische MainComponent ohne Fenster und zeichnet sie in ein Bild –
    damit ist die GUI visuell pruefbar, ohne dass Audio oder X11 noetig ist.

    Aufruf:
      CERNPreview /tmp/cern-1100x750.png 1100 750 [patch|patchcv|morph]

    Modi (4. Argument):
      patch   - Patchbay-Ansicht (Audio-Routing)
      patchcv - Patchbay-Ansicht, Reiter "PARAMETER CV"
      morph   - A/B-Slots belegen, MORPH-Slider auf 50 %
      mixer   - Mixer-Ansicht (13 Kanalstreifen)
*/
#include <juce_gui_basics/juce_gui_basics.h>

#include "GUI/MainComponent.h"
#include "PluginProcessor.h"
#include "Parameters/ParameterIDs.h"

#include <iostream>

namespace
{
    juce::Button* findButton (juce::Component& parent, const juce::String& text)
    {
        for (auto* child : parent.getChildren())
        {
            if (auto* button = dynamic_cast<juce::Button*> (child))
                if (button->getButtonText().equalsIgnoreCase (text))
                    return button;

            if (auto* found = findButton (*child, text))
                return found;
        }

        return nullptr;
    }

    /** Finde den MORPH-Slider (einziger Slider mit Suffix " %" und Bereich 0..100). */
    juce::Slider* findMorphSlider (juce::Component& parent)
    {
        for (auto* child : parent.getChildren())
        {
            if (auto* slider = dynamic_cast<juce::Slider*> (child))
                if (slider->getTextValueSuffix() == " %"
                     && slider->getSliderStyle() == juce::Slider::LinearHorizontal
                     && slider->getMaximum() == 100.0)
                    return slider;

            if (auto* found = findMorphSlider (*child))
                return found;
        }

        return nullptr;
    }

    void clickIfFound (juce::Component& parent, const juce::String& buttonText)
    {
        if (auto* button = findButton (parent, buttonText))
            if (button->onClick != nullptr)
                button->onClick();
    }
}

int main (int argc, char** argv)
{
    juce::ScopedJuceInitialiser_GUI juceInit;

    const juce::String outPath = argc > 1 ? juce::String (argv[1])
                                          : juce::String ("/tmp/cern-preview.png");
    const int width  = argc > 2 ? juce::String (argv[2]).getIntValue() : 1100;
    const int height = argc > 3 ? juce::String (argv[3]).getIntValue() : 750;
    const juce::String mode = argc > 4 ? juce::String (argv[4]) : juce::String();

    CERNAudioProcessor processor;
    processor.prepareToPlay (48000.0, 512);

    MainComponent component (processor);
    component.setSize (juce::jmax (700, width), juce::jmax (500, height));

    if (mode == "layering" || mode == "layering150")
    {
        // Layering einschalten, einen Kick rendern und die Displays aktualisieren:
        // so ist im Screenshot sichtbar, wie die beiden Zusatz-Layer in der
        // Waveform liegen und welche Werte automatisch abgeleitet wurden.
        if (auto* parameter = processor.getAPVTS().getParameter (ParameterIDs::layeringEnabled))
            parameter->setValueNotifyingHost (1.0f);

        if (mode == "layering150")
            if (auto* amount = processor.getAPVTS().getParameter (ParameterIDs::layeringAmount))
                amount->setValueNotifyingHost (amount->convertTo0to1 (150.0f));

        processor.triggerNote (1.0f);

        juce::AudioBuffer<float> audio (2, 512);
        juce::MidiBuffer midi;

        for (int block = 0; block < 16; ++block)
        {
            audio.clear();
            midi.clear();
            processor.processBlock (audio, midi);
        }

        component.timerCallback();
    }

    if (mode == "patch")
        clickIfFound (component, "PATCHBAY");

    if (mode == "mixer")
        clickIfFound (component, "MIXER");

    if (mode == "patchcv")
    {
        clickIfFound (component, "PATCHBAY");
        clickIfFound (component, "PARAMETER CV");
    }

    if (mode == "morph")
    {
        // Ein Klick auf "A" fuellt beide Slots (der leere Ziel-Slot uebernimmt
        // den aktuellen State) -> MORPH-Regler wird sichtbar.
        clickIfFound (component, "A");

        if (auto* morph = findMorphSlider (component))
            morph->setValue (50.0, juce::sendNotificationSync);
    }

    if (mode == "material" || mode == "scrollbottom")
    {
        // Viewport hat keine ID – suche den ersten Viewport im Baum.
        juce::Viewport* sectionViewport = nullptr;
        for (int i = 0; i < component.getNumChildComponents(); ++i)
            if (auto* vp = dynamic_cast<juce::Viewport*> (component.getChildComponent (i)))
                sectionViewport = vp;

        if (sectionViewport != nullptr && sectionViewport->getViewedComponent() != nullptr)
        {
            auto* viewed = sectionViewport->getViewedComponent();
            const int maxY = juce::jmax (0, viewed->getHeight()
                                            - sectionViewport->getMaximumVisibleHeight());

            if (mode == "scrollbottom")
            {
                sectionViewport->setViewPosition (0, maxY);
            }
            else
            {
                // "material": Material Room-Sektion im Viewport zeigen.
                bool found = false;
                for (int i = 0; i < viewed->getNumChildComponents(); ++i)
                {
                    auto* child = viewed->getChildComponent (i);
                    if (child != nullptr && child->getTitle().containsIgnoreCase ("Material"))
                    {
                        const int targetY = juce::jlimit (0, maxY, child->getY() - 20);
                        sectionViewport->setViewPosition (0, targetY);
                        found = true;
                        break;
                    }
                }
                if (! found)
                    sectionViewport->setViewPosition (0, maxY);
            }
        }
    }

    juce::Image image (juce::Image::ARGB, component.getWidth(), component.getHeight(), true);
    {
        juce::Graphics g (image);
        component.paintEntireComponent (g, true);
    }

    const juce::File file (outPath);
    file.deleteFile();
    juce::FileOutputStream stream (file);

    if (! stream.openedOk())
    {
        std::cerr << "cannot open " << outPath << std::endl;
        return 1;
    }

    juce::PNGImageFormat png;
    const bool ok = png.writeImageToStream (image, stream);
    std::cout << (ok ? "wrote " : "failed ") << outPath << " ("
              << component.getWidth() << "x" << component.getHeight() << ")" << std::endl;

    component.setLookAndFeel (nullptr);
    return ok ? 0 : 1;
}
