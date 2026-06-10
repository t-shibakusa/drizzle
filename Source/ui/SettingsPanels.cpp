#include "SettingsPanels.h"
#include "DrizzleTheme.h"
#include "../SessionSettings.h"
#include "../Vst3HostIdentity.h"

AudioSettingsPanel::AudioSettingsPanel (AudioEngine& engine)
    : audioEngine (engine)
{
    selector = std::make_unique<juce::AudioDeviceSelectorComponent> (audioEngine.getDeviceManager(),
                                                                     1, 32,
                                                                     1, 32,
                                                                     false, false,
                                                                     true,
                                                                     false);
    selector->setItemHeight (24);
    viewport.setViewedComponent (selector.get(), false);
    viewport.setScrollBarsShown (true, false);
    addAndMakeVisible (viewport);
}

void AudioSettingsPanel::layoutSelector()
{
    if (selector == nullptr)
        return;

    const auto width = juce::jmax (1, viewport.getMaximumVisibleWidth());
    selector->setBounds (0, 0, width, 4096);
    selector->setSize (width, selector->getHeight());
}

void AudioSettingsPanel::resized()
{
    viewport.setBounds (getLocalBounds().reduced (8));
    layoutSelector();
}

VstSettingsPanel::VstSettingsPanel (AudioEngine& engine)
    : audioEngine (engine)
{
    titleLabel.setText (juce::String::fromUTF8 (u8"\u30d7\u30e9\u30b0\u30a4\u30f3\u8a2d\u5b9a"),
                        juce::dontSendNotification);
    DrizzleTheme::applyLabel (titleLabel);

    pathsLabel.setText (juce::String::fromUTF8 (u8"\u30b9\u30ad\u30e3\u30f3\u5bfe\u8c61\u30c7\u30a3\u30ec\u30c8\u30ea\uff08\u30d5\u30a9\u30fc\u30de\u30c3\u30c8\u5225\uff09"),
                        juce::dontSendNotification);
    DrizzleTheme::applyLabel (pathsLabel);

    for (int i = 0; i < (int) PluginPathCategory::count; ++i)
    {
        const auto category = (PluginPathCategory) i;
        formatTabs.addTab (PluginScanPaths::getCategoryDisplayName (category),
                           juce::Colours::darkgrey,
                           (int) category);
    }

    formatTabs.setCurrentTabIndex (0);
    formatTabs.addChangeListener (this);

    aaxNoteLabel.setText (juce::String::fromUTF8 (
        u8"AAX \u30bf\u30d6: \u30d1\u30b9\u767b\u9332\u306e\u307f\uff08JUCE \u6a19\u6e96\u3067\u306f\u30db\u30b9\u30c8\u975e\u5bfe\u5fdc\uff09"),
        juce::dontSendNotification);
    DrizzleTheme::applyLabel (aaxNoteLabel, true);

    vstNoteLabel.setText (juce::String::fromUTF8 (
        u8"VST/VST2 (.dll) \u306f\u3053\u306e\u30d3\u30eb\u30c9\u3067\u306f\u672a\u5bfe\u5fdc\u3067\u3059\u3002VST3 \u3092\u4f7f\u7528\u3057\u3066\u304f\u3060\u3055\u3044\u3002"
        u8"\u3053\u3053\u3067\u8ffd\u52a0\u3057\u305f\u30d5\u30a9\u30eb\u30c0\u5185\u306e .vst3 \u306f\u3001\u518d\u30b9\u30ad\u30e3\u30f3\u5f8c\u306b VST3 \u4e00\u89a7\u306b\u8868\u793a\u3055\u308c\u307e\u3059\u3002"),
        juce::dontSendNotification);
    DrizzleTheme::applyLabel (vstNoteLabel, true);

    hostIdentityLabel.setText (juce::String::fromUTF8 (u8"\u30e9\u30a4\u30bb\u30f3\u30b9\u8a8d\u8a3c\u306e\u305f\u3081\u306e\u8d77\u52d5\u65b9\u6cd5"),
                               juce::dontSendNotification);
    DrizzleTheme::applyLabel (hostIdentityLabel);

    const auto compatExe = DrizzleVst3Host::getLicenseCompatReaperExecutable();
    const auto installedExe = DrizzleVst3Host::getInstalledLicenseCompatExecutable();
    const auto compatPath = compatExe.getFullPathName();
    const bool runningCompat = DrizzleVst3Host::isLicenseCompatProcess();
    const bool hasInstalledCopy = installedExe.existsAsFile();

    if (runningCompat)
    {
        juce::String status = juce::String::fromUTF8 (u8"\u73fe\u5728: reaper.exe \u304b\u3089\u8d77\u52d5\u4e2d\uff08\u30e9\u30a4\u30bb\u30f3\u30b9\u4e92\u63db\u6709\u52b9\uff09");
        status << "\n" << juce::File::getSpecialLocation (juce::File::currentApplicationFile).getFullPathName();
        licenseCompatStatusLabel.setText (status, juce::dontSendNotification);
        licenseCompatStatusLabel.setColour (juce::Label::textColourId, juce::Colours::lightgreen);
    }
    else
    {
        licenseCompatStatusLabel.setText (juce::String::fromUTF8 (
            u8"\u73fe\u5728: Drizzle.exe \u304b\u3089\u8d77\u52d5\u4e2d \u2192 \u81ea\u52d5\u3067 reaper.exe \u306b\u5207\u308a\u66ff\u3048\u308b\u306f\u305a\u3067\u3059"),
            juce::dontSendNotification);
        licenseCompatStatusLabel.setColour (juce::Label::textColourId, juce::Colours::orange);
    }
    DrizzleTheme::applyLabel (licenseCompatStatusLabel);

    hostIdentityNoteLabel.setText (juce::String::fromUTF8 (
        u8"XLN \u306f\u30db\u30b9\u30c8\u306e exe \u540d\u3067\u30e9\u30a4\u30bb\u30f3\u30b9 ID \u3092\u5224\u5b9a\u3057\u307e\u3059\u3002"
        u8"\u307e\u3060\u8a8d\u8a3c\u306b\u5931\u6557\u3059\u308b\u5834\u5408\u306f\u300cReaper \u30d5\u30a9\u30eb\u30c0\u3078\u30a4\u30f3\u30b9\u30c8\u30fc\u30eb\u300d\u3092\u5b9f\u884c\u3057\u3066\u304b\u3089 Drizzle \u3092\u518d\u8d77\u52d5\u3057\u3066\u304f\u3060\u3055\u3044\u3002\n\n"
        u8"\u73fe\u5728\u306e\u8d77\u52d5\u5148:\n")
        + compatPath
        + (hasInstalledCopy
               ? juce::String::fromUTF8 (u8"\n\nReaper \u30d5\u30a9\u30eb\u30c0\u30a4\u30f3\u30b9\u30c8\u30fc\u30eb\u6e08\u307f:\n") + installedExe.getFullPathName()
               : juce::String::fromUTF8 (u8"\n\n\u203b \u307e\u3060 Reaper \u30d5\u30a9\u30eb\u30c0\u672a\u30a4\u30f3\u30b9\u30c8\u30fc\u30eb\u3002\u4e0b\u306e\u30dc\u30bf\u30f3\u3092\u5b9f\u884c\u3057\u3066\u304f\u3060\u3055\u3044\u3002")),
        juce::dontSendNotification);
    DrizzleTheme::applyLabel (hostIdentityNoteLabel, true);

    openLicenseCompatButton.setButtonText (juce::String::fromUTF8 (u8"\u30d5\u30a9\u30eb\u30c0\u3092\u958b\u304f"));
    openLicenseCompatButton.onClick = [compatExe]
    {
        compatExe.getParentDirectory().startAsProcess();
    };

    installLicenseCompatButton.setButtonText (juce::String::fromUTF8 (u8"Reaper \u30d5\u30a9\u30eb\u30c0\u3078\u30a4\u30f3\u30b9\u30c8\u30fc\u30eb"));
    installLicenseCompatButton.onClick = []
    {
        if (! DrizzleVst3Host::installLicenseCompatToReaperFolder())
        {
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                        juce::String::fromUTF8 (u8"\u30a4\u30f3\u30b9\u30c8\u30fc\u30eb"),
                                                        juce::String::fromUTF8 (u8"install_to_reaper_folder.bat \u304c\u898b\u3064\u304b\u308a\u307e\u305b\u3093\u3002"));
        }
    };

    restartLicenseCompatButton.setButtonText (juce::String::fromUTF8 (u8"reaper.exe \u3067\u518d\u8d77\u52d5"));
    restartLicenseCompatButton.onClick = []
    {
        if (! DrizzleVst3Host::launchLicenseCompatAndQuit())
        {
            juce::NativeMessageBox::showMessageBoxAsync (juce::MessageBoxIconType::WarningIcon,
                                                        juce::String::fromUTF8 (u8"\u30e9\u30a4\u30bb\u30f3\u30xb9\u4e92\u63db\u30e2\u30fc\u30c9"),
                                                        juce::String::fromUTF8 (u8"LicenseCompat\\reaper.exe \u304c\u898b\u3064\u304b\u308a\u307e\u305b\u3093\u3002"));
        }
    };

    hostIdentityCombo.setVisible (false);

    pathsList.setRowHeight (22);
    pathsList.setMultipleSelectionEnabled (false);

    addPathButton.onClick = [this] { addScanDirectory(); };
    removePathButton.onClick = [this] { removeSelectedDirectory(); };

    DrizzleTheme::applyLabel (pluginNameLabel);
    pluginNameLabel.setText ("Loaded: None", juce::dontSendNotification);

    gainSlider.setRange (0.0, 2.0, 0.01);
    gainSlider.setValue (audioEngine.getPluginGain());
    gainSlider.onValueChange = [this]
    {
        audioEngine.setPluginGain (static_cast<float> (gainSlider.getValue()));
    };

    gainLabel.setText ("Gain", juce::dontSendNotification);
    gainLabel.attachToComponent (&gainSlider, true);
    DrizzleTheme::applyLabel (gainLabel);

    pluginSelector.setTextWhenNothingSelected ("Select a plugin");

    scanButton.onClick = [this]
    {
        scanButton.setEnabled (false);
        audioEngine.getPluginChain().scanPlugins();
        refreshPluginList();
        scanButton.setEnabled (true);
    };

    loadButton.onClick = [this] { loadSelectedPlugin(); };
    browseButton.onClick = [this] { browseForPluginFile(); };
    editorButton.onClick = [this] { audioEngine.getPluginChain().showPluginEditor(); };
    clearButton.onClick = [this] { audioEngine.getPluginChain().clearPlugin(); };

    addAndMakeVisible (titleLabel);
    addAndMakeVisible (pathsLabel);
    addAndMakeVisible (formatTabs);
    addAndMakeVisible (pathsList);
    addAndMakeVisible (addPathButton);
    addAndMakeVisible (removePathButton);
    addAndMakeVisible (aaxNoteLabel);
    addAndMakeVisible (vstNoteLabel);
    addAndMakeVisible (hostIdentityLabel);
    addAndMakeVisible (licenseCompatStatusLabel);
    addAndMakeVisible (hostIdentityNoteLabel);
    addAndMakeVisible (openLicenseCompatButton);
    addAndMakeVisible (installLicenseCompatButton);
    addAndMakeVisible (restartLicenseCompatButton);
    addAndMakeVisible (pluginNameLabel);
    addAndMakeVisible (pluginSelector);
    addAndMakeVisible (gainSlider);
    addAndMakeVisible (gainLabel);
    addAndMakeVisible (scanButton);
    addAndMakeVisible (loadButton);
    addAndMakeVisible (browseButton);
    addAndMakeVisible (editorButton);
    addAndMakeVisible (clearButton);

    audioEngine.getPluginChain().addChangeListener (this);
    refreshPathsList();
    updatePluginLabel();
    refreshPluginList();

    setSize (600, 900);
}

