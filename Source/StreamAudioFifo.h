#pragma once

#include <juce_audio_basics/juce_audio_basics.h>
#include <atomic>
#include <vector>

/** Lock-free stereo FIFO for audio thread -> stream thread handoff. */
class StreamAudioFifo final
{
public:
    explicit StreamAudioFifo (int capacityFrames = 48000 * 2)
        : fifo (capacityFrames),
          buffer ((size_t) capacityFrames * 2, 0.0f)
    {
    }

    void reset() noexcept
    {
        fifo.reset();
        overflowDropCount.store (0);
    }

    int getNumReadyFrames() const noexcept
    {
        return fifo.getNumReady();
    }

    int getOverflowDropCount() const noexcept
    {
        return overflowDropCount.load();
    }

    void pushStereo (const float* left, const float* right, int numFrames) noexcept
    {
        if (left == nullptr || right == nullptr || numFrames <= 0)
            return;

        const int freeFrames = fifo.getFreeSpace();

        if (freeFrames < numFrames)
        {
            int dropFrames = numFrames - freeFrames;
            int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
            fifo.prepareToRead (dropFrames, start1, size1, start2, size2);
            fifo.finishedRead (size1 + size2);
            overflowDropCount.fetch_add (size1 + size2);
        }

        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToWrite (numFrames, start1, size1, start2, size2);

        int sourceFrame = 0;

        auto writeBlock = [&] (int startIndex, int blockSize)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const int frameIndex = startIndex + i;
                buffer[(size_t) frameIndex * 2]     = left[sourceFrame];
                buffer[(size_t) frameIndex * 2 + 1] = right[sourceFrame];
                ++sourceFrame;
            }
        };

        if (size1 > 0)
            writeBlock (start1, size1);

        if (size2 > 0)
            writeBlock (start2, size2);

        fifo.finishedWrite (size1 + size2);
    }

    int popInterleavedS16 (int16_t* dest, int maxFrames) noexcept
    {
        if (dest == nullptr || maxFrames <= 0)
            return 0;

        int start1 = 0, size1 = 0, start2 = 0, size2 = 0;
        fifo.prepareToRead (maxFrames, start1, size1, start2, size2);

        int outIndex = 0;

        auto readBlock = [&] (int startIndex, int blockSize)
        {
            for (int i = 0; i < blockSize; ++i)
            {
                const int frameIndex = startIndex + i;
                dest[outIndex++] = floatToS16 (buffer[(size_t) frameIndex * 2]);
                dest[outIndex++] = floatToS16 (buffer[(size_t) frameIndex * 2 + 1]);
            }
        };

        if (size1 > 0)
            readBlock (start1, size1);

        if (size2 > 0)
            readBlock (start2, size2);

        fifo.finishedRead (size1 + size2);
        return size1 + size2;
    }

private:
    static int16_t floatToS16 (float sample) noexcept
    {
        return (int16_t) juce::jlimit (-32768, 32767, juce::roundToInt (sample * 32767.0f));
    }

    juce::AbstractFifo fifo;
    std::vector<float> buffer;
    std::atomic<int> overflowDropCount { 0 };
};
