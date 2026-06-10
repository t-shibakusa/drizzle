#pragma once

#include "TrackMixerProcessor.h"
#include "TrackPluginManager.h"
#include "ui/MixerDbScale.h"
#include <juce_audio_processors/juce_audio_processors.h>

class StreamEngine;

class DrizzleAudioProcessor final : public juce::AudioProcessor
{

public:

    DrizzleAudioProcessor();



    const juce::String getName() const override { return "Drizzle"; }

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



    void prepareToPlay (double sampleRate, int blockSize) override;

    void releaseResources() override;



    void processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi) override;

    using juce::AudioProcessor::processBlock;



    void setConnectedInputChannelCount (int count);



    TrackMixerProcessor& getTrackMixer() noexcept { return trackMixer; }

    const TrackMixerProcessor& getTrackMixer() const noexcept { return trackMixer; }



    void setPluginGain (float gain) noexcept { pluginGain.store (gain); }

    void setMasterGainDb (float gainDb) noexcept { masterGainDb.store (gainDb); }

    void setMasterMute (bool mute) noexcept { masterMute.store (mute); }

    void setMasterMono (bool mono) noexcept { masterMono.store (mono); }

    float getMasterPeakLevel() const noexcept { return peakLevel.load(); }



    void setPluginInstance (std::unique_ptr<juce::AudioPluginInstance> instance);

    juce::AudioPluginInstance* getPluginInstance() const noexcept { return pluginInstance.get(); }

    bool hasPluginLoaded() const noexcept { return pluginInstance != nullptr; }

    void setStreamEngine (StreamEngine* engine) noexcept { streamEngine = engine; }

    void setTrackPluginManager (TrackPluginManager* manager) noexcept { trackPluginManager = manager; }

private:
    void applyMasterOutput (juce::AudioBuffer<float>& buffer);



    TrackMixerProcessor trackMixer;

    std::unique_ptr<juce::AudioPluginInstance> pluginInstance;

    juce::AudioBuffer<float> workBuffer;



    int connectedInputChannels = 2;



    std::atomic<float> pluginGain { 1.0f };

    std::atomic<float> masterGainDb { MixerDbScale::defaultDb };

    std::atomic<bool> masterMute { false };

    std::atomic<bool> masterMono { false };

    std::atomic<float> peakLevel { 0.0f };

    StreamEngine* streamEngine = nullptr;

    TrackPluginManager* trackPluginManager = nullptr;
};

