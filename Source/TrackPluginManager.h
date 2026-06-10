#pragma once

#include "TrackMixerProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <unordered_map>
#include <vector>

class TrackPluginManager final : public juce::ChangeBroadcaster
{
public:
    static constexpr int maxPluginsPerTrack = 16;

    struct PluginSlot
    {
        juce::PluginDescription description;
        std::unique_ptr<juce::AudioPluginInstance> instance;
    };

    TrackPluginManager();

    void setFormatManager (juce::AudioPluginFormatManager* manager) noexcept;
    void prepareToPlay (double sampleRate, int blockSize);
    void releaseResources();

    int getNumPlugins (int trackIndex) const;
    juce::String getPluginName (int trackIndex, int slotIndex) const;
    juce::Array<juce::PluginDescription> getPluginDescriptions (int trackIndex) const;

    bool addPlugin (int trackIndex, const juce::PluginDescription& description, juce::String& errorOut);
    bool removePlugin (int trackIndex, int slotIndex);
    void clearTrack (int trackIndex);
    void shiftPluginsOnTrackRemove (int removedIndex, int numTracksAfterRemove);
    void clearAll();

    void processTrack (int trackIndex, juce::AudioBuffer<float>& stereoBuffer, juce::MidiBuffer& midi);

    void showPluginEditor (int trackIndex, int slotIndex, juce::Component* modalParent = nullptr);
    void hidePluginEditor (int trackIndex, int slotIndex);
    void hideAllEditors();

private:
    std::unique_ptr<juce::AudioPluginInstance> createInstance (const juce::PluginDescription& description,
                                                                 juce::String& errorOut) const;

    juce::AudioPluginFormatManager* formatManager = nullptr;
    double currentSampleRate = 44100.0;
    int currentBlockSize = 512;

    mutable juce::CriticalSection lock;
    std::array<std::vector<PluginSlot>, TrackMixerProcessor::maxTracks> trackPlugins;

    struct EditorKey
    {
        int trackIndex = -1;
        int slotIndex = -1;

        bool operator== (const EditorKey& other) const noexcept
        {
            return trackIndex == other.trackIndex && slotIndex == other.slotIndex;
        }
    };

    struct EditorKeyHash
    {
        size_t operator() (const EditorKey& key) const noexcept
        {
            return (size_t) key.trackIndex * 100 + (size_t) key.slotIndex;
        }
    };

    std::unordered_map<EditorKey, std::unique_ptr<juce::DocumentWindow>, EditorKeyHash> editorWindows;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (TrackPluginManager)
};
