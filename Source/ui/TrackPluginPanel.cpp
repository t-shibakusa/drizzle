#include "TrackPluginPanel.h"

namespace
{
constexpr int kSidebarWidth = 200;
} // namespace

class TrackPluginPanel::AppliedListModel final : public juce::ListBoxModel
{
public:
    AppliedListModel (TrackPluginPanel& ownerIn)
        : owner (ownerIn)
    {
    }

    int getNumRows() override
    {
        return owner.audioEngine.getPluginChain().getNumTrackPlugins (owner.trackIndex);
    }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (rowIsSelected)
            g.fillAll (DrizzleTheme::accent().withAlpha (0.35f));

        g.setColour (DrizzleTheme::textPrimary());
        g.setFont (juce::FontOptions { 13.0f });
        g.drawText (owner.audioEngine.getPluginChain().getTrackPluginName (owner.trackIndex, row),
                    6, 0, width - 10, height,
                    juce::Justification::centredLeft);
    }

    void listBoxItemClicked (int row, const juce::MouseEvent&) override
    {
        if (row < 0)
            return;

        owner.appliedList.selectRow (row);
        owner.updateButtons();
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
    {
        if (row < 0)
            return;

        owner.appliedList.selectRow (row);
        owner.audioEngine.getPluginChain().showTrackPluginEditor (owner.trackIndex,
                                                                  row,
                                                                  owner.getModalParentComponent());
        owner.updateButtons();
    }

private:
    TrackPluginPanel& owner;
};

class TrackPluginPanel::AvailableListModel final : public juce::ListBoxModel
{
public:
    explicit AvailableListModel (TrackPluginPanel& ownerIn)
        : owner (ownerIn)
    {
    }

    int getNumRows() override
    {
        return plugins.size();
    }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool rowIsSelected) override
    {
        if (! juce::isPositiveAndBelow (row, plugins.size()))
            return;

        if (rowIsSelected)
            g.fillAll (DrizzleTheme::accent().withAlpha (0.25f));

        const auto& plugin = plugins.getReference (row);
        g.setColour (DrizzleTheme::textPrimary());
        g.setFont (juce::FontOptions { 13.0f });
        g.drawText (plugin.name + "  [" + plugin.pluginFormatName + "]",
                    6, 0, width - 10, height,
                    juce::Justification::centredLeft);
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
    {
        addPluginAtRow (row);
    }

    juce::Array<juce::PluginDescription> plugins;

private:
    void addPluginAtRow (int row)
    {
        if (! juce::isPositiveAndBelow (row, plugins.size()))
            return;

        owner.audioEngine.getPluginChain().addTrackPlugin (owner.trackIndex, plugins.getReference (row));
        owner.refreshAppliedList();
        owner.updateButtons();
    }

    TrackPluginPanel& owner;
};

