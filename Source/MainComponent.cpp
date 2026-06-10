#include "MainComponent.h"
#include "ApplicationShutdown.h"
#include "SessionSettings.h"
#include "Vst3HostIdentity.h"
#include "ui/SettingsPanels.h"

MainComponent::MainComponent()
{
    addAndMakeVisible (trackMixerPanel);
    addAndMakeVisible (streamPreviewPanel);
    addAndMakeVisible (streamSettingsPanel);
    addAndMakeVisible (commentPanel);
    addAndMakeVisible (statusBar);
    addAndMakeVisible (systemMetricsBar);

    const auto sessionSettings = SessionSettingsStore::load();
    DrizzleVst3Host::setHostApplicationName (sessionSettings.vst3HostIdentity);

    if (DrizzleVst3Host::isLicenseCompatProcess())
        DrizzleVst3Host::setHostApplicationName ("Reaper");

    audioEngine.initialise();
    trackMixerPanel.reloadFromEngine();
    streamSettingsPanel.reloadFromEngine();
    setSize (1600, 960);

    if (! DrizzleVst3Host::isLicenseCompatProcess())
    {
        juce::MessageManager::callAsync ([]
        {
            const auto compatPath = DrizzleVst3Host::getLicenseCompatReaperExecutable().getFullPathName();

            juce::NativeMessageBox::showMessageBoxAsync (
                juce::MessageBoxIconType::WarningIcon,
                juce::String::fromUTF8 (u8"XLN / iZotope \u30e9\u30a4\u30bb\u30f3\u30b9"),
                juce::String::fromUTF8 (
                    u8"LicenseCompat\\reaper.exe \u304c\u898b\u3064\u304b\u3089\u305a\u3001\u81ea\u52d5\u5207\u308a\u66ff\u3048\u3067\u304d\u307e\u305b\u3093\u3067\u3057\u305f\u3002\n"
                    u8"Release \u3092\u518d\u30d3\u30eb\u30c9\u3057\u3001\u6b21\u306e\u30d5\u30a1\u30a4\u30eb\u304b\u3089\u8d77\u52d5\u3057\u3066\u304f\u3060\u3055\u3044:\n")
                    + compatPath);
        });
    }
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
    streamSettingsPanel.prepareForShutdown();
    commentPanel.prepareForShutdown();

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
    statusBar.setBounds (area.removeFromBottom (96));
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
    return { juce::String::fromUTF8 (u8"\u8a2d\u5b9a"),
             juce::String::fromUTF8 (u8"\u30d8\u30eb\u30d7") };
}

juce::PopupMenu MainComponent::getMenuForIndex (int index, const juce::String&)
{
    juce::PopupMenu menu;

    switch (index)
    {
        case 0: // Settings
            menu.addItem (601, juce::String::fromUTF8 (u8"\u30aa\u30fc\u30c7\u30a3\u30aa\u8a2d\u5b9a..."));
            menu.addItem (602, juce::String::fromUTF8 (u8"VST\u30d7\u30e9\u30b0\u30a4\u30f3\u8a2d\u5b9a..."));
            menu.addSeparator();
            menu.addItem (608, juce::String::fromUTF8 (u8"\u30e9\u30a4\u30bb\u30f3\u30xb9\u4e92\u63db\u30e2\u30fc\u30c9\u3067\u518d\u8d77\u52d5 (reaper.exe)"));
            menu.addSeparator();
            menu.addItem (603, juce::String::fromUTF8 (u8"\u914d\u4fe1\u8a2d\u5b9a..."));
            menu.addItem (604, juce::String::fromUTF8 (u8"\u30b7\u30e7\u30fc\u30c8\u30ab\u30c3\u30c8\u8a2d\u5b9a..."));
            menu.addItem (605, juce::String::fromUTF8 (u8"\u5916\u898b\u8a2d\u5b9a..."));
            menu.addItem (606, juce::String::fromUTF8 (u8"\u8a2d\u5b9a\u3092\u30a8\u30af\u30b9\u30dd\u30fc\u30c8..."));
            menu.addItem (607, juce::String::fromUTF8 (u8"\u8a2d\u5b9a\u3092\u30a4\u30f3\u30dd\u30fc\u30c8..."));
            break;
        case 1: // Help
            menu.addItem (701, juce::String::fromUTF8 (u8"Drizzle \u306b\u3064\u3044\u3066..."));
            break;
        default:
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
    else if (menuItemID == 608)
    {
        if (DrizzleVst3Host::isLicenseCompatProcess())
        {
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                        juce::String::fromUTF8 (u8"\u30e9\u30a4\u30bb\u30f3\u30xb9\u4e92\u63db\u30e2\u30fc\u30c9"),
                                                        juce::String::fromUTF8 (u8"\u3059\u3067\u306b reaper.exe \u304b\u3089\u8d77\u52d5\u4e2d\u3067\u3059\u3002"));
            return;
        }

        if (! DrizzleVst3Host::launchLicenseCompatAndQuit())
        {
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                        juce::String::fromUTF8 (u8"\u30e9\u30a4\u30bb\u30f3\u30xb9\u4e92\u63db\u30e2\u30fc\u30c9"),
                                                        juce::String::fromUTF8 (u8"LicenseCompat\\reaper.exe \u304c\u898b\u3064\u304b\u308a\u307e\u305b\u3093\u3002"
                                                                                u8"Release \u30r\u30d3\u30eb\u30c9\u3092\u518d\u5b9f\u884c\u3057\u3066\u304f\u3060\u3055\u3044\u3002"));
        }
    }
    else if (menuItemID == 701)
    {
        juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::InfoIcon,
                                                    "Drizzle",
                                                    "Drizzle v0.1.0");
    }
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
