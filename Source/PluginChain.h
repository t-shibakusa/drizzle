#pragma once

#include "PluginScanPaths.h"
#include "TrackMixerProcessor.h"
#include "TrackPluginManager.h"
#include <juce_audio_processors/juce_audio_processors.h>

class DrizzleAudioProcessor;

class PluginChain : public juce::ChangeBroadcaster
{
public:
    PluginChain();
    ~PluginChain() override;

    juce::AudioProcessor& getAudioProcessor() noexcept;

    PluginScanPaths& getScanPaths() noexcept { return scanPaths; }
    const PluginScanPaths& getScanPaths() const noexcept { return scanPaths; }

    void scanPlugins();
    const juce::KnownPluginList& getKnownPlugins() const noexcept { return knownPluginList; }
    juce::Array<juce::PluginDescription> getPluginsForUi() const;
    juce::Array<juce::PluginDescription> getPluginsForUiByFormat (const juce::String& formatName) const;

    TrackPluginManager& getTrackPluginManager() noexcept { return trackPluginManager; }
    const TrackPluginManager& getTrackPluginManager() const noexcept { return trackPluginManager; }

    bool addTrackPlugin (int trackIndex, const juce::PluginDescription& description);
    bool removeTrackPlugin (int trackIndex, int slotIndex);
    int getNumTrackPlugins (int trackIndex) const;
    juce::String getTrackPluginName (int trackIndex, int slotIndex) const;
    void showTrackPluginEditor (int trackIndex, int slotIndex, juce::Component* modalParent = nullptr);
    void onTrackRemoved (int removedIndex, int numTracksAfterRemove);

    void loadPlugin (const juce::PluginDescription& description);
    void loadPluginFromFile (const juce::File& pluginFile);
    void clearPlugin();
    /** Unloads the master insert plugin only (keeps scan cache and track plugins). */
    void unloadMasterPlugin();
    /** Full teardown on app shutdown. */
    void releaseAll();
    bool hasScannedPlugins() const noexcept;
    static bool isVst2HostingEnabled() noexcept;

    bool hasPluginLoaded() const noexcept;
    juce::String getLoadedPluginName() const;

    void setGain (float newGain);
    void setMasterGainDb (float gainDb);
    void setMasterMute (bool mute);
    void setMasterMono (bool mono);
    float getMasterPeakLevel() const noexcept;

    void showPluginEditor();
    void hidePluginEditor();

    TrackMixerProcessor& getTrackMixer() noexcept;
    const TrackMixerProcessor& getTrackMixer() const noexcept;

    void handleAudioDeviceChanged (int activeInputChannels);

private:
    void setPluginInstance (std::unique_ptr<juce::AudioPluginInstance> instance);
    void scanFormat (juce::AudioPluginFormat& format, const juce::FileSearchPath& searchPaths);

    std::unique_ptr<DrizzleAudioProcessor> audioProcessor;
    std::unique_ptr<juce::AudioPluginFormatManager> formatManager;
    PluginScanPaths scanPaths;
    juce::KnownPluginList knownPluginList;
    juce::File deadMansPedalFile;

    std::unique_ptr<juce::DocumentWindow> pluginEditorWindow;
    TrackPluginManager trackPluginManager;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (PluginChain)
};