TrackPluginPanel::TrackPluginPanel (AudioEngine& engine, int trackIndexIn, std::function<void()> onCloseIn)
    : audioEngine (engine),
      trackIndex (trackIndexIn),
      onClose (std::move (onCloseIn)),
      appliedList ("applied", nullptr),
      availableList ("available", nullptr)
{
    const auto trackName = audioEngine.getTrackName (trackIndex);
    titleLabel.setText (trackName + juce::String::fromUTF8 (u8" - VST"),
                        juce::dontSendNotification);
    DrizzleTheme::applyLabel (titleLabel, true);
    titleLabel.setFont (juce::FontOptions { 15.0f }.withStyle ("Bold"));

    appliedTitleLabel.setText (juce::String::fromUTF8 (u8"\u9069\u7528\u4e2d"),
                               juce::dontSendNotification);
    availableTitleLabel.setText (juce::String::fromUTF8 (u8"\u8ffd\u52a0\u53ef\u80fd"),
                                 juce::dontSendNotification);
    DrizzleTheme::applyLabel (appliedTitleLabel);
    DrizzleTheme::applyLabel (availableTitleLabel);

    formatTabs.addTab ("VST3", juce::Colours::darkgrey, 0);
    formatTabs.addTab ("VST", juce::Colours::darkgrey, 1);
    formatTabs.addTab (juce::String::fromUTF8 (u8"\u3059\u3079\u3066"), juce::Colours::darkgrey, 2);
    formatTabs.setCurrentTabIndex (0);
    formatTabs.addChangeListener (this);

    appliedList.setRowHeight (24);
    availableList.setRowHeight (24);

    appliedModel = std::make_unique<AppliedListModel> (*this);
    availableModel = std::make_unique<AvailableListModel> (*this);
    appliedList.setModel (appliedModel.get());
    availableList.setModel (availableModel.get());

    removeButton.onClick = [this]
    {
        const int row = appliedList.getSelectedRow();

        if (row < 0)
            return;

        audioEngine.getPluginChain().removeTrackPlugin (trackIndex, row);
        refreshAppliedList();
        updateButtons();
    };

    closeButton.onClick = [this]
    {
        if (onClose != nullptr)
            onClose();
    };

    emptyHintDisplay.setMultiLine (true);
    emptyHintDisplay.setReadOnly (true);
    emptyHintDisplay.setScrollbarsShown (false);
    emptyHintDisplay.setCaretVisible (false);
    emptyHintDisplay.setFont (juce::FontOptions { 13.0f });
    emptyHintDisplay.setColour (juce::TextEditor::backgroundColourId, juce::Colours::transparentBlack);
    emptyHintDisplay.setColour (juce::TextEditor::outlineColourId, juce::Colours::transparentBlack);
    emptyHintDisplay.setColour (juce::TextEditor::textColourId, DrizzleTheme::textMuted());

    scanButton.onClick = [this]
    {
        scanButton.setEnabled (false);
        audioEngine.getPluginChain().scanPlugins();
        refreshAvailableList();
        scanButton.setEnabled (true);
    };

    addAndMakeVisible (titleLabel);
    addAndMakeVisible (appliedTitleLabel);
    addAndMakeVisible (availableTitleLabel);
    addAndMakeVisible (appliedList);
    addAndMakeVisible (availableList);
    addAndMakeVisible (emptyHintDisplay);
    addAndMakeVisible (scanButton);
    addAndMakeVisible (formatTabs);
    addAndMakeVisible (removeButton);
    addAndMakeVisible (closeButton);

    audioEngine.getPluginChain().addChangeListener (this);
    audioEngine.getPluginChain().getTrackPluginManager().addChangeListener (this);
    refreshAvailableList();
    refreshAppliedList();
    updateButtons();

    setSize (720, 520);
}

TrackPluginPanel::~TrackPluginPanel()
{
    formatTabs.removeChangeListener (this);
    audioEngine.getPluginChain().removeChangeListener (this);
    audioEngine.getPluginChain().getTrackPluginManager().removeChangeListener (this);
    appliedList.setModel (nullptr);
    availableList.setModel (nullptr);
}

juce::Component* TrackPluginPanel::getModalParentComponent() const
{
    if (auto* dialog = findParentComponentOfClass<juce::DialogWindow>())
        return dialog;

    return getTopLevelComponent();
}

void TrackPluginPanel::paint (juce::Graphics& g)
{
    g.fillAll (DrizzleTheme::panelBackground());

    auto sidebar = getLocalBounds().reduced (12);
    sidebar.removeFromTop (36);
    sidebar.setWidth (kSidebarWidth);

    g.setColour (DrizzleTheme::panelBorder().withAlpha (0.5f));
    g.drawLine ((float) sidebar.getRight(), (float) sidebar.getY(),
                (float) sidebar.getRight(), (float) getHeight() - 12.0f, 1.0f);
}

void TrackPluginPanel::resized()
{
    auto bounds = getLocalBounds().reduced (12);

    auto header = bounds.removeFromTop (32);
    closeButton.setBounds (header.removeFromRight (80).reduced (0, 4));
    titleLabel.setBounds (header);

    bounds.removeFromBottom (8);
    auto footer = bounds.removeFromBottom (34);
    removeButton.setBounds (footer.removeFromLeft (100).reduced (0, 4));

    auto sidebar = bounds.removeFromLeft (kSidebarWidth);
    sidebar.removeFromRight (8);
    appliedTitleLabel.setBounds (sidebar.removeFromTop (22));
    appliedList.setBounds (sidebar);

    availableTitleLabel.setBounds (bounds.removeFromTop (22));
    auto tabsArea = bounds.removeFromTop (28);
    formatTabs.setBounds (tabsArea);

    auto scanRow = bounds.removeFromBottom (30);
    scanButton.setBounds (scanRow.removeFromRight (100).reduced (0, 4));

    if (emptyHintDisplay.isVisible())
        emptyHintDisplay.setBounds (bounds.reduced (4));
    else
        availableList.setBounds (bounds);
}

