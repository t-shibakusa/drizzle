#include "DrizzlePanels.h"
#include <functional>

namespace
{
constexpr int kFaderMeterWidth = 10;

void paintHorizontalMeter (juce::Graphics& g, juce::Rectangle<int> area, float level)
{
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (area.toFloat(), 3.0f);

    const auto filled = area.withWidth (juce::roundToInt ((float) area.getWidth() * juce::jlimit (0.0f, 1.0f, level)));
    juce::ColourGradient grad (DrizzleTheme::meterGreen(), 0, 0,
                               DrizzleTheme::meterRed(), (float) area.getRight(), 0, false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (filled.toFloat(), 3.0f);
}

void paintVerticalMeter (juce::Graphics& g, juce::Rectangle<int> area, float level)
{
    g.setColour (juce::Colours::black.withAlpha (0.35f));
    g.fillRoundedRectangle (area.toFloat(), 2.0f);

    auto filled = area.removeFromBottom (juce::roundToInt ((float) area.getHeight() * juce::jlimit (0.0f, 1.0f, level)));
    juce::ColourGradient grad (DrizzleTheme::meterGreen(), 0, (float) area.getBottom(),
                               DrizzleTheme::meterRed(), 0, 0, false);
    g.setGradientFill (grad);
    g.fillRoundedRectangle (filled.toFloat(), 2.0f);
}
} // namespace

namespace
{
class InputSelectionPanel final : public juce::Component,
                                  private juce::ListBoxModel
{
public:
    InputSelectionPanel (AudioEngine& engine, int trackIndexIn, std::function<void()> onSelected)
        : audioEngine (engine),
          trackIndex (trackIndexIn),
          onSelectedCallback (std::move (onSelected))
    {
        titleLabel.setText (juce::String::fromUTF8 (u8"\u5165\u529b\u9078\u629e"),
                            juce::dontSendNotification);
        DrizzleTheme::applyLabel (titleLabel);

        options = audioEngine.getAvailableInputOptions();
        list.setModel (this);
        list.setRowHeight (24);
        list.selectRow (findRowForChannel (audioEngine.getTrackInputChannel (trackIndex)));

        okButton.setButtonText (juce::String::fromUTF8 (u8"\u9078\u629e"));
        cancelButton.setButtonText (juce::String::fromUTF8 (u8"\u30ad\u30e3\u30f3\u30bb\u30eb"));

        okButton.onClick = [this] { applySelection(); };
        cancelButton.onClick = [this]
        {
            if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
                dw->exitModalState (0);
        };

        addAndMakeVisible (titleLabel);
        addAndMakeVisible (list);
        addAndMakeVisible (okButton);
        addAndMakeVisible (cancelButton);

        setSize (420, 320);
    }

    void resized() override
    {
        auto area = getLocalBounds().reduced (12);
        titleLabel.setBounds (area.removeFromTop (24));
        area.removeFromTop (8);
        auto buttons = area.removeFromBottom (32);
        okButton.setBounds (buttons.removeFromRight (90).reduced (2));
        cancelButton.setBounds (buttons.removeFromRight (90).reduced (2));
        list.setBounds (area);
    }

private:
    int getNumRows() override { return options.size(); }

    void paintListBoxItem (int row, juce::Graphics& g, int width, int height, bool selected) override
    {
        if (! juce::isPositiveAndBelow (row, options.size()))
            return;

        if (selected)
            g.fillAll (DrizzleTheme::accent().withAlpha (0.35f));

        g.setColour (DrizzleTheme::textPrimary());
        g.setFont (juce::FontOptions { 13.0f });
        g.drawText (options.getReference (row).label, 6, 0, width - 12, height, juce::Justification::centredLeft);
    }

    void listBoxItemDoubleClicked (int row, const juce::MouseEvent&) override
    {
        list.selectRow (row);
        applySelection();
    }

    int findRowForChannel (int channel) const
    {
        for (int i = 0; i < options.size(); ++i)
            if (options.getReference (i).channelIndex == channel)
                return i;

        return 0;
    }

    void applySelection()
    {
        const int row = list.getSelectedRow();

        if (juce::isPositiveAndBelow (row, options.size()))
            audioEngine.setTrackInputChannel (trackIndex, options.getReference (row).channelIndex);

        audioEngine.saveSessionSettings();

        if (onSelectedCallback)
            onSelectedCallback();

        if (auto* dw = findParentComponentOfClass<juce::DialogWindow>())
            dw->exitModalState (1);
    }

    AudioEngine& audioEngine;
    int trackIndex;
    juce::Array<AudioInputOption> options;
    juce::Label titleLabel;
    juce::ListBox list { "Inputs", this };
    juce::TextButton okButton;
    juce::TextButton cancelButton;
    std::function<void()> onSelectedCallback;
};
} // namespace

namespace
{
constexpr int kTrackRowHeight      = 110;
constexpr int kAddTrackButtonHeight = 52;
constexpr int kMasterSectionHeight  = 130;
constexpr int kPanelTitleHeight     = 28;
constexpr int kFaderWidth           = 42;
constexpr int kFaderMeterGap        = 12;
} // namespace

//==============================================================================
class TrackMixerPanel::TracksListContent final : public juce::Component
{
public:
    int getContentHeight() const noexcept
    {
        return juce::jmax (1, owner.getNumTrackRows()) * kTrackRowHeight;
    }

