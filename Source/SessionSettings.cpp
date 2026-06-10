#include "SessionSettings.h"
#include "ui/MixerDbScale.h"

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
            const auto masterGain = (float) master->getDoubleAttribute ("gain", settings.master.gainDb);
            settings.master.gainDb = MixerDbScale::migrateLegacyLinearGain (masterGain);
            settings.master.mute = master->getBoolAttribute ("mute", settings.master.mute);
            settings.master.mono = master->getBoolAttribute ("mono", settings.master.mono);
        }

        settings.pluginGain = (float) xml->getDoubleAttribute ("pluginGain", settings.pluginGain);
        settings.vst3HostIdentity = xml->getStringAttribute ("vst3HostIdentity", settings.vst3HostIdentity);

        if (auto* windowNode = xml->getChildByName ("WINDOW"))
        {
            settings.window.x = windowNode->getIntAttribute ("x", settings.window.x);
            settings.window.y = windowNode->getIntAttribute ("y", settings.window.y);
            settings.window.width = windowNode->getIntAttribute ("width", settings.window.width);
            settings.window.height = windowNode->getIntAttribute ("height", settings.window.height);
            settings.window.hasSavedBounds = windowNode->getBoolAttribute ("saved", false);
        }

        if (auto* streamNode = xml->getChildByName ("STREAM"))
        {
            settings.stream.title = streamNode->getStringAttribute ("title", settings.stream.title);
            settings.stream.streamKey = streamNode->getStringAttribute ("streamKey");
            settings.stream.serviceId = streamNode->getIntAttribute ("serviceId", settings.stream.serviceId);
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
                const auto trackGain = (float) trackNode->getDoubleAttribute ("gain", track.gainDb);
                track.gainDb = MixerDbScale::migrateLegacyLinearGain (trackGain);
                track.pan = (float) trackNode->getDoubleAttribute ("pan", track.pan);
                track.mute = trackNode->getBoolAttribute ("mute", track.mute);
                track.solo = trackNode->getBoolAttribute ("solo", track.solo);
                track.panelColourArgb = (juce::uint32) (juce::int64) trackNode->getIntAttribute ("panelColour", 0);
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
    root.setAttribute ("vst3HostIdentity", settings.vst3HostIdentity);

    auto* master = root.createNewChildElement ("MASTER");
    master->setAttribute ("gain", settings.master.gainDb);
    master->setAttribute ("mute", settings.master.mute);
    master->setAttribute ("mono", settings.master.mono);

    auto* windowNode = root.createNewChildElement ("WINDOW");
    windowNode->setAttribute ("x", settings.window.x);
    windowNode->setAttribute ("y", settings.window.y);
    windowNode->setAttribute ("width", settings.window.width);
    windowNode->setAttribute ("height", settings.window.height);
    windowNode->setAttribute ("saved", settings.window.hasSavedBounds);

    auto* streamNode = root.createNewChildElement ("STREAM");
    streamNode->setAttribute ("title", settings.stream.title);
    streamNode->setAttribute ("streamKey", settings.stream.streamKey);
    streamNode->setAttribute ("serviceId", settings.stream.serviceId);

    auto* tracksNode = root.createNewChildElement ("TRACKS");
    tracksNode->setAttribute ("count", settings.trackCount);

    for (int i = 0; i < settings.trackCount; ++i)
    {
        const auto& track = settings.tracks[(size_t) i];
        auto* trackNode = tracksNode->createNewChildElement ("TRACK");
        trackNode->setAttribute ("index", i);
        trackNode->setAttribute ("name", track.name);
        trackNode->setAttribute ("inputChannel", track.inputChannelIndex);
        trackNode->setAttribute ("gain", track.gainDb);
        trackNode->setAttribute ("pan", track.pan);
        trackNode->setAttribute ("mute", track.mute);
        trackNode->setAttribute ("solo", track.solo);
        trackNode->setAttribute ("panelColour", (int) track.panelColourArgb);
    }

    const auto file = getSettingsFile();
    file.getParentDirectory().createDirectory();
    root.writeTo (file, {});
}

SessionSettings SessionSettingsStore::captureFrom (const TrackMixerProcessor& trackMixer,
                                                   float masterGain,
                                                   bool masterMute,
                                                   bool masterMono,
                                                   float pluginGain,
                                                   const StreamConfig& streamConfig)
{
    SessionSettings settings;
    settings.master.gainDb = masterGain;
    settings.master.mute = masterMute;
    settings.master.mono = masterMono;
    settings.pluginGain = pluginGain;
    settings.stream.title = streamConfig.title;
    settings.stream.streamKey = streamConfig.streamKey;
    settings.stream.serviceId = streamConfig.serviceId;
    settings.trackCount = trackMixer.getNumTracks();

    for (int i = 0; i < settings.trackCount; ++i)
    {
        const auto& src = trackMixer.getTrack (i);
        auto& dst = settings.tracks[(size_t) i];
        dst.name = src.name;
        dst.inputChannelIndex = src.inputChannelIndex;
        dst.gainDb = src.gainDb;
        dst.pan = src.pan;
        dst.mute = src.mute;
        dst.solo = src.solo;
        dst.panelColourArgb = src.panelColourArgb;
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
        dst.gainDb = juce::jlimit (MixerDbScale::minDb, MixerDbScale::maxDb,
                                  MixerDbScale::migrateLegacyLinearGain (src.gainDb));
        dst.pan = src.pan;
        dst.mute = src.mute;
        dst.solo = src.solo;

        if (src.panelColourArgb != 0)
            dst.panelColourArgb = src.panelColourArgb;
        else
        {
            static constexpr juce::uint32 palette[] {
                0xff5b8fd9, 0xffd97b5b, 0xff6bbd6b, 0xffc97bd9,
                0xffd9c45b, 0xff5bd9d9, 0xff8a8ad9, 0xffd95b8f,
                0xff7bd99b, 0xffb8a45b
            };

            dst.panelColourArgb = palette[(size_t) i % (sizeof (palette) / sizeof (palette[0]))];
        }
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
