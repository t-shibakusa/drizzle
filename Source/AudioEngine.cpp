#include "AudioEngine.h"
#include "DrizzleAudioProcessor.h"
#include "ui/MixerDbScale.h"


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

    pluginChain.onTrackRemoved (trackIndex, getNumTracks());
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

    masterGainDb = settings.master.gainDb;
    masterMute = settings.master.mute;
    masterMono = settings.master.mono;
    pluginGain = settings.pluginGain;
    pluginChain.setGain (pluginGain);
    applyMasterState();

    StreamConfig streamConfig;
    streamConfig.title = settings.stream.title;
    streamConfig.streamKey = settings.stream.streamKey;
    streamConfig.serviceId = settings.stream.serviceId;
    streamEngine.setConfig (streamConfig);
}

void AudioEngine::saveSessionSettings()
{
    auto settings = SessionSettingsStore::load();

    const auto captured = SessionSettingsStore::captureFrom (getTrackMixer(),
                                                             masterGainDb,
                                                             masterMute,
                                                             masterMono,
                                                             pluginGain,
                                                             streamEngine.getConfig());
    settings.trackCount = captured.trackCount;
    settings.tracks = captured.tracks;
    settings.master = captured.master;
    settings.pluginGain = captured.pluginGain;
    settings.stream = captured.stream;

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

    ensureDeviceChannelsEnabled();

    static_cast<DrizzleAudioProcessor&> (pluginChain.getAudioProcessor()).setStreamEngine (&streamEngine);

    streamEngine.addChangeListener (this);
    lastStreamState = streamEngine.getState();
    deviceManager.addChangeListener (this);
    graphPlayer.setProcessor (&pluginChain.getAudioProcessor());
    deviceManager.addAudioCallback (&graphPlayer);
    notifyAudioDeviceChanged();
    saveSettings();
}

void AudioEngine::shutdown()
{
    if (hasShutdown)
        return;

    hasShutdown = true;

    streamEngine.stopStream();
    youtubeChatClient.stopPolling();
    saveSessionSettings();

    graphPlayer.setProcessor (nullptr);
    deviceManager.removeAudioCallback (&graphPlayer);
    pluginChain.releaseAll();
    deviceManager.removeChangeListener (this);
    saveSettings();
    deviceManager.closeAudioDevice();
}

void AudioEngine::stopStreaming()
{
    const auto state = streamEngine.getState();

    if (state != StreamState::live && state != StreamState::starting)
        return;

    youtubeChatClient.cancelPendingBroadcastOperations();
    streamEngine.stopStream();
    youtubeChatClient.resetStreamSessionState();
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
    if (source == &streamEngine)
    {
        const auto newState = streamEngine.getState();
        const bool wasStreaming = lastStreamState == StreamState::live
                                  || lastStreamState == StreamState::starting;
        const bool isStreaming = newState == StreamState::live
                                 || newState == StreamState::starting;

        if (! wasStreaming && (newState == StreamState::starting || newState == StreamState::live))
        {
            const auto streamKey = streamEngine.getConfig().streamKey;

            if (streamKey.isNotEmpty())
                youtubeChatClient.setActiveStreamKey (streamKey);
        }

        lastStreamState = newState;
        return;
    }

    if (source == &deviceManager)
    {
        saveSettings();
        notifyAudioDeviceChanged();
    }
}

int AudioEngine::getActiveInputChannelCount() const noexcept
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getActiveInputChannels().countNumberOfSetBits();

    return 0;
}

int AudioEngine::getActiveOutputChannelCount() const noexcept
{
    if (auto* device = deviceManager.getCurrentAudioDevice())
        return device->getActiveOutputChannels().countNumberOfSetBits();

    return 0;
}

void AudioEngine::ensureDeviceChannelsEnabled()
{
    auto setup = deviceManager.getAudioDeviceSetup();

    if (auto* device = deviceManager.getCurrentAudioDevice())
    {
        const int numInputs  = device->getInputChannelNames().size();
        const int numOutputs = device->getOutputChannelNames().size();
        const int inputsToEnable  = juce::jmin (numInputs,  2);
        const int outputsToEnable = juce::jmin (numOutputs, 2);

        if (setup.inputChannels.countNumberOfSetBits() < inputsToEnable && inputsToEnable > 0)
        {
            setup.inputChannels.clear();

            for (int ch = 0; ch < inputsToEnable; ++ch)
                setup.inputChannels.setBit (ch);
        }

        if (setup.outputChannels.countNumberOfSetBits() < outputsToEnable && outputsToEnable > 0)
        {
            setup.outputChannels.clear();

            for (int ch = 0; ch < outputsToEnable; ++ch)
                setup.outputChannels.setBit (ch);
        }
    }

    deviceManager.setAudioDeviceSetup (setup, true);
}