    void resized() override
    {
        owner.layoutTracksList();
    }

    explicit TracksListContent (TrackMixerPanel& panelIn)
        : owner (panelIn)
    {
    }

private:
    TrackMixerPanel& owner;
};

class TrackMixerPanel::TrackRowComponent final : public juce::Component
{
public:
    TrackRowComponent (TrackMixerPanel& panelIn,
                       AudioEngine& engineIn,
                       MixerFaderLookAndFeel& lafIn,
                       int index)
        : panel (panelIn),
          audioEngine (engineIn),
          faderLookAndFeel (lafIn),
          trackIndex (index)
    {
        nameLabel.setJustificationType (juce::Justification::centredLeft);
        nameLabel.setEditable (true, false, false);
        DrizzleTheme::applyLabel (nameLabel, true);
        nameLabel.setFont (juce::FontOptions { 14.0f }.withStyle ("Bold"));
        nameLabel.setColour (juce::Label::textWhenEditingColourId, DrizzleTheme::textPrimary());
        nameLabel.setColour (juce::Label::backgroundWhenEditingColourId, juce::Colours::transparentBlack);
        nameLabel.setColour (juce::Label::outlineWhenEditingColourId, juce::Colours::transparentBlack);
        nameLabel.onEditorHide = [this]
        {
            juce::String text = nameLabel.getText();

            if (auto* editor = nameLabel.getCurrentTextEditor())
                text = editor->getText();

            audioEngine.setTrackName (trackIndex, text);
            nameLabel.setText (audioEngine.getTrackName (trackIndex), juce::dontSendNotification);
        };

        removeButton.setButtonText (juce::String::fromUTF8 (u8"\u00d7"));
        removeButton.setTooltip (juce::String::fromUTF8 (u8"\u30c8\u30e9\u30c3\u30af\u3092\u524a\u9664"));
        removeButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        removeButton.setColour (juce::TextButton::textColourOffId, DrizzleTheme::textMuted());
        removeButton.onClick = [this] { panel.confirmRemoveTrack (trackIndex); };

        inputButton.setClickingTogglesState (false);
        inputButton.setColour (juce::TextButton::buttonColourId, juce::Colours::transparentBlack);
        inputButton.setColour (juce::TextButton::buttonOnColourId, juce::Colours::transparentBlack);
        inputButton.setColour (juce::TextButton::textColourOffId, DrizzleTheme::textMuted());
        inputButton.onClick = [this] { panel.showInputSelectionForTrack (trackIndex); };

        if (trackIndex == 2)
        {
            vstLabel.setText ("VST", juce::dontSendNotification);
            DrizzleTheme::applyLabel (vstLabel, true);
        }

        soloButton.onClick = [this]
        {
            panel.commitAllNameEdits();
            audioEngine.setTrackSolo (trackIndex, soloButton.getToggleState());
            panel.updateTrackButtonStyles (*this);
        };
        muteButton.onClick = [this]
        {
            panel.commitAllNameEdits();
            audioEngine.setTrackMute (trackIndex, muteButton.getToggleState());
            panel.updateTrackButtonStyles (*this);
        };

        panSlider.setSliderStyle (juce::Slider::RotaryHorizontalVerticalDrag);
        panSlider.setTextBoxStyle (juce::Slider::NoTextBox, false, 0, 0);
        panSlider.setRange (0.0, 1.0, 0.01);
        panSlider.onValueChange = [this]
        {
            panel.commitAllNameEdits();
            audioEngine.setTrackPan (trackIndex, (float) panSlider.getValue());
        };

        applyMixerFaderStyle (faderSlider, faderLookAndFeel);
        faderSlider.onValueChange = [this]
        {
            panel.commitAllNameEdits();
            audioEngine.setTrackGain (trackIndex, (float) faderSlider.getValue());
        };

        addAndMakeVisible (nameLabel);
        addAndMakeVisible (removeButton);
        addAndMakeVisible (inputButton);
        addAndMakeVisible (soloButton);
        addAndMakeVisible (muteButton);
        addAndMakeVisible (panSlider);
        addAndMakeVisible (faderSlider);

        if (trackIndex == 2)
            addAndMakeVisible (vstLabel);

        syncFromEngine();
    }

