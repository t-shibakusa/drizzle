#pragma once

#include "../AudioEngine.h"
#include "DrizzleTheme.h"
#include "MixerFaderLookAndFeel.h"
#include <juce_gui_extra/juce_gui_extra.h>

class TrackMixerPanel final : public juce::Component,
                              private juce::Timer
{
public:
    explicit TrackMixerPanel (AudioEngine& engine);
    ~TrackMixerPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    void reloadFromEngine();
    void prepareForShutdown();

private:
    class TrackRowComponent;
    class TracksListContent;

    void timerCallback() override;
    int getNumTrackRows() const;
    void rebuildTrackList();
    void layoutTracksList();
    void updateAddTrackButton();
    void commitAllNameEdits();
    void syncUIFromEngine();
    void showInputSelectionForTrack (int trackIndex);
    void confirmRemoveTrack (int trackIndex);
    void updateTrackButtonStyles (TrackRowComponent& row);

    AudioEngine& audioEngine;
    MixerFaderLookAndFeel faderLookAndFeel;

    juce::Viewport tracksViewport;
    std::unique_ptr<TracksListContent> tracksListContent;
    juce::OwnedArray<TrackRowComponent> trackRows;

    juce::TextButton addTrackButton { juce::String::fromUTF8 (u8"+ \u30c8\u30e9\u30c3\u30af\u3092\u8ffd\u52a0") };

    juce::Label masterTitle;
    juce::Slider masterFader;
    juce::TextButton masterMuteButton { juce::String::fromUTF8 (u8"\u30df\u30e5\u30fc\u30c8") };
    juce::TextButton masterMonoButton { "Mono" };
};

class StreamPreviewPanel final : public juce::Component,
                                 private juce::Timer
{
public:
    StreamPreviewPanel();
    ~StreamPreviewPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    juce::String liveTimer { "00:12:34" };
    float masterLevel = 0.45f;
};

class StreamSettingsPanel final : public juce::Component
{
public:
    StreamSettingsPanel();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::Label serviceLabel;
    juce::ComboBox serviceBox;
    juce::Label titleLabel;
    juce::TextEditor titleEditor;
    juce::TextButton testButton { juce::String::fromUTF8 (u8"\u63a5\u7d9a\u30c6\u30b9\u30c8") };
    juce::TextButton streamButton { juce::String::fromUTF8 (u8"\u914d\u4fe1\u958b\u59cb") };
};

class CommentPanel final : public juce::Component
{
public:
    CommentPanel();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::TextEditor commentDisplay;
    juce::TextEditor commentInput;
    juce::TextButton sendButton { juce::String::fromUTF8 (u8"\u9001\u4fe1") };
    juce::Label viewerLabel;
};

class StatusBarComponent final : public juce::Component
{
public:
    StatusBarComponent();

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    juce::Label statusLabel;
    juce::Label statsLabel;
};

class SystemMetricsBar final : public juce::Component
{
public:
    SystemMetricsBar();

    void paint (juce::Graphics& g) override;
};
