#pragma once

#include "TrackMixerProcessor.h"
#include <juce_audio_processors/juce_audio_processors.h>

struct SessionSettings
{
    struct MasterState
    {
        float gain = 0.85f;
        bool mute = false;
        bool mono = false;
    };

    struct TrackSnapshot
    {
        juce::String name;
        int inputChannelIndex = 0;
        float gain = 0.75f;
        float pan = 0.5f;
        bool mute = false;
        bool solo = false;
    };

    struct WindowState
    {
        int x = 100;
        int y = 100;
        int width = 1600;
        int height = 960;
        bool hasSavedBounds = false;
    };

    int trackCount = TrackMixerProcessor::minTracks;
    std::array<TrackSnapshot, TrackMixerProcessor::maxTracks> tracks;
    MasterState master;
    float pluginGain = 0.5f;
    WindowState window;
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
                                        float pluginGain);

    static void applyTo (const SessionSettings& settings,
                         TrackMixerProcessor& trackMixer);

    static void saveWindowBounds (int x, int y, int width, int height);
};