    ~TrackRowComponent() override
    {
        faderSlider.setLookAndFeel (nullptr);
    }

    void paint (juce::Graphics& g) override
    {
        auto meterArea = faderSlider.getBounds().translated (-kFaderMeterGap, 0)
                            .withWidth (kFaderMeterWidth)
                            .withHeight (faderSlider.getHeight());

        if (! meterArea.isEmpty())
            paintVerticalMeter (g, meterArea, meterLevel);
    }

    void resized() override
    {
        auto bounds = getLocalBounds().reduced (4, 2);

        auto faderStrip = bounds.removeFromRight (kFaderWidth + kFaderMeterGap + kFaderMeterWidth);
        faderSlider.setBounds (faderStrip.removeFromRight (kFaderWidth));

        auto header = bounds.removeFromTop (22);
        removeButton.setBounds (header.removeFromRight (24).reduced (2));
        nameLabel.setBounds (header);

        auto sub = bounds.removeFromTop (18);
        inputButton.setBounds (sub.removeFromLeft (sub.getWidth() * 2 / 3));
        vstLabel.setBounds (sub);

        auto controls = bounds.removeFromBottom (26);
        soloButton.setBounds (controls.removeFromLeft (26));
        muteButton.setBounds (controls.removeFromLeft (26));
        panSlider.setBounds (controls.removeFromLeft (38).reduced (0, 2));
    }

    void syncFromEngine()
    {
        if (! nameLabel.isBeingEdited())
            nameLabel.setText (audioEngine.getTrackName (trackIndex), juce::dontSendNotification);

        inputButton.setButtonText (audioEngine.getTrackInputLabel (trackIndex));
        faderSlider.setValue (audioEngine.getTrackGain (trackIndex), juce::dontSendNotification);
        panSlider.setValue (audioEngine.getTrackPan (trackIndex), juce::dontSendNotification);
        soloButton.setToggleState (audioEngine.getTrackSolo (trackIndex), juce::dontSendNotification);
        muteButton.setToggleState (audioEngine.getTrackMute (trackIndex), juce::dontSendNotification);
        panel.updateTrackButtonStyles (*this);

        removeButton.setVisible (audioEngine.getNumTracks() > TrackMixerProcessor::minTracks);
    }

