#pragma once

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include <juce_audio_processors/juce_audio_processors.h>
#include "ParameterIDs.h"

//==============================================================================
/** Smoothed parameter access for the audio thread.

    Reads atomic parameter targets and applies exponential smoothing so that
    parameter changes are click-free.

    Ziele werden einmal pro Block gelesen (update()). Die Ausgabe erfolgt
    entweder blockbasiert (getById/get) oder samplegenau (getNextValueById/
    getNextValue). Für eine echte samplegenaue Rampe ruft der DSP
    getNextValue() pro Sample auf.

    Intern: O(1) Lookup über unordered_map<juce::String, size_t> (keine
    lineare Suche), idToIndex (nur für juce::String-Keys) sowie
    viewToIndex (std::string_view auf ownedIdStorage, das den Schlüssel
    besitzt und somit sicher vor dangling-References ist). Lookups über
    const char* konstruieren damit keinen std::string mehr.
*/
class ParameterSmoothing
{
public:
    explicit ParameterSmoothing (juce::AudioProcessorValueTreeState& apvtsRef)
        : apvts (apvtsRef)
    {
        const auto n = ParameterIDs::getAllIDs().size();
        currentValues.resize (n, 0.0f);
        targetValues.resize (n, 0.0f);
        sampleSteps.resize (n, 0.0f);
        modulationOffsets.resize (n, 0.0f);

        const auto& ids = ParameterIDs::getAllIDs();

        // Erst alle Schlüssel kopieren (stabile Speicherorte), dann die
        // string_view-Map aufbauen: kein Realloc invalidiert die Views.
        ownedIdStorage.reserve (ids.size());

        for (const auto& id : ids)
            ownedIdStorage.emplace_back (id.toRawUTF8());

        for (size_t i = 0; i < ids.size(); ++i)
        {
            idToIndex.emplace (ids[i], i);
            viewToIndex.emplace (std::string_view (ownedIdStorage[i]), i);
        }

        // APVTS-Parameterobjekte leben so lange wie der APVTS; die Zeiger
        // dürfen daher gecacht werden. Das vermeidet 1229 String-Lookups
        // pro Block im Audio-Thread.
        rawValuePointers.reserve (ids.size());
        for (const auto& id : ids)
            rawValuePointers.push_back (apvts.getRawParameterValue (id));
    }

    void prepare (double sampleRate)
    {
        rate = sampleRate;
        update();
        for (size_t i = 0; i < currentValues.size(); ++i)
            currentValues[i] = targetValues[i];

        std::fill (modulationOffsets.begin(), modulationOffsets.end(), 0.0f);
    }

    /** Set the per-block modulation offset (effective value minus raw target).

        The engine passes the already-computed effective, denormalized parameter
        values. Only the additive difference to the raw target is stored, so the
        15 ms smoothing in update()/getNextValue() keeps working and modulation
        never overwrites the smoothed state.
     */
    void applyModulatedValues (const float* values, int count)
    {
        if (values == nullptr)
            return;

        const int n = juce::jmin (count, (int) modulationOffsets.size());

        for (int i = 0; i < n; ++i)
            modulationOffsets[(size_t) i] = values[i] - targetValues[(size_t) i];
    }

    /** Blockgröße setzen (für korrekte pro-Schritt-Rampen-Berechnung). */
    void setBlockSize (int newSamplesPerBlock)
    {
        samplesPerBlock = juce::jmax (1, newSamplesPerBlock);
    }

    /** Read all parameter targets and advance currentValues by one block step.
        Call once per block at the start of processBlock. */
    void update()
    {
        const float oneOverTau = 1.0f / (0.015f * (float) rate); // tau ≈ 15 ms
        const float coeff = std::exp (-oneOverTau * (float) samplesPerBlock);
        const size_t count = juce::jmin (rawValuePointers.size(), currentValues.size());

        for (size_t i = 0; i < count; ++i)
        {
            if (const auto* param = rawValuePointers[i])
            {
                targetValues[i] = param->load();
                currentValues[i] += (targetValues[i] - currentValues[i]) * (1.0f - coeff);
                const float remaining = targetValues[i] - currentValues[i];
                sampleSteps[i] = remaining / (float) samplesPerBlock;
            }
        }
    }

