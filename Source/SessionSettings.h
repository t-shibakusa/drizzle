#pragma once

#include "StreamEngine.h"
#include "TrackMixerProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>

struct SessionSettings
{
    struct MasterState
    {
        float gainDb = 0.0f;
        bool mute = false;
        bool mono = false;
    };

    struct TrackSnapshot
    {
        juce::String name;
        int inputChannelIndex = 0;
        float gainDb = 0.0f;
        float pan = 0.5f;
        bool mute = false;
        bool solo = false;
        juce::uint32 panelColourArgb = 0;
    };

    struct WindowState
    {
        int x = 100;
        int y = 100;
        int width = 1600;
        int height = 960;
        bool hasSavedBounds = false;
    };

    struct StreamState
    {
        juce::String title { juce::String::fromUTF8 (u8"\u5f39\u304d\u8a9e\u308a\u30e9\u30a4\u30d6\u914d\u4fe1") };
        juce::String streamKey;
        int serviceId = 1;
    };

    int trackCount = TrackMixerProcessor::minTracks;
    std::array<TrackSnapshot, TrackMixerProcessor::maxTracks> tracks;
    MasterState master;
    float pluginGain = 0.5f;
    juce::String vst3HostIdentity { "Reaper" };
    WindowState window;
    StreamState stream;
};

class SessionSettingsStore
{
public:
    static juce::File getSettingsFile();

    static SessionSettings load();
    static void save (const SessionSettings& settings);

    static SessionSettings captureFrom (const TrackMixerProcessor& trackMixer,
                                        float masterGain,
                                        bool masterMute,
                                        bool masterMono,
                                        float pluginGain,
                                        const StreamConfig& streamConfig);

    static void applyTo (const SessionSettings& settings,
                         TrackMixerProcessor& trackMixer);

    static void saveWindowBounds (int x, int y, int width, int height);
};