    void setMeterLevel (float level) noexcept { meterLevel = level; }

    int trackIndex = 0;
    float meterLevel = 0.0f;

    juce::Label nameLabel;
    juce::TextButton removeButton;
    juce::TextButton inputButton;
    juce::Label vstLabel;
    juce::TextButton soloButton { "S" };
    juce::TextButton muteButton { "M" };
    juce::Slider panSlider;
    juce::Slider faderSlider;

private:
    TrackMixerPanel& panel;
    AudioEngine& audioEngine;
    MixerFaderLookAndFeel& faderLookAndFeel;
};

//==============================================================================
TrackMixerPanel::TrackMixerPanel (AudioEngine& engine)
    : audioEngine (engine)
{
    tracksListContent = std::make_unique<TracksListContent> (*this);
    tracksViewport.setViewedComponent (tracksListContent.get(), false);
    tracksViewport.setScrollBarsShown (true, false);
    tracksViewport.setScrollBarThickness (12);
    tracksViewport.getVerticalScrollBar().setColour (juce::ScrollBar::thumbColourId, DrizzleTheme::accent().withAlpha (0.7f));
    tracksViewport.getVerticalScrollBar().setColour (juce::ScrollBar::trackColourId, DrizzleTheme::panelBackground().darker (0.2f));

    addTrackButton.setColour (juce::TextButton::buttonColourId, DrizzleTheme::accent().withAlpha (0.35f));
    addTrackButton.setColour (juce::TextButton::textColourOffId, DrizzleTheme::textPrimary());
    addTrackButton.onClick = [this]
    {
        commitAllNameEdits();

        if (audioEngine.addTrack())
            rebuildTrackList();
    };

    masterTitle.setText (juce::String::fromUTF8 (u8"\u30e1\u30a4\u30f3\u51fa\u529b"), juce::dontSendNotification);
    DrizzleTheme::applyLabel (masterTitle);
    applyMixerFaderStyle (masterFader, faderLookAndFeel);
    masterFader.setValue (audioEngine.getMasterGain(), juce::dontSendNotification);
    masterFader.onValueChange = [this]
    {
        commitAllNameEdits();
        audioEngine.setMasterGain ((float) masterFader.getValue());
    };
    masterMuteButton.setClickingTogglesState (true);
    masterMuteButton.onClick = [this]
    {
        commitAllNameEdits();
        audioEngine.setMasterMute (masterMuteButton.getToggleState());
    };
    masterMonoButton.setClickingTogglesState (true);
    masterMonoButton.onClick = [this]
    {
        commitAllNameEdits();
        audioEngine.setMasterMono (masterMonoButton.getToggleState());
    };

    addAndMakeVisible (tracksViewport);
    addAndMakeVisible (addTrackButton);
    addAndMakeVisible (masterTitle);
    addAndMakeVisible (masterFader);
    addAndMakeVisible (masterMuteButton);
    addAndMakeVisible (masterMonoButton);

    rebuildTrackList();
    startTimerHz (20);
}

int TrackMixerPanel::getNumTrackRows() const
{
    return audioEngine.getNumTracks();
}

TrackMixerPanel::~TrackMixerPanel()
{
    prepareForShutdown();
    trackRows.clear();
    masterFader.setLookAndFeel (nullptr);
}

void TrackMixerPanel::rebuildTrackList()
{
    commitAllNameEdits();
    trackRows.clear();
    tracksListContent->removeAllChildren();

    for (int i = 0; i < audioEngine.getNumTracks(); ++i)
    {
        auto* row = trackRows.add (new TrackRowComponent (*this, audioEngine, faderLookAndFeel, i));
        tracksListContent->addAndMakeVisible (row);
    }

    layoutTracksList();
    updateAddTrackButton();
    resized();
}

void TrackMixerPanel::layoutTracksList()
{
    for (int i = 0; i < trackRows.size(); ++i)
        trackRows.getUnchecked (i)->setBounds (0, i * kTrackRowHeight, tracksListContent->getWidth(), kTrackRowHeight);

    tracksListContent->setSize (tracksListContent->getWidth(), tracksListContent->getContentHeight());
}

void TrackMixerPanel::updateAddTrackButton()
{
    const bool canAdd = audioEngine.getNumTracks() < TrackMixerProcessor::maxTracks;
    addTrackButton.setVisible (canAdd);
    addTrackButton.setEnabled (canAdd);
}

void TrackMixerPanel::commitAllNameEdits()
{
    for (auto* row : trackRows)
        if (row->nameLabel.isBeingEdited())
            row->nameLabel.hideEditor (false);
}

void TrackMixerPanel::prepareForShutdown()
{
    stopTimer();
    commitAllNameEdits();
}

void TrackMixerPanel::updateTrackButtonStyles (TrackRowComponent& row)
{
    DrizzleTheme::applyTrackToggleButton (row.soloButton,
                                          audioEngine.getTrackSolo (row.trackIndex),
                                          false);
    DrizzleTheme::applyTrackToggleButton (row.muteButton,
                                          audioEngine.getTrackMute (row.trackIndex),
                                          true);
}

void TrackMixerPanel::reloadFromEngine()
{
    rebuildTrackList();
    syncUIFromEngine();
}

void TrackMixerPanel::syncUIFromEngine()
{
    for (auto* row : trackRows)
        row->syncFromEngine();

    masterFader.setValue (audioEngine.getMasterGain(), juce::dontSendNotification);
    masterMuteButton.setToggleState (audioEngine.getMasterMute(), juce::dontSendNotification);
    masterMonoButton.setToggleState (audioEngine.getMasterMono(), juce::dontSendNotification);
    updateAddTrackButton();
}

void TrackMixerPanel::confirmRemoveTrack (int trackIndex)
{
    if (audioEngine.getNumTracks() <= TrackMixerProcessor::minTracks)
        return;

    const auto trackName = audioEngine.getTrackName (trackIndex);
    const auto message = juce::String::fromUTF8 (u8"\u30c8\u30e9\u30c3\u30af\u300c") + trackName
                       + juce::String::fromUTF8 (u8"\u300d\u3092\u524a\u9664\u3057\u307e\u3059\u304b\uff1f");

    juce::NativeMessageBox::showOkCancelBox (juce::MessageBoxIconType::WarningIcon,
                                             juce::String::fromUTF8 (u8"\u30c8\u30e9\u30c3\u30af\u524a\u9664"),
                                             message,
                                             this,
                                             juce::ModalCallbackFunction::create ([this, trackIndex] (int result)
    {
        if (result != 0 && audioEngine.removeTrack (trackIndex))
            rebuildTrackList();
    }));
}

void TrackMixerPanel::showInputSelectionForTrack (int trackIndex)
{
    commitAllNameEdits();

    auto panel = std::make_unique<InputSelectionPanel> (audioEngine, trackIndex, [this]
    {
        syncUIFromEngine();
    });

    juce::DialogWindow::LaunchOptions options;
    options.dialogTitle = juce::String::fromUTF8 (u8"\u5165\u529b\u9078\u629e");
    options.content.setOwned (panel.release());
    options.componentToCentreAround = this;
    options.useNativeTitleBar = true;
    options.resizable = false;
    options.dialogBackgroundColour = DrizzleTheme::panelBackground();
    options.launchAsync();
}

void TrackMixerPanel::timerCallback()
{
    for (auto* row : trackRows)
        row->setMeterLevel (audioEngine.getTrackPeakLevel (row->trackIndex));

    for (auto* row : trackRows)
        row->repaint();
}

void TrackMixerPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    DrizzleTheme::paintPanel (g, bounds, juce::String::fromUTF8 (u8"\u30c8\u30e9\u30c3\u30af"));

