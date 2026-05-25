#include "MainComponent.h"
#include "ApplicationShutdown.h"
#include "ui/SettingsPanels.h"

MainComponent::MainComponent()
{
    addAndMakeVisible (trackMixerPanel);
    addAndMakeVisible (streamPreviewPanel);
    addAndMakeVisible (streamSettingsPanel);
    addAndMakeVisible (commentPanel);
    addAndMakeVisible (statusBar);
    addAndMakeVisible (systemMetricsBar);

    audioEngine.initialise();
    trackMixerPanel.reloadFromEngine();
    setSize (1600, 960);
}

MainComponent::~MainComponent()
{
    prepareForShutdown();
}

void MainComponent::prepareForShutdown()
{
    if (shutdownPrepared)
        return;

    shutdownPrepared = true;

    trackMixerPanel.prepareForShutdown();

    ApplicationShutdown::dismissModalComponents();
    ApplicationShutdown::closeExtraTopLevelWindows (this);

    audioEngine.shutdown();
}

void MainComponent::paint (juce::Graphics& g)
{
    g.fillAll (DrizzleTheme::background());
}

void MainComponent::resized()
{
    auto area = getLocalBounds();
    statusBar.setBounds (area.removeFromBottom (28));
    systemMetricsBar.setBounds (area.removeFromTop (24).removeFromRight (360).reduced (8, 4));

    const int commentWidth = juce::jmax (280, area.getWidth() / 5);
    commentPanel.setBounds (area.removeFromRight (commentWidth));

    const int trackWidth = juce::jmax (260, area.getWidth() / 5);
    trackMixerPanel.setBounds (area.removeFromLeft (trackWidth));

    auto center = area;
    const int previewHeight = juce::roundToInt ((float) center.getHeight() * 0.56f);
    streamPreviewPanel.setBounds (center.removeFromTop (previewHeight));
    streamSettingsPanel.setBounds (center);
}

juce::StringArray MainComponent::getMenuBarNames()
{
    return { juce::String::fromUTF8 (u8"\u30d5\u30a1\u30a4\u30eb"),
             juce::String::fromUTF8 (u8"\u7de8\u96c6"),
             juce::String::fromUTF8 (u8"\u8868\u793a"),
             juce::String::fromUTF8 (u8"\u30c8\u30e9\u30c3\u30af"),
             juce::String::fromUTF8 (u8"\u914d\u4fe1"),
             juce::String::fromUTF8 (u8"\u30c4\u30fc\u30eb"),
             juce::String::fromUTF8 (u8"\u8a2d\u5b9a"),
             juce::String::fromUTF8 (u8"\u30d8\u30eb\u30d7") };
}

juce::PopupMenu MainComponent::getMenuForIndex (int index, const juce::String&)
{
    juce::PopupMenu menu;

    switch (index)
    {
        case 6: // Settings
            menu.addItem (601, juce::String::fromUTF8 (u8"\u30aa\u30fc\u30c7\u30a3\u30aa\u8a2d\u5b9a..."));
            menu.addItem (602, juce::String::fromUTF8 (u8"VST\u30d7\u30e9\u30b0\u30a4\u30f3\u8a2d\u5b9a..."));
            menu.addSeparator();
            menu.addItem (603, juce::String::fromUTF8 (u8"\u914d\u4fe1\u8a2d\u5b9a..."));
            menu.addItem (604, juce::String::fromUTF8 (u8"\u30b7\u30e7\u30fc\u30c8\u30ab\u30c3\u30c8\u8a2d\u5b9a..."));
            menu.addItem (605, juce::String::fromUTF8 (u8"\u5916\u898b\u8a2d\u5b9a..."));
            menu.addItem (606, juce::String::fromUTF8 (u8"\u8a2d\u5b9a\u3092\u30a8\u30af\u30b9\u30dd\u30fc\u30c8..."));
            menu.addItem (607, juce::String::fromUTF8 (u8"\u8a2d\u5b9a\u3092\u30a4\u30f3\u30dd\u30fc\u30c8..."));
            break;
        default:
            menu.addItem (1000 + index, juce::String::fromUTF8 (u8"\uff08\u6e96\u5099\u4e2d\uff09"));
            break;
    }

    return menu;
}

void MainComponent::menuItemSelected (int menuItemID, int)
{
    if (menuItemID == 601)
        showAudioSettings();
    else if (menuItemID == 602)
        showVstSettings();
}

void MainComponent::showAudioSettings()
{
    showSettingsDialog (*this,
                        juce::String::fromUTF8 (u8"\u30aa\u30fc\u30c7\u30a3\u30aa\u8a2d\u5b9a"),
                        std::make_unique<AudioSettingsPanel> (audioEngine));
}

void MainComponent::showVstSettings()
{
    showSettingsDialog (*this,
                        juce::String::fromUTF8 (u8"VST\u30d7\u30e9\u30b0\u30a4\u30f3\u8a2d\u5b9a"),
                        std::make_unique<VstSettingsPanel> (audioEngine));
}