VstSettingsPanel::~VstSettingsPanel()
{
    formatTabs.removeChangeListener (this);
    audioEngine.getPluginChain().removeChangeListener (this);
    audioEngine.getPluginChain().hidePluginEditor();
}

void VstSettingsPanel::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &formatTabs)
    {
        pathsList.deselectAllRows();
        refreshPathsList();
        return;
    }

    if (source == &audioEngine.getPluginChain())
        updatePluginLabel();
}

PluginPathCategory VstSettingsPanel::getActiveCategory() const noexcept
{
    return (PluginPathCategory) juce::jlimit (0, (int) PluginPathCategory::count - 1, formatTabs.getCurrentTabIndex());
}

int VstSettingsPanel::getNumRows()
{
    return audioEngine.getPluginChain().getScanPaths().getPaths (getActiveCategory()).size();
}

void VstSettingsPanel::paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected)
{
    const auto& paths = audioEngine.getPluginChain().getScanPaths().getPaths (getActiveCategory());

    if (! juce::isPositiveAndBelow (row, paths.size()))
        return;

    if (selected)
        g.fillAll (DrizzleTheme::accent().withAlpha (0.35f));

    g.setColour (DrizzleTheme::textPrimary());
    g.setFont (juce::FontOptions { 13.0f });
    g.drawText (paths[row], 4, 0, width - 8, height, juce::Justification::centredLeft);
}