    auto masterMeter = masterFader.getBounds().translated (-kFaderMeterGap, 0).withWidth (kFaderMeterWidth);
    if (! masterMeter.isEmpty())
        paintVerticalMeter (g, masterMeter, 0.6f);
}

void TrackMixerPanel::resized()
{
    auto area = getLocalBounds().reduced (6);
    area.removeFromTop (kPanelTitleHeight);

    auto masterArea = area.removeFromBottom (kMasterSectionHeight).reduced (4, 2);
    if (addTrackButton.isVisible())
        addTrackButton.setBounds (area.removeFromBottom (kAddTrackButtonHeight).reduced (2, 4));

    tracksViewport.setBounds (area.reduced (2));

    const int viewportWidth = juce::jmax (1, tracksViewport.getWidth());
    tracksListContent->setSize (viewportWidth, tracksListContent->getContentHeight());
    layoutTracksList();

    auto masterFaderStrip = masterArea.removeFromRight (kFaderWidth + kFaderMeterGap + kFaderMeterWidth);
    masterFader.setBounds (masterFaderStrip.removeFromRight (kFaderWidth));
    masterTitle.setBounds (masterArea.removeFromTop (20));
    auto masterControls = masterArea.removeFromBottom (26);
    masterMuteButton.setBounds (masterControls.removeFromLeft (60));
    masterMonoButton.setBounds (masterControls.removeFromLeft (60));
}

