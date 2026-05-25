#include "SessionSettings.h"

juce::File SessionSettingsStore::getSettingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Drizzle")
               .getChildFile ("session_settings.xml");
}

SessionSettings SessionSettingsStore::load()
{
    SessionSettings settings;

    const auto file = getSettingsFile();

    if (! file.existsAsFile())
        return settings;

    if (auto xml = juce::XmlDocument::parse (file))
    {
        if (auto* master = xml->getChildByName ("MASTER"))
        {
            settings.master.gain = (float) master->getDoubleAttribute ("gain", settings.master.gain);
            settings.master.mute = master->getBoolAttribute ("mute", settings.master.mute);
            settings.master.mono = master->getBoolAttribute ("mono", settings.master.mono);
        }

        settings.pluginGain = (float) xml->getDoubleAttribute ("pluginGain", settings.pluginGain);

        if (auto* windowNode = xml->getChildByName ("WINDOW"))
        {
            settings.window.x = windowNode->getIntAttribute ("x", settings.window.x);
            settings.window.y = windowNode->getIntAttribute ("y", settings.window.y);
            settings.window.width = windowNode->getIntAttribute ("width", settings.window.width);
            settings.window.height = windowNode->getIntAttribute ("height", settings.window.height);
            settings.window.hasSavedBounds = windowNode->getBoolAttribute ("saved", false);
        }

        if (auto* tracksNode = xml->getChildByName ("TRACKS"))
        {
            int highestIndex = -1;

            for (auto* trackNode : tracksNode->getChildIterator())
            {
                if (! trackNode->hasTagName ("TRACK"))
                    continue;

                const int index = trackNode->getIntAttribute ("index", -1);

                if (! juce::isPositiveAndBelow (index, TrackMixerProcessor::maxTracks))
                    continue;

                highestIndex = juce::jmax (highestIndex, index);

                auto& track = settings.tracks[(size_t) index];
                track.name = trackNode->getStringAttribute ("name");
                track.inputChannelIndex = trackNode->getIntAttribute ("inputChannel", track.inputChannelIndex);
                track.gain = (float) trackNode->getDoubleAttribute ("gain", track.gain);
                track.pan = (float) trackNode->getDoubleAttribute ("pan", track.pan);
                track.mute = trackNode->getBoolAttribute ("mute", track.mute);
                track.solo = trackNode->getBoolAttribute ("solo", track.solo);
            }

            settings.trackCount = tracksNode->getIntAttribute ("count",
                                                                highestIndex >= 0 ? highestIndex + 1
                                                                                  : TrackMixerProcessor::minTracks);
            settings.trackCount = juce::jlimit (TrackMixerProcessor::minTracks,
                                                TrackMixerProcessor::maxTracks,
                                                settings.trackCount);
        }
    }

    return settings;
}

void SessionSettingsStore::save (const SessionSettings& settings)
{
    juce::XmlElement root ("DRIZZLE_SESSION");

    root.setAttribute ("pluginGain", settings.pluginGain);

    auto* master = root.createNewChildElement ("MASTER");
    master->setAttribute ("gain", settings.master.gain);
    master->setAttribute ("mute", settings.master.mute);
    master->setAttribute ("mono", settings.master.mono);

    auto* windowNode = root.createNewChildElement ("WINDOW");
    windowNode->setAttribute ("x", settings.window.x);
    windowNode->setAttribute ("y", settings.window.y);
    windowNode->setAttribute ("width", settings.window.width);
    windowNode->setAttribute ("height", settings.window.height);
    windowNode->setAttribute ("saved", settings.window.hasSavedBounds);

    auto* tracksNode = root.createNewChildElement ("TRACKS");
    tracksNode->setAttribute ("count", settings.trackCount);

    for (int i = 0; i < settings.trackCount; ++i)
    {
        const auto& track = settings.tracks[(size_t) i];
        auto* trackNode = tracksNode->createNewChildElement ("TRACK");
        trackNode->setAttribute ("index", i);
        trackNode->setAttribute ("name", track.name);
        trackNode->setAttribute ("inputChannel", track.inputChannelIndex);
        trackNode->setAttribute ("gain", track.gain);
        trackNode->setAttribute ("pan", track.pan);
        trackNode->setAttribute ("mute", track.mute);
        trackNode->setAttribute ("solo", track.solo);
    }

    const auto file = getSettingsFile();
    file.getParentDirectory().createDirectory();
    root.writeTo (file, {});
}

SessionSettings SessionSettingsStore::captureFrom (const TrackMixerProcessor& trackMixer,
                                                   float masterGain,
                                                   bool masterMute,
                                                   bool masterMono,
                                                   float pluginGain)
{
    SessionSettings settings;
    settings.master.gain = masterGain;
    settings.master.mute = masterMute;
    settings.master.mono = masterMono;
    settings.pluginGain = pluginGain;
    settings.trackCount = trackMixer.getNumTracks();

    for (int i = 0; i < settings.trackCount; ++i)
    {
        const auto& src = trackMixer.getTrack (i);
        auto& dst = settings.tracks[(size_t) i];
        dst.name = src.name;
        dst.inputChannelIndex = src.inputChannelIndex;
        dst.gain = src.gain;
        dst.pan = src.pan;
        dst.mute = src.mute;
        dst.solo = src.solo;
    }

    return settings;
}

void SessionSettingsStore::applyTo (const SessionSettings& settings,
                                      TrackMixerProcessor& trackMixer)
{
    trackMixer.setNumTracks (settings.trackCount);

    for (int i = 0; i < trackMixer.getNumTracks(); ++i)
    {
        const auto& src = settings.tracks[(size_t) i];
        auto& dst = trackMixer.getTrack (i);

        if (src.name.isNotEmpty())
            dst.name = src.name;
        else
            dst.name = juce::String::fromUTF8 (u8"\u30c8\u30e9\u30c3\u30af") + juce::String (i + 1);

        dst.inputChannelIndex = juce::jlimit (0, TrackMixerProcessor::maxInputChannels - 1, src.inputChannelIndex);
        dst.gain = src.gain;
        dst.pan = src.pan;
        dst.mute = src.mute;
        dst.solo = src.solo;
    }
}

void SessionSettingsStore::saveWindowBounds (int x, int y, int width, int height)
{
    auto settings = load();
    settings.window.x = x;
    settings.window.y = y;
    settings.window.width = juce::jmax (400, width);
    settings.window.height = juce::jmax (300, height);
    settings.window.hasSavedBounds = true;
    save (settings);
}
