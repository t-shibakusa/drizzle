#include "AudioEngine.h"


juce::File AudioEngine::getSettingsFile()
{
    return juce::File::getSpecialLocation (juce::File::userApplicationDataDirectory)
               .getChildFile ("Drizzle")
               .getChildFile ("audio_settings.xml");
}

std::unique_ptr<juce::XmlElement> AudioEngine::loadSettingsXml() const
{
    const auto file = getSettingsFile();

    if (file.existsAsFile())
        return juce::XmlDocument::parse (file);

    return {};
}

AudioEngine::AudioEngine()
{
    pluginChain.addChangeListener (this);
    loadSessionSettings();
}

AudioEngine::~AudioEngine()
{
    shutdown();
    pluginChain.removeChangeListener (this);
}

TrackMixerProcessor& AudioEngine::getTrackMixer() noexcept
{
    return pluginChain.getTrackMixer();
}

const TrackMixerProcessor& AudioEngine::getTrackMixer() const noexcept
{
    return pluginChain.getTrackMixer();
}

int AudioEngine::getNumTracks() const noexcept
{
    return getTrackMixer().getNumTracks();
}

bool AudioEngine::addTrack()
{
    if (! getTrackMixer().addTrack())
        return false;

    const int index = getNumTracks() - 1;
    getTrackMixer().getTrack (index).name = getDefaultTrackName (index);
    saveSessionSettings();
    return true;
}

bool AudioEngine::removeTrack (int trackIndex)
{
    if (getNumTracks() <= TrackMixerProcessor::minTracks)
        return false;

    if (! getTrackMixer().removeTrack (trackIndex))
        return false;

    saveSessionSettings();
    return true;
}

juce::String AudioEngine::getDefaultTrackName (int trackIndex)
{
    return juce::String::fromUTF8 (u8"\u30c8\u30e9\u30c3\u30af") + juce::String (trackIndex + 1);
}

void AudioEngine::loadSessionSettings()
{
    const auto settings = SessionSettingsStore::load();
    SessionSettingsStore::applyTo (settings, getTrackMixer());

    masterGain = settings.master.gain;
    masterMute = settings.master.mute;
    masterMono = settings.master.mono;
    pluginGain = settings.pluginGain;
    pluginChain.setGain (pluginGain);
}

void AudioEngine::saveSessionSettings()
{
    auto settings = SessionSettingsStore::load();

    const auto captured = SessionSettingsStore::captureFrom (getTrackMixer(),
                                                             masterGain,
                                                             masterMute,
                                                             masterMono,
                                                             pluginGain);
    settings.trackCount = captured.trackCount;
    settings.tracks = captured.tracks;
    settings.master = captured.master;
    settings.pluginGain = captured.pluginGain;

    SessionSettingsStore::save (settings);
}

void AudioEngine::initialise()
{
    pluginChain.setGain (pluginGain);

    const auto error = deviceManager.initialise (TrackMixerProcessor::maxInputChannels,
                                                 2,
                                                 loadSettingsXml().get(),
                                                 true);

    if (error.isNotEmpty())
    {
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                    "Audio Error",
                                                    error);
    }

    deviceManager.addChangeListener (this);
    deviceManager.addAudioCallback (&graphPlayer);
    graphPlayer.setProcessor (&pluginChain.getGraph());
}

void AudioEngine::shutdown()
{
    if (hasShutdown)
        return;

    hasShutdown = true;

    saveSessionSettings();

    graphPlayer.setProcessor (nullptr);
    deviceManager.removeAudioCallback (&graphPlayer);
    pluginChain.releaseAll();
    deviceManager.removeChangeListener (this);
    saveSettings();
    deviceManager.closeAudioDevice();
}

void AudioEngine::saveSettings()
{
    if (deviceManager.createStateXml() == nullptr)
    {
        auto setup = deviceManager.getAudioDeviceSetup();
        deviceManager.setAudioDeviceSetup (setup, true);
    }

    if (auto state = deviceManager.createStateXml())
    {
        const auto file = getSettingsFile();
        file.getParentDirectory().createDirectory();
        state->writeTo (file, {});
    }
}

void AudioEngine::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &deviceManager)
        saveSettings();
}

juce::String AudioEngine::getTrackName (int trackIndex) const
{
    return getTrackMixer().getTrack (trackIndex).name;
}

