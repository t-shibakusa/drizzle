#pragma once

#include "../AudioEngine.h"
#include "DrizzleTheme.h"
#include <juce_gui_extra/juce_gui_extra.h>

class TrackPluginPanel final : public juce::Component,
                               private juce::ChangeListener
{
public:
    TrackPluginPanel (AudioEngine& engine, int trackIndexIn, std::function<void()> onCloseIn);
    ~TrackPluginPanel() override;

    void paint (juce::Graphics& g) override;
    void resized() override;

    juce::Component* getModalParentComponent() const;

private:
    class AppliedListModel;
    class AvailableListModel;

    void changeListenerCallback (juce::ChangeBroadcaster* source) override;
    void refreshAppliedList();
    void refreshAvailableList();
    void updateButtons();
    void updateEmptyState();
    juce::Array<juce::PluginDescription> getAvailablePluginsForActiveTab() const;

    AudioEngine& audioEngine;
    int trackIndex = 0;
    std::function<void()> onClose;

    juce::Label titleLabel;
    juce::Label appliedTitleLabel;
    juce::Label availableTitleLabel;
    juce::ListBox appliedList;
    juce::ListBox availableList;
    juce::TextEditor emptyHintDisplay;
    juce::TextButton scanButton { juce::String::fromUTF8 (u8"\u518d\u30b9\u30ad\u30e3\u30f3") };
    juce::TextButton removeButton { juce::String::fromUTF8 (u8"\u524a\u9664") };
    juce::TextButton closeButton { juce::String::fromUTF8 (u8"\u9589\u3058\u308b") };
    juce::TabbedButtonBar formatTabs { juce::TabbedButtonBar::TabsAtTop };

    std::unique_ptr<AppliedListModel> appliedModel;
    std::unique_ptr<AvailableListModel> availableModel;
};
