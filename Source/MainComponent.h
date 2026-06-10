#pragma once

#include "AudioEngine.h"
#include "ui/DrizzlePanels.h"
#include <juce_gui_extra/juce_gui_extra.h>

class MainComponent final : public juce::Component,
                            public juce::MenuBarModel
{
public:
    MainComponent();
    ~MainComponent() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    juce::StringArray getMenuBarNames() override;
    juce::PopupMenu getMenuForIndex (int topLevelMenuIndex, const juce::String& menuName) override;
    void menuItemSelected (int menuItemID, int topLevelMenuIndex) override;

    void prepareForShutdown();

private:
    void showAudioSettings();
    void showVstSettings();

    AudioEngine audioEngine;
    TrackMixerPanel trackMixerPanel { audioEngine };
    StreamPreviewPanel streamPreviewPanel { audioEngine };
    StreamSettingsPanel streamSettingsPanel { audioEngine };
    CommentPanel commentPanel { audioEngine };
    StatusBarComponent statusBar { audioEngine };
    SystemMetricsBar systemMetricsBar;

    bool shutdownPrepared = false;

    JUCE_DECLARE_NON_COPYABLE_WITH_LEAK_DETECTOR (MainComponent)
};