void VstSettingsPanel::listBoxItemClicked (int row, const juce::MouseEvent&)
{
    pathsList.selectRow (row);
    refreshPathsList();
}

void VstSettingsPanel::refreshPathsList()
{
    const auto category = getActiveCategory();
    const bool isAax = category == PluginPathCategory::Aax;
    const bool showVstNote = ! PluginChain::isVst2HostingEnabled()
                             && (category == PluginPathCategory::Vst
                                 || category == PluginPathCategory::Vst2);

    aaxNoteLabel.setVisible (isAax);
    vstNoteLabel.setVisible (showVstNote);
    scanButton.setEnabled (! isAax);

    pathsList.updateContent();
    pathsList.repaint();

    const bool hasSelection = pathsList.getSelectedRow() >= 0;
    const bool hasPaths = getNumRows() > 0;
    removePathButton.setEnabled (hasSelection && hasPaths);
}

void VstSettingsPanel::refreshPluginList()
{
    pluginSelector.clear();

    const auto plugins = audioEngine.getPluginChain().getPluginsForUi();

    for (int i = 0; i < plugins.size(); ++i)
    {
        const auto& plugin = plugins.getReference (i);
        const auto label = plugin.name + " [" + plugin.pluginFormatName + "]";
        pluginSelector.addItem (label, i + 1);
    }

    if (plugins.size() > 0)
        pluginSelector.setSelectedId (1, juce::dontSendNotification);
}

