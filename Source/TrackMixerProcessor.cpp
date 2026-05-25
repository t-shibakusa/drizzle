#include "TrackMixerProcessor.h"

namespace
{
juce::String makeDefaultTrackName (int index)
{
    return juce::String::fromUTF8 (u8"\u30c8\u30e9\u30c3\u30af") + juce::String (index + 1);
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

    return input == juce::AudioChannelSet::stereo()
        || input == juce::AudioChannelSet::discreteChannels (maxInputChannels);
}

void TrackMixerProcessor::setNumTracks (int count)
{
    numTracks = juce::jlimit (minTracks, maxTracks, count);
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
    track.gain = 0.75f;
    track.pan = 0.5f;
    track.mute = false;
    track.solo = false;
    track.peakLevel.store (0.0f);
}

void TrackMixerProcessor::copyTrackState (int destIndex, int srcIndex) noexcept
{
    const auto& src = tracks[(size_t) srcIndex];
    auto& dst = tracks[(size_t) destIndex];

    dst.name = src.name;
    dst.inputChannelIndex = src.inputChannelIndex;
    dst.gain = src.gain;
    dst.pan = src.pan;
    dst.mute = src.mute;
    dst.solo = src.solo;
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

void TrackMixerProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer&)
{
    auto inputBuffer  = getBusBuffer (buffer, true, 0);
    auto outputBuffer = getBusBuffer (buffer, false, 0);
    outputBuffer.clear();

    const int numInputCh = inputBuffer.getNumChannels();
    const int numSamples = outputBuffer.getNumSamples();

    bool anySolo = false;

    for (int i = 0; i < numTracks; ++i)
        if (tracks[(size_t) i].solo)
            anySolo = true;

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

        float peak = 0.0f;

        const float pan = juce::jlimit (0.0f, 1.0f, track.pan);
        const float panL = 1.0f - pan;
        const float panR = pan;

        for (int sample = 0; sample < numSamples; ++sample)
        {
            const float left  = inputBuffer.getSample (ch,  sample) * track.gain * panL;
            const float right = inputBuffer.getSample (ch2, sample) * track.gain * panR;
            peak = juce::jmax (peak, std::abs (left), std::abs (right));

            outputBuffer.addSample (0, sample, left);
            outputBuffer.addSample (1, sample, right);
        }

        track.peakLevel.store (juce::jlimit (0.0f, 1.0f, peak * 2.5f));
    }

    for (int i = numTracks; i < maxTracks; ++i)
        tracks[(size_t) i].peakLevel.store (0.0f);
}
