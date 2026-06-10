#pragma once

#include <juce_audio_processors/juce_audio_processors.h>
#include <array>
#include <atomic>
#include <functional>

class TrackMixerProcessor final : public juce::AudioProcessor
{
public:
    static constexpr int minTracks = 1;
    static constexpr int maxTracks = 10;
    static constexpr int maxInputChannels = 32;

    struct TrackState
    {
        juce::String name;
        int inputChannelIndex = 0;
        float gainDb = 0.0f;
        float pan = 0.5f;
        bool mute = false;
        bool solo = false;
        juce::uint32 panelColourArgb = 0;
        std::atomic<float> peakLevel { 0.0f };
    };

    TrackMixerProcessor();

    const juce::String getName() const override { return "Track Mixer"; }
    bool acceptsMidi() const override { return false; }
    bool producesMidi() const override { return false; }
    double getTailLengthSeconds() const override { return 0.0; }
    int getNumPrograms() override { return 1; }
    int getCurrentProgram() override { return 0; }
    void setCurrentProgram (int) override {}
    const juce::String getProgramName (int) override { return {}; }
    void changeProgramName (int, const juce::String&) override {}
    bool hasEditor() const override { return false; }
    juce::AudioProcessorEditor* createEditor() override { return nullptr; }
    void getStateInformation (juce::MemoryBlock&) override {}
    void setStateInformation (const void*, int) override {}

    bool isBusesLayoutSupported (const BusesLayout& layouts) const override;

    void prepareToPlay (double, int) override {}
    void releaseResources() override {}

    void processBlock (juce::AudioBuffer<float>&, juce::MidiBuffer&) override;
    using juce::AudioProcessor::processBlock;

    int getNumTracks() const noexcept { return numTracks; }
    void setNumTracks (int count);

    bool addTrack();
    bool removeTrack (int index);

    TrackState& getTrack (int index) noexcept;
    const TrackState& getTrack (int index) const noexcept;

    void resetTrackToDefaults (int index, const juce::String& defaultName);

    void setConnectedInputChannelCount (int count) noexcept;

    using TrackAudioCallback = std::function<void (int trackIndex, juce::AudioBuffer<float>& trackBuffer, int numSamples)>;

    void setTrackAudioCallback (TrackAudioCallback callback);
    void mixFromDeviceBuffer (const juce::AudioBuffer<float>& deviceBuffer,
                              juce::AudioBuffer<float>& stereoOutput,
                              int numDeviceInputChannels);

private:
    TrackAudioCallback trackAudioCallback;
    void copyTrackState (int destIndex, int srcIndex) noexcept;

    int numTracks = minTracks;
    int connectedInputChannelCount = 2;
    std::array<TrackState, maxTracks> tracks;
};