//==============================================================================
StreamPreviewPanel::StreamPreviewPanel()
{
    startTimerHz (4);
}

StreamPreviewPanel::~StreamPreviewPanel()
{
    stopTimer();
}

void StreamPreviewPanel::paint (juce::Graphics& g)
{
    auto bounds = getLocalBounds();
    DrizzleTheme::paintPanel (g, bounds, juce::String::fromUTF8 (u8"\u914d\u4fe1\u30d7\u30ec\u30d3\u30e5\u30fc"));

    auto content = bounds.reduced (8);
    content.removeFromTop (28);
    auto preview = content.removeFromTop (content.getHeight() - 36).reduced (2);
    auto meterArea = content;

    g.setColour (juce::Colour (0xff2b3140));
    g.fillRoundedRectangle (preview.toFloat(), 6.0f);

    g.setColour (DrizzleTheme::textMuted());
    g.setFont (juce::FontOptions { 22.0f }.withStyle ("Bold"));
    g.drawText ("Drizzle LIVE STREAMING", preview, juce::Justification::centred);

    g.setColour (DrizzleTheme::accentRed());
    g.fillEllipse (preview.getRight() - 120.0f, preview.getY() + 12.0f, 10.0f, 10.0f);
    g.setColour (DrizzleTheme::textPrimary());
    g.setFont (juce::FontOptions { 14.0f }.withStyle ("Bold"));
    g.drawText ("LIVE  " + liveTimer, preview.removeFromTop (36).removeFromRight (140),
                juce::Justification::centredRight);

    paintHorizontalMeter (g, meterArea, masterLevel);
    g.setColour (DrizzleTheme::textMuted());
    g.drawText ("-6.2 dB", meterArea.removeFromRight (56), juce::Justification::centredRight);
}

void StreamPreviewPanel::resized() {}

void StreamPreviewPanel::timerCallback()
{
    masterLevel = 0.35f + 0.25f * std::sin ((float) juce::Time::getMillisecondCounterHiRes() * 0.002f);
    repaint();
}

