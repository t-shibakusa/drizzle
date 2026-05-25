#pragma once

#include "PluginScanPaths.h"
#include "TrackMixerProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>

class GainAudioProcessor;

class PluginChain : public juce::ChangeBroadcaster
{
public:
    PluginChain();
    ~PluginChain() override;

    juce::AudioProcessorGraph& getGraph() noexcept { return graph; }

    PluginScanPaths& getScanPaths() noexcept { return scanPaths; }
    const PluginScanPaths& getScanPaths() const noexcept { return scanPaths; }

    void scanPlugins();
    const juce::KnownPluginList& getKnownPlugins() const noexcept { return knownPluginList; }
    juce::Array<juce::PluginDescription> getPluginsForUi() const;

    void loadPlugin (const juce::PluginDescription& description);
    void loadPluginFromFile (const juce::File& pluginFile);
    void clearPlugin();
    /** Unloads the active plugin and releases scan cache (DLL handles). */
    void releaseAll();

    bool hasPluginLoaded() const noexcept { return pluginNode != nullptr; }
    juce::String getLoadedPluginName() const;

    void setGain (float newGain);
    void showPluginEditor();
    void hidePluginEditor();

    TrackMixerProcessor& getTrackMixer() noexcept;
    const TrackMixerProcessor& getTrackMixer() const noexcept;

private:
    void ensureIoNodes();
    void rebuildConnections();
    void setPluginInstance (std::unique_ptr<juce::AudioPluginInstance> instance);
    void scanFormat (juce::AudioPluginFormat& format, const juce::FileSearchPath& searchPaths);

    std::unique_ptr<juce::AudioPluginFormatManager> formatManager;
    PluginScanPaths scanPaths;
    juce::KnownPluginList knownPluginList;
    juce::File deadMansPedalFile;

    juce::AudioProcessorGraph graph;
    juce::AudioProcessorGraph::Node::Ptr inputNode;
    juce::AudioProcessorGraph::Node::Ptr outputNode;
    juce::AudioProcessorGraph::Node::Ptr trackMixerNode;
    juce::AudioProcessorGraph::Node::Ptr gainNode;
    juce::AudioProcessorGraph::Node::Ptr pluginNode;

    std::unique_ptr<juce::DocumentWindow> pluginEditorWindow;
    TrackMixerProcessor* trackMixerProcessor = nullptr;
    GainAudioProcessor* gainProcessor = nullptr;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginChain)
};