void AudioEngine::clampTrackInputChannels()
{
    const int activeIns = getActiveInputChannelCount();

    if (activeIns <= 0)
        return;

    for (int i = 0; i < getNumTracks(); ++i)
    {
        auto& track = getTrackMixer().getTrack (i);

        if (track.inputChannelIndex >= activeIns)
            track.inputChannelIndex = 0;
    }
}

void AudioEngine::notifyAudioDeviceChanged()
{
    ensureDeviceChannelsEnabled();
    clampTrackInputChannels();

    const int activeIns = getActiveInputChannelCount();
    pluginChain.handleAudioDeviceChanged (activeIns);

    if (auto* device = deviceManager.getCurrentAudioDevice())
        streamEngine.setSampleRate (device->getCurrentSampleRate());

    if (activeIns == 0 || getActiveOutputChannelCount() == 0)
    {
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                     juce::String::fromUTF8 (u8"\u30aa\u30fc\u30c7\u30a3\u30aa\u8a2d\u5b9a"),
                                                     juce::String::fromUTF8 (u8"\u5165\u529b\u307e\u305f\u306f\u51fa\u529b\u30c1\u30e3\u30f3\u30cd\u30eb\u304c\u6709\u52b9\u5316\u3055\u308c\u3066\u3044\u307e\u305b\u3093\u3002\u8a2d\u5b9a \u2192 \u30aa\u30fc\u30c7\u30a3\u30aa\u8a2d\u5b9a\u304b\u3089\u3001AG06MK2 \u306e\u5165\u51fa\u529b\u30c1\u30e3\u30f3\u30cd\u30eb\u306b\u30c1\u30a7\u30c3\u30af\u3092\u5165\u308c\u3066\u304f\u3060\u3055\u3044\u3002"));
    }
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
    return getTrackMixer().getTrack (trackIndex).gainDb;
}

void AudioEngine::setTrackGain (int trackIndex, float gainDb)
{
    getTrackMixer().getTrack (trackIndex).gainDb
        = juce::jlimit (MixerDbScale::minDb, MixerDbScale::maxDb, MixerDbScale::applyZeroSnap (gainDb));
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

juce::Colour AudioEngine::getTrackPanelColour (int trackIndex) const
{
    const auto argb = getTrackMixer().getTrack (trackIndex).panelColourArgb;

    if (argb != 0)
        return juce::Colour (argb);

    static constexpr juce::uint32 palette[] {
        0xff5b8fd9, 0xffd97b5b, 0xff6bbd6b, 0xffc97bd9,
        0xffd9c45b, 0xff5bd9d9, 0xff8a8ad9, 0xffd95b8f,
        0xff7bd99b, 0xffb8a45b
    };

    return juce::Colour (palette[(size_t) trackIndex % (sizeof (palette) / sizeof (palette[0]))]);
}

void AudioEngine::setTrackPanelColour (int trackIndex, juce::Colour colour)
{
    getTrackMixer().getTrack (trackIndex).panelColourArgb = (juce::uint32) colour.getARGB();
    saveSessionSettings();
}

void AudioEngine::setMasterGainDb (float gainDb)
{
    masterGainDb = juce::jlimit (MixerDbScale::minDb, MixerDbScale::maxDb, MixerDbScale::applyZeroSnap (gainDb));
    pluginChain.setMasterGainDb (masterGainDb);
    saveSessionSettings();
}

void AudioEngine::setMasterMute (bool mute)
{
    masterMute = mute;
    pluginChain.setMasterMute (masterMute);
    saveSessionSettings();
}

void AudioEngine::setMasterMono (bool mono)
{
    masterMono = mono;
    pluginChain.setMasterMono (masterMono);
    saveSessionSettings();
}

float AudioEngine::getMasterPeakLevel() const noexcept
{
    return pluginChain.getMasterPeakLevel();
}

void AudioEngine::applyMasterState()
{
    pluginChain.setMasterGainDb (masterGainDb);
    pluginChain.setMasterMute (masterMute);
    pluginChain.setMasterMono (masterMono);
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
