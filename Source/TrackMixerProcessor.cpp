#include "TrackMixerProcessor.h"
#include "ui/MixerDbScale.h"

namespace
{
juce::String makeDefaultTrackName (int index)
{
    return juce::String::fromUTF8 (u8"\u30c8\u30e9\u30c3\u30af") + juce::String (index + 1);
}

juce::uint32 defaultTrackPanelColourArgb (int index) noexcept
{
    static constexpr juce::uint32 palette[] {
        0xff5b8fd9, 0xffd97b5b, 0xff6bbd6b, 0xffc97bd9,
        0xffd9c45b, 0xff5bd9d9, 0xff8a8ad9, 0xffd95b8f,
        0xff7bd99b, 0xffb8a45b
    };

    return palette[(size_t) index % (sizeof (palette) / sizeof (palette[0]))];
}
} // namespace

TrackMixerProcessor::TrackMixerProcessor()
    : juce::AudioProcessor (BusesProperties()
                                .withInput  ("Input",  juce::AudioChannelSet::discreteChannels (maxInputChannels), true)
                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true))
{
    setNumTracks (minTracks);
    resetTrackToDefaults (0, makeDefaultTrackName (0));
}

bool TrackMixerProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const
{
    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())
        return false;

    const auto& input = layouts.getMainInputChannelSet();

    if (input.isDisabled())
        return false;

    const auto numInputChannels = input.size();
    return numInputChannels >= 1 && numInputChannels <= (size_t) maxInputChannels;
}

void TrackMixerProcessor::setNumTracks (int count)
{
    numTracks = juce::jlimit (minTracks, maxTracks, count);
}

void TrackMixerProcessor::setConnectedInputChannelCount (int count) noexcept
{
    connectedInputChannelCount = juce::jlimit (1, maxInputChannels, count);
}

bool TrackMixerProcessor::addTrack()
{
    if (numTracks >= maxTracks)
        return false;

    resetTrackToDefaults (numTracks, makeDefaultTrackName (numTracks));
    ++numTracks;
    return true;
}

bool TrackMixerProcessor::removeTrack (int index)
{
    if (numTracks <= minTracks || ! juce::isPositiveAndBelow (index, numTracks))
        return false;

    for (int i = index; i < numTracks - 1; ++i)
        copyTrackState (i, i + 1);

    --numTracks;
    return true;
}

void TrackMixerProcessor::resetTrackToDefaults (int index, const juce::String& defaultName)
{
    jassert (juce::isPositiveAndBelow (index, maxTracks));

    auto& track = tracks[(size_t) index];
    track.name = defaultName;
    track.inputChannelIndex = 0;
    track.gainDb = MixerDbScale::defaultDb;
    track.pan = 0.5f;
    track.mute = false;
    track.solo = false;
    track.panelColourArgb = defaultTrackPanelColourArgb (index);
    track.peakLevel.store (0.0f);
}

void TrackMixerProcessor::copyTrackState (int destIndex, int srcIndex) noexcept
{
    const auto& src = tracks[(size_t) srcIndex];
    auto& dst = tracks[(size_t) destIndex];

    dst.name = src.name;
    dst.inputChannelIndex = src.inputChannelIndex;
    dst.gainDb = src.gainDb;
    dst.pan = src.pan;
    dst.mute = src.mute;
    dst.solo = src.solo;
    dst.panelColourArgb = src.panelColourArgb;
    dst.peakLevel.store (src.peakLevel.load());
}

TrackMixerProcessor::TrackState& TrackMixerProcessor::getTrack (int index) noexcept
{
    jassert (juce::isPositiveAndBelow (index, numTracks));
    return tracks[(size_t) index];
}

const TrackMixerProcessor::TrackState& TrackMixerProcessor::getTrack (int index) const noexcept
{
    jassert (juce::isPositiveAndBelow (index, numTracks));
    return tracks[(size_t) index];
}

void TrackMixerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)
{
    juce::AudioBuffer<float> stereoOutput;
    stereoOutput.setSize (2, buffer.getNumSamples(), false, false, true);
    mixFromDeviceBuffer (buffer, stereoOutput, connectedInputChannelCount);

    for (int ch = 0; ch < juce::jmin (2, buffer.getNumChannels()); ++ch)
        buffer.copyFrom (ch, 0, stereoOutput, ch, 0, stereoOutput.getNumSamples());

    juce::ignoreUnused (midi);
}

void TrackMixerProcessor::setTrackAudioCallback (TrackAudioCallback callback)
{
    trackAudioCallback = std::move (callback);
}

void TrackMixerProcessor::mixFromDeviceBuffer (const juce::AudioBuffer<float>& deviceBuffer,
                                               juce::AudioBuffer<float>& stereoOutput,
                                               int numDeviceInputChannels)
{
    stereoOutput.clear();

    const int numInputCh = juce::jmin (deviceBuffer.getNumChannels(),
                                       juce::jmin (numDeviceInputChannels, connectedInputChannelCount));
    const int numSamples = juce::jmin (deviceBuffer.getNumSamples(), stereoOutput.getNumSamples());

    if (numInputCh <= 0 || numSamples <= 0)
    {
        for (int i = 0; i < numTracks; ++i)
            tracks[(size_t) i].peakLevel.store (0.0f);

        for (int i = numTracks; i < maxTracks; ++i)
            tracks[(size_t) i].peakLevel.store (0.0f);

        return;
    }

    bool anySolo = false;

    for (int i = 0; i < numTracks; ++i)
        if (tracks[(size_t) i].solo)
            anySolo = true;

    juce::AudioBuffer<float> trackBuffer;
    trackBuffer.setSize (2, numSamples, false, false, true);

    for (int i = 0; i < numTracks; ++i)
    {
        auto& track = tracks[(size_t) i];

        if (track.mute || (anySolo && ! track.solo))
        {
            track.peakLevel.store (0.0f);
            continue;
        }

        const int ch = juce::jlimit (0, juce::jmax (0, numInputCh - 1), track.inputChannelIndex);
        const int ch2 = (ch + 1 < numInputCh) ? ch + 1 : ch;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            trackBuffer.setSample (0, sample, deviceBuffer.getSample (ch, sample));
            trackBuffer.setSample (1, sample, deviceBuffer.getSample (ch2, sample));
        }

        if (trackAudioCallback)
            trackAudioCallback (i, trackBuffer, numSamples);

        float peak = 0.0f;

        const float pan = juce::jlimit (0.0f, 1.0f, track.pan);
        const float panL = 1.0f - pan;
        const float panR = pan;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float linearGain = MixerDbScale::dbToLinear (track.gainDb);
            const float left  = trackBuffer.getSample (0, sample) * linearGain * panL;
            const float right = trackBuffer.getSample (1, sample) * linearGain * panR;
            peak = juce::jmax (peak, std::abs (left), std::abs (right));

            stereoOutput.addSample (0, sample, left);
            stereoOutput.addSample (1, sample, right);
        }

        track.peakLevel.store (juce::jlimit (0.0f, 1.0f, peak * 2.5f));
    }

    for (int i = numTracks; i < maxTracks; ++i)
        tracks[(size_t) i].peakLevel.store (0.0f);
}
