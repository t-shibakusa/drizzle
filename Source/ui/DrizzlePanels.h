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
    void showTrackColourPicker (int trackIndex, juce::Component& anchor);
    void showTrackPluginDialog (int trackIndex);
    void updateTrackButtonStyles (TrackRowComponent& row);
    void updateMasterButtonStyles();

    AudioEngine& audioEngine;
    float masterLevel = 0.0f;
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
                                 private juce::Timer,
                                 private juce::ChangeListener
{
public:
    explicit StreamPreviewPanel (AudioEngine& engine);
    ~StreamPreviewPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void updateYoutubeLiveLink();

    AudioEngine& audioEngine;
    juce::TextButton youtubeLiveLink { juce::String::fromUTF8 (u8"YouTube \u914d\u4fe1\u753b\u9762") };
    juce::URL youtubeLiveUrl;
    juce::String liveTimer { "00:00:00" };
    float masterLevel = 0.0f;
};

class StreamSettingsPanel final : public juce::Component,
                                  private juce::ChangeListener
{
public:
    explicit StreamSettingsPanel (AudioEngine& engine);
    ~StreamSettingsPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void reloadFromEngine();
    void prepareForShutdown();

private:
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void applyConfigFromUI();
    void updateStreamButton();
    void onTestClicked();
    void onStreamClicked();

    AudioEngine& audioEngine;
    juce::Label serviceLabel;
    juce::ComboBox serviceBox;
    juce::Label titleLabel;
    juce::TextEditor titleEditor;
    juce::Label streamKeyLabel;
    juce::TextEditor streamKeyEditor;
    juce::TextButton testButton { juce::String::fromUTF8 (u8"\u63a5\u7d9a\u30c6\u30b9\u30c8") };
    juce::TextButton streamButton { juce::String::fromUTF8 (u8"\u914d\u4fe1\u958b\u59cb") };
};

class CommentPanel final : public juce::Component,
                           private juce::Timer,
                           private juce::ChangeListener
{
public:
    explicit CommentPanel (AudioEngine& engine);
    ~CommentPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;
    void prepareForShutdown();

private:
    void timerCallback() override;
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void refreshComments();
    void updateViewerLabel();
    void updateAuthButton();
    void syncChatPollingWithStream();
    void onSendClicked();
    void onAuthClicked();
    void showConnectDialog();

    AudioEngine& audioEngine;
    juce::TextButton authButton { juce::String::fromUTF8 (u8"YouTube\u9023\u643a") };
    juce::TextEditor commentDisplay;
    juce::TextEditor commentInput;
    juce::TextButton sendButton { juce::String::fromUTF8 (u8"\u9001\u4fe1") };
    juce::Label viewerLabel;
    juce::Label statusLabel;
};

class StatusBarComponent final : public juce::Component,
                                  private juce::Timer
{
public:
    explicit StatusBarComponent (AudioEngine& engine);

    void paint (juce::Graphics& g) override;
    void resized() override;

private:
    void timerCallback() override;

    AudioEngine& audioEngine;
    juce::TextEditor statusDisplay;
    juce::Label statsLabel;
};

class SystemMetricsBar final : public juce::Component
{
public:
    SystemMetricsBar();

    void paint (juce::Graphics& g) override;
};