void VstSettingsPanel::updatePluginLabel()
{
    pluginNameLabel.setText ("Loaded: " + audioEngine.getPluginChain().getLoadedPluginName(),
                             juce::dontSendNotification);
}

void VstSettingsPanel::loadSelectedPlugin()
{
    const int index = pluginSelector.getSelectedItemIndex();

    if (index < 0)
        return;

    const auto plugins = audioEngine.getPluginChain().getPluginsForUi();

    if (juce::isPositiveAndBelow (index, plugins.size()))
        audioEngine.getPluginChain().loadPlugin (plugins.getReference (index));
}

void VstSettingsPanel::browseForPluginFile()
{
    fileChooser = std::make_unique<juce::FileChooser> ("Select a plugin file",
                                                       juce::File{},
                                                       "*.vst3;*.dll;*.vst");

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectFiles,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto file = fc.getResult();

                                  if (file.existsAsFile())
                                      audioEngine.getPluginChain().loadPluginFromFile (file);
                              });
}

void VstSettingsPanel::addScanDirectory()
{
    const auto categoryName = PluginScanPaths::getCategoryDisplayName (getActiveCategory());

    fileChooser = std::make_unique<juce::FileChooser> (categoryName
                                                         + juce::String::fromUTF8 (u8" \u30b9\u30ad\u30e3\u30f3\u5bfe\u8c61\u30d5\u30a9\u30eb\u30c0\u3092\u9078\u629e"),
                                                       juce::File{},
                                                       "*",
                                                       true);

    fileChooser->launchAsync (juce::FileBrowserComponent::openMode | juce::FileBrowserComponent::canSelectDirectories,
                              [this] (const juce::FileChooser& fc)
                              {
                                  const auto dir = fc.getResult();

                                  if (dir.isDirectory())
                                  {
                                      audioEngine.getPluginChain().getScanPaths().addPath (getActiveCategory(), dir);
                                      refreshPathsList();
                                  }
                              });
}

