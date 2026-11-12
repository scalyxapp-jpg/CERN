//==============================================================================
/** Plugin entry point required by juce_audio_plugin_client.

    The plugin client module declares and calls the global
    `::createPluginFilter()`; provide it here.
*/
#include <juce_audio_processors/juce_audio_processors.h>
#include "PluginProcessor.h"

juce::AudioProcessor* createPluginFilter()
{
    return new CERNAudioProcessor();
}
