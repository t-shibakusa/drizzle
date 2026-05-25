#pragma once

#include "PluginChain.h"
#include "SessionSettings.h"
#include <juce_audio_utils/juce_audio_utils.h>

struct AudioInputOption
{
    int channelIndex = 0;
    juce::String label;
};

class AudioEngine : private juce::ChangeListener
{
public:
    AudioEngine();
    ~AudioEngine() override;

    juce::AudioDeviceManager& getDeviceManager() noexcept { return deviceManager; }
    PluginChain& getPluginChain() noexcept { return pluginChain; }
    const PluginChain& getPluginChain() const noexcept { return pluginChain; }

    void initialise();
    void shutdown();
    void saveSettings();

    int getNumTracks() const noexcept;

    bool addTrack();
    bool removeTrack (int trackIndex);

    juce::String getTrackName (int trackIndex) const;
    void setTrackName (int trackIndex, const juce::String& name);

    juce::String getTrackInputLabel (int trackIndex) const;
    int getTrackInputChannel (int trackIndex) const;
    void setTrackInputChannel (int trackIndex, int channelIndex);

    float getTrackGain (int trackIndex) const;
    void setTrackGain (int trackIndex, float gain);

    float getTrackPan (int trackIndex) const;
    void setTrackPan (int trackIndex, float pan);

    bool getTrackMute (int trackIndex) const;
    void setTrackMute (int trackIndex, bool mute);

    bool getTrackSolo (int trackIndex) const;
    void setTrackSolo (int trackIndex, bool solo);

    float getTrackPeakLevel (int trackIndex) const;

    float getMasterGain() const noexcept { return masterGain; }
    void setMasterGain (float gain);

    bool getMasterMute() const noexcept { return masterMute; }
    void setMasterMute (bool mute);

    bool getMasterMono() const noexcept { return masterMono; }
    void setMasterMono (bool mono);

    float getPluginGain() const noexcept { return pluginGain; }
    void setPluginGain (float gain);

    juce::Array<AudioInputOption> getAvailableInputOptions() const;
    static juce::String makeInputLabel (int channelIndex);
    static juce::String getDefaultTrackName (int trackIndex);

    void saveSessionSettings();
    void loadSessionSettings();

private:
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    static juce::File getSettingsFile();
    std::unique_ptr<juce::XmlElement> loadSettingsXml() const;

    TrackMixerProcessor& getTrackMixer() noexcept;
    const TrackMixerProcessor& getTrackMixer() const noexcept;

    juce::AudioDeviceManager deviceManager;
    juce::AudioProcessorPlayer graphPlayer;
    PluginChain pluginChain;

    float masterGain = 0.85f;
    bool masterMute = false;
    bool masterMono = false;
    float pluginGain = 0.5f;
    bool hasShutdown = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (AudioEngine)
};