void VstSettingsPanel::removeSelectedDirectory()
{
    const int row = pathsList.getSelectedRow();

    if (audioEngine.getPluginChain().getScanPaths().removePathAt (getActiveCategory(), row))
        refreshPathsList();
}

void VstSettingsPanel::resized()
{
    auto area = getLocalBounds().reduced (12);
    titleLabel.setBounds (area.removeFromTop (24));
    area.removeFromTop (6);

    pathsLabel.setBounds (area.removeFromTop (20));
    formatTabs.setBounds (area.removeFromTop (28));
    area.removeFromTop (4);

    auto pathButtons = area.removeFromTop (28);
    addPathButton.setBounds (pathButtons.removeFromLeft (100).reduced (0, 2));
    removePathButton.setBounds (pathButtons.removeFromLeft (72).reduced (0, 2));
    pathsList.setBounds (area.removeFromTop (130));
    area.removeFromTop (4);
    auto noteArea = area.removeFromTop (40);
    aaxNoteLabel.setBounds (noteArea);
    vstNoteLabel.setBounds (noteArea);
    area.removeFromTop (8);

    hostIdentityLabel.setBounds (area.removeFromTop (20));
    licenseCompatStatusLabel.setBounds (area.removeFromTop (40));
    auto compatButtons = area.removeFromTop (28);
    openLicenseCompatButton.setBounds (compatButtons.removeFromLeft (110).reduced (0, 2));
    installLicenseCompatButton.setBounds (compatButtons.removeFromLeft (220).reduced (0, 2));
    restartLicenseCompatButton.setBounds (compatButtons.removeFromLeft (150).reduced (0, 2));
    area.removeFromTop (6);
    hostIdentityNoteLabel.setBounds (area.removeFromTop (96));
    area.removeFromTop (8);

    pluginNameLabel.setBounds (area.removeFromTop (22));
    pluginSelector.setBounds (area.removeFromTop (28));
    area.removeFromTop (8);

    auto buttons = area.removeFromTop (28);
    const int w = buttons.getWidth() / 5;
    scanButton.setBounds (buttons.removeFromLeft (w).reduced (2, 0));
    loadButton.setBounds (buttons.removeFromLeft (w).reduced (2, 0));
    browseButton.setBounds (buttons.removeFromLeft (w).reduced (2, 0));
    editorButton.setBounds (buttons.removeFromLeft (w).reduced (2, 0));
    clearButton.setBounds (buttons.reduced (2, 0));

    area.removeFromTop (12);
    gainSlider.setBounds (area.removeFromTop (36));
}

void showSettingsDialog (juce::Component& owner,
                         const juce::String& title,
                         std::unique_ptr<juce::Component> content)
{
    const int dialogW = juce::jmax (560, content->getWidth());
    const int dialogH = juce::jmax (640, content->getHeight());

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = title;
    options.content.setOwned (content.release());
    options.componentToCentreAround = &owner;
    options.useNativeTitleBar = true;
    options.resizable = true;
    options.dialogBackgroundColour = DrizzleTheme::panelBackground();

    if (auto* window = options.launchAsync())
        window->setSize (dialogW, dialogH);
}