void TrackPluginPanel::changeListenerCallback (juce::ChangeBroadcaster* source)
{
    if (source == &formatTabs)
    {
        refreshAvailableList();
        return;
    }

    if (source == &audioEngine.getPluginChain())
    {
        refreshAvailableList();
        return;
    }

    if (source == &audioEngine.getPluginChain().getTrackPluginManager())
    {
        refreshAppliedList();
        updateButtons();
    }
}

void TrackPluginPanel::refreshAppliedList()
{
    appliedList.updateContent();
    appliedList.repaint();

    const int count = audioEngine.getPluginChain().getNumTrackPlugins (trackIndex);

    if (count > 0 && appliedList.getSelectedRow() < 0)
        appliedList.selectRow (0);
}

void TrackPluginPanel::refreshAvailableList()
{
    if (availableModel != nullptr)
        availableModel->plugins = getAvailablePluginsForActiveTab();

    availableList.updateContent();
    availableList.repaint();
    updateEmptyState();
}

void TrackPluginPanel::updateEmptyState()
{
    const bool hasPlugins = availableModel != nullptr && availableModel->plugins.size() > 0;

    availableList.setVisible (hasPlugins);
    emptyHintDisplay.setVisible (! hasPlugins);

    if (! hasPlugins)
    {
        const int tab = formatTabs.getCurrentTabIndex();

        if (tab == 1 && ! PluginChain::isVst2HostingEnabled())
        {
            emptyHintDisplay.setText (juce::String::fromUTF8 (
                u8"\u3053\u306e\u30d3\u30eb\u30c9\u3067\u306f VST2 (.dll) \u306f\u672a\u5bfe\u5fdc\u3067\u3059\u3002\n"
                u8"VST3 \u306f\u300cVST3\u300d\u30bf\u30d6\u304b\u3089\u9078\u629e\u3057\u3066\u304f\u3060\u3055\u3044\u3002\n"
                u8"\u6b63\u898f\u30d1\u30b9: C:\\Program Files\\Common Files\\VST3\n"
                u8"VstPlugIns \u7b49\u306e\u30b3\u30d4\u30fc\u304b\u3089\u8aad\u307f\u8fbc\u3080\u3068\u30e9\u30a4\u30bb\u30f3\u30b9\u8a8d\u8a3c\u306b\u5931\u6557\u3059\u308b\u3053\u3068\u304c\u3042\u308a\u307e\u3059\u3002"));
        }
        else if (! audioEngine.getPluginChain().getScanPaths().hasAnyScannablePaths())
        {
            emptyHintDisplay.setText (juce::String::fromUTF8 (
                u8"\u30d7\u30e9\u30b0\u30a4\u30f3\u304c\u898b\u3064\u304b\u308a\u307e\u305b\u3093\u3002\n"
                u8"\u8a2d\u5b9a \u2192 VST\u30d7\u30e9\u30b0\u30a4\u30f3\u8a2d\u5b9a \u3067\u30b9\u30ad\u30e3\u30f3\u5bfe\u8c61\u30d5\u30a9\u30eb\u30c0\u3092\u8ffd\u52a0\u3057\u3066\u304f\u3060\u3055\u3044\u3002"));
        }
        else
        {
            emptyHintDisplay.setText (juce::String::fromUTF8 (
                u8"\u4e00\u89a7\u304c\u7a7a\u3067\u3059\u3002\u300c\u518d\u30b9\u30ad\u30e3\u30f3\u300d\u3092\u62bc\u3059\u304b\u3001"
                u8"\u8a2d\u5b9a \u2192 VST\u30d7\u30e9\u30b0\u30a4\u30f3\u8a2d\u5b9a \u3067 Scan \u3092\u5b9f\u884c\u3057\u3066\u304f\u3060\u3055\u3044\u3002"));
        }
    }

    resized();
}

void TrackPluginPanel::updateButtons()
{
    removeButton.setEnabled (appliedList.getSelectedRow() >= 0);
}

juce::Array<juce::PluginDescription> TrackPluginPanel::getAvailablePluginsForActiveTab() const
{
    const int tab = formatTabs.getCurrentTabIndex();

    if (tab == 0)
        return audioEngine.getPluginChain().getPluginsForUiByFormat ("VST3");

    if (tab == 1)
        return audioEngine.getPluginChain().getPluginsForUiByFormat ("VST");

    return audioEngine.getPluginChain().getPluginsForUi();
}
