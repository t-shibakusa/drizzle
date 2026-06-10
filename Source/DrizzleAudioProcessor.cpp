#include "DrizzleAudioProcessor.h"
#include "StreamEngine.h"


DrizzleAudioProcessor::DrizzleAudioProcessor()

    : juce::AudioProcessor (BusesProperties()

                                .withInput  ("Input",  juce::AudioChannelSet::discreteChannels (TrackMixerProcessor::maxInputChannels), true)

                                .withOutput ("Output", juce::AudioChannelSet::stereo(), true))

{

    setConnectedInputChannelCount (2);

    trackMixer.setTrackAudioCallback ([this] (int trackIndex, juce::AudioBuffer<float>& trackBuffer, int numSamples)
    {
        if (trackPluginManager == nullptr)
            return;

        juce::MidiBuffer midi;
        juce::ignoreUnused (numSamples);
        trackPluginManager->processTrack (trackIndex, trackBuffer, midi);
    });

}



bool DrizzleAudioProcessor::isBusesLayoutSupported (const BusesLayout& layouts) const

{

    if (layouts.getMainOutputChannelSet() != juce::AudioChannelSet::stereo())

        return false;



    const auto& input = layouts.getMainInputChannelSet();



    if (input.isDisabled())

        return false;



    const auto numInputChannels = input.size();

    return numInputChannels >= 1 && numInputChannels <= (size_t) TrackMixerProcessor::maxInputChannels;

}



void DrizzleAudioProcessor::setConnectedInputChannelCount (int count)

{

    connectedInputChannels = juce::jlimit (1, TrackMixerProcessor::maxInputChannels, count);

    trackMixer.setConnectedInputChannelCount (connectedInputChannels);



    juce::AudioProcessor::BusesLayout layout;

    layout.inputBuses.add (juce::AudioChannelSet::discreteChannels (connectedInputChannels));

    layout.outputBuses.add (juce::AudioChannelSet::stereo());

    setBusesLayout (layout);

}



void DrizzleAudioProcessor::prepareToPlay (double sampleRate, int blockSize)

{

    if (pluginInstance != nullptr)

        pluginInstance->prepareToPlay (sampleRate, blockSize);

    if (trackPluginManager != nullptr)

        trackPluginManager->prepareToPlay (sampleRate, blockSize);

}



void DrizzleAudioProcessor::releaseResources()

{

    if (pluginInstance != nullptr)

        pluginInstance->releaseResources();

    if (trackPluginManager != nullptr)

        trackPluginManager->releaseResources();

}



void DrizzleAudioProcessor::setPluginInstance (std::unique_ptr<juce::AudioPluginInstance> instance)

{

    if (pluginInstance != nullptr)

        pluginInstance->releaseResources();



    pluginInstance = std::move (instance);



    if (pluginInstance != nullptr)

    {

        pluginInstance->enableAllBuses();



        if (getSampleRate() > 0 && getBlockSize() > 0)

            pluginInstance->prepareToPlay (getSampleRate(), getBlockSize());

    }

}



void DrizzleAudioProcessor::applyMasterOutput (juce::AudioBuffer<float>& buffer)

{

    if (masterMute.load())

    {

        buffer.clear();

        peakLevel.store (0.0f);

        return;

    }



    const float totalGain = pluginGain.load() * MixerDbScale::dbToLinear (masterGainDb.load());

    const int numSamples = buffer.getNumSamples();

    const int numChannels = buffer.getNumChannels();

    float peak = 0.0f;



    if (masterMono.load() && numChannels >= 2)

    {

        auto* left  = buffer.getWritePointer (0);

        auto* right = buffer.getWritePointer (1);



        for (int i = 0; i < numSamples; ++i)

        {

            const float mono = (left[i] + right[i]) * 0.5f * totalGain;

            left[i] = mono;

            right[i] = mono;

            peak = juce::jmax (peak, std::abs (mono));

        }



        for (int ch = 2; ch < numChannels; ++ch)

            buffer.clear (ch, 0, numSamples);

    }

    else

    {

        for (int ch = 0; ch < numChannels; ++ch)

        {

            auto* data = buffer.getWritePointer (ch);



            for (int i = 0; i < numSamples; ++i)

            {

                data[i] *= totalGain;

                peak = juce::jmax (peak, std::abs (data[i]));

            }

        }

    }



    peakLevel.store (juce::jlimit (0.0f, 1.0f, peak * 2.5f));

}



void DrizzleAudioProcessor::processBlock (juce::AudioBuffer<float>& buffer, juce::MidiBuffer& midi)

{

    const int numSamples = buffer.getNumSamples();



    if (numSamples <= 0 || connectedInputChannels <= 0)

    {

        buffer.clear();

        peakLevel.store (0.0f);

        return;

    }



    workBuffer.setSize (2, numSamples, false, false, true);

    trackMixer.mixFromDeviceBuffer (buffer, workBuffer, connectedInputChannels);



    if (pluginInstance != nullptr)

    {

        juce::MidiBuffer pluginMidi;

        pluginInstance->processBlock (workBuffer, pluginMidi);

    }



    applyMasterOutput (workBuffer);

    if (streamEngine != nullptr)
        streamEngine->pushAudio (workBuffer, numSamples);

    const int numOut = juce::jmin (2, buffer.getNumChannels());


    for (int ch = 0; ch < numOut; ++ch)

        buffer.copyFrom (ch, 0, workBuffer, ch, 0, numSamples);



    juce::ignoreUnused (midi);

}


