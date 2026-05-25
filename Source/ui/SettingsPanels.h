#pragma once

#include "../AudioEngine.h"
#include "../PluginScanPaths.h"
#include <juce_gui_extra/juce_gui_extra.h>

class AudioSettingsPanel final : public juce::Component
{
public:
    explicit AudioSettingsPanel (AudioEngine& engine);

    void resized() override;

private:
    void layoutSelector();

    AudioEngine& audioEngine;
    std::unique_ptr<juce::AudioDeviceSelectorComponent> selector;
    juce::Viewport viewport;
};

class VstSettingsPanel final : public juce::Component,
                               private juce::ChangeListener,
                               private juce::ListBoxModel
{
public:
    explicit VstSettingsPanel (AudioEngine& engine);
    ~VstSettingsPanel() override;

    void resized() override;

private:
    void changeListenerCallback (juce::ChangeBroadcaster* source) override;

    int getNumRows() override;
    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override;
    void listBoxItemClicked (int row, const juce::MouseEvent&) override;

    PluginPathCategory getActiveCategory() const noexcept;
    void refreshPathsList();
    void refreshPluginList();
    void loadSelectedPlugin();
    void browseForPluginFile();
    void addScanDirectory();
    void removeSelectedDirectory();
    void updatePluginLabel();

    AudioEngine& audioEngine;
    juce::Label titleLabel;
    juce::Label pathsLabel;
    juce::TabbedButtonBar formatTabs { juce::TabbedButtonBar::TabsAtTop };
    juce::ListBox pathsList { "Paths", this };
    juce::TextButton addPathButton { juce::String::fromUTF8 (u8"\u8ffd\u52a0...") };
    juce::TextButton removePathButton { juce::String::fromUTF8 (u8"\u524a\u9664") };
    juce::Label aaxNoteLabel;
    juce::Label pluginNameLabel;
    juce::ComboBox pluginSelector;
    juce::Slider gainSlider;
    juce::Label gainLabel;
    juce::TextButton scanButton { juce::String::fromUTF8 (u8"\u30b9\u30ad\u30e3\u30f3") };
    juce::TextButton loadButton { "Load" };
    juce::TextButton browseButton { "Browse..." };
    juce::TextButton editorButton { "Editor" };
    juce::TextButton clearButton { "Clear" };
    std::unique_ptr<juce::FileChooser> fileChooser;
};

void showSettingsDialog (juce::Component& owner, const juce::String& title, std::unique_ptr<juce::Component> content);