//==============================================================================
StreamSettingsPanel::StreamSettingsPanel()
{
    serviceLabel.setText (juce::String::fromUTF8 (u8"\u914d\u4fe1\u30b5\u30fc\u30d3\u30b9"), juce::dontSendNotification);
    titleLabel.setText (juce::String::fromUTF8 (u8"\u30bf\u30a4\u30c8\u30eb"), juce::dontSendNotification);
    DrizzleTheme::applyLabel (serviceLabel);
    DrizzleTheme::applyLabel (titleLabel);

    serviceBox.addItem ("YouTube Live", 1);
    serviceBox.setSelectedId (1, juce::dontSendNotification);

    titleEditor.setMultiLine (false);
    titleEditor.setText (juce::String::fromUTF8 (u8"\u5f39\u304d\u8a9e\u308a\u30e9\u30a4\u30d6\u914d\u4fe1"));

    streamButton.setColour (juce::TextButton::buttonColourId, DrizzleTheme::accentRed());

    addAndMakeVisible (serviceLabel);
    addAndMakeVisible (serviceBox);
    addAndMakeVisible (titleLabel);
    addAndMakeVisible (titleEditor);
    addAndMakeVisible (testButton);
    addAndMakeVisible (streamButton);
}

void StreamSettingsPanel::paint (juce::Graphics& g)
{
    DrizzleTheme::paintPanel (g, getLocalBounds(), juce::String::fromUTF8 (u8"\u914d\u4fe1\u8a2d\u5b9a"));

    auto area = getLocalBounds().reduced (8);
    area.removeFromTop (28);
    area.removeFromLeft (area.getWidth() * 2 / 3);
    auto scenes = area.removeFromLeft (area.getWidth() / 2).reduced (6, 8);
    auto sources = area.reduced (6, 8);

    g.setColour (DrizzleTheme::textMuted());
    g.setFont (juce::FontOptions { 12.0f }.withStyle ("Bold"));
    g.drawText (juce::String::fromUTF8 (u8"\u30b7\u30fc\u30f3"), scenes.removeFromTop (18), juce::Justification::centredLeft);
    g.drawText (juce::String::fromUTF8 (u8"\u30bd\u30fc\u30b9"), sources.removeFromTop (18), juce::Justification::centredLeft);

    g.setFont (juce::FontOptions { 13.0f });
    g.setColour (DrizzleTheme::textPrimary());
    const juce::StringArray sceneItems {
        juce::String::fromUTF8 (u8"\u30e1\u30a4\u30f3\u30b7\u30fc\u30f3"),
        juce::String::fromUTF8 (u8"\u30c8\u30fc\u30af\u30b7\u30fc\u30f3"),
        juce::String::fromUTF8 (u8"\u6f14\u594f\u30b7\u30fc\u30f3"),
        juce::String::fromUTF8 (u8"\u5f85\u6a5f\u753b\u9762")
    };
    const juce::StringArray sourceItems {
        juce::String::fromUTF8 (u8"\u6620\u50cf\u30ad\u30e3\u30d7\u30c1\u30e3"),
        "logo.png",
        juce::String::fromUTF8 (u8"\u30bf\u30a4\u30c8\u30eb"),
        juce::String::fromUTF8 (u8"\u30a6\u30a3\u30f3\u30c9\u30a6")
    };

    int y = 0;
    for (const auto& item : sceneItems)
    {
        auto row = scenes.removeFromTop (22);
        g.setColour (item == sceneItems[0] ? DrizzleTheme::accent().withAlpha (0.3f) : juce::Colours::transparentBlack);
        g.fillRect (row);
        g.setColour (DrizzleTheme::textPrimary());
        g.drawText (item, row, juce::Justification::centredLeft);
    }

    for (const auto& item : sourceItems)
    {
        auto row = sources.removeFromTop (22);
        g.drawText (item, row, juce::Justification::centredLeft);
    }
}

