#pragma once

#include "StreamAudioFifo.h"
#include <juce_audio_basics/juce_audio_basics.h>
#include <juce_events/juce_events.h>
#include <atomic>

struct StreamConfig
{
    juce::String title;
    juce::String streamKey;
    int serviceId = 1;
};

enum class StreamState
{
    idle,
    starting,
    live,
    stopping,
    error
};

struct FfmpegProcess;

class StreamEngine final : public juce::ChangeBroadcaster
{
public:
    StreamEngine();
    ~StreamEngine() override;

    void setSampleRate (double sampleRateIn) noexcept;
    void pushAudio (const juce::AudioBuffer<float>& stereoBuffer, int numSamples);

    void setConfig (const StreamConfig& config);
    StreamConfig getConfig() const;

    bool startStream();
    void stopStream();
    bool testConnection();

    StreamState getState() const noexcept { return state.load(); }
    juce::String getStatusText() const;
    juce::String getLastError() const;
    double getLiveDurationSeconds() const noexcept;
    int getUnderrunCount() const noexcept { return underrunCount.load(); }
    float getOutputPeakLevel() const noexcept { return outputPeakLevel.load(); }

    static juce::File findFfmpegExecutable();
    static juce::String normalizeYoutubeStreamKey (const juce::String& input);
    static juce::String getYoutubeRtmpUrl (const juce::String& streamKey);

    void runStreamThread();

private:
    void setState (StreamState newState, const juce::String& error = {});
    juce::String buildFfmpegCommand (const juce::File& ffmpeg,
                                     const juce::String& streamKey,
                                     const juce::String& title,
                                     bool hasFont) const;
    bool prepareStreamAssets (const juce::String& title);
    static juce::File findTitleFontSource();

    StreamConfig config;
    StreamAudioFifo audioFifo;
    std::unique_ptr<juce::Thread> streamThread;
    std::unique_ptr<FfmpegProcess> ffmpegProcess;

    std::atomic<StreamState> state { StreamState::idle };
    std::atomic<bool> stopRequested { false };
    std::atomic<double> sampleRate { 44100.0 };
    std::atomic<int> underrunCount { 0 };
    std::atomic<float> outputPeakLevel { 0.0f };
    std::atomic<int64_t> liveStartMs { 0 };

    juce::CriticalSection configLock;
    juce::String lastError;
    juce::File streamWorkDir;
    juce::File streamFontFile;
};