void AudioEngine::setTrackName (int trackIndex, const juce::String& name)
{
    auto trimmed = name.trim();

    if (trimmed.isEmpty())
        trimmed = getDefaultTrackName (trackIndex);

    getTrackMixer().getTrack (trackIndex).name = trimmed;
    saveSessionSettings();
}

juce::String AudioEngine::makeInputLabel (int channelIndex)
{
    const int inputNumber = channelIndex + 1;
    return juce::String::fromUTF8 (u8"\u5165\u529b ") + juce::String (inputNumber)
         + " (IN " + juce::String (inputNumber) + ")";
}

juce::String AudioEngine::getTrackInputLabel (int trackIndex) const
{
    const auto ch = getTrackMixer().getTrack (trackIndex).inputChannelIndex;

    for (const auto& option : getAvailableInputOptions())
        if (option.channelIndex == ch)
            return option.label;

    return makeInputLabel (ch);
}

int AudioEngine::getTrackInputChannel (int trackIndex) const
{
    return getTrackMixer().getTrack (trackIndex).inputChannelIndex;
}

void AudioEngine::setTrackInputChannel (int trackIndex, int channelIndex)
{
    getTrackMixer().getTrack (trackIndex).inputChannelIndex
        = juce::jlimit (0, TrackMixerProcessor::maxInputChannels - 1, channelIndex);
    saveSessionSettings();
}

float AudioEngine::getTrackGain (int trackIndex) const
{
    return getTrackMixer().getTrack (trackIndex).gain;
}

void AudioEngine::setTrackGain (int trackIndex, float gain)
{
    getTrackMixer().getTrack (trackIndex).gain = gain;
    saveSessionSettings();
}

float AudioEngine::getTrackPan (int trackIndex) const
{
    return getTrackMixer().getTrack (trackIndex).pan;
}

void AudioEngine::setTrackPan (int trackIndex, float pan)
{
    getTrackMixer().getTrack (trackIndex).pan = juce::jlimit (0.0f, 1.0f, pan);
    saveSessionSettings();
}

bool AudioEngine::getTrackMute (int trackIndex) const
{
    return getTrackMixer().getTrack (trackIndex).mute;
}

void AudioEngine::setTrackMute (int trackIndex, bool mute)
{
    getTrackMixer().getTrack (trackIndex).mute = mute;
    saveSessionSettings();
}

bool AudioEngine::getTrackSolo (int trackIndex) const
{
    return getTrackMixer().getTrack (trackIndex).solo;
}

void AudioEngine::setTrackSolo (int trackIndex, bool solo)
{
    getTrackMixer().getTrack (trackIndex).solo = solo;
    saveSessionSettings();
}

float AudioEngine::getTrackPeakLevel (int trackIndex) const
{
    return getTrackMixer().getTrack (trackIndex).peakLevel.load();
}

void AudioEngine::setMasterGain (float gain)
{
    masterGain = juce::jlimit (0.0f, 1.0f, gain);
    saveSessionSettings();
}

void AudioEngine::setMasterMute (bool mute)
{
    masterMute = mute;
    saveSessionSettings();
}

void AudioEngine::setMasterMono (bool mono)
{
    masterMono = mono;
    saveSessionSettings();
}

void AudioEngine::setPluginGain (float gain)
{
    pluginGain = gain;
    pluginChain.setGain (pluginGain);
    saveSessionSettings();
}

juce::Array<AudioInputOption> AudioEngine::getAvailableInputOptions() const
{
    juce::Array<AudioInputOption> options;

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        const auto active = device->getActiveInputChannels();

        for (int ch = 0; ch < device->getInputChannelNames().size(); ++ch)
        {
            if (! active[ch])
                continue;

            AudioInputOption option;
            option.channelIndex = ch;
            option.label = makeInputLabel (ch);

            if (const auto name = device->getInputChannelNames()[ch]; name.isNotEmpty())
                option.label += " - " + name;

            options.add (option);
        }
    }

    if (options.isEmpty())
    {
        for (int ch = 0; ch < 8; ++ch)
        {
            AudioInputOption option;
            option.channelIndex = ch;
            option.label = makeInputLabel (ch);
            options.add (option);
        }
    }

    return options;
}