    /** Smoothed (click-free) value by index. Blockbasiert, nicht avancierend.
        Enthält den additiven Modulations-Offset des aktuellen Blocks. */
    float get (int index) const
    {
        if (! juce::isPositiveAndBelow (index, (int) currentValues.size()))
            return 0.0f;

        const auto i = (size_t) index;
        return sanitise (currentValues[i] + modulationOffsets[i]);
    }

    /** Roher Zielwert (denormalisiert) - Basis für Parameter-CV, ohne
        Rückkopplung mit dem vorherigen modulierten Block. */
    float getTarget (int index) const
    {
        if (juce::isPositiveAndBelow (index, (int) targetValues.size()))
            return targetValues[(size_t) index];
        return 0.0f;
    }

    /** Smoothed (click-free) value by parameter ID. Blockbasiert, nicht avancierend. */
    float getById (const juce::String& id) const
    {
        auto it = idToIndex.find (id);
        if (it != idToIndex.end())
            return get ((int) it->second);
        return 0.0f;
    }

    /** Allokationsfreie Variante für const-char*-IDs (Audio-Thread). */
    float getById (const char* id) const
    {
        auto it = viewToIndex.find (std::string_view (id));
        if (it != viewToIndex.end())
            return get ((int) it->second);
        return 0.0f;
    }

    /** Ungeglätteter Zielwert für Choice-/Bool-Parameter (SND-133/147).

        Choice- und Bool-Werte dürfen nicht über die 15-ms-Glättung laufen:
        ein halb geglätteter Modus (z. B. 0.5 zwischen "Internal" und "Host")
        ist semantisch ungültig und Gate-Flanken würden verschliffen.
        Liefert den rohen, denormalisierten Wert des aktuellen Blocks
        (identisch mit getTarget(index)). getById() bleibt unverändert
        geglättet, damit bestehende Float-Pfade kompatibel bleiben.
    */
    float getUnsmoothed (int index) const
    {
        if (! juce::isPositiveAndBelow (index, (int) targetValues.size()))
            return 0.0f;

        return sanitise (targetValues[(size_t) index]);
    }

    float getUnsmoothedById (const juce::String& id) const
    {
        auto it = idToIndex.find (id);
        if (it != idToIndex.end())
            return getUnsmoothed ((int) it->second);
        return 0.0f;
    }

    /** Allokationsfreie Variante für const-char*-IDs (Audio-Thread). */
    float getUnsmoothedById (const char* id) const
    {
        auto it = viewToIndex.find (std::string_view (id));
        if (it != viewToIndex.end())
            return getUnsmoothed ((int) it->second);
        return 0.0f;
    }

    /** Index einer const-char*-ID (allokationsfrei), -1 wenn unbekannt. */
    int indexOf (const char* id) const
    {
        auto it = viewToIndex.find (std::string_view (id));
        return it != viewToIndex.end() ? (int) it->second : -1;
    }

    /** Nächster sample-genauer Wert (avanciert um einen Schritt). */
    float getNextValue (int index)
    {
        if (! juce::isPositiveAndBelow (index, (int) currentValues.size()))
            return 0.0f;

        const auto i = (size_t) index;
        const float value = sanitise (currentValues[i] + modulationOffsets[i]);
        currentValues[i] += sampleSteps[i];
        return value;
    }

    /** Nächster sample-genauer Wert nach ID (avanciert um einen Schritt). */
    float getNextValueById (const juce::String& id)
    {
        auto it = idToIndex.find (id);
        if (it != idToIndex.end())
            return getNextValue ((int) it->second);
        return 0.0f;
    }

    /** Allokationsfreie Variante für const-char*-IDs (Audio-Thread). */
    float getNextValueById (const char* id)
    {
        auto it = viewToIndex.find (std::string_view (id));
        if (it != viewToIndex.end())
            return getNextValue ((int) it->second);
        return 0.0f;
    }

private:
    static float sanitise (float value)
    {
        return std::isfinite (value) ? value : 0.0f;
    }

    juce::AudioProcessorValueTreeState& apvts;
    std::unordered_map<juce::String, size_t> idToIndex;
    std::vector<std::string> ownedIdStorage; ///< besitzt die Schlüssel-Bytes
    std::unordered_map<std::string_view, size_t> viewToIndex;
    std::vector<float> currentValues;
    std::vector<float> targetValues;
    std::vector<float> sampleSteps;
    std::vector<float> modulationOffsets;
    std::vector<std::atomic<float>*> rawValuePointers;
    int samplesPerBlock { 1 };
    double rate { 48000.0 };
};