void StreamSettingsPanel::resized()
{
    auto area = getLocalBounds().reduced (8);
    area.removeFromTop (28);

    auto left = area.removeFromLeft (area.getWidth() / 3).reduced (4);

    serviceLabel.setBounds (left.removeFromTop (20));
    serviceBox.setBounds (left.removeFromTop (28));
    left.removeFromTop (8);
    titleLabel.setBounds (left.removeFromTop (20));
    titleEditor.setBounds (left.removeFromTop (28));
    left.removeFromTop (12);
    testButton.setBounds (left.removeFromTop (30));
    streamButton.setBounds (left.removeFromBottom (40));
}

//==============================================================================
CommentPanel::CommentPanel()
{
    commentDisplay.setMultiLine (true);
    commentDisplay.setReadOnly (true);
    commentDisplay.setText (
        "Yuki: " + juce::String::fromUTF8 (u8"\u3044\u3044\u97f3\u3059\u304d\u3067\u3059\u306d\uff01") + "\n"
        "Taro: " + juce::String::fromUTF8 (u8"\u30ea\u30af\u30a8\u30b9\u30c8\u3042\u308a\u304c\u3068\u3046\u3054\u3056\u3044\u307e\u3059") + "\n"
        "Mika: " + juce::String::fromUTF8 (u8"\u30ae\u30bf\u306e\u97f3\u8272\u304c\u304d\u308c\u3044") + "\n");

    viewerLabel.setText (juce::String::fromUTF8 (u8"\u8996\u8074\u8005\u6570: 128"), juce::dontSendNotification);
    DrizzleTheme::applyLabel (viewerLabel, true);
    commentInput.setTextToShowWhenEmpty (juce::String::fromUTF8 (u8"\u30e1\u30c3\u30bb\u30fc\u30b8\u3092\u5165\u529b..."), DrizzleTheme::textMuted());

    addAndMakeVisible (commentDisplay);
    addAndMakeVisible (commentInput);
    addAndMakeVisible (sendButton);
    addAndMakeVisible (viewerLabel);
}

void CommentPanel::paint (juce::Graphics& g)
{
    DrizzleTheme::paintPanel (g, getLocalBounds(), "YouTube");
}

void CommentPanel::resized()
{
    auto area = getLocalBounds().reduced (8);
    area.removeFromTop (28);
    viewerLabel.setBounds (area.removeFromBottom (22));
    auto inputRow = area.removeFromBottom (34);
    sendButton.setBounds (inputRow.removeFromRight (64));
    commentInput.setBounds (inputRow.reduced (0, 2));
    commentDisplay.setBounds (area.reduced (2));
}

//==============================================================================
StatusBarComponent::StatusBarComponent()
{
    statusLabel.setText (juce::String::fromUTF8 (u8"\u6e96\u5099\u5b8c\u4e86"), juce::dontSendNotification);
    statsLabel.setText ("Drop 0 (0.0%)   Frame 6012 (0.0%)", juce::dontSendNotification);
    DrizzleTheme::applyLabel (statusLabel);
    DrizzleTheme::applyLabel (statsLabel, true);
    addAndMakeVisible (statusLabel);
    addAndMakeVisible (statsLabel);
}

void StatusBarComponent::paint (juce::Graphics& g)
{
    g.setColour (DrizzleTheme::panelBackground().darker (0.15f));
    g.fillRect (getLocalBounds());
    g.setColour (DrizzleTheme::panelBorder());
    g.drawHorizontalLine (0, 0.0f, (float) getWidth());
}

void StatusBarComponent::resized()
{
    auto area = getLocalBounds().reduced (10, 4);
    statusLabel.setBounds (area.removeFromLeft (area.getWidth() / 2));
    statsLabel.setBounds (area);
}

//==============================================================================
SystemMetricsBar::SystemMetricsBar() {}

void SystemMetricsBar::paint (juce::Graphics& g)
{
    g.setColour (DrizzleTheme::textMuted());
    g.setFont (juce::FontOptions { 12.0f });
    g.drawText ("CPU 12%   48.0 kHz   Buffer 256 samples",
                getLocalBounds(),
                juce::Justification::centredRight);
}
